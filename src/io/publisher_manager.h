#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "detect/dbscan.h"
#include "config/config.h"

// Abstract interface for sink publishers
class ISinkPublisher {
public:
    virtual ~ISinkPublisher() = default;
    virtual bool start(const SinkConfig& config) = 0;
    virtual void publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items) = 0;
    virtual void stop() = 0;
    virtual bool isEnabled() const = 0;
    virtual std::string getType() const = 0;
    virtual std::string getUrl() const = 0;
};

// Forward declarations
class NngBus;
class OscPublisher;

// NNG sink publisher implementation
class NngSinkPublisher : public ISinkPublisher {
private:
    std::unique_ptr<NngBus> bus_;
    std::string url_;
    bool enabled_;

public:
    NngSinkPublisher();
    ~NngSinkPublisher() override;
    
    bool start(const SinkConfig& config) override;
    void publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items) override;
    void stop() override;
    bool isEnabled() const override;
    std::string getType() const override { return "nng"; }
    std::string getUrl() const override { return url_; }
};

// OSC sink publisher implementation
class OscSinkPublisher : public ISinkPublisher {
private:
    std::unique_ptr<OscPublisher> osc_;
    std::string url_;
    bool enabled_;

public:
    OscSinkPublisher();
    ~OscSinkPublisher() override;
    
    bool start(const SinkConfig& config) override;
    void publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items) override;
    void stop() override;
    bool isEnabled() const override;
    std::string getType() const override { return "osc"; }
    std::string getUrl() const override { return url_; }
};

// Publisher manager for handling multiple sinks
class PublisherManager {
private:
    using PublisherArray = std::vector<std::unique_ptr<ISinkPublisher>>;
    mutable std::mutex publishers_mutex_;
    std::shared_ptr<PublisherArray> publishers_;

public:
    PublisherManager();
    ~PublisherManager();
    
    // Configure publishers from sink configuration
    bool configure(const std::vector<SinkConfig>& sinks);
    
    // Publish clusters to all active publishers
    void publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items) const;
    
    // Stop all publishers
    void stopAll();
    
    // Get current publisher count for logging/monitoring
    size_t getPublisherCount() const;
    size_t getEnabledPublisherCount() const;
};