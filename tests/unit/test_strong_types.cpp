#include <gtest/gtest.h>
#include "domain/value_objects/strong_types.hpp"

using namespace neurometrix::core4d::domain;

TEST(StrongHU, ArithmeticAndLiterals) {
    constexpr HU a = 65.0_hu;
    constexpr HU b = 40.0_hu;
    constexpr HU c = a + b;
    EXPECT_DOUBLE_EQ(c.value(), 105.0);

    constexpr HU d = a - b;
    EXPECT_DOUBLE_EQ(d.value(), 25.0);

    constexpr HU e = a * 2.0;
    EXPECT_DOUBLE_EQ(e.value(), 130.0);
}

TEST(StrongHU, ClinicalPredicates) {
    EXPECT_TRUE(HU{-1000.0}.is_air());
    EXPECT_TRUE(HU{40.0}.is_white_matter());
    EXPECT_TRUE(HU{50.0}.is_gray_matter());
    EXPECT_TRUE(HU{65.0}.is_acute_blood());
    EXPECT_TRUE(HU{200.0}.is_calcification());
    EXPECT_TRUE(HU{800.0}.is_bone());
    EXPECT_TRUE(HU{30.0}.is_tissue());
    EXPECT_FALSE(HU{-500.0}.is_tissue());
}

TEST(StrongTime, DurationToMillisAndMinutes) {
    DurationSeconds d{1.5};
    EXPECT_DOUBLE_EQ(d.to_millis(), 1500.0);
    EXPECT_DOUBLE_EQ(d.to_minutes(), 0.025);

    auto d2 = DurationSeconds::from_millis(250.0);
    EXPECT_DOUBLE_EQ(d2.value(), 0.25);
}

TEST(StrongTime, TimeIndexRequiresDt) {
    TimeIndex idx{9};
    DurationSeconds dt_half{0.5};
    DurationSeconds t = idx.to_seconds(dt_half);
    EXPECT_DOUBLE_EQ(t.value(), 4.5);
}

TEST(StrongTime, SeriesLengthIs4D) {
    EXPECT_FALSE(TimeSeriesLength{1}.is_4d());
    EXPECT_TRUE(TimeSeriesLength{20}.is_4d());
}

TEST(StrongSpatial, VoxelCoordEquality) {
    VoxelCoord a{5, 10, 2};
    VoxelCoord b{5, 10, 2};
    VoxelCoord c{5, 11, 2};
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_EQ(a.to_string(), "(5,10,2)");
}

TEST(StrongSpatial, VoxelVolumeFromSpacing) {
    auto v = VoxelVolumeMM3::from_spacing(Millimetres{1.0},
                                          Millimetres{1.0},
                                          Millimetres{2.5});
    EXPECT_DOUBLE_EQ(v.value(), 2.5);
    EXPECT_DOUBLE_EQ(v.to_ml(), 0.0025);
}

TEST(StrongPhysiology, CentralVolumeTheorem) {
    CBV_mL_per_100g cbv{4.0};
    CBF_mL_per_100g_per_min cbf{50.0};
    MTT_seconds mtt = central_volume_theorem(cbv, cbf);
    // 4.0 / 50.0 * 60 = 4.8 seconds
    EXPECT_NEAR(mtt.value(), 4.8, 1e-9);
    EXPECT_TRUE(mtt.is_normal());
}

TEST(StrongPhysiology, ClinicalRanges) {
    EXPECT_TRUE(CBF_mL_per_100g_per_min{50.0}.is_normal());
    EXPECT_TRUE(CBF_mL_per_100g_per_min{10.0}.is_low());
    EXPECT_TRUE(CBV_mL_per_100g{4.0}.is_normal());
    EXPECT_TRUE(TMax_seconds{7.0}.is_penumbra_threshold());
    EXPECT_TRUE(TMax_seconds{11.0}.is_severe());
}
