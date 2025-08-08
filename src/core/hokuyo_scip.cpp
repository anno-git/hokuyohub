#include "hokuyo_scip.hpp"
HokuyoScipClient::HokuyoScipClient(const std::string&){ }
bool HokuyoScipClient::start(){ return true; }
bool HokuyoScipClient::stop(){ return true; }
bool HokuyoScipClient::isRunning() const{ return true; }
bool HokuyoScipClient::fetch(RawScan&){ return false; }