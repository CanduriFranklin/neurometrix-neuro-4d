#pragma once

#include "contracts/messages.hpp"

#include <cstdint>
#include <expected>

namespace neurometrix::core4d::application {

enum class PublishError : std::uint8_t {
    BrokerUnavailable,
    Timeout,
    UnknownError,
};

class IMessagePublisher {
public:
    virtual ~IMessagePublisher() = default;

    virtual std::expected<void, PublishError>
    publish_result(const contracts::VolumeResult& result) noexcept = 0;
};

} // namespace neurometrix::core4d::application