-- Core 4D — Schema alineado con postgres_repository_adapter (clon 3D)
DROP TABLE IF EXISTS temporal_metrics CASCADE;
DROP TABLE IF EXISTS perfusion_maps CASCADE;
DROP TABLE IF EXISTS findings CASCADE;
DROP TABLE IF EXISTS results CASCADE;
DROP TABLE IF EXISTS jobs CASCADE;

CREATE TABLE jobs (
    id                    BIGSERIAL PRIMARY KEY,
    message_id            BIGINT NOT NULL UNIQUE,
    study_uid             TEXT   NOT NULL,
    source_path           TEXT   NOT NULL,
    status                TEXT   NOT NULL DEFAULT 'queued',
    started_at_utc_ms     BIGINT,
    finished_at_utc_ms    BIGINT,
    processing_ms         BIGINT,
    slice_count           INTEGER,
    error_code            TEXT,
    error_detail          TEXT,
    created_at            TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at            TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX idx_jobs_status     ON jobs(status);
CREATE INDEX idx_jobs_study_uid  ON jobs(study_uid);
CREATE INDEX idx_jobs_created_at ON jobs(created_at DESC);

CREATE TABLE results (
    id                BIGSERIAL PRIMARY KEY,
    job_id            BIGINT NOT NULL REFERENCES jobs(id) ON DELETE CASCADE,
    metric_name       TEXT   NOT NULL,
    metric_value      DOUBLE PRECISION,
    payload           JSONB,
    created_at        TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX idx_results_job    ON results(job_id);
CREATE INDEX idx_results_metric ON results(metric_name);

CREATE TABLE findings (
    id                BIGSERIAL PRIMARY KEY,
    job_id            BIGINT NOT NULL REFERENCES jobs(id) ON DELETE CASCADE,
    finding_type      TEXT   NOT NULL,
    confidence        DOUBLE PRECISION NOT NULL,
    centroid_x        INTEGER NOT NULL,
    centroid_y        INTEGER NOT NULL,
    centroid_z        INTEGER NOT NULL,
    volume_mm3        DOUBLE PRECISION,
    mean_hu           DOUBLE PRECISION,
    note              TEXT,
    created_at        TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX idx_findings_job  ON findings(job_id);
CREATE INDEX idx_findings_type ON findings(finding_type);

CREATE TABLE perfusion_maps (
    id                BIGSERIAL PRIMARY KEY,
    job_id            BIGINT NOT NULL REFERENCES jobs(id) ON DELETE CASCADE,
    map_type          TEXT   NOT NULL,
    mean_value        DOUBLE PRECISION,
    std_value         DOUBLE PRECISION,
    min_value         DOUBLE PRECISION,
    max_value         DOUBLE PRECISION,
    histogram         JSONB,
    created_at        TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX idx_perfusion_job  ON perfusion_maps(job_id);
CREATE INDEX idx_perfusion_type ON perfusion_maps(map_type);

CREATE TABLE temporal_metrics (
    id                BIGSERIAL PRIMARY KEY,
    job_id            BIGINT NOT NULL REFERENCES jobs(id) ON DELETE CASCADE,
    metric_name       TEXT   NOT NULL,
    mean_value        DOUBLE PRECISION,
    std_value         DOUBLE PRECISION,
    per_slice         JSONB,
    created_at        TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX idx_temporal_job ON temporal_metrics(job_id);
