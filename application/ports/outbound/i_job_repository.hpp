#pragma once

#include "contracts/messages.hpp"

#include <cstdint>
#include <expected>
#include <string_view>

namespace neurometrix::core4d::application {

enum class RepositoryError : std::uint8_t {
    DuplicateKey,
    ConnectionLost,
    UnknownError,
};

class IJobRepository {
public:
    virtual ~IJobRepository() = default;

    virtual std::expected<void, RepositoryError>
    save_job(const contracts::VolumeRequest& request) noexcept = 0;

    virtual std::expected<void, RepositoryError>
    mark_processing(std::uint64_t message_id,
                    std::int64_t started_at_ms,
                    std::uint32_t slice_count) noexcept = 0;

    virtual std::expected<void, RepositoryError>
    save_result(const contracts::VolumeResult& result,
                std::int64_t started_at_ms,
                std::int64_t finished_at_ms) noexcept = 0;

    virtual std::expected<void, RepositoryError>
    mark_failed(std::uint64_t message_id,
                std::string_view error_code,
                std::string_view error_detail) noexcept = 0;
};

} // namespace neurometrix::core4d::application