#include <gtest/gtest.h>
#include "domain/models/volume_4d_tensor.hpp"
using namespace neurometrix::core4d::domain;
TEST(Volume4DTensor, Dimensions) {
    Volume4DTensor v(4,5,6,3);
    EXPECT_EQ(v.height(), 4u); EXPECT_EQ(v.width(), 5u);
    EXPECT_EQ(v.depth(), 6u); EXPECT_EQ(v.timepoints(), 3u);
    v.at(1,2,3,0) = 42.0f;
    EXPECT_FLOAT_EQ(v.at(1,2,3,0), 42.0f);
}
TEST(Volume4DTensor, SlicePerTimepoint) {
    Volume4DTensor v(4,4,2,3);
    for (std::uint32_t t = 0; t < 3; ++t)
        for (std::uint32_t y = 0; y < 4; ++y)
            for (std::uint32_t x = 0; x < 4; ++x)
                v.at(x,y,0,t) = static_cast<float>(t*100 + y*10 + x);
    EXPECT_FLOAT_EQ(v.slice_at(0,2)(3,3), 233.0f);
}
TEST(Volume4DTensor, ZeroDimRejected) {
    EXPECT_THROW(Volume4DTensor(0,4,4,4), std::invalid_argument);
    EXPECT_THROW(Volume4DTensor(4,4,4,0), std::invalid_argument);
}
