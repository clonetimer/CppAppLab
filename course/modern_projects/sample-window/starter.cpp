#include "contract.hpp"

namespace modern {
std::optional<Summary> summarize(std::span<const double> samples) {
    // TODO: retain finite values in [0,100], summarize without modifying samples.
    (void)samples;
    return std::nullopt;
}
}
