#pragma once

#include "contracts/messages.hpp"

#include <cstdint>
#include <expected>

namespace neurometrix::core4d::application {

enum class UseCaseError : std::uint8_t {
    InputValidationFailed,
    DicomReadFailed,
    ComputeFailed,
    RepositoryUnavailable,
    PublisherUnavailable,
    InternalError,
};

class IProcessVolumeUseCase {
public:
    virtual ~IProcessVolumeUseCase() = default;

    virtual std::expected<contracts::VolumeResult, UseCaseError>
    execute(const contracts::VolumeRequest& request) noexcept = 0;
};

} // namespace neurometrix::core4d::application