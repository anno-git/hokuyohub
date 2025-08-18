#pragma once

#include "detect/prefilter.h"
#include "detect/postfilter.h"
#include "config/config.h"
#include <json/json.h>
#include <memory>
#include <mutex>

class FilterManager {
public:
    FilterManager(const PrefilterConfig& prefilter_config, const PostfilterConfig& postfilter_config, AppConfig& app_config);
    
    // Update filter configurations from JSON
    bool updatePrefilterConfig(const Json::Value& config);
    bool updatePostfilterConfig(const Json::Value& config);
    bool updateFilterConfig(const Json::Value& config);
    
    // Get current configurations as JSON
    Json::Value getPrefilterConfigAsJson() const;
    Json::Value getPostfilterConfigAsJson() const;
    Json::Value getFilterConfigAsJson() const;
    
    // Apply filters (thread-safe)
    Prefilter::FilterResult applyPrefilter(const std::vector<float>& xy, const std::vector<uint8_t>& sid);
    Postfilter::FilterResult applyPostfilter(const std::vector<Cluster>& clusters,
                                            const std::vector<float>& xy,
                                            const std::vector<uint8_t>& sid);
    
    // Check if filters are enabled
    bool isPrefilterEnabled() const { return prefilter_config_.enabled; }
    bool isPostfilterEnabled() const { return postfilter_config_.enabled; }

private:
    mutable std::mutex mutex_;
    PrefilterConfig prefilter_config_;
    PostfilterConfig postfilter_config_;
    std::unique_ptr<Prefilter> prefilter_;
    std::unique_ptr<Postfilter> postfilter_;
    AppConfig& app_config_;  // Reference to main config for immediate updates
    
    void recreatePrefilter();
    void recreatePostfilter();
    
    // Helper methods to convert JSON to config structs
    PrefilterConfig jsonToPrefilterConfig(const Json::Value& json) const;
    PostfilterConfig jsonToPostfilterConfig(const Json::Value& json) const;
    
    // Helper methods to convert config structs to JSON
    Json::Value prefilterConfigToJson(const PrefilterConfig& config) const;
    Json::Value postfilterConfigToJson(const PostfilterConfig& config) const;
};