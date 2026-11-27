#include "contract.hpp"
#include <algorithm>
#include <cmath>
#include <ranges>

namespace modern {
std::optional<Summary> summarize(std::span<const double> samples) {
    auto valid = samples | std::views::filter([](double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 100.0;
    });
    Summary result{0, 0.0, 100.0, 0.0};
    for (const double value : valid) {
        ++result.count;
        result.mean += value;
        result.minimum = std::min(result.minimum, value);
        result.maximum = std::max(result.maximum, value);
    }
    if (result.count == 0) return std::nullopt;
    result.mean /= static_cast<double>(result.count);
    return result;
}
}
