#pragma once

#include "application/ports/outbound/i_job_repository.hpp"
#include "infrastructure/config/config_loader.hpp"
#include "infrastructure/security/tls_context.hpp"

#include <memory>

namespace neurometrix::core4d::infrastructure::postgres {

class PostgresRepositoryAdapter final : public application::IJobRepository {
public:
    PostgresRepositoryAdapter(
        const config::PostgresConfig& config,
        const security::TLSContext&   tls
    ) noexcept;

    ~PostgresRepositoryAdapter() override;

    PostgresRepositoryAdapter(const PostgresRepositoryAdapter&) = delete;
    PostgresRepositoryAdapter& operator=(const PostgresRepositoryAdapter&) = delete;

    bool start() noexcept;
    void stop() noexcept;

    [[nodiscard]] std::expected<void, application::RepositoryError>
    save_job(const contracts::VolumeRequest& request) noexcept override;

    [[nodiscard]] std::expected<void, application::RepositoryError>
    mark_processing(std::uint64_t message_id,
                    std::int64_t started_at_ms,
                    std::uint32_t slice_count) noexcept override;

    [[nodiscard]] std::expected<void, application::RepositoryError>
    save_result(const contracts::VolumeResult& result,
                std::int64_t started_at_ms,
                std::int64_t finished_at_ms) noexcept override;

    [[nodiscard]] std::expected<void, application::RepositoryError>
    mark_failed(std::uint64_t message_id,
                std::string_view error_code,
                std::string_view error_detail) noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace neurometrix::core4d::infrastructure::postgres