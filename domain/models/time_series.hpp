#pragma once
#include <cstdint>
#include <vector>
#include "domain/value_objects/time_index.hpp"
namespace neurometrix::core4d::domain {
struct TimePoint { TimeIdx idx; double offset_seconds{0.0}; double trigger_time_ms{0.0}; };
class TimeSeries {
public:
    void add(TimePoint tp) { points_.push_back(tp); }
    std::size_t size() const noexcept { return points_.size(); }
    bool empty() const noexcept { return points_.empty(); }
    const TimePoint& at(std::size_t i) const { return points_.at(i); }
    double temporal_resolution_seconds() const noexcept {
        if (points_.size() < 2) return 0.0;
        double s = 0.0;
        for (std::size_t i = 1; i < points_.size(); ++i)
            s += points_[i].offset_seconds - points_[i-1].offset_seconds;
        return s / (points_.size()-1);
    }
private:
    std::vector<TimePoint> points_;
};
}
