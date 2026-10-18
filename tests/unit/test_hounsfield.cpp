#include "domain/value_objects/hounsfield_window.hpp"
#include <gtest/gtest.h>

using namespace neurometrix::core4d::domain;

TEST(Hounsfield, AffineConversion) {
    EXPECT_DOUBLE_EQ(HounsfieldWindow::to_hu(0, 1.0, -1024.0), -1024.0);
    EXPECT_DOUBLE_EQ(HounsfieldWindow::to_hu(1024, 1.0, -1024.0), 0.0);
    EXPECT_DOUBLE_EQ(HounsfieldWindow::to_hu(2024, 1.0, -1024.0), 1000.0);
}

TEST(Hounsfield, WindowMappingBounded) {
    const auto w = window_of(WindowPreset::Brain);
    EXPECT_DOUBLE_EQ(HounsfieldWindow::apply_window(-100.0, w), 0.0);
    EXPECT_DOUBLE_EQ(HounsfieldWindow::apply_window(80.0, w), 1.0);
}

TEST(Hounsfield, ClassificationOrdering) {
    EXPECT_EQ(HounsfieldWindow::classify_hu(-1100.0), "air");
    EXPECT_EQ(HounsfieldWindow::classify_hu(65.0),    "acute_blood");
    EXPECT_EQ(HounsfieldWindow::classify_hu(1500.0),  "bone");
}

TEST(Hounsfield, ClinicalRangesDisjoint) {
    for (double hu = -500; hu <= 500; hu += 1.0) {
        EXPECT_FALSE(HounsfieldWindow::is_acute_blood(hu) &&
                     HounsfieldWindow::is_ischemic_risk(hu));
    }
}