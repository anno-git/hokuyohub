#include "postfilter.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <unordered_set>

Postfilter::Postfilter(const PostfilterConfig& config) : config_(config) {}

Postfilter::FilterResult Postfilter::apply(const std::vector<Cluster>& input_clusters,
                                          const std::vector<float>& xy,
                                          const std::vector<uint8_t>& sid) const {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    stats_.reset();
    stats_.input_clusters = input_clusters.size();
    
    if (!config_.enabled || input_clusters.empty()) {
        FilterResult result;
        result.clusters = input_clusters;
        result.stats = stats_;
        stats_.output_clusters = input_clusters.size();
        return result;
    }

    // Build output
    FilterResult result;
    result.clusters = input_clusters;

    // Apply filters in sequence
    if (config_.isolation_removal.enabled) {
        for(auto it = result.clusters.begin(); it != result.clusters.end();) {
            if(!applyIsolationRemovalFilter(*it, xy, sid)) {
                it = result.clusters.erase(it);
            } else {
                ++it;
            }
        }
    }

    stats_.output_clusters = result.clusters.size();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.processing_time_us = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    
    result.stats = stats_;
    return result;
}

bool Postfilter::applyIsolationRemovalFilter(Cluster& cluster,
                                             const std::vector<float>& xy,
                                             const std::vector<uint8_t>& sid) const {
    const auto& cfg = config_.isolation_removal;

    // remove points that is isolated in the cluster
    std::vector<size_t> isolated_points;
    for (size_t i = 0; i < cluster.point_indices.size(); ++i) {
        // Check isolation only among points in the same cluster
        size_t point_idx = cluster.point_indices[i];
        const float px = xy[2 * point_idx];
        const float py = xy[2 * point_idx + 1];
        const float radius_sq = cfg.isolation_radius * cfg.isolation_radius;
        int neighbor_count = 0;
        for (size_t j = 0; j < cluster.point_indices.size(); ++j) {
            if (i == j) continue;
            size_t other_idx = cluster.point_indices[j];
            const float dx = xy[2 * other_idx] - px;
            const float dy = xy[2 * other_idx + 1] - py;
            const float dist_sq = dx * dx + dy * dy;
            if (dist_sq < radius_sq) {
                if(++neighbor_count >= cfg.required_neighbors) {
                    break;
                }
            }
        }
        if (neighbor_count < cfg.required_neighbors) {
            isolated_points.push_back(point_idx);
        }
    }
    if (isolated_points.empty()) {
        return true; // No points removed, cluster remains valid
    }
    if(cluster.point_indices.size() - isolated_points.size() < cfg.min_points_size) {
        ++stats_.removed_by_isolation;
        stats_.points_removed_total += cluster.point_indices.size();
        return false; // Not enough points remain, cluster is invalid
    }
    stats_.points_removed_total += isolated_points.size();
    // create new point_indices
    cluster.point_indices.erase(std::remove_if(cluster.point_indices.begin(),
                                                cluster.point_indices.end(),
                                                [&](size_t idx) {
                                                    return std::find(isolated_points.begin(), isolated_points.end(), idx) != isolated_points.end();
                                                }), cluster.point_indices.end());
    // rebuild cluster after removal
    rebuildClusterFromPoints(cluster, xy, sid, cluster.point_indices);

    return true;
}

std::vector<size_t> Postfilter::findNearbyPoints(const std::vector<float>& xy,
                                                 float center_x, float center_y,
                                                 float radius) const {
    std::vector<size_t> nearby_points;
    const float radius_sq = radius * radius;
    
    for (size_t i = 0; i < xy.size() / 2; ++i) {
        const float dx = xy[2 * i] - center_x;
        const float dy = xy[2 * i + 1] - center_y;
        const float dist_sq = dx * dx + dy * dy;
        
        if (dist_sq <= radius_sq) {
            nearby_points.push_back(i);
        }
    }
    
    return nearby_points;
}

void Postfilter::rebuildClusterFromPoints(Cluster& cluster,
                                         const std::vector<float>& xy,
                                         const std::vector<uint8_t>& sid,
                                         const std::vector<size_t>& point_indices) const {
    // Recalculate cluster statistics
    float sum_x = 0.0f, sum_y = 0.0f;
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    uint8_t sensor_mask = 0;
    
    for (size_t idx : point_indices) {
        const float px = xy[2 * idx];
        const float py = xy[2 * idx + 1];
        const uint8_t sensor_id = sid[idx];
        
        sum_x += px;
        sum_y += py;
        min_x = std::min(min_x, px);
        min_y = std::min(min_y, py);
        max_x = std::max(max_x, px);
        max_y = std::max(max_y, py);
        
        if (sensor_id < 8) {
            sensor_mask |= (1u << sensor_id);
        }
    }
    
    // Update cluster data
    cluster.cx = sum_x / point_indices.size();
    cluster.cy = sum_y / point_indices.size();
    cluster.minx = min_x;
    cluster.miny = min_y;
    cluster.maxx = max_x;
    cluster.maxy = max_y;
    cluster.point_indices = point_indices;
    cluster.sensor_mask = sensor_mask;
}

void Postfilter::enableStrategy(const std::string& strategy_name, bool enabled) {
    if (strategy_name == "isolation_removal") {
        config_.isolation_removal.enabled = enabled;
    }
    // Future strategies can be added here
}

bool Postfilter::isStrategyEnabled(const std::string& strategy_name) const {
    if (strategy_name == "isolation_removal") {
        return config_.isolation_removal.enabled;
    }
    // Future strategies can be added here
    return false;
}

void Postfilter::setIsolationRemovalParams(int min_points_size, float isolation_radius, int required_neighbors) {
    config_.isolation_removal.min_points_size = min_points_size;
    config_.isolation_removal.isolation_radius = isolation_radius;
    config_.isolation_removal.required_neighbors = required_neighbors;
}