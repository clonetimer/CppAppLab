#pragma once
#include <cstdint>
#include <expected>
#include <span>

namespace modern {
enum class FrameError { Size, Magic, Version, Length };
struct FrameHeader {
    std::uint16_t payload_length;
    std::uint32_t sequence;
};
std::expected<FrameHeader, FrameError>
decode_header(std::span<const std::uint8_t> bytes);
}
