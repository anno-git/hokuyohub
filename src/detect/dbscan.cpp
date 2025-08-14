#include "dbscan.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <queue>
#include <limits>
#include <iostream>
#ifdef DBSCAN_PROFILE
#include <chrono>
#endif

void DBSCAN2D::setSensorModel(uint8_t sid, float delta_theta_deg, float sigma0, float alpha) {
    sensor_models_[sid] = {static_cast<float>(delta_theta_deg * M_PI / 180.0), sigma0, alpha};
}

void DBSCAN2D::setPerformanceParams(float h_min, float h_max, int R_max, int M_max) {
    h_min_ = h_min;
    h_max_ = h_max;
    R_max_ = R_max;
    M_max_ = M_max;
}

// Hash function for grid cell coordinates
struct CellHash {
    std::size_t operator()(const std::pair<int, int>& cell) const {
        return std::hash<int>()(cell.first) ^ (std::hash<int>()(cell.second) << 1);
    }
};

std::vector<Cluster> DBSCAN2D::run(std::span<const float> xy, std::span<const uint8_t> sid, uint64_t t_ns, uint32_t seq) {
#ifdef DBSCAN_PROFILE
    auto start_time = std::chrono::high_resolution_clock::now();
#endif

    const size_t N = xy.size() / 2;
    if (N == 0 || sid.size() != N) return {};
    
    const float eps_norm = eps_; // Treat eps_ as eps_norm for now
    const float eps_norm_sq = eps_norm * eps_norm;
    
    // Dynamic candidate cap based on frame size (more generous for large eps_norm)
    const int M_dyn = std::max(M_max_, static_cast<int>(std::floor(0.1f * N)));
    
    // Step 1: Calculate local scales s_i and search radii eps_i
    std::vector<float> scales(N);
    std::vector<float> search_radii(N);
    
    for (size_t i = 0; i < N; ++i) {
        const float x = xy[2*i];
        const float y = xy[2*i + 1];
        const float r = std::hypot(x, y);
        const uint8_t sensor_id = sid[i];
        
        // Get sensor model (use default if not found)
        const auto& model = sensor_models_.count(sensor_id) ?
            sensor_models_[sensor_id] : sensor_models_[0];
        
        // Calculate local scale: s_i^2 = σ_r(r)^2 + (k_effective * r * Δθ)^2
        // k_effective = (1/eps_norm) * k_scale for theoretical consistency
        const float sigma_r = model.sigma0 + model.alpha * r;
        const float k_effective = (1.0f / eps_norm) * k_scale_;
        const float angular_term = k_effective * r * model.delta_theta_rad;
        scales[i] = std::sqrt(sigma_r * sigma_r + angular_term * angular_term);
        search_radii[i] = eps_norm * scales[i];
    }

    // Step 2: Determine grid cell size h with small-N fallback
    float h;
    if (N < 2000) {
        h = 0.03f; // Small-N fallback
    } else {
        std::vector<float> sorted_scales = scales;
        std::nth_element(sorted_scales.begin(), sorted_scales.begin() + N/2, sorted_scales.end());
        const float s_median = sorted_scales[N/2];
        h = std::clamp(0.8f * s_median, h_min_, h_max_);
    }
    
    // Step 3: Build spatial grid with better capacity estimation
    std::unordered_map<std::pair<int, int>, std::vector<size_t>, CellHash> grid;
    grid.reserve(std::max(static_cast<size_t>(N / 3), static_cast<size_t>(16))); // Better estimate
    
    for (size_t i = 0; i < N; ++i) {
        const int ix = static_cast<int>(std::floor(xy[2*i] / h));
        const int iy = static_cast<int>(std::floor(xy[2*i + 1] / h));
        grid[{ix, iy}].push_back(i);
    }
    
    // Step 4: DBSCAN algorithm with normalized distance
    std::vector<int> cluster_id(N, -1); // -1 = unvisited, -2 = noise, >=0 = cluster
    std::vector<bool> visited(N, false);
    int current_cluster = 0;
    
    // Pre-allocate neighbor vector to avoid repeated allocations
    std::vector<size_t> neighbors;
    neighbors.reserve(std::min(static_cast<size_t>(M_dyn), N));
    
    auto findNeighbors = [&](size_t point_idx) -> size_t {
        neighbors.clear();
        const float px = xy[2*point_idx];
        const float py = xy[2*point_idx + 1];
        const float eps_i = search_radii[point_idx];
        const float scale_i = scales[point_idx];
        const float scale_i_sq = scale_i * scale_i;
        
        // Calculate search radius in cells
        const int R_i = std::min(R_max_, static_cast<int>(std::ceil(eps_i / h)));
        const int ix = static_cast<int>(std::floor(px / h));
        const int iy = static_cast<int>(std::floor(py / h));
        
        int candidate_count = 0;
        
        // Add self to neighbors for inclusive minPts semantics
        neighbors.push_back(point_idx);
        
        // Search neighboring cells
        for (int dx = -R_i; dx <= R_i && candidate_count < M_dyn; ++dx) {
            for (int dy = -R_i; dy <= R_i && candidate_count < M_dyn; ++dy) {
                auto it = grid.find({ix + dx, iy + dy});
                if (it == grid.end()) continue;
                
                for (size_t j : it->second) {
                    if (j == point_idx) continue; // Skip self (already added)
                    
                    candidate_count++;
                    if (candidate_count >= M_dyn) break;
                    
                    // Calculate normalized distance (optimized)
                    const float qx = xy[2*j];
                    const float qy = xy[2*j + 1];
                    const float dx_norm = px - qx;
                    const float dy_norm = py - qy;
                    const float dist_sq = dx_norm * dx_norm + dy_norm * dy_norm;
                    
                    const float scale_j = scales[j];
                    const float combined_scale_sq = scale_i_sq + scale_j * scale_j;
                    const float d_norm_sq = dist_sq / combined_scale_sq;
                    
                    if (d_norm_sq <= eps_norm_sq) {
                        neighbors.push_back(j);
                        
                        // Early exit optimization: stop once we have enough neighbors for core point
                        if (static_cast<int>(neighbors.size()) >= minPts_) {
                            // Continue scanning to collect all neighbors in this iteration
                            // but we know this is a core point
                        }
                    }
                }
            }
        }
        
        return neighbors.size();
    };
    
    // Main DBSCAN loop
    for (size_t i = 0; i < N; ++i) {
        if (visited[i]) continue;
        visited[i] = true;
        
        size_t neighbor_count = findNeighbors(i);
        
        // Inclusive minPts: neighbor_count includes the point itself
        if (static_cast<int>(neighbor_count) < minPts_) {
            cluster_id[i] = -2; // Mark as noise
            continue;
        }
        
        // Start new cluster
        cluster_id[i] = current_cluster;
        
        // Expand cluster using queue (reuse neighbors vector, but skip self)
        std::queue<size_t> seed_set;
        for (size_t neighbor : neighbors) {
            if (neighbor != i) { // Don't add self to expansion queue
                seed_set.push(neighbor);
            }
        }
        
        while (!seed_set.empty()) {
            size_t q = seed_set.front();
            seed_set.pop();
            
            if (!visited[q]) {
                visited[q] = true;
                size_t q_neighbor_count = findNeighbors(q);
                
                if (static_cast<int>(q_neighbor_count) >= minPts_) {
                    for (size_t qn : neighbors) {
                        if (qn != q) { // Don't add self to expansion queue
                            seed_set.push(qn);
                        }
                    }
                }
            }
            
            if (cluster_id[q] < 0) { // If noise or unassigned
                cluster_id[q] = current_cluster;
            }
        }
        
        current_cluster++;
    }
    
    // Step 5: Generate cluster output
    std::vector<Cluster> clusters;
    if (current_cluster == 0) return clusters;
    
    clusters.resize(current_cluster);
    
    // Initialize clusters
    for (int c = 0; c < current_cluster; ++c) {
        clusters[c] = {
            static_cast<uint32_t>(c), 0, 0.0f, 0.0f,
            std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
            std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
            0
        };
    }
    
    // Accumulate cluster statistics
    for (size_t i = 0; i < N; ++i) {
        const int cid = cluster_id[i];
        if (cid < 0) continue; // Skip noise points
        
        auto& cluster = clusters[cid];
        const float x = xy[2*i];
        const float y = xy[2*i + 1];
        const uint8_t sensor_id = sid[i];
        
        // Update bounding box
        cluster.minx = std::min(cluster.minx, x);
        cluster.miny = std::min(cluster.miny, y);
        cluster.maxx = std::max(cluster.maxx, x);
        cluster.maxy = std::max(cluster.maxy, y);
        
        // Update centroid (accumulate for now)
        cluster.cx += x;
        cluster.cy += y;
        cluster.count++;
        
        // Update sensor mask (guard against sid >= 8 for 8-bit mask)
        if (sensor_id < 8) {
            cluster.sensor_mask |= (1u << sensor_id);
        }
    }
    
    // Finalize centroids (convert sum to average)
    for (auto& cluster : clusters) {
        if (cluster.count > 0) {
            cluster.cx /= cluster.count;
            cluster.cy /= cluster.count;
        }
    }

#ifdef DBSCAN_PROFILE
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    std::cout << "[DBSCAN] N=" << N << " clusters=" << current_cluster
              << " time=" << duration.count() << "μs" << std::endl;
#endif

    return clusters;
}
