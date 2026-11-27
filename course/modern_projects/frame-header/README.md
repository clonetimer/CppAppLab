# C++23 · 定宽报文头解析

实现 decode_header：恰好 8 字节；byte 0=0xA5，byte 1=1，byte 2–3 为大端 payload_length（0–1024），byte 4–7 为大端 uint32 sequence。错误优先级：长度、magic、version、payload_length。使用 expected 返回具体错误，禁止未检查读取。字节移位或经明确主机字节序判断的 byteswap 均可。只解析独立头部，不解析载荷，不代表真实卫星协议。

Implement decode_header for exactly eight bytes: magic 0xA5, version 1, big-endian payload length 0–1024 (bytes 2–3), big-endian uint32 sequence (4–7). Error precedence: size, magic, version, length. Return std::expected with specific errors; never read unchecked offsets. Shifts or endian-aware byteswap are allowed. Synthetic header only; no real satellite protocol or payload parser.

Fixed editable file: implementation.cpp (copied from starter.cpp, never reference.cpp). Read-only contract.hpp; 8 independent platform-owned GoogleTest cases. Reference is maintained for CI, not copied into the learner workspace. The workbench stages only the source snapshot and platform fixture into a disposable build directory. C++23, GCC 13, CMake, GoogleTest. No production or flight qualification.
