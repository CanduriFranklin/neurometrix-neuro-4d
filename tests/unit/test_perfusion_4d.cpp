#include <gtest/gtest.h>
#include "domain/services/perfusion_analysis_4d.hpp"
#include "domain/services/hemodynamic_maps.hpp"
using namespace neurometrix::core4d::domain;
namespace {
Eigen::VectorXf make_synth(std::uint32_t h, std::uint32_t w, std::uint32_t d, std::uint32_t t) {
    const std::uint32_t plane = h*w, vol = plane*d;
    Eigen::VectorXf v = Eigen::VectorXf::Zero(plane*d*t);
    for (std::uint32_t ti = 0; ti < t; ++ti) {
        const float ph = static_cast<float>(ti)/static_cast<float>(t-1);
        const float b = std::exp(-((ph-0.45f)*(ph-0.45f))/0.02f);
        for (std::uint32_t z=0; z<d; ++z)
        for (std::uint32_t y=0; y<h; ++y)
        for (std::uint32_t x=0; x<w; ++x) {
            const std::uint32_t sp = z*plane + y*w + x;
            v[static_cast<Eigen::Index>(ti)*vol + sp] = 40.0f + b*60.0f;
        }
    }
    return v;
}
}
TEST(Perfusion4D, AifDetectsPeak) {
    auto v = make_synth(8,8,2,12);
    auto aif = ArterialInput::detect(v, 8,8,2,12, 1.0, 40.0, 200.0);
    EXPECT_GT(aif.peak_hu, 50.0);
    EXPECT_GT(aif.time_to_peak_s, 2.0);
    EXPECT_LT(aif.time_to_peak_s, 10.0);
}
TEST(Perfusion4D, ComputesMaps) {
    auto v = make_synth(8,8,2,12);
    auto aif = ArterialInput::detect(v, 8,8,2,12, 1.0, 40.0, 200.0);
    PerfusionParams p{}; p.dt_seconds = 1.0;
    auto m = PerfusionAnalysis4D::compute(v, 8,8,2,12, aif, p);
    EXPECT_EQ(m.cbf.size(), 8*8*2);
    EXPECT_GE(m.cbv.minCoeff(), 0.0f);
}
TEST(Perfusion4D, MismatchClassifies) {
    auto v = make_synth(8,8,2,12);
    auto aif = ArterialInput::detect(v, 8,8,2,12, 1.0, 40.0, 200.0);
    PerfusionParams p{};
    auto m = PerfusionAnalysis4D::compute(v, 8,8,2,12, aif, p);
    auto mm = HemodynamicMaps::classify(m);
    EXPECT_LT(mm.core_voxels, 8ull*8*2);
}
