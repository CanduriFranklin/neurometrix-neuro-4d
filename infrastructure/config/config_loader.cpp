#include "config_loader.hpp"

#include <cstdlib>
#include <optional>
#include <string>

namespace neurometrix::core4d::infrastructure::config {

namespace {

[[nodiscard]] std::optional<std::string> read_env(std::string_view name) {
    const std::string key{name};
    const char* v = std::getenv(key.c_str());
    if (v == nullptr) return std::nullopt;
    return std::string{v};
}

[[nodiscard]] std::expected<std::string, ConfigError>
read_required(std::string_view name) {
    auto v = read_env(name);
    if (!v.has_value()) return std::unexpected(ConfigError::MissingRequiredVariable);
    if (v->empty())     return std::unexpected(ConfigError::EmptyValue);
    return *v;
}

[[nodiscard]] std::string read_default(std::string_view name, std::string fallback) {
    auto v = read_env(name);
    return v.has_value() && !v->empty() ? *v : std::move(fallback);
}

[[nodiscard]] std::expected<std::uint32_t, ConfigError>
read_uint(std::string_view name, std::uint32_t fallback) {
    auto v = read_env(name);
    if (!v.has_value() || v->empty()) return fallback;
    try { return static_cast<std::uint32_t>(std::stoul(*v)); }
    catch (...) { return std::unexpected(ConfigError::InvalidInteger); }
}

} // anonymous namespace

std::expected<Config, ConfigError> load_config() noexcept {
    try {
        Config cfg{};

        auto bs = read_required(env_vars::kKafkaBootstrapServers);
        if (!bs) return std::unexpected(bs.error());
        cfg.kafka.bootstrap_servers = std::move(*bs);

        cfg.kafka.security_protocol = read_default(env_vars::kKafkaSecurityProtocol, "PLAINTEXT");
        cfg.kafka.sasl_mechanism    = read_default(env_vars::kKafkaSaslMechanism,    "PLAIN");
        cfg.kafka.sasl_username     = read_default(env_vars::kKafkaSaslUsername,     "dummy");
        cfg.kafka.sasl_password     = read_default(env_vars::kKafkaSaslPassword,     "dummy");
        cfg.kafka.ssl_ca_location   = read_default(env_vars::kKafkaSslCaLocation,    "dummy");
        cfg.kafka.ssl_cert_location = read_default(env_vars::kKafkaSslCertLocation,  "dummy");
        cfg.kafka.ssl_key_location  = read_default(env_vars::kKafkaSslKeyLocation,   "dummy");

        auto ti = read_required(env_vars::kKafkaTopicIn);
        if (!ti) return std::unexpected(ti.error());
        cfg.kafka.topic_in = std::move(*ti);

        auto to = read_required(env_vars::kKafkaTopicOut);
        if (!to) return std::unexpected(to.error());
        cfg.kafka.topic_out = std::move(*to);

        auto cg = read_required(env_vars::kKafkaConsumerGroup);
        if (!cg) return std::unexpected(cg.error());
        cfg.kafka.consumer_group = std::move(*cg);

        auto t = read_uint(env_vars::kKafkaTimeoutMs, 5000);
        if (!t) return std::unexpected(t.error());
        cfg.kafka.timeout_ms = *t;

        auto url = read_required(env_vars::kDatabaseUrl);
        if (!url) return std::unexpected(url.error());
        cfg.postgres.connection_string = std::move(*url);

        cfg.postgres.ssl_mode               = read_default(env_vars::kDatabaseSslMode, "disable");
        cfg.postgres.ssl_cert_location      = read_default(env_vars::kDatabaseSslCert, "dummy");
        cfg.postgres.ssl_key_location       = read_default(env_vars::kDatabaseSslKey,  "dummy");
        cfg.postgres.ssl_root_cert_location = read_default(env_vars::kDatabaseSslRootCert, "dummy");

        auto pt = read_uint(env_vars::kDatabaseTimeoutMs, 5000);
        if (!pt) return std::unexpected(pt.error());
        cfg.postgres.connection_timeout_ms = *pt;

        auto mp = read_uint(env_vars::kMetricsPort, 9092);
        if (!mp) return std::unexpected(mp.error());
        cfg.metrics.port = static_cast<std::uint16_t>(*mp);
        cfg.metrics.bind_address = read_default(env_vars::kMetricsBindAddress, "0.0.0.0");

        cfg.logging.level  = read_default(env_vars::kLogLevel,  "info");
        cfg.logging.format = read_default(env_vars::kLogFormat, "text");

        return cfg;
    } catch (...) {
        return std::unexpected(ConfigError::UnknownError);
    }
}

} // namespace neurometrix::core4d::infrastructure::config