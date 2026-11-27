#include "contract.hpp"

namespace modern {
std::expected<FrameHeader, FrameError>
decode_header(std::span<const std::uint8_t> bytes) {
    // TODO: check size, magic, version and payload length; decode big-endian fields.
    (void)bytes;
    return std::unexpected(FrameError::Size);
}
}
