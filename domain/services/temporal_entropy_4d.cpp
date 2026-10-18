#include "domain/services/temporal_entropy_4d.hpp"
#include <algorithm>
#include <cmath>
namespace neurometrix::core4d::domain {
Eigen::VectorXf TemporalEntropy4D::compute(const Eigen::VectorXf& vol4d,
    std::uint32_t h, std::uint32_t w, std::uint32_t d, std::uint32_t t, int bins) {
    const std::uint32_t plane = h*w;
    const std::uint32_t vol = plane*d;
    Eigen::VectorXf out = Eigen::VectorXf::Zero(plane*d);
    std::vector<float> curve(t);
    for (std::uint32_t z = 0; z < d; ++z)
    for (std::uint32_t y = 0; y < h; ++y)
    for (std::uint32_t x = 0; x < w; ++x) {
        const std::uint32_t sp = z*plane + y*w + x;
        for (std::uint32_t ti = 0; ti < t; ++ti)
            curve[ti] = vol4d[static_cast<Eigen::Index>(ti)*vol + sp];
        auto mm = std::minmax_element(curve.begin(), curve.end());
        if (*mm.second - *mm.first < 1e-3f) { out[sp] = 0.0f; continue; }
        const float range = *mm.second - *mm.first;
        std::vector<int> hist(static_cast<std::size_t>(bins), 0);
        for (float v : curve) {
            int b = static_cast<int>((v - *mm.first)/range*(bins-1) + 0.5f);
            b = std::clamp(b, 0, bins-1);
            hist[static_cast<std::size_t>(b)]++;
        }
        double H = 0.0;
        for (int c : hist) if (c) { const double p = static_cast<double>(c)/t; H -= p*std::log2(p); }
        out[sp] = static_cast<float>(H);
    }
    return out;
}
}
