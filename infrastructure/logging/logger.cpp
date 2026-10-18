#include "logger.hpp"

#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace neurometrix::core4d::infrastructure::logging {

namespace {
std::shared_ptr<spdlog::logger> g_logger;
}

void init_logger(const config::LoggingConfig& cfg) noexcept {
    try {
        auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        g_logger = std::make_shared<spdlog::logger>("core-4d", sink);
        g_logger->set_level(spdlog::level::from_str(cfg.level));
        g_logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(g_logger);
    } catch (...) {}
}

std::shared_ptr<spdlog::logger> get() noexcept {
    if (g_logger) return g_logger;
    static auto noop = []() {
        auto s = std::make_shared<spdlog::sinks::null_sink_mt>();
        return std::make_shared<spdlog::logger>("null", s);
    }();
    return noop;
}

void log_kafka_error(std::uint64_t message_id,
                     const std::string& error_code,
                     const std::string& topic) noexcept {
    get()->error("Kafka error: message_id={} error_code={} topic={}",
                 message_id, error_code, topic);
}

void log_volume_processed(std::uint64_t message_id,
                          const std::string& study_uid,
                          std::uint32_t depth,
                          std::uint64_t processing_ms,
                          bool gpu_used) noexcept {
    get()->info("Volume processed: message_id={} study_uid={} depth={} processing_ms={} gpu={}",
                message_id, study_uid, depth, processing_ms, gpu_used);
}

} // namespace neurometrix::core4d::infrastructure::logging