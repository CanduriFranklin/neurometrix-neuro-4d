#pragma once
#include <Eigen/Dense>
#include <cstdint>
namespace neurometrix::core4d::domain {
struct TV4DParams { double lambda_spatial{0.10}; double lambda_temporal{0.05}; int iterations{10}; };
struct PM4DParams { double kappa{30.0}; double lambda{0.15}; int iterations{8}; };
class TemporalFilters4D {
public:
    static Eigen::VectorXf total_variation_4d(const Eigen::VectorXf& vol,
        std::uint32_t h, std::uint32_t w, std::uint32_t d, std::uint32_t t,
        const TV4DParams& p = {});
    static Eigen::VectorXf perona_malik_4d(const Eigen::VectorXf& vol,
        std::uint32_t h, std::uint32_t w, std::uint32_t d, std::uint32_t t,
        const PM4DParams& p = {});
};
}
