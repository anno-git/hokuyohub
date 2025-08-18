#include "publisher_manager.h"
#include "nng_bus.h"
#include "osc_publisher.h"
#include <iostream>
#include <memory>

// NngSinkPublisher implementation
NngSinkPublisher::NngSinkPublisher() : enabled_(false) {
    bus_ = std::make_unique<NngBus>();
}

NngSinkPublisher::~NngSinkPublisher() {
    stop();
}

bool NngSinkPublisher::start(const SinkConfig& config) {
    if (!config.isNng()) {
        enabled_ = false;
        return false;
    }
    
    url_ = config.nng().url;
    bus_->startPublisher(config);
    enabled_ = bus_->isEnabled();
    
    if (enabled_) {
        std::cout << "[NngSinkPublisher] Started on " << url_ 
                  << " (topic: " << config.topic 
                  << ", rate_limit: " << config.rate_limit << "Hz)" << std::endl;
    } else {
        std::cerr << "[NngSinkPublisher] Failed to start on " << url_ << std::endl;
    }
    
    return enabled_;
}

void NngSinkPublisher::publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items) {
    if (enabled_ && bus_) {
        bus_->publishClusters(t_ns, seq, items);
    }
}

void NngSinkPublisher::stop() {
    if (bus_) {
        bus_->stop();
    }
    enabled_ = false;
}

bool NngSinkPublisher::isEnabled() const {
    return enabled_;
}

// OscSinkPublisher implementation
OscSinkPublisher::OscSinkPublisher() : enabled_(false) {
    osc_ = std::make_unique<OscPublisher>();
}

OscSinkPublisher::~OscSinkPublisher() {
    stop();
}

bool OscSinkPublisher::start(const SinkConfig& config) {
    if (!config.isOsc()) {
        enabled_ = false;
        return false;
    }
    
    url_ = config.osc().url;
    osc_->start(config);
    enabled_ = osc_->isEnabled();
    
    if (enabled_) {
        std::cout << "[OscSinkPublisher] Started on " << url_ 
                  << " (topic: " << config.topic 
                  << ", rate_limit: " << config.rate_limit << "Hz)" << std::endl;
    } else {
        std::cerr << "[OscSinkPublisher] Failed to start on " << url_ << std::endl;
    }
    
    return enabled_;
}

void OscSinkPublisher::publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items) {
    if (enabled_ && osc_) {
        osc_->publishClusters(t_ns, seq, items);
    }
}

void OscSinkPublisher::stop() {
    if (osc_) {
        osc_->stop();
    }
    enabled_ = false;
}

bool OscSinkPublisher::isEnabled() const {
    return enabled_;
}

// PublisherManager implementation
PublisherManager::PublisherManager() {
    publishers_ = std::make_shared<PublisherArray>();
}

PublisherManager::~PublisherManager() {
    stopAll();
}

bool PublisherManager::configure(const std::vector<SinkConfig>& sinks) {
    std::cout << "[PublisherManager] Configuring " << sinks.size() << " sink(s)..." << std::endl;
    
    // Create new publisher array
    auto new_publishers = std::make_shared<PublisherArray>();
    new_publishers->reserve(sinks.size());
    
    int success_count = 0;
    int failure_count = 0;
    
    // Create and start publishers for each sink
    for (const auto& sink : sinks) {
        std::unique_ptr<ISinkPublisher> publisher;
        
        if (sink.isNng()) {
            publisher = std::make_unique<NngSinkPublisher>();
        } else if (sink.isOsc()) {
            publisher = std::make_unique<OscSinkPublisher>();
        } else {
            std::cerr << "[PublisherManager] Unknown sink type in configuration" << std::endl;
            failure_count++;
            continue;
        }
        
        if (publisher->start(sink)) {
            success_count++;
        } else {
            failure_count++;
        }
        
        // Add publisher to array regardless of start success (for monitoring)
        new_publishers->push_back(std::move(publisher));
    }
    
    // Stop old publishers before swapping
    {
        std::lock_guard<std::mutex> lock(publishers_mutex_);
        if (publishers_) {
            for (auto& pub : *publishers_) {
                if (pub) {
                    pub->stop();
                }
            }
        }
        
        // Swap to new publishers
        publishers_ = new_publishers;
    }
    
    std::cout << "[PublisherManager] Configuration complete: "
              << success_count << " started, "
              << failure_count << " failed, "
              << new_publishers->size() << " total" << std::endl;
    
    return failure_count == 0;
}

void PublisherManager::publishClusters(uint64_t t_ns, uint32_t seq, const std::vector<Cluster>& items) const {
    // Get current snapshot of publishers
    std::shared_ptr<PublisherArray> current_publishers;
    {
        std::lock_guard<std::mutex> lock(publishers_mutex_);
        current_publishers = publishers_;
    }
    
    if (!current_publishers) {
        return;
    }
    
    // Publish to all enabled publishers
    for (const auto& publisher : *current_publishers) {
        if (publisher && publisher->isEnabled()) {
            try {
                publisher->publishClusters(t_ns, seq, items);
            } catch (const std::exception& e) {
                // Log error but continue with other publishers
                std::cerr << "[PublisherManager] Error publishing to "
                          << publisher->getType() << " sink "
                          << publisher->getUrl() << ": " << e.what() << std::endl;
            }
        }
    }
}

void PublisherManager::stopAll() {
    std::lock_guard<std::mutex> lock(publishers_mutex_);
    if (publishers_) {
        for (auto& publisher : *publishers_) {
            if (publisher) {
                publisher->stop();
            }
        }
    }
    
    // Replace with empty array
    publishers_ = std::make_shared<PublisherArray>();
    std::cout << "[PublisherManager] All publishers stopped" << std::endl;
}

size_t PublisherManager::getPublisherCount() const {
    std::lock_guard<std::mutex> lock(publishers_mutex_);
    return publishers_ ? publishers_->size() : 0;
}

size_t PublisherManager::getEnabledPublisherCount() const {
    std::shared_ptr<PublisherArray> current_publishers;
    {
        std::lock_guard<std::mutex> lock(publishers_mutex_);
        current_publishers = publishers_;
    }
    
    if (!current_publishers) {
        return 0;
    }
    
    size_t enabled_count = 0;
    for (const auto& publisher : *current_publishers) {
        if (publisher && publisher->isEnabled()) {
            enabled_count++;
        }
    }
    return enabled_count;
}