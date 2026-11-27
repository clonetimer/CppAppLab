#include "contract.hpp"
#include <bit>
#include <cstring>

namespace modern {
std::expected<FrameHeader, FrameError>
decode_header(std::span<const std::uint8_t> bytes) {
    if (bytes.size() != 8) return std::unexpected(FrameError::Size);
    if (bytes[0] != 0xa5) return std::unexpected(FrameError::Magic);
    if (bytes[1] != 1) return std::unexpected(FrameError::Version);
    const auto length = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(bytes[2]) << 8) | bytes[3]);
    if (length > 1024) return std::unexpected(FrameError::Length);
    std::uint32_t sequence{};
    if constexpr (std::endian::native == std::endian::little) {
        std::memcpy(&sequence, bytes.data() + 4, sizeof(sequence));
        sequence = std::byteswap(sequence);
    } else {
        for (std::size_t i = 4; i < 8; ++i) {
            sequence = (sequence << 8) | bytes[i];
        }
    }
    return FrameHeader{length, sequence};
}
}
