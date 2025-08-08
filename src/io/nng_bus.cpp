#include "nng_bus.h"
#ifdef USE_NNG
#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>
#endif
#include <cstdio>

void NngBus::startPublisher(const std::string& url){ url_ = url; /* TODO: nng_dial or nng_listen */ }
void NngBus::publishClusters(uint64_t, uint32_t, const std::vector<Cluster>&){
  // TODO: MessagePack でシリアライズして nng_send
  (void)url_;
}

