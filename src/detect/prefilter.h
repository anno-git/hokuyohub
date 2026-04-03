#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <cmath>
#include <unordered_map>
#include "config/config.h"

// Use vector instead of span for broader C++ compatibility
template<typename T>
using span = const std::vector<T>&;

// Statistics for before/after comparison
struct PrefilterStats {
    size_t input_points{0};
    size_t output_points{0};
    size_t removed_by_neighborhood{0};
    size_t removed_by_spike{0};
    size_t removed_by_outlier{0};
    size_t removed_by_intensity{0};
    size_t removed_by_isolation{0};
    double processing_time_us{0.0};
    
    void reset() {
        input_points = output_points = 0;
        removed_by_neighborhood = removed_by_spike = removed_by_outlier = 0;
        removed_by_intensity = removed_by_isolation = 0;
        processing_time_us = 0.0;
    }
    
    size_t total_removed() const {
        return removed_by_neighborhood + removed_by_spike + removed_by_outlier + 
               removed_by_intensity + removed_by_isolation;
    }
};

// Point data structure for internal processing
struct FilterPoint {
    float x, y;                  // World coordinates
    uint8_t sid;                 // Sensor ID
    float range;                 // Distance from sensor origin
    float angle;                 // Angle from sensor
    float intensity{0.0f};       // Intensity value (if available)
    bool valid{true};            // Whether point passes all filters
    size_t original_index;       // Index in original data
};

class Prefilter {
private:
    PrefilterConfig config_;
    mutable PrefilterStats stats_;
    
    // Internal filtering methods
    void applyNeighborhoodFilter(std::vector<FilterPoint>& points) const;
    void applySpikeRemovalFilter(std::vector<FilterPoint>& points) const;
    void applyOutlierRemovalFilter(std::vector<FilterPoint>& points) const;
    void applyIntensityFilter(std::vector<FilterPoint>& points) const;
    void applyIsolationRemovalFilter(std::vector<FilterPoint>& points) const;
    
    // Spatial grid for O(N) neighbor queries
    struct SpatialGrid {
        float cell_size;
        std::unordered_map<int64_t, std::vector<size_t>> cells;

        void build(const std::vector<FilterPoint>& points, float radius);
        int64_t cellKey(int ix, int iy) const { return (static_cast<int64_t>(ix) << 32) | static_cast<uint32_t>(iy); }
        size_t countNeighbors(const std::vector<FilterPoint>& points, size_t point_idx, float radius_sq) const;
    };
    mutable SpatialGrid grid_;

    // Helper methods
    float calculateAngularDerivative(const std::vector<FilterPoint>& points,
                                     const std::vector<size_t>& sorted_indices,
                                     size_t j) const;
    float calculateMovingMedian(const std::vector<FilterPoint>& points,
                                const std::vector<size_t>& sorted_indices,
                                size_t j, int window_size) const;
    
public:
    explicit Prefilter(const PrefilterConfig& config = PrefilterConfig{});
    
    // Main filtering interface
    // Input: xy coordinates [x0,y0,x1,y1,...], sensor IDs, optional intensities
    // Output: filtered coordinates and sensor IDs
    struct FilterResult {
        std::vector<float> xy;       // Filtered [x0,y0,x1,y1,...]
        std::vector<uint8_t> sid;    // Corresponding sensor IDs
        std::vector<float> dist;     // Sensor distances [m]
        PrefilterStats stats;        // Processing statistics
    };

    FilterResult apply(const std::vector<float>& xy_in,
                      const std::vector<uint8_t>& sid_in,
                      const std::vector<float>& dist_in,
                      const std::vector<float>& intensities = {}) const;
    
    // Configuration management
    void setConfig(const PrefilterConfig& config) { config_ = config; }
    const PrefilterConfig& getConfig() const { return config_; }
    
    // Statistics access
    const PrefilterStats& getLastStats() const { return stats_; }
    
    // Individual strategy enable/disable
    void enableStrategy(const std::string& strategy_name, bool enabled);
    bool isStrategyEnabled(const std::string& strategy_name) const;
    
    // Parameter setters for dynamic configuration
    void setNeighborhoodParams(int k, float r_base, float r_scale = 1.0f);
    void setSpikeRemovalParams(float dr_threshold, int window_size = 3);
    void setOutlierRemovalParams(int median_window, float outlier_threshold = 2.0f);
    void setIntensityFilterParams(float min_intensity, float min_reliability = 0.0f);
    void setIsolationRemovalParams(int min_cluster_size, float isolation_radius);
};