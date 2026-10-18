#include "kafka_consumer_adapter.hpp"

#include "infrastructure/logging/logger.hpp"

#include <librdkafka/rdkafka.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace neurometrix::core4d::infrastructure::kafka {

namespace {

struct ConfDeleter { void operator()(rd_kafka_conf_t* p) const noexcept { if (p) rd_kafka_conf_destroy(p); } };
using ConfPtr = std::unique_ptr<rd_kafka_conf_t, ConfDeleter>;

struct ConsumerDeleter { void operator()(rd_kafka_t* p) const noexcept { if (p) rd_kafka_destroy(p); } };
using ConsumerPtr = std::unique_ptr<rd_kafka_t, ConsumerDeleter>;

struct TplDeleter { void operator()(rd_kafka_topic_partition_list_t* p) const noexcept { if (p) rd_kafka_topic_partition_list_destroy(p); } };
using TplPtr = std::unique_ptr<rd_kafka_topic_partition_list_t, TplDeleter>;

// Same binary layout as the Router emits:
//   [8] message_id
//   [8] timestamp_utc
//   [4] study_uid_len + [N] study_uid
//   [4] dicom_path_len + [M] dicom_path
[[nodiscard]] bool
deserialize(const char* data, std::size_t size, contracts::VolumeRequest& out) noexcept {
    try {
        if (data == nullptr || size < 20) return false;
        std::uint64_t mid = 0, ts = 0;
        std::memcpy(&mid, data, 8);
        std::memcpy(&ts,  data + 8, 8);

        std::uint32_t uid_len = 0;
        std::memcpy(&uid_len, data + 16, 4);
        if (size < 20 + uid_len + 4) return false;
        out.study_uid.assign(data + 20, uid_len);

        std::uint32_t path_len = 0;
        std::memcpy(&path_len, data + 20 + uid_len, 4);
        if (size < 20 + uid_len + 4 + path_len) return false;
        out.dicom_path.assign(data + 20 + uid_len + 4, path_len);

        out.message_id  = mid;
        out.timestamp_utc = ts;
        return true;
    } catch (...) { return false; }
}

} // anonymous namespace

struct KafkaConsumerAdapter::Impl {
    ConsumerPtr                        consumer{nullptr};
    std::string                        topic_in;
    std::string                        consumer_group;
    std::uint32_t                      timeout_ms{5000};
    std::atomic<bool>                  stop_requested{false};
    application::IProcessVolumeUseCase* use_case{nullptr};
};

KafkaConsumerAdapter::KafkaConsumerAdapter(
    const config::KafkaConfig&          config,
    const security::TLSContext&         tls,
    application::IProcessVolumeUseCase& use_case
) noexcept
    : pimpl_(std::make_unique<Impl>())
{
    try {
        pimpl_->topic_in       = config.topic_in;
        pimpl_->consumer_group = config.consumer_group;
        pimpl_->timeout_ms     = config.timeout_ms;
        pimpl_->use_case       = &use_case;

        ConfPtr conf{rd_kafka_conf_new()};
        if (!conf) return;

        auto set = [&](const char* k, const std::string& v) {
            char errstr[512] = {0};
            (void)rd_kafka_conf_set(conf.get(), k, v.c_str(), errstr, sizeof(errstr));
        };

        set("bootstrap.servers", config.bootstrap_servers);
        set("group.id",          config.consumer_group);
        set("client.id",         "core-4d-consumer");
        set("security.protocol", config.security_protocol);
        set("sasl.mechanism",    config.sasl_mechanism);
        set("sasl.username",     config.sasl_username);
        set("sasl.password",     config.sasl_password);
        set("ssl.ca.location",   tls.ca_path());
        set("ssl.certificate.location", tls.cert_path());
        set("ssl.key.location",  tls.key_path());
        set("enable.auto.commit","false");
        set("auto.offset.reset", "earliest");
        set("enable.partition.eof", "false");
        set("session.timeout.ms",   "45000");
        set("max.poll.interval.ms", "300000");

        rd_kafka_conf_t* raw = conf.release();
        char errstr[512] = {0};
        rd_kafka_t* c = rd_kafka_new(RD_KAFKA_CONSUMER, raw, errstr, sizeof(errstr));
        if (!c) return;
        rd_kafka_poll_set_consumer(c);
        pimpl_->consumer.reset(c);
    } catch (...) {}
}

KafkaConsumerAdapter::~KafkaConsumerAdapter() { stop(); }

bool KafkaConsumerAdapter::run() noexcept {
    if (!pimpl_ || !pimpl_->consumer || !pimpl_->use_case) return false;

    try {
        logging::get()->info("[consumer] run() started. topic={} group={}",
                             pimpl_->topic_in, pimpl_->consumer_group);

        TplPtr topics{rd_kafka_topic_partition_list_new(1)};
        if (!topics) return false;
        rd_kafka_topic_partition_list_add(
            topics.get(), pimpl_->topic_in.c_str(), RD_KAFKA_PARTITION_UA);

        if (rd_kafka_subscribe(pimpl_->consumer.get(), topics.get())
            != RD_KAFKA_RESP_ERR_NO_ERROR) {
            logging::get()->error("[consumer] subscribe failed");
            return false;
        }
        logging::get()->info("[consumer] subscribed to '{}'", pimpl_->topic_in);

        constexpr int kPollTimeoutMs = 500;

        while (!pimpl_->stop_requested.load(std::memory_order_acquire)) {
            rd_kafka_message_t* msg =
                rd_kafka_consumer_poll(pimpl_->consumer.get(), kPollTimeoutMs);

            if (msg == nullptr) continue;

            if (msg->err != RD_KAFKA_RESP_ERR_NO_ERROR) {
                if (msg->err == RD_KAFKA_RESP_ERR__PARTITION_EOF ||
                    msg->err == RD_KAFKA_RESP_ERR__TIMED_OUT) {
                    rd_kafka_message_destroy(msg);
                    continue;
                }
                logging::get()->error("[consumer] broker error: {}",
                                      rd_kafka_err2str(msg->err));
                rd_kafka_message_destroy(msg);
                return false;
            }

            contracts::VolumeRequest req{};
            if (!deserialize(static_cast<const char*>(msg->payload), msg->len, req)) {
                logging::get()->warn("[consumer] deserialize failed (len={})", msg->len);
                (void)rd_kafka_commit_message(pimpl_->consumer.get(), msg, 0);
                rd_kafka_message_destroy(msg);
                continue;
            }

            logging::get()->info("[consumer] processing id={} path='{}'",
                                 req.message_id, req.dicom_path);

            auto result = pimpl_->use_case->execute(req);
            if (!result) {
                logging::get()->error("[consumer] use case failed for id={}",
                                      req.message_id);
                rd_kafka_message_destroy(msg);
                continue;
            }

            logging::get()->info("[consumer] committed offset {} (id={})",
                                 msg->offset, req.message_id);
            (void)rd_kafka_commit_message(pimpl_->consumer.get(), msg, 0);
            rd_kafka_message_destroy(msg);
        }

        (void)rd_kafka_consumer_close(pimpl_->consumer.get());
        return true;
    } catch (...) { return false; }
}

void KafkaConsumerAdapter::stop() noexcept {
    if (!pimpl_) return;
    pimpl_->stop_requested.store(true, std::memory_order_release);
}

} // namespace neurometrix::core4d::infrastructure::kafka