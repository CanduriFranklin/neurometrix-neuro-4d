#pragma once

#include "application/ports/inbound/i_process_volume_use_case.hpp"
#include "infrastructure/config/config_loader.hpp"
#include "infrastructure/security/tls_context.hpp"

#include <atomic>
#include <memory>

namespace neurometrix::core4d::infrastructure::kafka {

class KafkaConsumerAdapter {
public:
    KafkaConsumerAdapter(
        const config::KafkaConfig&          config,
        const security::TLSContext&         tls,
        application::IProcessVolumeUseCase& use_case
    ) noexcept;

    ~KafkaConsumerAdapter();

    KafkaConsumerAdapter(const KafkaConsumerAdapter&) = delete;
    KafkaConsumerAdapter& operator=(const KafkaConsumerAdapter&) = delete;

    bool run() noexcept;
    void stop() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace neurometrix::core4d::infrastructure::kafka