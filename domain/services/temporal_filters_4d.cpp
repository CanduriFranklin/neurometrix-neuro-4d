#include "domain/services/temporal_filters_4d.hpp"
#include <cmath>
namespace neurometrix::core4d::domain {
namespace {
inline Eigen::Index idx4(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t t,
                          std::uint32_t h, std::uint32_t w, std::uint32_t d) {
    return static_cast<Eigen::Index>(t)*(h*w*d)
         + static_cast<Eigen::Index>(z)*(h*w)
         + static_cast<Eigen::Index>(y)*w + x;
}
}
Eigen::VectorXf TemporalFilters4D::total_variation_4d(const Eigen::VectorXf& vol,
    std::uint32_t h, std::uint32_t w, std::uint32_t d, std::uint32_t t, const TV4DParams& p) {
    Eigen::VectorXf u = vol;
    for (int it = 0; it < p.iterations; ++it) {
        Eigen::VectorXf next = u;
        for (std::uint32_t ti = 0; ti < t; ++ti)
        for (std::uint32_t z = 0; z < d; ++z)
        for (std::uint32_t y = 0; y < h; ++y)
        for (std::uint32_t x = 0; x < w; ++x) {
            const auto c = idx4(x,y,z,ti,h,w,d);
            double acc = 0.0;
            if (x+1 < w) { const auto n=idx4(x+1,y,z,ti,h,w,d); const double g=u[n]-u[c]; acc += g/std::sqrt(1e-3+g*g); }
            if (y+1 < h) { const auto n=idx4(x,y+1,z,ti,h,w,d); const double g=u[n]-u[c]; acc += g/std::sqrt(1e-3+g*g); }
            if (z+1 < d) { const auto n=idx4(x,y,z+1,ti,h,w,d); const double g=u[n]-u[c]; acc += g/std::sqrt(1e-3+g*g); }
            double dt = 0.0;
            if (ti+1 < t) { const auto n=idx4(x,y,z,ti+1,h,w,d); const double g=u[n]-u[c]; dt = g/std::sqrt(1e-3+g*g); }
            next[c] = static_cast<float>(u[c] + p.lambda_spatial*acc + p.lambda_temporal*dt);
        }
        u = std::move(next);
    }
    return u;
}
Eigen::VectorXf TemporalFilters4D::perona_malik_4d(const Eigen::VectorXf& vol,
    std::uint32_t h, std::uint32_t w, std::uint32_t d, std::uint32_t t, const PM4DParams& p) {
    Eigen::VectorXf u = vol;
    const double k2 = p.kappa*p.kappa;
    auto gfun = [k2](double g) { return std::exp(-(g*g)/k2) * g; };
    for (int it = 0; it < p.iterations; ++it) {
        Eigen::VectorXf next = u;
        for (std::uint32_t ti = 0; ti < t; ++ti)
        for (std::uint32_t z = 0; z < d; ++z)
        for (std::uint32_t y = 0; y < h; ++y)
        for (std::uint32_t x = 0; x < w; ++x) {
            const auto c = idx4(x,y,z,ti,h,w,d);
            double flux = 0.0;
            if (x+1 < w) { const auto n=idx4(x+1,y,z,ti,h,w,d); flux += gfun(u[n]-u[c]); }
            if (y+1 < h) { const auto n=idx4(x,y+1,z,ti,h,w,d); flux += gfun(u[n]-u[c]); }
            if (z+1 < d) { const auto n=idx4(x,y,z+1,ti,h,w,d); flux += gfun(u[n]-u[c]); }
            if (ti+1 < t) { const auto n=idx4(x,y,z,ti+1,h,w,d); flux += 0.5*gfun(u[n]-u[c]); }
            next[c] = static_cast<float>(u[c] + p.lambda*flux);
        }
        u = std::move(next);
    }
    return u;
}
}
