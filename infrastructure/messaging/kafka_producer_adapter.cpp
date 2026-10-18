#include "kafka_producer_adapter.hpp"

#include "infrastructure/logging/logger.hpp"

#include <librdkafka/rdkafka.h>

#include <cstdint>
#include <memory>
#include <string>

namespace neurometrix::core4d::infrastructure::kafka {

namespace {

struct ConfDeleter { void operator()(rd_kafka_conf_t* p) const noexcept { if (p) rd_kafka_conf_destroy(p); } };
using ConfPtr = std::unique_ptr<rd_kafka_conf_t, ConfDeleter>;

struct ProducerDeleter { void operator()(rd_kafka_t* p) const noexcept { if (p) rd_kafka_destroy(p); } };
using ProducerPtr = std::unique_ptr<rd_kafka_t, ProducerDeleter>;

[[nodiscard]] std::string esc(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

[[nodiscard]] std::string
result_to_json(const contracts::VolumeResult& r) {
    std::string j;
    j.reserve(1024);
    j += "{\"message_id\":" + std::to_string(r.message_id);
    j += ",\"study_uid\":\"" + esc(r.study_uid) + "\"";
    j += ",\"status\":\"" + esc(r.status) + "\"";
    j += ",\"height\":" + std::to_string(r.height);
    j += ",\"width\":"  + std::to_string(r.width);
    j += ",\"depth\":"  + std::to_string(r.depth);
    j += ",\"mean_hu\":" + std::to_string(r.mean_hu);
    j += ",\"stddev_hu\":" + std::to_string(r.stddev_hu);
    j += ",\"min_hu\":" + std::to_string(r.min_hu);
    j += ",\"max_hu\":" + std::to_string(r.max_hu);
    j += ",\"mean_entropy\":" + std::to_string(r.mean_entropy);
    j += ",\"peak_vesselness\":" + std::to_string(r.peak_vesselness);
    j += ",\"asymmetry_score\":" + std::to_string(r.asymmetry_score);
    j += ",\"total_volume_mm3\":" + std::to_string(r.total_volume_mm3);
    j += ",\"processing_ms\":" + std::to_string(r.processing_ms);
    j += ",\"gpu_used\":" + std::string(r.gpu_used ? "true" : "false");

    j += ",\"findings\":[";
    for (std::size_t i = 0; i < r.findings.size(); ++i) {
        if (i > 0) j += ',';
        const auto& f = r.findings[i];
        j += "{\"type\":\"" + std::string(domain::to_string(f.type)) + "\"";
        j += ",\"confidence\":" + std::to_string(f.confidence);
        j += ",\"centroid_x\":" + std::to_string(f.centroid_x);
        j += ",\"centroid_y\":" + std::to_string(f.centroid_y);
        j += ",\"centroid_z\":" + std::to_string(f.centroid_z);
        j += ",\"volume_mm3\":" + std::to_string(f.volume_mm3);
        j += ",\"mean_hu\":" + std::to_string(f.mean_hu);
        j += ",\"note\":\"" + esc(f.note) + "\"}";
    }
    j += "]";

    if (!r.error_detail.empty()) {
        j += ",\"error_detail\":\"" + esc(r.error_detail) + "\"";
    }
    j += "}";
    return j;
}

} // anonymous namespace

struct KafkaProducerAdapter::Impl {
    ProducerPtr   producer{nullptr};
    std::string   topic_out;
    std::uint32_t timeout_ms{5000};
    bool          started{false};
};

KafkaProducerAdapter::KafkaProducerAdapter(
    const config::KafkaConfig&  config,
    const security::TLSContext& tls
) noexcept
    : pimpl_(std::make_unique<Impl>())
{
    try {
        pimpl_->topic_out  = config.topic_out;
        pimpl_->timeout_ms = config.timeout_ms;

        ConfPtr conf{rd_kafka_conf_new()};
        if (!conf) return;

        auto set = [&](const char* k, const std::string& v) {
            char errstr[512] = {0};
            (void)rd_kafka_conf_set(conf.get(), k, v.c_str(), errstr, sizeof(errstr));
        };

        set("bootstrap.servers", config.bootstrap_servers);
        set("security.protocol", config.security_protocol);
        set("sasl.mechanism",    config.sasl_mechanism);
        set("sasl.username",     config.sasl_username);
        set("sasl.password",     config.sasl_password);
        set("ssl.ca.location",   tls.ca_path());
        set("ssl.certificate.location", tls.cert_path());
        set("ssl.key.location",  tls.key_path());
        set("acks",              "all");
        set("enable.idempotence","true");
        set("compression.type",  "zstd");
        set("message.timeout.ms", std::to_string(pimpl_->timeout_ms));

        rd_kafka_conf_t* raw = conf.release();
        char errstr[512] = {0};
        rd_kafka_t* p = rd_kafka_new(RD_KAFKA_PRODUCER, raw, errstr, sizeof(errstr));
        if (!p) return;
        pimpl_->producer.reset(p);
    } catch (...) {}
}

KafkaProducerAdapter::~KafkaProducerAdapter() { stop(); }

bool KafkaProducerAdapter::start() noexcept {
    if (!pimpl_ || !pimpl_->producer) return false;
    pimpl_->started = true;
    return true;
}

void KafkaProducerAdapter::stop() noexcept {
    if (!pimpl_ || !pimpl_->producer) return;
    (void)rd_kafka_flush(pimpl_->producer.get(), static_cast<int>(pimpl_->timeout_ms));
    pimpl_->producer.reset();
    pimpl_->started = false;
}

std::expected<void, application::PublishError>
KafkaProducerAdapter::publish_result(const contracts::VolumeResult& result) noexcept {
    if (!pimpl_ || !pimpl_->producer || !pimpl_->started) {
        return std::unexpected(application::PublishError::BrokerUnavailable);
    }
    try {
        const std::string payload = result_to_json(result);
        const std::string key     = std::to_string(result.message_id);

        const rd_kafka_resp_err_t err = rd_kafka_producev(
            pimpl_->producer.get(),
            RD_KAFKA_V_TOPIC(pimpl_->topic_out.c_str()),
            RD_KAFKA_V_KEY(key.data(), key.size()),
            RD_KAFKA_V_VALUE(const_cast<char*>(payload.data()), payload.size()),
            RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
            RD_KAFKA_V_END);

        if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
            logging::log_kafka_error(result.message_id, rd_kafka_err2str(err), pimpl_->topic_out);
            return std::unexpected(application::PublishError::UnknownError);
        }

        const rd_kafka_resp_err_t flush_err = rd_kafka_flush(
            pimpl_->producer.get(), static_cast<int>(pimpl_->timeout_ms));

        if (flush_err == RD_KAFKA_RESP_ERR__TIMED_OUT)
            return std::unexpected(application::PublishError::Timeout);
        if (flush_err != RD_KAFKA_RESP_ERR_NO_ERROR)
            return std::unexpected(application::PublishError::UnknownError);

        logging::log_volume_processed(result.message_id, result.study_uid,
                                      result.depth, result.processing_ms,
                                      result.gpu_used);
        return {};
    } catch (...) {
        return std::unexpected(application::PublishError::UnknownError);
    }
}

} // namespace neurometrix::core4d::infrastructure::kafka