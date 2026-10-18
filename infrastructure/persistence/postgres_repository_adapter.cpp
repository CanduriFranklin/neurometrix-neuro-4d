#include "postgres_repository_adapter.hpp"

#include "infrastructure/logging/logger.hpp"

#include <libpq-fe.h>

#include <cstring>
#include <memory>
#include <string>

namespace neurometrix::core4d::infrastructure::postgres {

namespace {

struct PgResultDeleter { void operator()(PGresult* p) const noexcept { if (p) PQclear(p); } };
using PgResultPtr = std::unique_ptr<PGresult, PgResultDeleter>;

[[nodiscard]] std::string
build_conninfo(const config::PostgresConfig& cfg,
               const security::TLSContext&   tls) {
    std::string c = cfg.connection_string;
    if (c.empty()) return c;

    const bool is_uri = c.rfind("postgresql://", 0) == 0 ||
                        c.rfind("postgres://",   0) == 0;

    auto add = [&](std::string_view k, std::string_view v) {
        if (v.empty()) return;
        if (is_uri) {
            c += (c.find('?') == std::string::npos ? '?' : '&');
            c += k; c += '='; c += v;
        } else {
            if (!c.empty() && c.back() != ' ') c += ' ';
            c += k; c += '='; c += v;
        }
    };

    add("sslmode",         cfg.ssl_mode);
    add("connect_timeout", std::to_string(cfg.connection_timeout_ms / 1000));

    const bool tls_on = cfg.ssl_mode != "disable" && cfg.ssl_mode != "allow";
    if (tls_on) {
        add("sslcert",     tls.cert_path());
        add("sslkey",      tls.key_path());
        add("sslrootcert", tls.ca_path());
    }
    return c;
}

[[nodiscard]] application::RepositoryError map_sqlstate(const char* s) {
    if (!s) return application::RepositoryError::UnknownError;
    if (std::strcmp(s, "23505") == 0) return application::RepositoryError::DuplicateKey;
    if (std::strncmp(s, "08", 2) == 0) return application::RepositoryError::ConnectionLost;
    return application::RepositoryError::UnknownError;
}

} // anonymous namespace

struct PostgresRepositoryAdapter::Impl {
    PGconn*     conn{nullptr};
    std::string conninfo;
    bool        started{false};

    ~Impl() { if (conn) PQfinish(conn); }
};

PostgresRepositoryAdapter::PostgresRepositoryAdapter(
    const config::PostgresConfig& config,
    const security::TLSContext&   tls
) noexcept
    : pimpl_(std::make_unique<Impl>())
{
    try { pimpl_->conninfo = build_conninfo(config, tls); }
    catch (...) { pimpl_->conninfo.clear(); }
}

PostgresRepositoryAdapter::~PostgresRepositoryAdapter() { stop(); }

bool PostgresRepositoryAdapter::start() noexcept {
    if (!pimpl_ || pimpl_->conninfo.empty()) return false;
    try {
        pimpl_->conn = PQconnectdb(pimpl_->conninfo.c_str());
        if (!pimpl_->conn) return false;
        if (PQstatus(pimpl_->conn) != CONNECTION_OK) {
            logging::get()->error("[postgres] {}", PQerrorMessage(pimpl_->conn));
            return false;
        }
        pimpl_->started = true;
        return true;
    } catch (...) { return false; }
}

void PostgresRepositoryAdapter::stop() noexcept {
    if (!pimpl_) return;
    if (pimpl_->conn) { PQfinish(pimpl_->conn); pimpl_->conn = nullptr; }
    pimpl_->started = false;
}

std::expected<void, application::RepositoryError>
PostgresRepositoryAdapter::save_job(const contracts::VolumeRequest& r) noexcept {
    if (!pimpl_ || !pimpl_->started) {
        return std::unexpected(application::RepositoryError::ConnectionLost);
    }
    static constexpr const char* kSql =
        "INSERT INTO public.jobs (message_id, study_uid, source_path, status) "
        "VALUES ($1, $2, $3, 'queued') "
        "ON CONFLICT (message_id) DO NOTHING";

    const std::string mid = std::to_string(r.message_id);
    const char* params[3] = { mid.c_str(), r.study_uid.c_str(), r.dicom_path.c_str() };

    PgResultPtr res{PQexecParams(pimpl_->conn, kSql, 3, nullptr, params,
                                 nullptr, nullptr, 0)};
    if (!res || PQresultStatus(res.get()) != PGRES_COMMAND_OK) {
        const char* st = res ? PQresultErrorField(res.get(), PG_DIAG_SQLSTATE) : nullptr;
        return std::unexpected(map_sqlstate(st));
    }
    return {};
}

std::expected<void, application::RepositoryError>
PostgresRepositoryAdapter::mark_processing(std::uint64_t mid,
                                            std::int64_t started_at_ms,
                                            std::uint32_t slice_count) noexcept {
    if (!pimpl_ || !pimpl_->started) {
        return std::unexpected(application::RepositoryError::ConnectionLost);
    }
    static constexpr const char* kSql =
        "UPDATE public.jobs SET status='processing', started_at_utc_ms=$2, "
        "slice_count=$3, updated_at=NOW() WHERE message_id=$1";

    const std::string id  = std::to_string(mid);
    const std::string ts  = std::to_string(started_at_ms);
    const std::string sc  = std::to_string(slice_count);
    const char* params[3] = { id.c_str(), ts.c_str(), sc.c_str() };

    PgResultPtr res{PQexecParams(pimpl_->conn, kSql, 3, nullptr, params,
                                 nullptr, nullptr, 0)};
    if (!res || PQresultStatus(res.get()) != PGRES_COMMAND_OK) {
        return std::unexpected(application::RepositoryError::UnknownError);
    }
    return {};
}

std::expected<void, application::RepositoryError>
PostgresRepositoryAdapter::save_result(const contracts::VolumeResult& r,
                                        std::int64_t started_at_ms,
                                        std::int64_t finished_at_ms) noexcept {
    if (!pimpl_ || !pimpl_->started) {
        return std::unexpected(application::RepositoryError::ConnectionLost);
    }
    static constexpr const char* kSql =
        "UPDATE public.jobs SET status='done', finished_at_utc_ms=$2, "
        "processing_ms=$3, updated_at=NOW() WHERE message_id=$1";

    const std::string id  = std::to_string(r.message_id);
    const std::string fin = std::to_string(finished_at_ms);
    const std::string dur = std::to_string(r.processing_ms);
    (void)started_at_ms;
    const char* params[3] = { id.c_str(), fin.c_str(), dur.c_str() };

    PgResultPtr res{PQexecParams(pimpl_->conn, kSql, 3, nullptr, params,
                                 nullptr, nullptr, 0)};
    if (!res || PQresultStatus(res.get()) != PGRES_COMMAND_OK) {
        return std::unexpected(application::RepositoryError::UnknownError);
    }

    // Persist 4D metrics into results
    static constexpr const char* kSqlR =
        "INSERT INTO public.results (job_id, metric_name, metric_value) "
        "SELECT id, $2, $3::double precision FROM public.jobs WHERE message_id=$1";

    auto insert_metric = [&](const char* name, double val) {
        const std::string nm = name;
        const std::string vv = std::to_string(val);
        const char* p2[3] = { id.c_str(), nm.c_str(), vv.c_str() };
        PgResultPtr r2{PQexecParams(pimpl_->conn, kSqlR, 3, nullptr, p2,
                                    nullptr, nullptr, 0)};
    };
    insert_metric("mean_hu",          r.mean_hu);
    insert_metric("stddev_hu",        r.stddev_hu);
    insert_metric("min_hu",           r.min_hu);
    insert_metric("max_hu",           r.max_hu);
    insert_metric("mean_entropy",     r.mean_entropy);
    insert_metric("peak_vesselness",  r.peak_vesselness);
    insert_metric("asymmetry_score",  r.asymmetry_score);
    insert_metric("total_volume_mm3", r.total_volume_mm3);

    // ---- 4D: persist perfusion summary ----
    if (r.is_4d) {
        insert_metric("timepoints",        static_cast<double>(r.timepoints));
        insert_metric("core_voxels",       static_cast<double>(r.mismatch.core_voxels));
        insert_metric("penumbra_voxels",   static_cast<double>(r.mismatch.penumbra_voxels));
        insert_metric("mismatch_ratio",    r.mismatch.mismatch_ratio);

        // perfusion_maps: one row per map type (mean_value only)
        static constexpr const char* kSqlP =
            "INSERT INTO public.perfusion_maps (job_id, map_type, mean_value) "
            "SELECT id, $2, $3::double precision FROM public.jobs WHERE message_id=$1";

        auto insert_perfusion = [&](const char* map_type, double val) {
            const std::string mt = map_type;
            const std::string vv = std::to_string(val);
            const char* pp[3] = { id.c_str(), mt.c_str(), vv.c_str() };
            PgResultPtr rp{PQexecParams(pimpl_->conn, kSqlP, 3, nullptr, pp,
                                        nullptr, nullptr, 0)};
        };
        insert_perfusion("CBF",  r.perfusion.mean_cbf);
        insert_perfusion("CBV",  r.perfusion.mean_cbv);
        insert_perfusion("MTT",  r.perfusion.mean_mtt);
        insert_perfusion("TTP",  r.perfusion.mean_ttp);
        insert_perfusion("TMax", r.perfusion.mean_tmax);

        // temporal_metrics: timepoints + mean entropy over time
        static constexpr const char* kSqlT =
            "INSERT INTO public.temporal_metrics (job_id, metric_name, mean_value) "
            "SELECT id, $2, $3::double precision FROM public.jobs WHERE message_id=$1";

        auto insert_temporal = [&](const char* name, double val) {
            const std::string nm = name;
            const std::string vv = std::to_string(val);
            const char* pt[3] = { id.c_str(), nm.c_str(), vv.c_str() };
            PgResultPtr rt{PQexecParams(pimpl_->conn, kSqlT, 3, nullptr, pt,
                                        nullptr, nullptr, 0)};
        };
        insert_temporal("timepoints",    static_cast<double>(r.timepoints));
        insert_temporal("mean_entropy",  r.mean_entropy);
    }

    // Persist findings into public.findings
    if (!r.findings.empty()) {
        static constexpr const char* kSqlF =
            "INSERT INTO public.findings "
            "(job_id, finding_type, confidence, centroid_x, centroid_y, centroid_z, "
            " volume_mm3, mean_hu, note) "
            "SELECT id, $2, $3::double precision, $4::int, $5::int, $6::int, "
            "       $7::double precision, $8::double precision, $9 "
            "FROM public.jobs WHERE message_id=$1";

        for (const auto& f : r.findings) {
            const std::string ft  = domain::to_string(f.type);
            const std::string cf  = std::to_string(f.confidence);
            const std::string cx  = std::to_string(f.centroid_x);
            const std::string cy  = std::to_string(f.centroid_y);
            const std::string cz  = std::to_string(f.centroid_z);
            const std::string vv  = std::to_string(f.volume_mm3);
            const std::string hu  = std::to_string(f.mean_hu);
            const char* pf[9] = {
                id.c_str(), ft.c_str(), cf.c_str(), cx.c_str(), cy.c_str(),
                cz.c_str(), vv.c_str(), hu.c_str(), f.note.c_str()
            };
            PgResultPtr rf{PQexecParams(pimpl_->conn, kSqlF, 9, nullptr, pf,
                                        nullptr, nullptr, 0)};
        }
    }
    return {};
}

std::expected<void, application::RepositoryError>
PostgresRepositoryAdapter::mark_failed(std::uint64_t mid,
                                        std::string_view code,
                                        std::string_view detail) noexcept {
    if (!pimpl_ || !pimpl_->started) {
        return std::unexpected(application::RepositoryError::ConnectionLost);
    }
    static constexpr const char* kSql =
        "UPDATE public.jobs SET status='failed', error_code=$2, "
        "error_detail=$3, updated_at=NOW() WHERE message_id=$1";

    const std::string id = std::to_string(mid);
    const std::string c{code};
    const std::string d{detail};
    const char* params[3] = { id.c_str(), c.c_str(), d.c_str() };

    PgResultPtr res{PQexecParams(pimpl_->conn, kSql, 3, nullptr, params,
                                 nullptr, nullptr, 0)};
    if (!res || PQresultStatus(res.get()) != PGRES_COMMAND_OK) {
        return std::unexpected(application::RepositoryError::UnknownError);
    }
    return {};
}

} // namespace neurometrix::core4d::infrastructure::postgres