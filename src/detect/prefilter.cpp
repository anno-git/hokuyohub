#include "prefilter.h"
#include <algorithm>
#include <chrono>
#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <iostream>
#include <numeric>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Prefilter::Prefilter(const PrefilterConfig& config) : config_(config) {}

// ── SpatialGrid ─────────────────────────────────────────────

void Prefilter::SpatialGrid::build(const std::vector<FilterPoint>& points, float radius) {
    cell_size = radius;
    cells.clear();
    cells.reserve(points.size() / 3 + 16);
    for (size_t i = 0; i < points.size(); ++i) {
        if (!points[i].valid) continue;
        const int ix = static_cast<int>(std::floor(points[i].x / cell_size));
        const int iy = static_cast<int>(std::floor(points[i].y / cell_size));
        cells[cellKey(ix, iy)].push_back(i);
    }
}

size_t Prefilter::SpatialGrid::countNeighbors(const std::vector<FilterPoint>& points,
                                               size_t point_idx, float radius_sq) const {
    const auto& pt = points[point_idx];
    const int ix = static_cast<int>(std::floor(pt.x / cell_size));
    const int iy = static_cast<int>(std::floor(pt.y / cell_size));
    size_t count = 0;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            auto it = cells.find(cellKey(ix + dx, iy + dy));
            if (it == cells.end()) continue;
            for (size_t j : it->second) {
                if (!points[j].valid) continue;
                const float ddx = points[j].x - pt.x;
                const float ddy = points[j].y - pt.y;
                if (ddx * ddx + ddy * ddy <= radius_sq) {
                    ++count;
                }
            }
        }
    }
    return count; // includes self
}

// ── apply ───────────────────────────────────────────────────

Prefilter::FilterResult Prefilter::apply(const std::vector<float>& xy_in,
                                        const std::vector<uint8_t>& sid_in,
                                        const std::vector<float>& dist_in,
                                        const std::vector<float>& intensities) const {
    auto start_time = std::chrono::high_resolution_clock::now();

    stats_.reset();
    stats_.input_points = xy_in.size() / 2;

    if (!config_.enabled || xy_in.empty() || xy_in.size() % 2 != 0 ||
        sid_in.size() != xy_in.size() / 2) {
        FilterResult result;
        result.xy = xy_in;
        result.sid = sid_in;
        result.dist = dist_in;
        result.stats = stats_;
        return result;
    }

    const size_t num_points = xy_in.size() / 2;

    // Check if angle is needed (only for spike/outlier removal)
    const bool need_angle = config_.spike_removal.enabled || config_.outlier_removal.enabled;

    // Convert input to internal format
    std::vector<FilterPoint> points;
    points.reserve(num_points);

    for (size_t i = 0; i < num_points; ++i) {
        FilterPoint pt;
        pt.x = xy_in[2 * i];
        pt.y = xy_in[2 * i + 1];
        pt.sid = sid_in[i];
        pt.range = (i < dist_in.size()) ? dist_in[i] : std::sqrt(pt.x * pt.x + pt.y * pt.y);
        pt.angle = need_angle ? std::atan2(pt.y, pt.x) : 0.0f;
        pt.intensity = (i < intensities.size()) ? intensities[i] : 0.0f;
        pt.valid = true;
        pt.original_index = i;
        points.push_back(pt);
    }

    // Apply filters in sequence
    if (config_.neighborhood.enabled) {
        applyNeighborhoodFilter(points);
    }

    if (config_.spike_removal.enabled) {
        applySpikeRemovalFilter(points);
    }

    if (config_.outlier_removal.enabled) {
        applyOutlierRemovalFilter(points);
    }

    if (config_.intensity_filter.enabled) {
        applyIntensityFilter(points);
    }

    if (config_.isolation_removal.enabled) {
        applyIsolationRemovalFilter(points);
    }

    // Build output
    FilterResult result;
    result.xy.reserve(points.size() * 2);
    result.sid.reserve(points.size());
    result.dist.reserve(points.size());

    for (const auto& pt : points) {
        if (pt.valid) {
            result.xy.push_back(pt.x);
            result.xy.push_back(pt.y);
            result.sid.push_back(pt.sid);
            result.dist.push_back(pt.range);
        }
    }

    stats_.output_points = result.xy.size() / 2;

    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.processing_time_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();

    result.stats = stats_;
    return result;
}

// ── Neighborhood filter (grid-based) ────────────────────────

void Prefilter::applyNeighborhoodFilter(std::vector<FilterPoint>& points) const {
    const auto& cfg = config_.neighborhood;

    // Use max possible radius for grid cell size
    float max_range = 0.0f;
    for (const auto& pt : points) {
        if (pt.valid && pt.range > max_range) max_range = pt.range;
    }
    const float max_radius = cfg.r_base + cfg.r_scale * max_range;
    grid_.build(points, max_radius);

    size_t removed = 0;
    for (size_t i = 0; i < points.size(); ++i) {
        if (!points[i].valid) continue;

        const float radius = cfg.r_base + cfg.r_scale * points[i].range;
        const float radius_sq = radius * radius;
        size_t neighbor_count = grid_.countNeighbors(points, i, radius_sq);

        // neighbor_count includes self
        if (static_cast<int>(neighbor_count) < cfg.k) {
            points[i].valid = false;
            removed++;
        }
    }

    stats_.removed_by_neighborhood = removed;
}

// ── Spike removal (using sorted indices) ────────────────────

void Prefilter::applySpikeRemovalFilter(std::vector<FilterPoint>& points) const {
    const auto& cfg = config_.spike_removal;
    size_t removed = 0;

    // Group points by sensor ID
    std::unordered_map<uint8_t, std::vector<size_t>> sensor_groups;
    for (size_t i = 0; i < points.size(); ++i) {
        if (points[i].valid) {
            sensor_groups[points[i].sid].push_back(i);
        }
    }

    for (auto& [sid, indices] : sensor_groups) {
        // Sort by angle
        std::sort(indices.begin(), indices.end(), [&points](size_t a, size_t b) {
            return points[a].angle < points[b].angle;
        });

        for (size_t j = 0; j < indices.size(); ++j) {
            size_t idx = indices[j];
            if (!points[idx].valid) continue;

            float dr_dtheta = calculateAngularDerivative(points, indices, j);

            if (std::abs(dr_dtheta) > cfg.dr_threshold) {
                points[idx].valid = false;
                removed++;
            }
        }
    }

    stats_.removed_by_spike = removed;
}

// ── Outlier removal (using sorted indices) ──────────────────

void Prefilter::applyOutlierRemovalFilter(std::vector<FilterPoint>& points) const {
    const auto& cfg = config_.outlier_removal;
    size_t removed = 0;

    std::unordered_map<uint8_t, std::vector<size_t>> sensor_groups;
    for (size_t i = 0; i < points.size(); ++i) {
        if (points[i].valid) {
            sensor_groups[points[i].sid].push_back(i);
        }
    }

    for (auto& [sid, indices] : sensor_groups) {
        std::sort(indices.begin(), indices.end(), [&points](size_t a, size_t b) {
            return points[a].angle < points[b].angle;
        });

        for (size_t j = 0; j < indices.size(); ++j) {
            size_t idx = indices[j];
            if (!points[idx].valid) continue;

            float median_range = calculateMovingMedian(points, indices, j, cfg.median_window);
            float deviation = std::abs(points[idx].range - median_range);

            // Calculate local standard deviation within the window
            int half_window = cfg.median_window / 2;
            int start = std::max(0, static_cast<int>(j) - half_window);
            int end = std::min(static_cast<int>(indices.size()) - 1, static_cast<int>(j) + half_window);

            float local_std = 0.0f;
            int count = 0;
            for (int k = start; k <= end; ++k) {
                if (points[indices[k]].valid) {
                    float diff = points[indices[k]].range - median_range;
                    local_std += diff * diff;
                    count++;
                }
            }

            if (count > 1) {
                local_std = std::sqrt(local_std / (count - 1));
                if (deviation > cfg.outlier_threshold * local_std) {
                    points[idx].valid = false;
                    removed++;
                }
            }
        }
    }

    stats_.removed_by_outlier = removed;
}

// ── Intensity filter ────────────────────────────────────────

void Prefilter::applyIntensityFilter(std::vector<FilterPoint>& points) const {
    const auto& cfg = config_.intensity_filter;
    size_t removed = 0;

    for (auto& pt : points) {
        if (!pt.valid) continue;

        if (pt.intensity < cfg.min_intensity) {
            pt.valid = false;
            removed++;
        }
    }

    stats_.removed_by_intensity = removed;
}

// ── Isolation removal (grid-based) ──────────────────────────

void Prefilter::applyIsolationRemovalFilter(std::vector<FilterPoint>& points) const {
    const auto& cfg = config_.isolation_removal;

    // Rebuild grid with isolation radius
    grid_.build(points, cfg.isolation_radius);

    size_t removed = 0;
    for (size_t i = 0; i < points.size(); ++i) {
        if (!points[i].valid) continue;

        const float radius_sq = cfg.isolation_radius * cfg.isolation_radius;
        size_t neighbor_count = grid_.countNeighbors(points, i, radius_sq);

        if (static_cast<int>(neighbor_count) < cfg.min_cluster_size) {
            points[i].valid = false;
            removed++;
        }
    }

    stats_.removed_by_isolation = removed;
}

// ── Angular derivative (O(1) using sorted indices) ──────────

float Prefilter::calculateAngularDerivative(const std::vector<FilterPoint>& points,
                                            const std::vector<size_t>& sorted_indices,
                                            size_t j) const {
    const auto& center_pt = points[sorted_indices[j]];

    // Find prev valid
    float prev_range = center_pt.range, prev_angle = center_pt.angle;
    bool found_prev = false;
    for (size_t k = j; k > 0; --k) {
        if (points[sorted_indices[k - 1]].valid) {
            prev_range = points[sorted_indices[k - 1]].range;
            prev_angle = points[sorted_indices[k - 1]].angle;
            found_prev = true;
            break;
        }
    }

    // Find next valid
    float next_range = center_pt.range, next_angle = center_pt.angle;
    bool found_next = false;
    for (size_t k = j + 1; k < sorted_indices.size(); ++k) {
        if (points[sorted_indices[k]].valid) {
            next_range = points[sorted_indices[k]].range;
            next_angle = points[sorted_indices[k]].angle;
            found_next = true;
            break;
        }
    }

    if (found_prev && found_next) {
        float dtheta = next_angle - prev_angle;
        float dr = next_range - prev_range;
        return (dtheta != 0.0f) ? dr / dtheta : 0.0f;
    } else if (found_prev) {
        float dtheta = center_pt.angle - prev_angle;
        float dr = center_pt.range - prev_range;
        return (dtheta != 0.0f) ? dr / dtheta : 0.0f;
    } else if (found_next) {
        float dtheta = next_angle - center_pt.angle;
        float dr = next_range - center_pt.range;
        return (dtheta != 0.0f) ? dr / dtheta : 0.0f;
    }

    return 0.0f;
}

// ── Moving median (O(W) using sorted indices) ───────────────

float Prefilter::calculateMovingMedian(const std::vector<FilterPoint>& points,
                                       const std::vector<size_t>& sorted_indices,
                                       size_t j, int window_size) const {
    int half_window = window_size / 2;
    int start = std::max(0, static_cast<int>(j) - half_window);
    int end = std::min(static_cast<int>(sorted_indices.size()) - 1, static_cast<int>(j) + half_window);

    std::vector<float> ranges;
    ranges.reserve(end - start + 1);
    for (int k = start; k <= end; ++k) {
        if (points[sorted_indices[k]].valid) {
            ranges.push_back(points[sorted_indices[k]].range);
        }
    }

    if (ranges.empty()) return points[sorted_indices[j]].range;

    size_t mid = ranges.size() / 2;
    std::nth_element(ranges.begin(), ranges.begin() + mid, ranges.end());

    if (ranges.size() % 2 == 0 && mid > 0) {
        float lower = *std::max_element(ranges.begin(), ranges.begin() + mid);
        return (lower + ranges[mid]) / 2.0f;
    }
    return ranges[mid];
}

// ── Strategy management ─────────────────────────────────────

void Prefilter::enableStrategy(const std::string& strategy_name, bool enabled) {
    if (strategy_name == "neighborhood") {
        config_.neighborhood.enabled = enabled;
    } else if (strategy_name == "spike_removal") {
        config_.spike_removal.enabled = enabled;
    } else if (strategy_name == "outlier_removal") {
        config_.outlier_removal.enabled = enabled;
    } else if (strategy_name == "intensity_filter") {
        config_.intensity_filter.enabled = enabled;
    } else if (strategy_name == "isolation_removal") {
        config_.isolation_removal.enabled = enabled;
    }
}

bool Prefilter::isStrategyEnabled(const std::string& strategy_name) const {
    if (strategy_name == "neighborhood") {
        return config_.neighborhood.enabled;
    } else if (strategy_name == "spike_removal") {
        return config_.spike_removal.enabled;
    } else if (strategy_name == "outlier_removal") {
        return config_.outlier_removal.enabled;
    } else if (strategy_name == "intensity_filter") {
        return config_.intensity_filter.enabled;
    } else if (strategy_name == "isolation_removal") {
        return config_.isolation_removal.enabled;
    }
    return false;
}

void Prefilter::setNeighborhoodParams(int k, float r_base, float r_scale) {
    config_.neighborhood.k = k;
    config_.neighborhood.r_base = r_base;
    config_.neighborhood.r_scale = r_scale;
}

void Prefilter::setSpikeRemovalParams(float dr_threshold, int window_size) {
    config_.spike_removal.dr_threshold = dr_threshold;
    config_.spike_removal.window_size = window_size;
}

void Prefilter::setOutlierRemovalParams(int median_window, float outlier_threshold) {
    config_.outlier_removal.median_window = median_window;
    config_.outlier_removal.outlier_threshold = outlier_threshold;
}

void Prefilter::setIntensityFilterParams(float min_intensity, float min_reliability) {
    config_.intensity_filter.min_intensity = min_intensity;
    config_.intensity_filter.min_reliability = min_reliability;
}

void Prefilter::setIsolationRemovalParams(int min_cluster_size, float isolation_radius) {
    config_.isolation_removal.min_cluster_size = min_cluster_size;
    config_.isolation_removal.isolation_radius = isolation_radius;
}
