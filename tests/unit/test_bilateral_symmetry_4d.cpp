#include "domain/services/bilateral_symmetry_4d.hpp"
#include <gtest/gtest.h>
#include <Eigen/Dense>

using namespace neurometrix::core4d::domain;

TEST(BilateralSymmetry4D, PerfectSymmetryScoresZero) {
    const std::uint32_t h = 8, w = 8, d = 4;
    const std::uint32_t plane = h * w;
    Eigen::MatrixXf vol = Eigen::MatrixXf::Zero(h * w * d, 1);
    for (std::uint32_t z = 0; z < d; ++z)
        for (std::uint32_t y = 0; y < h; ++y)
            for (std::uint32_t x = 0; x < w / 2; ++x) {
                const float v = 30.0f + 0.5f * (x + y + z);
                vol(z * plane + y * w + x) = v;
                vol(z * plane + y * w + (w - 1 - x)) = v;
            }

    const auto r = BilateralSymmetry4D::analyze(vol, h, w, d);
    EXPECT_LT(r.asymmetry_score, 1e-3);
}

TEST(BilateralSymmetry4D, UnilateralLesionDetected) {
    const std::uint32_t h = 16, w = 16, d = 8;
    const std::uint32_t plane = h * w;
    Eigen::MatrixXf vol = Eigen::MatrixXf::Zero(h * w * d, 1);
    // Bright block on left hemisphere, z in [3,5), y in [6,10), x in [2,6)
    for (std::uint32_t z = 3; z < 5; ++z)
        for (std::uint32_t y = 6; y < 10; ++y)
            for (std::uint32_t x = 2; x < 6; ++x)
                vol(z * plane + y * w + x) = 80.0f;

    const auto r = BilateralSymmetry4D::analyze(vol, h, w, d);
    EXPECT_GT(r.asymmetry_score, 0.1);
    EXPECT_GT(r.peak_value, 60.0);
}