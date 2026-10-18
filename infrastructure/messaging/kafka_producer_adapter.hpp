#pragma once

#include "application/ports/outbound/i_message_publisher.hpp"
#include "infrastructure/config/config_loader.hpp"
#include "infrastructure/security/tls_context.hpp"

#include <memory>

namespace neurometrix::core4d::infrastructure::kafka {

class KafkaProducerAdapter final : public application::IMessagePublisher {
public:
    KafkaProducerAdapter(
        const config::KafkaConfig&  config,
        const security::TLSContext& tls
    ) noexcept;

    ~KafkaProducerAdapter() override;

    KafkaProducerAdapter(const KafkaProducerAdapter&) = delete;
    KafkaProducerAdapter& operator=(const KafkaProducerAdapter&) = delete;

    bool start() noexcept;
    void stop() noexcept;

    [[nodiscard]] std::expected<void, application::PublishError>
    publish_result(const contracts::VolumeResult& result) noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace neurometrix::core4d::infrastructure::kafka