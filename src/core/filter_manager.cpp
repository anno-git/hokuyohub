#include "filter_manager.h"
#include <iostream>

FilterManager::FilterManager(PrefilterConfig& prefilter_config, PostfilterConfig& postfilter_config)
    : prefilter_config_(prefilter_config), postfilter_config_(postfilter_config) {
    recreatePrefilter();
    recreatePostfilter();
}

bool FilterManager::updatePrefilterConfig(const Json::Value& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        PrefilterConfig new_config = jsonToPrefilterConfig(config);
        prefilter_config_ = new_config;
        
        recreatePrefilter();
        std::cout << "[FilterManager] Prefilter configuration updated" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[FilterManager] Failed to update prefilter config: " << e.what() << std::endl;
        return false;
    }
}

bool FilterManager::updatePostfilterConfig(const Json::Value& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        PostfilterConfig new_config = jsonToPostfilterConfig(config);
        postfilter_config_ = new_config;
        
        recreatePostfilter();
        std::cout << "[FilterManager] Postfilter configuration updated" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[FilterManager] Failed to update postfilter config: " << e.what() << std::endl;
        return false;
    }
}

bool FilterManager::updateFilterConfig(const Json::Value& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        bool success = true;
        
        if (config.isMember("prefilter")) {
            PrefilterConfig new_prefilter_config = jsonToPrefilterConfig(config["prefilter"]);
            prefilter_config_ = new_prefilter_config;
            
            recreatePrefilter();
        }
        
        if (config.isMember("postfilter")) {
            PostfilterConfig new_postfilter_config = jsonToPostfilterConfig(config["postfilter"]);
            postfilter_config_ = new_postfilter_config;
            
            recreatePostfilter();
        }
        
        std::cout << "[FilterManager] Filter configuration updated" << std::endl;
        return success;
    } catch (const std::exception& e) {
        std::cerr << "[FilterManager] Failed to update filter config: " << e.what() << std::endl;
        return false;
    }
}

void FilterManager::reloadFromAppConfig() {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        // Recreate filters with new configurations
        recreatePrefilter();
        recreatePostfilter();
        
        std::cout << "[FilterManager] Configuration reloaded from AppConfig" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[FilterManager] Failed to reload from AppConfig: " << e.what() << std::endl;
    }
}

Json::Value FilterManager::getPrefilterConfigAsJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return prefilterConfigToJson(prefilter_config_);
}

Json::Value FilterManager::getPostfilterConfigAsJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return postfilterConfigToJson(postfilter_config_);
}

Json::Value FilterManager::getFilterConfigAsJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Json::Value result;
    result["prefilter"] = prefilterConfigToJson(prefilter_config_);
    result["postfilter"] = postfilterConfigToJson(postfilter_config_);
    return result;
}

Prefilter::FilterResult FilterManager::applyPrefilter(const std::vector<float>& xy, const std::vector<uint8_t>& sid) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (prefilter_ && prefilter_config_.enabled) {
        return prefilter_->apply(xy, sid);
    } else {
        // Return unfiltered data
        Prefilter::FilterResult result;
        result.xy = xy;
        result.sid = sid;
        result.stats.input_points = xy.size() / 2;
        result.stats.output_points = xy.size() / 2;
        return result;
    }
}

Postfilter::FilterResult FilterManager::applyPostfilter(const std::vector<Cluster>& clusters,
                                                       const std::vector<float>& xy,
                                                       const std::vector<uint8_t>& sid) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (postfilter_ && postfilter_config_.enabled) {
        return postfilter_->apply(clusters, xy, sid);
    } else {
        // Return unfiltered clusters
        Postfilter::FilterResult result;
        result.clusters = clusters;
        result.stats.input_clusters = clusters.size();
        result.stats.output_clusters = clusters.size();
        return result;
    }
}

void FilterManager::recreatePrefilter() {
    prefilter_ = std::make_unique<Prefilter>(prefilter_config_);
}

void FilterManager::recreatePostfilter() {
    postfilter_ = std::make_unique<Postfilter>(postfilter_config_);
}

PrefilterConfig FilterManager::jsonToPrefilterConfig(const Json::Value& json) const {
    PrefilterConfig config;
    
    config.enabled = json.get("enabled", true).asBool();
    
    if (json.isMember("neighborhood")) {
        const auto& nb = json["neighborhood"];
        config.neighborhood.enabled = nb.get("enabled", true).asBool();
        config.neighborhood.k = nb.get("k", 5).asInt();
        config.neighborhood.r_base = nb.get("r_base", 0.05f).asFloat();
        config.neighborhood.r_scale = nb.get("r_scale", 1.0f).asFloat();
    }
    
    if (json.isMember("spike_removal")) {
        const auto& sr = json["spike_removal"];
        config.spike_removal.enabled = sr.get("enabled", true).asBool();
        config.spike_removal.dr_threshold = sr.get("dr_threshold", 0.3f).asFloat();
        config.spike_removal.window_size = sr.get("window_size", 3).asInt();
    }
    
    if (json.isMember("outlier_removal")) {
        const auto& or_cfg = json["outlier_removal"];
        config.outlier_removal.enabled = or_cfg.get("enabled", true).asBool();
        config.outlier_removal.median_window = or_cfg.get("median_window", 5).asInt();
        config.outlier_removal.outlier_threshold = or_cfg.get("outlier_threshold", 2.0f).asFloat();
    }
    
    if (json.isMember("intensity_filter")) {
        const auto& if_cfg = json["intensity_filter"];
        config.intensity_filter.enabled = if_cfg.get("enabled", false).asBool();
        config.intensity_filter.min_intensity = if_cfg.get("min_intensity", 0).asInt();
        config.intensity_filter.min_reliability = if_cfg.get("min_reliability", 0.0f).asFloat();
    }
    
    if (json.isMember("isolation_removal")) {
        const auto& ir = json["isolation_removal"];
        config.isolation_removal.enabled = ir.get("enabled", true).asBool();
        config.isolation_removal.min_cluster_size = ir.get("min_cluster_size", 3).asInt();
        config.isolation_removal.isolation_radius = ir.get("isolation_radius", 0.1f).asFloat();
    }
    
    return config;
}

PostfilterConfig FilterManager::jsonToPostfilterConfig(const Json::Value& json) const {
    PostfilterConfig config;
    
    config.enabled = json.get("enabled", true).asBool();
    
    if (json.isMember("isolation_removal")) {
        const auto& ir = json["isolation_removal"];
        config.isolation_removal.enabled = ir.get("enabled", true).asBool();
        config.isolation_removal.min_points_size = ir.get("min_points_size", 3).asInt();
        config.isolation_removal.isolation_radius = ir.get("isolation_radius", 0.2f).asFloat();
        config.isolation_removal.required_neighbors = ir.get("required_neighbors", 1).asInt();
    }
    
    return config;
}

Json::Value FilterManager::prefilterConfigToJson(const PrefilterConfig& config) const {
    Json::Value json;
    
    json["enabled"] = config.enabled;
    
    json["neighborhood"]["enabled"] = config.neighborhood.enabled;
    json["neighborhood"]["k"] = config.neighborhood.k;
    json["neighborhood"]["r_base"] = config.neighborhood.r_base;
    json["neighborhood"]["r_scale"] = config.neighborhood.r_scale;
    
    json["spike_removal"]["enabled"] = config.spike_removal.enabled;
    json["spike_removal"]["dr_threshold"] = config.spike_removal.dr_threshold;
    json["spike_removal"]["window_size"] = config.spike_removal.window_size;
    
    json["outlier_removal"]["enabled"] = config.outlier_removal.enabled;
    json["outlier_removal"]["median_window"] = config.outlier_removal.median_window;
    json["outlier_removal"]["outlier_threshold"] = config.outlier_removal.outlier_threshold;
    
    json["intensity_filter"]["enabled"] = config.intensity_filter.enabled;
    json["intensity_filter"]["min_intensity"] = config.intensity_filter.min_intensity;
    json["intensity_filter"]["min_reliability"] = config.intensity_filter.min_reliability;
    
    json["isolation_removal"]["enabled"] = config.isolation_removal.enabled;
    json["isolation_removal"]["min_cluster_size"] = config.isolation_removal.min_cluster_size;
    json["isolation_removal"]["isolation_radius"] = config.isolation_removal.isolation_radius;
    
    return json;
}

Json::Value FilterManager::postfilterConfigToJson(const PostfilterConfig& config) const {
    Json::Value json;
    
    json["enabled"] = config.enabled;
    
    json["isolation_removal"]["enabled"] = config.isolation_removal.enabled;
    json["isolation_removal"]["min_points_size"] = config.isolation_removal.min_points_size;
    json["isolation_removal"]["isolation_radius"] = config.isolation_removal.isolation_radius;
    json["isolation_removal"]["required_neighbors"] = config.isolation_removal.required_neighbors;

    return json;
}