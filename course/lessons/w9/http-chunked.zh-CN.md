# W9-A · 增量式 HTTP/1.1 分块传输解码

## 从这里开始

沿着消息边界解码一个分块正文。

1. 阅读一个完整示例，识别十六进制分块大小。
2. 解析大小行，然后恰好消耗指定数量的载荷字节。
3. 在读取下一个分块前，验证后面的 CRLF。
4. 将同一个正文分片送入，包括将大小行和载荷分别拆开。
5. 识别大小为零的最后一个分块，并解析尾部字段。
6. 测试无效大小、缺失 CRLF 和正文大小限制。

**理解模型：**最终长度无法预先获知时，分块编码为正文划定边界。TCP 可能拆开其中的每个部分；调用方保留并扩充累计缓冲区，无状态解析器在每次调用时检查该缓冲区。


## 目标

扩展 W8 增量 HTTP 解析器，使其在不丢失 TCP 流边界的前提下解码 HTTP/1.1 分块请求正文。解析器必须保持增量特性：分块大小行、分块载荷、CRLF 终止标记或尾部字段块不完整时，返回 NeedMoreData，而不是请求格式错误。

## 消息定界规则

W8 解析器已经支持严格的 Content-Length 定界。本任务增加一个刻意限定范围的分块处理规则集：

- 恰好接受一种 Transfer-Encoding 编码：chunked（ASCII 大小写不敏感，值周围允许可选空白）。
- 拒绝同时出现 Content-Length 和 Transfer-Encoding。绝不猜测哪个定界首部优先。
- 拒绝不支持的编码以及 gzip, chunked 这样的编码链。
- 解析每个十六进制分块大小、可选的可见 ASCII 扩展文本、恰好指定数量的载荷字节，以及随后的 CRLF。
- 大小为零的分块标志尾部字段区的开始。验证尾部字段语法，拒绝重复的尾部字段名称，并拒绝定界/路由字段（Content-Length、Transfer-Encoding、Host、Connection、Trailer、Upgrade）。尾部字段会被验证并消耗，但不会合并到请求首部中。
- 执行现有的 1 MiB 解码后正文上限，以及 1 KiB 分块大小行上限和 16 KiB 尾部字段区上限。
- 返回的 consumed 只计算到最后一个空尾部行为止。任何流水线请求都留在输入缓冲区中。

本课程规则集有意不解码传输编码链，也不向处理器公开尾部字段值。它不改变现有的 Content-Length 行为。

## 分块传输的字节格式

```text
Transfer-Encoding: chunked

4;name=value\r\n
Wiki\r\n
5\r\n
pedia\r\n
0\r\n
X-Trace: abc\r\n
\r\n
```

解码后的正文为 Wikipedia。分块元数据和尾部字段块不属于正文。

## 增量状态机

1. 读取一个以 CRLF 结尾的分块大小行。使用 std::from_chars(base=16) 解析十六进制大小。
2. 如果分块大小非零，验证累计解码后的正文不会超过 1 MiB。等待完整载荷及 CRLF，恰好追加载荷字节，然后继续。
3. 如果分块大小为零，解析尾部字段，直到最后一个空 CRLF 行。
4. 返回 Complete，以及从原始缓冲区中消耗的精确字节数。
5. 如果缺少任何必需字节，返回 NeedMoreData，且 consumed == 0。
6. 如果定界有歧义、格式错误或超出限制，返回 Error。

绝不要使用 strlen、面向行的文本 I/O 或 C 字符串终止符来确定分块载荷长度。分块数据可以是任意字节。

## 学习任务 — w9.http-chunked

**工作区文件**

- include/http_message.hpp
- include/http_request_parser.hpp
- src/http_request_parser.cpp

在解析器中实现分块定界分支。保留 W8 请求模型，以及 ParseStatus / ParseResult 契约。

**固定验收：**course/fixtures/w9/http-chunked 中由平台维护的 35 项 GoogleTests。它们覆盖多分块和二进制载荷、扩展、尾部字段、增量边界、流水线、定界歧义、格式错误的输入以及资源限制。

```bash
./build/cppapplab start w9.http-chunked "$HOME/cppapplab-work/w9-http-chunked"
./build/cppapplab lesson "$HOME/cppapplab-work/w9-http-chunked"
./build/cppapplab hint "$HOME/cppapplab-work/w9-http-chunked" 1
./build/cppapplab check "$HOME/cppapplab-work/w9-http-chunked"
```

## 解释你的设计

1. 为什么 Content-Length 与 Transfer-Encoding 不能共存？
2. 为什么分块解析器既需要解码后正文大小上限，也需要元数据行上限？
3. 为什么分块大小以字节而不是字符计量？
4. 为什么解析器必须等待每个非零分块之后的 CRLF？
5. 为什么尾部字段中禁止出现定界和路由字段？
6. 为什么尾部字段不应静默覆盖请求首部？
7. 为什么 consumed 必须停在下一个流水线请求之前？
8. 为什么不完整的尾部字段块返回 NeedMoreData？

## 故障排查

使用固定解析器检查验证本库任务；工作区不提供独立的解析器应用程序。

- **分片分块解析失败，或载荷字节重复：**检查每次 `parse` 调用传入的完整缓冲区。等待更多输入时保留先前字节，重新检查大小行/数据/终止标记拆分的用例，确保返回 `NeedMoreData` 且已消耗字节数为零。
- **解码数据包含元数据，或丢失二进制字节：**检查十六进制大小，以及 CRLF 前的载荷边界。重新检查二进制载荷和内嵌 CRLF，用精确载荷字节数计量，不要按行读取数据。
- **后续请求消失，或尾部字段覆盖首部：**检查游标经过零分块直至最后一个空尾部行的过程。重新检查已消耗字节数和尾部字段用例，验证重复/禁用字段，且不将尾部字段合并到请求首部中。
- **有歧义或超限的输入被接受：**检查 Content-Length 与 Transfer-Encoding 同时存在的情况、HTTP 版本以及编码值。重新检查这些拒绝用例，以及解码后正文、分块行和尾部字段的限制；W9 只接受有明确边界限制的 HTTP/1.1 chunked 规则集。

## 验收要求

- [ ] 单个及多个分块都解码为精确的字节。
- [ ] 分块扩展只作为有长度限制的可见 ASCII 元数据被接受。
- [ ] 正确处理空正文和有效尾部字段。
- [ ] 大小行/数据/终止标记/尾部字段的部分输入均以增量方式处理。
- [ ] 拒绝 Content-Length + Transfer-Encoding。
- [ ] 拒绝不支持的传输编码链。
- [ ] 执行分块和尾部字段的元数据限制。
- [ ] 流水线字节保持未消耗状态。
- [ ] 现有 Content-Length 定界保持不变。
- [ ] 全部 35 项固定检查通过。
