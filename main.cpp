#include "application/use_cases/process_volume_use_case.hpp"
#include "infrastructure/compression/dicom_volume_loader_adapter.hpp"
#include "infrastructure/config/config_loader.hpp"
#include "infrastructure/logging/logger.hpp"
#include "infrastructure/messaging/kafka_consumer_adapter.hpp"
#include "infrastructure/messaging/kafka_producer_adapter.hpp"
#include "infrastructure/metrics/metrics_server.hpp"
#include "infrastructure/persistence/postgres_repository_adapter.hpp"
#include "infrastructure/security/tls_context.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int /*sig*/) {
    g_stop.store(true, std::memory_order_relaxed);
}

} // namespace

int main(int /*argc*/, char** /*argv*/) {
    using namespace neurometrix::core4d;

    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    // ---- Config ----
    auto cfg = infrastructure::config::load_config();
    if (!cfg) {
        std::cerr << "core-4d: config load failed (missing required env vars)\n";
        return 2;
    }

    // ---- Logging ----
    infrastructure::logging::init_logger(cfg->logging);
    auto log = infrastructure::logging::get();
    log->info("Core-4D starting up");

    // ---- Metrics ----
    infrastructure::metrics::MetricsServer metrics{cfg->metrics};
    if (!metrics.start()) {
        log->error("metrics server failed to start");
        return 3;
    }
    log->info("Metrics server started on port {}", cfg->metrics.port);

    // ---- TLS (Kafka) ----
    auto tls_exp = infrastructure::security::TLSContext::load(
        cfg->kafka.ssl_ca_location,
        cfg->kafka.ssl_cert_location,
        cfg->kafka.ssl_key_location
    );
    if (!tls_exp) {
        log->error("TLS context load failed");
        return 4;
    }
    const auto& tls = *tls_exp;

    // ---- Postgres ----
    infrastructure::postgres::PostgresRepositoryAdapter repository{cfg->postgres, tls};
    if (!repository.start()) {
        log->error("postgres adapter failed to start");
        return 5;
    }
    log->info("Postgres adapter started");

    // ---- DICOM loader ----
    infrastructure::dicom::DicomVolumeLoaderAdapter loader{};
    log->info("DICOM 4D loader ready");

    // ---- Kafka producer ----
    infrastructure::kafka::KafkaProducerAdapter publisher{cfg->kafka, tls};
    if (!publisher.start()) {
        log->error("kafka producer failed to start");
        return 6;
    }
    log->info("Kafka producer started");

    // ---- Use case ----
    application::ProcessVolumeUseCase use_case{loader, repository, publisher};
    log->info("Use case wired");

    // ---- Kafka consumer ----
    infrastructure::kafka::KafkaConsumerAdapter consumer{cfg->kafka, tls, use_case};

    log->info("Core-4D is now consuming from Kafka");
    std::thread consumer_thread{[&consumer, &log]() {
        if (!consumer.run()) {
            log->error("[consumer] run() returned false");
        }
    }};

    // ---- Main loop ----
    while (!g_stop.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    log->info("Core-4D shutdown requested");
    consumer.stop();
    if (consumer_thread.joinable()) consumer_thread.join();
    publisher.stop();
    repository.stop();
    metrics.stop();
    log->info("Core-4D stopped");
    return 0;
}
