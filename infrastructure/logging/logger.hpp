#pragma once

#include "infrastructure/config/config_loader.hpp"

#include <spdlog/spdlog.h>
#include <cstdint>
#include <memory>
#include <string>

namespace neurometrix::core4d::infrastructure::logging {

void init_logger(const config::LoggingConfig& cfg) noexcept;
[[nodiscard]] std::shared_ptr<spdlog::logger> get() noexcept;

void log_kafka_error(std::uint64_t message_id,
                     const std::string& error_code,
                     const std::string& topic) noexcept;

void log_volume_processed(std::uint64_t message_id,
                          const std::string& study_uid,
                          std::uint32_t depth,
                          std::uint64_t processing_ms,
                          bool gpu_used) noexcept;

} // namespace neurometrix::core4d::infrastructure::logging