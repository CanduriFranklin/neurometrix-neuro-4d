#include "metrics_server.hpp"

#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include <memory>
#include <string>

namespace neurometrix::core4d::infrastructure::metrics {

struct MetricsServer::Impl {
    std::unique_ptr<prometheus::Exposer>  exposer;
    std::shared_ptr<prometheus::Registry> registry;
};

MetricsServer::MetricsServer(const config::MetricsConfig& cfg) noexcept
    : pimpl_(std::make_unique<Impl>())
{
    try {
        const std::string addr = cfg.bind_address + ":" + std::to_string(cfg.port);
        pimpl_->exposer  = std::make_unique<prometheus::Exposer>(addr);
        pimpl_->registry = std::make_shared<prometheus::Registry>();
        pimpl_->exposer->RegisterCollectable(pimpl_->registry);
    } catch (...) {}
}

MetricsServer::~MetricsServer() { stop(); }

bool MetricsServer::start() noexcept {
    return pimpl_ && pimpl_->exposer != nullptr;
}

void MetricsServer::stop() noexcept {
    if (!pimpl_) return;
    pimpl_->exposer.reset();
}

} // namespace neurometrix::core4d::infrastructure::metrics