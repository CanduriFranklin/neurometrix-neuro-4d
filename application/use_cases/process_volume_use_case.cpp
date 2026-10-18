#include "process_volume_use_case.hpp"

#include "domain/services/physics_filters_4d.hpp"
#include "domain/services/morphological_ops_4d.hpp"
#include "domain/services/hessian_analysis_4d.hpp"
#include "domain/services/bilateral_symmetry_4d.hpp"
#include "domain/services/entropy_map_4d.hpp"
#include "domain/services/volumetric_metrics.hpp"
#include "domain/services/perfusion_analysis_4d.hpp"
#include "domain/services/hemodynamic_maps.hpp"
#include "domain/services/temporal_entropy_4d.hpp"
#include "infrastructure/logging/logger.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace neurometrix::core4d::application {

namespace {

[[nodiscard]] std::int64_t now_ms() noexcept {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// Extract findings from the 4D score maps.
// Thresholds correspond to clinical ranges (HU).
void extract_findings(const domain::Volume4D& vol,
                       const Eigen::MatrixXf& tophat,
                       const Eigen::MatrixXf& vesselness,
                       const domain::AsymmetryResult4D& sym,
                       std::vector<domain::Finding4D>& out)
{
    const std::uint32_t h = vol.height();
    const std::uint32_t w = vol.width();
    const std::uint32_t d = vol.depth();
    const std::uint32_t plane = h * w;

    // --- Micro-hemorrhage: top-hat response peak ---
    double th_peak = 0.0;
    int th_idx = -1;
    for (int i = 0; i < static_cast<int>(tophat.rows()); ++i) {
        const double hu_i = static_cast<double>(vol.hu()(i));
        if (hu_i < -200.0 || hu_i > 2000.0) continue;
        if (tophat(i) > th_peak) { th_peak = tophat(i); th_idx = i; }
    }
    if (th_idx >= 0 && th_peak > 25.0) {
        const double hu_here = static_cast<double>(vol.hu()(th_idx));
        if (hu_here >= 45.0 && hu_here <= 80.0) {
            domain::Finding4D f{};
            f.type = domain::FindingType::MicroHemorrhage;
            f.confidence = std::min(1.0, th_peak / 60.0);
            const std::uint32_t z = th_idx / plane;
            const std::uint32_t y = (th_idx / w) % h;
            const std::uint32_t x = th_idx % w;
            f.centroid_x = static_cast<int>(x);
            f.centroid_y = static_cast<int>(y);
            f.centroid_z = static_cast<int>(z);
            f.mean_hu = hu_here;
            f.note = "high 4D top-hat response in acute blood HU range";
            out.push_back(f);
        }
    }

    // --- Asymmetry peak ---
    if (sym.peak_value > 15.0) {
        domain::Finding4D f{};
        f.type = domain::FindingType::AcuteHemorrhage;
        f.confidence = std::min(1.0, sym.peak_value / 80.0);
        f.centroid_x = sym.peak_x;
        f.centroid_y = sym.peak_y;
        f.centroid_z = sym.peak_z;
        const std::uint32_t idx =
            static_cast<std::uint32_t>(sym.peak_z) * plane +
            static_cast<std::uint32_t>(sym.peak_y) * w +
            static_cast<std::uint32_t>(sym.peak_x);
        if (idx < vol.hu().rows()) {
            f.mean_hu = static_cast<double>(vol.hu()(idx));
        }
        f.note = "hemispheric asymmetry peak in 4D";
        out.push_back(f);
    }

    // --- Vascular / calcification from vesselness ---
    double v_peak = 0.0;
    int v_idx = -1;
    for (int i = 0; i < static_cast<int>(vesselness.rows()); ++i) {
        const double hu_i = static_cast<double>(vol.hu()(i));
        if (hu_i < -200.0 || hu_i > 400.0) continue;
        if (vesselness(i) > v_peak) { v_peak = vesselness(i); v_idx = i; }
    }
    if (v_idx >= 0 && v_peak > 0.15) {
        domain::Finding4D f{};
        f.type = domain::FindingType::Calcification;
        f.confidence = std::min(1.0, v_peak / 0.5);
        const std::uint32_t z = v_idx / plane;
        const std::uint32_t y = (v_idx / w) % h;
        const std::uint32_t x = v_idx % w;
        f.centroid_x = static_cast<int>(x);
        f.centroid_y = static_cast<int>(y);
        f.centroid_z = static_cast<int>(z);
        f.mean_hu = static_cast<double>(vol.hu()(v_idx));
        f.note = "elongated high-HU 4D structure";
        out.push_back(f);
    }

    (void)d;
}

} // anonymous namespace

ProcessVolumeUseCase::ProcessVolumeUseCase(
    const IDicomVolumeLoader& dicom_loader,
    IJobRepository&           repository,
    IMessagePublisher&        publisher
) noexcept
    : dicom_loader_(dicom_loader),
      repository_(repository),
      publisher_(publisher)
{}

std::expected<void, UseCaseError>
ProcessVolumeUseCase::validate(const contracts::VolumeRequest& request) const noexcept {
    if (request.message_id == 0) return std::unexpected(UseCaseError::InputValidationFailed);
    if (request.dicom_path.empty()) return std::unexpected(UseCaseError::InputValidationFailed);
    if (request.study_uid.empty()) return std::unexpected(UseCaseError::InputValidationFailed);
    return {};
}

std::expected<contracts::VolumeResult, UseCaseError>
ProcessVolumeUseCase::execute(const contracts::VolumeRequest& request) noexcept {
    const std::int64_t start_ms = now_ms();

    if (auto v = validate(request); !v) return std::unexpected(v.error());

    if (auto saved = repository_.save_job(request); !saved) {
        return std::unexpected(UseCaseError::RepositoryUnavailable);
    }

    if (!dicom_loader_.can_read(request.dicom_path)) {
        (void)repository_.mark_failed(request.message_id,
                                      "dicom_file_not_found", "path not readable");
        return std::unexpected(UseCaseError::DicomReadFailed);
    }

    auto volume_result = dicom_loader_.load(request.dicom_path);
    if (!volume_result) {
        const char* code = "dicom_read_failed";
        const char* detail = "loader rejected volume";
        switch (volume_result.error()) {
            case DicomError::UnsupportedModality:
                code = "unsupported_modality"; detail = "not CT"; break;
            case DicomError::UnsupportedBodyPart:
                code = "unsupported_body_part"; detail = "not head/brain"; break;
            case DicomError::InsufficientSlices:
                code = "insufficient_slices"; detail = "not enough timepoints for 4D"; break;
            case DicomError::FileNotFound:
                code = "dicom_file_not_found"; detail = "file not found"; break;
            case DicomError::CorruptedPixelData:
                code = "dicom_corrupted_pixels"; detail = "corrupt pixel data"; break;
            default: break;
        }
        (void)repository_.mark_failed(request.message_id, code, detail);
        return std::unexpected(UseCaseError::DicomReadFailed);
    }

    const auto& vol = *volume_result;
    (void)repository_.mark_processing(request.message_id, start_ms, vol.depth() * vol.timepoints());

    contracts::VolumeResult result{};
    result.message_id = request.message_id;
    result.study_uid  = request.study_uid;
    result.status     = "processed";
    result.height     = vol.height();
    result.width      = vol.width();
    result.depth      = vol.depth();
    result.timepoints = vol.timepoints();
    result.is_4d      = vol.is_4d();

    try {
        using namespace domain;

        // 1. Base volumetric stats
        const auto base = VolumetricMetrics::compute(
            vol.hu(), vol.height(), vol.width(), vol.depth(), 1.0f);
        result.mean_hu          = base.mean_hu;
        result.stddev_hu        = base.stddev_hu;
        result.min_hu           = base.min_hu;
        result.max_hu           = base.max_hu;
        result.total_volume_mm3 = base.total_volume_mm3;

        // 2. TV denoising (10 iterations to bound latency on large volumes)
        const auto denoised = PhysicsFilters4D::total_variation_denoise(
            vol.hu(), vol.height(), vol.width(), vol.depth(), 0.1f, 5);

        // 3. White top-hat for bright focal spots
        const auto tophat = MorphologicalOps4D::white_tophat(
            denoised, vol.height(), vol.width(), vol.depth(), 2);

        // 4. Frangi vesselness
        const auto vesselness = HessianAnalysis4D::frangi_vesselness(
            denoised, vol.height(), vol.width(), vol.depth(),
            1.0f, 3.0f, 2);
        result.peak_vesselness = static_cast<double>(vesselness.maxCoeff());

        // 5. Hemispheric symmetry
        const auto sym = BilateralSymmetry4D::analyze(
            denoised, vol.height(), vol.width(), vol.depth());
        result.asymmetry_score = sym.asymmetry_score;

        // 6. Local entropy
        const auto entropy = EntropyMap4D::compute(
            denoised, vol.height(), vol.width(), vol.depth(), 5);
        result.mean_entropy = static_cast<double>(entropy.mean());

        // 7. Findings (3D features on timepoint 0)
        extract_findings(vol, tophat, vesselness, sym, result.findings);
        result.gpu_used = false;

        // 8. 4D perfusion (only when the volume has T > 1)
        if (vol.is_4d()) {
            const std::uint32_t h4 = vol.height();
            const std::uint32_t w4 = vol.width();
            const std::uint32_t d4 = vol.depth();
            const std::uint32_t t4 = vol.timepoints();
            const auto& flat = vol.tensor().flat();

            // Arterial Input Function detection
            const auto aif = ArterialInput::detect(flat, h4, w4, d4, t4,
                                                    vol.frame_interval().value(),
                                                    40.0, 500.0);

            if (aif.peak_hu > 0.0) {
                PerfusionParams pp{};
                pp.dt_seconds = vol.frame_interval().value();   // real dt from DICOM

                const auto maps = PerfusionAnalysis4D::compute(
                    flat, h4, w4, d4, t4, aif, pp);

                // ---- Brain mask: exclude air/skull based on baseline HU ----
                const Eigen::Index n4 = static_cast<Eigen::Index>(h4) * w4 * d4;
                const std::uint32_t t_base = std::max<std::uint32_t>(1, t4 / 5);
                Eigen::VectorXf baseline(n4);
                for (Eigen::Index i = 0; i < n4; ++i) {
                    double acc = 0.0;
                    for (std::uint32_t ti = 0; ti < t_base; ++ti)
                        acc += flat[static_cast<Eigen::Index>(ti) * n4 + i];
                    baseline[i] = static_cast<float>(acc / t_base);
                }

                auto masked_stats = [&](const Eigen::VectorXf& v,
                                         double& out_mean, double& out_median) {
                    std::vector<float> vals;
                    vals.reserve(static_cast<std::size_t>(n4));
                    for (Eigen::Index i = 0; i < n4; ++i) {
                        const float b = baseline[i];
                        if (b < -200.0f || b > 2000.0f) continue;
                        vals.push_back(v[i]);
                    }
                    if (vals.empty()) { out_mean = 0.0; out_median = 0.0; return; }
                    double s = 0.0;
                    for (float x : vals) s += x;
                    out_mean = s / static_cast<double>(vals.size());
                    const std::size_t m = vals.size() / 2;
                    std::nth_element(vals.begin(), vals.begin() + m, vals.end());
                    out_median = vals[m];
                };

                double mean_cbf = 0.0, med_cbf = 0.0;
                double mean_cbv = 0.0, med_cbv = 0.0;
                double mean_mtt = 0.0, med_mtt = 0.0;
                double mean_ttp = 0.0, med_ttp = 0.0;
                double mean_tmax = 0.0, med_tmax = 0.0;

                masked_stats(maps.cbf,  mean_cbf,  med_cbf);
                masked_stats(maps.cbv,  mean_cbv,  med_cbv);
                masked_stats(maps.mtt,  mean_mtt,  med_mtt);
                masked_stats(maps.ttp,  mean_ttp,  med_ttp);
                masked_stats(maps.tmax, mean_tmax, med_tmax);

                result.perfusion.mean_cbf  = mean_cbf;
                result.perfusion.mean_cbv  = mean_cbv;
                result.perfusion.mean_mtt  = mean_mtt;
                result.perfusion.mean_ttp  = mean_ttp;
                result.perfusion.mean_tmax = mean_tmax;

                infrastructure::logging::get()->info(
                    "[perfusion] masked: CBF mean={:.2f} med={:.2f}  "
                    "CBV mean={:.2f} med={:.2f}  MTT mean={:.2f} med={:.2f}  "
                    "TMax mean={:.2f}",
                    mean_cbf, med_cbf, mean_cbv, med_cbv,
                    mean_mtt, med_mtt, mean_tmax);

                const auto mm = HemodynamicMaps::classify(maps);
                result.mismatch.core_voxels     = mm.core_voxels;
                result.mismatch.penumbra_voxels = mm.penumbra_voxels;
                result.mismatch.mismatch_ratio  = mm.mismatch_ratio;

                infrastructure::logging::get()->info(
                    "[perfusion] AIF peak={:.1f} HU @ t={:.1f}s  "
                    "CBF={:.2f} CBV={:.2f} MTT={:.2f}  core={} penumbra={}",
                    aif.peak_hu, aif.time_to_peak_s,
                    result.perfusion.mean_cbf,
                    result.perfusion.mean_cbv,
                    result.perfusion.mean_mtt,
                    result.mismatch.core_voxels,
                    result.mismatch.penumbra_voxels);
            } else {
                infrastructure::logging::get()->warn("[perfusion] no AIF detected");
            }
        }
    } catch (...) {
        (void)repository_.mark_failed(request.message_id,
                                      "compute_failed", "exception");
        return std::unexpected(UseCaseError::ComputeFailed);
    }

    const std::int64_t finish_ms = now_ms();
    result.processing_ms = static_cast<std::uint64_t>(finish_ms - start_ms);

    if (auto published = publisher_.publish_result(result); !published) {
        (void)repository_.mark_failed(request.message_id,
                                      "publisher_unavailable", "kafka");
        return std::unexpected(UseCaseError::PublisherUnavailable);
    }

    (void)repository_.save_result(result, start_ms, finish_ms);
    return result;
}

} // namespace neurometrix::core4d::application