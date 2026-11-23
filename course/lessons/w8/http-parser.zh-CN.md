# W8-D2 · 增量 HTTP 请求解析器

## 从这里开始

解析器检查调用方当前已累积的字节。你的首要目标是识别尚未完整到达的请求，而不将其判定为格式错误。

1. 在一个缓冲区中传入完整请求，检查解析得到的方法、目标和首部。
2. 将同一请求逐字节追加到调用方拥有的缓冲区中，每次追加后调用 `parse(buffer)`。
3. 每次调用后，区分 `NeedMoreData`、完整请求和格式错误的请求。
4. 加入 Content-Length 消息体处理，并验证已消耗字节数是否精确。
5. 测试重复或相互冲突的消息定界首部，以及配置的大小限制。
6. 确认属于下一条请求的字节未被消耗。

**核心理解：**解析器在调用方累积的缓冲区上工作，是一个无状态的消息边界检测器。返回 `NeedMoreData` 时保留缓冲区；返回 `Complete` 后，准确移除 `consumed` 个字节，并保留后续请求的所有字节。


## 目标

将任意 TCP 字节缓冲区转换为完整的 `HttpRequest`，不能假设一次 `recv()` 等于一条 HTTP 请求。解析器必须区分输入不完整与输入格式错误，并准确报告属于第一条完整请求的字节数。

## 课程材料规定的范围

这个教学解析器支持：
- HTTP/1.0 和 HTTP/1.1 请求行
- 不区分大小写的首部，并以小写名称存储
- 通过 Content-Length 定界的请求消息体
- HTTP/1.1 的 Host 必需项检查
- 严格的十进制 Content-Length 解析
- 明确的解析状态：`NeedMoreData`、`Complete`、`Error`
- 在一个 TCP 缓冲区包含多条请求时报告已消耗字节数

这个教学解析器明确拒绝或不实现：
- `Transfer-Encoding`（包括分块传输）
- 重复首部
- HTTP/2
- multipart 解析
- 压缩消息体

拒绝重复首部是教学场景中采用的保守策略，并非适用于所有 HTTP 首部的通用规则。

## 解析器契约

```cpp
enum class ParseStatus {
    NeedMoreData,
    Complete,
    Error
};

struct ParseResult {
    ParseStatus status;
    HttpRequest request;
    std::size_t consumed;
    std::string error;
};

class HttpRequestParser {
public:
    ParseResult parse(std::string_view buffer) const;
};
```

- `NeedMoreData`：尚未出现完整的首部结束标记，或声明长度的消息体尚未全部到达。
- `Complete`：已有一条完整请求；`consumed` 恰好等于第一条请求的字节长度。
- `Error`：请求格式错误或不受支持、首部重复、Content-Length 无效，或超过资源限制。

## 必需的解析顺序

1. 查找 `\r\n\r\n`。如果未找到，则返回 NeedMoreData，除非首部缓冲区已超过 16 KiB。
2. 检查首部大小不得超过 16 KiB，且计入结束标记。
3. 从请求行中解析且仅解析三个字段：方法、目标和版本。仅支持 HTTP/1.0 和 HTTP/1.1。
4. 解析每一行 `Name: Value` 首部，验证字段名称是合法的 HTTP token 且冒号之前没有空白，去除值两端可选的空白，将名称转为小写，并拒绝重复首部。
5. 对 HTTP/1.1 要求提供 Host。
6. 拒绝 Transfer-Encoding。
7. 使用 `std::from_chars` 解析 Content-Length；要求完整消耗整个值。
8. 检查消息体大小不得超过 1 MiB，并保护首部大小与消息体大小的加法计算，避免溢出。
9. 如果已收到的消息体字节数少于 Content-Length，则返回 NeedMoreData。
10. 精确复制声明长度的消息体字节，返回 Complete，并设置 consumed。

解析器不得消耗后续通过流水线发送的请求。调用方可以移除开头的 `consumed` 个字节，然后再次解析。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/http-parser --help
cat request.http | ./build/http-parser parse-stdin
```

## 平台检查内容

固定测试夹具测试完整的 GET/POST 请求、不完整的首部与消息体、流水线请求、已消耗字节数报告、HTTP 版本与 Host 验证、重复首部、拒绝 Transfer-Encoding、严格的 Content-Length、首部和消息体限制、UTF-8 字节计数，以及消息体数据中含有 CRLFCRLF 的情形。

## 解释你的设计

1. 为什么 NeedMoreData 与 Error 不同？
2. 为什么解析器必须在解析首部之前找到 CRLFCRLF？
3. 为什么解析器要返回已消耗字节数？
4. 为什么使用 from_chars 而不是 stoi 解析 Content-Length？
5. 为什么教学解析器拒绝 Transfer-Encoding？
6. 为什么将首部名称统一为小写？
7. 为什么这一教学版本的解析器拒绝重复首部？
8. 为什么需要限制首部和消息体的大小？
9. 为什么单个输入缓冲区中可能包含两条完整请求？
10. 为什么调用方应该循环解析，直到返回 NeedMoreData？

## 故障排查

- **分片到达的请求解析失败或始终无法完成：**检查调用方累积的缓冲区、`\r\n\r\n` 和声明的消息体长度。每次调用 `parse` 时传入截至当前收到的全部字节；输入不完整时必须保留缓冲区，并报告 `consumed == 0`。
- **第二条请求消失，或消息体字节变成了首部：**将 `consumed` 与第一条请求在线路上的字节长度进行比较。重新检查流水线请求和消息体包含 CRLFCRLF 的测试情形，仅移除已完成请求对应的缓冲区前缀。
- **看起来熟悉的首部被拒绝：**检查 HTTP 版本、Host、重复名称和冒号之前的空白。按本任务规则修正输入；HTTP/1.1 需要 Host，本课仍不支持 Transfer-Encoding。
- **Content-Length 或大小边界检查失败：**检查完整的数值字符串，包括正负号和尾随字符，并将首部结束标记的字节计入长度。重新检查严格的十进制解析及 16 KiB 首部 / 1 MiB 消息体限制，不要提高这些限制。

## 验收要求

- [ ] 完整的 GET 和 POST 请求能正确解析。
- [ ] 不完整的首部和消息体返回 NeedMoreData。
- [ ] 格式错误的请求返回 Error，并提供诊断信息。
- [ ] HTTP/1.1 要求 Host；HTTP/1.0 不要求。
- [ ] 拒绝重复首部和 Transfer-Encoding。
- [ ] Content-Length 严格解析，并以字节为单位。
- [ ] 首部和消息体的大小限制得到执行。
- [ ] consumed 只报告第一条请求的字节数。
- [ ] 全部 39 项验收检查通过。
