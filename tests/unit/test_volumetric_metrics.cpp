#include "domain/services/volumetric_metrics.hpp"
#include "domain/services/entropy_map_4d.hpp"
#include <gtest/gtest.h>
#include <Eigen/Dense>

using namespace neurometrix::core4d::domain;

TEST(VolumetricMetrics, ConstantVolumeStats) {
    const std::uint32_t h = 8, w = 8, d = 4;
    Eigen::MatrixXf vol = Eigen::MatrixXf::Constant(h * w * d, 1, 42.0f);
    const auto m = VolumetricMetrics::compute(vol, h, w, d, 1.0f);
    EXPECT_DOUBLE_EQ(m.mean_hu, 42.0);
    EXPECT_DOUBLE_EQ(m.stddev_hu, 0.0);
    EXPECT_DOUBLE_EQ(m.min_hu, 42.0);
    EXPECT_DOUBLE_EQ(m.max_hu, 42.0);
    EXPECT_DOUBLE_EQ(m.total_volume_mm3, 8.0 * 8.0 * 4.0 * 1.0);
}

TEST(VolumetricMetrics, VolumeOfHuRange) {
    const std::uint32_t h = 4, w = 4, d = 4;
    Eigen::MatrixXf vol = Eigen::MatrixXf::Zero(h * w * d, 1);
    // Plant 5 voxels in [60,70] HU
    vol(0) = 65; vol(1) = 65; vol(2) = 65; vol(3) = 65; vol(4) = 65;
    const double v = VolumetricMetrics::volume_of_hu_range(vol, 60.0, 70.0, 1.0f);
    EXPECT_DOUBLE_EQ(v, 5.0);
}

TEST(EntropyMap4D, ConstantIsZero) {
    const std::uint32_t h = 8, w = 8, d = 4;
    Eigen::MatrixXf vol = Eigen::MatrixXf::Constant(h * w * d, 1, 5.0f);
    const auto e = EntropyMap4D::compute(vol, h, w, d, 5);
    EXPECT_LT(std::abs(e(0)), 1e-3f);
}

TEST(EntropyMap4D, BoundedByLog2Bins) {
    const std::uint32_t h = 8, w = 8, d = 4;
    Eigen::MatrixXf vol = Eigen::MatrixXf::Random(h * w * d, 1);
    const auto e = EntropyMap4D::compute(vol, h, w, d, 5);
    // 16 bins -> max entropy = log2(16) = 4
    EXPECT_LE(e.maxCoeff(), 4.0f + 1e-3f);
    EXPECT_GE(e.minCoeff(), -1e-6f);
}