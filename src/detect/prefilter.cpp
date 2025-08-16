#include "prefilter.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

Prefilter::Prefilter(const PrefilterConfig& config) : config_(config) {}

Prefilter::FilterResult Prefilter::apply(const std::vector<float>& xy_in, 
                                        const std::vector<uint8_t>& sid_in,
                                        const std::vector<float>& intensities) const {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    stats_.reset();
    stats_.input_points = xy_in.size() / 2;
    
    if (!config_.enabled || xy_in.empty() || xy_in.size() % 2 != 0 || 
        sid_in.size() != xy_in.size() / 2) {
        FilterResult result;
        result.xy = xy_in;
        result.sid = sid_in;
        result.stats = stats_;
        return result;
    }
    
    const size_t num_points = xy_in.size() / 2;
    
    // Convert input to internal format
    std::vector<FilterPoint> points;
    points.reserve(num_points);
    
    for (size_t i = 0; i < num_points; ++i) {
        FilterPoint pt;
        pt.x = xy_in[2 * i];
        pt.y = xy_in[2 * i + 1];
        pt.sid = sid_in[i];
        pt.range = std::sqrt(pt.x * pt.x + pt.y * pt.y);
        pt.angle = std::atan2(pt.y, pt.x);
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
    
    for (const auto& pt : points) {
        if (pt.valid) {
            result.xy.push_back(pt.x);
            result.xy.push_back(pt.y);
            result.sid.push_back(pt.sid);
        }
    }
    
    stats_.output_points = result.xy.size() / 2;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.processing_time_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    
    result.stats = stats_;
    return result;
}

void Prefilter::applyNeighborhoodFilter(std::vector<FilterPoint>& points) const {
    const auto& cfg = config_.neighborhood;
    size_t removed = 0;
    
    for (size_t i = 0; i < points.size(); ++i) {
        if (!points[i].valid) continue;
        
        // Calculate adaptive radius based on distance
        const float radius = cfg.r_base + cfg.r_scale * points[i].range;
        
        // Find neighbors within radius
        auto neighbors = findNeighbors(points, i, radius);
        
        // Check if point has enough neighbors (including itself)
        if (static_cast<int>(neighbors.size()) < cfg.k) {
            points[i].valid = false;
            removed++;
        }
    }
    
    stats_.removed_by_neighborhood = removed;
}

void Prefilter::applySpikeRemovalFilter(std::vector<FilterPoint>& points) const {
    const auto& cfg = config_.spike_removal;
    size_t removed = 0;
    
    // Group points by sensor ID for angular continuity
    std::unordered_map<uint8_t, std::vector<size_t>> sensor_groups;
    for (size_t i = 0; i < points.size(); ++i) {
        if (points[i].valid) {
            sensor_groups[points[i].sid].push_back(i);
        }
    }
    
    // Process each sensor group separately
    for (auto& [sid, indices] : sensor_groups) {
        // Sort by angle for proper derivative calculation
        std::sort(indices.begin(), indices.end(), [&points](size_t a, size_t b) {
            return points[a].angle < points[b].angle;
        });
        
        // Calculate angular derivatives and mark spikes
        for (size_t j = 0; j < indices.size(); ++j) {
            size_t idx = indices[j];
            if (!points[idx].valid) continue;
            
            float dr_dtheta = calculateAngularDerivative(points, idx, cfg.window_size);
            
            if (std::abs(dr_dtheta) > cfg.dr_threshold) {
                points[idx].valid = false;
                removed++;
            }
        }
    }
    
    stats_.removed_by_spike = removed;
}

void Prefilter::applyOutlierRemovalFilter(std::vector<FilterPoint>& points) const {
    const auto& cfg = config_.outlier_removal;
    size_t removed = 0;
    
    // Group points by sensor ID for local analysis
    std::unordered_map<uint8_t, std::vector<size_t>> sensor_groups;
    for (size_t i = 0; i < points.size(); ++i) {
        if (points[i].valid) {
            sensor_groups[points[i].sid].push_back(i);
        }
    }
    
    for (auto& [sid, indices] : sensor_groups) {
        // Sort by angle for moving window analysis
        std::sort(indices.begin(), indices.end(), [&points](size_t a, size_t b) {
            return points[a].angle < points[b].angle;
        });
        
        // Apply moving median filter
        for (size_t j = 0; j < indices.size(); ++j) {
            size_t idx = indices[j];
            if (!points[idx].valid) continue;
            
            float median_range = calculateMovingMedian(points, idx, cfg.median_window);
            float deviation = std::abs(points[idx].range - median_range);
            
            // Calculate local standard deviation for threshold
            float local_std = 0.0f;
            int count = 0;
            int half_window = cfg.median_window / 2;
            
            for (int k = std::max(0, static_cast<int>(j) - half_window); 
                 k <= std::min(static_cast<int>(indices.size()) - 1, static_cast<int>(j) + half_window); ++k) {
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

void Prefilter::applyIsolationRemovalFilter(std::vector<FilterPoint>& points) const {
    const auto& cfg = config_.isolation_removal;
    size_t removed = 0;
    
    // Find isolated points (points with too few neighbors in isolation radius)
    for (size_t i = 0; i < points.size(); ++i) {
        if (!points[i].valid) continue;
        
        auto neighbors = findNeighbors(points, i, cfg.isolation_radius);
        
        // If point has fewer than min_cluster_size neighbors (including itself), mark as isolated
        if (static_cast<int>(neighbors.size()) < cfg.min_cluster_size) {
            points[i].valid = false;
            removed++;
        }
    }
    
    stats_.removed_by_isolation = removed;
}

std::vector<size_t> Prefilter::findNeighbors(const std::vector<FilterPoint>& points, 
                                             size_t point_idx, float radius) const {
    std::vector<size_t> neighbors;
    const auto& query_pt = points[point_idx];
    const float radius_sq = radius * radius;
    
    for (size_t i = 0; i < points.size(); ++i) {
        if (!points[i].valid) continue;
        
        float dx = points[i].x - query_pt.x;
        float dy = points[i].y - query_pt.y;
        float dist_sq = dx * dx + dy * dy;
        
        if (dist_sq <= radius_sq) {
            neighbors.push_back(i);
        }
    }
    
    return neighbors;
}

float Prefilter::calculateAngularDerivative(const std::vector<FilterPoint>& points, 
                                           size_t idx, int window_size) const {
    // Simple finite difference approximation
    // Find neighboring points by angle for the same sensor
    const auto& center_pt = points[idx];
    
    // Find closest points before and after in angle
    float prev_range = center_pt.range;
    float next_range = center_pt.range;
    float prev_angle = center_pt.angle;
    float next_angle = center_pt.angle;
    
    bool found_prev = false, found_next = false;
    float min_prev_diff = std::numeric_limits<float>::max();
    float min_next_diff = std::numeric_limits<float>::max();
    
    for (size_t i = 0; i < points.size(); ++i) {
        if (i == idx || !points[i].valid || points[i].sid != center_pt.sid) continue;
        
        float angle_diff = points[i].angle - center_pt.angle;
        
        if (angle_diff < 0 && std::abs(angle_diff) < min_prev_diff) {
            prev_range = points[i].range;
            prev_angle = points[i].angle;
            min_prev_diff = std::abs(angle_diff);
            found_prev = true;
        } else if (angle_diff > 0 && angle_diff < min_next_diff) {
            next_range = points[i].range;
            next_angle = points[i].angle;
            min_next_diff = angle_diff;
            found_next = true;
        }
    }
    
    // Calculate derivative
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

float Prefilter::calculateMovingMedian(const std::vector<FilterPoint>& points, 
                                      size_t center_idx, int window_size) const {
    const auto& center_pt = points[center_idx];
    std::vector<float> ranges;
    
    // Collect ranges from nearby points of the same sensor
    for (size_t i = 0; i < points.size(); ++i) {
        if (!points[i].valid || points[i].sid != center_pt.sid) continue;
        
        float angle_diff = std::abs(points[i].angle - center_pt.angle);
        if (angle_diff <= M_PI / 180.0 * window_size) { // Convert window to radians
            ranges.push_back(points[i].range);
        }
    }
    
    if (ranges.empty()) return center_pt.range;
    
    // Calculate median
    std::sort(ranges.begin(), ranges.end());
    size_t mid = ranges.size() / 2;
    
    if (ranges.size() % 2 == 0) {
        return (ranges[mid - 1] + ranges[mid]) / 2.0f;
    } else {
        return ranges[mid];
    }
}

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