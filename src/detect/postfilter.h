#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <cmath>
#include "config/config.h"
#include "detect/dbscan.h"

// Statistics for before/after comparison
struct PostfilterStats {
    size_t input_clusters{0};
    size_t output_clusters{0};
    size_t removed_by_isolation{0};
    size_t points_removed_total{0};
    double processing_time_us{0.0};
    
    void reset() {
        input_clusters = output_clusters = 0;
        removed_by_isolation = 0;
        points_removed_total = 0;
        processing_time_us = 0.0;
    }
    
    size_t total_clusters_removed() const {
        return removed_by_isolation;
    }
};

class Postfilter {
private:
    PostfilterConfig config_;
    mutable PostfilterStats stats_;
    
    // Internal filtering methods
    bool applyIsolationRemovalFilter(Cluster& cluster,
                                     const std::vector<float>& xy,
                                     const std::vector<uint8_t>& sid) const;
    
    // Helper methods
    std::vector<size_t> findNearbyPoints(const std::vector<float>& xy,
                                         float center_x, float center_y,
                                         float radius) const;
    void rebuildClusterFromPoints(Cluster& cluster,
                                  const std::vector<float>& xy,
                                  const std::vector<uint8_t>& sid,
                                  const std::vector<size_t>& point_indices) const;
    
public:
    explicit Postfilter(const PostfilterConfig& config = PostfilterConfig{});
    
    // Main filtering interface
    // Input: clusters from DBSCAN, original point data
    // Output: filtered clusters
    struct FilterResult {
        std::vector<Cluster> clusters;  // Filtered clusters
        PostfilterStats stats;          // Processing statistics
    };
    
    FilterResult apply(const std::vector<Cluster>& input_clusters,
                      const std::vector<float>& xy,
                      const std::vector<uint8_t>& sid) const;
    
    // Configuration management
    void setConfig(const PostfilterConfig& config) { config_ = config; }
    const PostfilterConfig& getConfig() const { return config_; }
    
    // Statistics access
    const PostfilterStats& getLastStats() const { return stats_; }
    
    // Individual strategy enable/disable
    void enableStrategy(const std::string& strategy_name, bool enabled);
    bool isStrategyEnabled(const std::string& strategy_name) const;
    
    // Parameter setters for dynamic configuration
    void setIsolationRemovalParams(int min_points_size, float isolation_radius, int required_neighbors);
};