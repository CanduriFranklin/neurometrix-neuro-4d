#pragma once
#include <Eigen/Dense>
#include <cstdint>
namespace neurometrix::core4d::domain {
class TemporalEntropy4D {
public:
    static Eigen::VectorXf compute(const Eigen::VectorXf& vol4d,
        std::uint32_t h, std::uint32_t w, std::uint32_t d, std::uint32_t t, int bins = 16);
};
}
