#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace neurometrix::core4d::infrastructure::config {

enum class ConfigError : std::uint8_t {
    MissingRequiredVariable,
    InvalidInteger,
    EmptyValue,
    UnknownError,
};

struct KafkaConfig {
    std::string   bootstrap_servers;
    std::string   security_protocol;
    std::string   sasl_mechanism;
    std::string   sasl_username;
    std::string   sasl_password;
    std::string   ssl_ca_location;
    std::string   ssl_cert_location;
    std::string   ssl_key_location;
    std::string   topic_in;
    std::string   topic_out;
    std::string   consumer_group;
    std::uint32_t timeout_ms;
};

struct PostgresConfig {
    std::string   connection_string;
    std::string   ssl_mode;
    std::string   ssl_cert_location;
    std::string   ssl_key_location;
    std::string   ssl_root_cert_location;
    std::uint32_t connection_timeout_ms;
};

struct MetricsConfig {
    std::uint16_t port;
    std::string   bind_address;
};

struct LoggingConfig {
    std::string level;
    std::string format;
};

struct Config {
    KafkaConfig    kafka;
    PostgresConfig postgres;
    MetricsConfig  metrics;
    LoggingConfig  logging;
};

[[nodiscard]] std::expected<Config, ConfigError> load_config() noexcept;

namespace env_vars {
    inline constexpr std::string_view kKafkaBootstrapServers = "KAFKA_BOOTSTRAP_SERVERS";
    inline constexpr std::string_view kKafkaSecurityProtocol = "KAFKA_SECURITY_PROTOCOL";
    inline constexpr std::string_view kKafkaSaslMechanism    = "KAFKA_SASL_MECHANISM";
    inline constexpr std::string_view kKafkaSaslUsername     = "KAFKA_SASL_USERNAME";
    inline constexpr std::string_view kKafkaSaslPassword     = "KAFKA_SASL_PASSWORD";
    inline constexpr std::string_view kKafkaSslCaLocation    = "KAFKA_SSL_CA_LOCATION";
    inline constexpr std::string_view kKafkaSslCertLocation  = "KAFKA_SSL_CERT_LOCATION";
    inline constexpr std::string_view kKafkaSslKeyLocation   = "KAFKA_SSL_KEY_LOCATION";
    inline constexpr std::string_view kKafkaTopicIn          = "KAFKA_TOPIC_IN";
    inline constexpr std::string_view kKafkaTopicOut         = "KAFKA_TOPIC_OUT";
    inline constexpr std::string_view kKafkaConsumerGroup    = "KAFKA_CONSUMER_GROUP";
    inline constexpr std::string_view kKafkaTimeoutMs        = "KAFKA_TIMEOUT_MS";
    inline constexpr std::string_view kDatabaseUrl           = "DATABASE_URL";
    inline constexpr std::string_view kDatabaseSslMode       = "DATABASE_SSLMODE";
    inline constexpr std::string_view kDatabaseSslCert       = "DATABASE_SSL_CERT";
    inline constexpr std::string_view kDatabaseSslKey        = "DATABASE_SSL_KEY";
    inline constexpr std::string_view kDatabaseSslRootCert   = "DATABASE_SSL_ROOT_CERT";
    inline constexpr std::string_view kDatabaseTimeoutMs     = "DATABASE_TIMEOUT_MS";
    inline constexpr std::string_view kMetricsPort           = "METRICS_PORT";
    inline constexpr std::string_view kMetricsBindAddress    = "METRICS_BIND_ADDRESS";
    inline constexpr std::string_view kLogLevel              = "LOG_LEVEL";
    inline constexpr std::string_view kLogFormat             = "LOG_FORMAT";
}

} // namespace neurometrix::core4d::infrastructure::config