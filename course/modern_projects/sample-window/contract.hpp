#pragma once
#include <cstddef>
#include <optional>
#include <span>

namespace modern {
struct Summary {
    std::size_t count;
    double mean;
    double minimum;
    double maximum;
};
std::optional<Summary> summarize(std::span<const double> samples);
}
