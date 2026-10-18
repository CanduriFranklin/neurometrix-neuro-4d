#include "domain/value_objects/volume_4d.hpp"
#include <gtest/gtest.h>
#include <Eigen/Dense>

using namespace neurometrix::core4d::domain;

TEST(Volume4D, DimensionsAndAccess) {
    const std::uint32_t h = 4, w = 4, d = 8;
    Eigen::MatrixXf vol(h * w * d, 1);
    for (int i = 0; i < vol.rows(); ++i) vol(i) = static_cast<float>(i);

    Volume4D v(std::move(vol), h, w, d, "CT", "HEAD");
    EXPECT_EQ(v.height(), h);
    EXPECT_EQ(v.width(), w);
    EXPECT_EQ(v.depth(), d);
    EXPECT_EQ(v.modality(), "CT");
    EXPECT_EQ(v.body_part(), "HEAD");
    EXPECT_FALSE(v.empty());
}

TEST(Volume4D, SliceExtraction) {
    const std::uint32_t h = 2, w = 2, d = 4;
    Eigen::MatrixXf vol(h * w * d, 1);
    // z=0 plane: 0,1,2,3
    for (int i = 0; i < 4; ++i) vol(i) = static_cast<float>(i);
    // z=2 plane: 100,101,102,103
    for (int i = 8; i < 12; ++i) vol(i) = static_cast<float>(100 + (i - 8));

    Volume4D v(std::move(vol), h, w, d, "CT", "HEAD");
    const auto s0 = v.slice(0);
    EXPECT_FLOAT_EQ(s0(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(s0(1, 1), 3.0f);
    const auto s2 = v.slice(2);
    EXPECT_FLOAT_EQ(s2(0, 0), 100.0f);
    EXPECT_FLOAT_EQ(s2(1, 1), 103.0f);
}