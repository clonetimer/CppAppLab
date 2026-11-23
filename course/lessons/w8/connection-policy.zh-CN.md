# W8-D6-A · HTTP 连接策略与响应序列化

## 从这里开始

首先判断一个请求处理完毕后，是否应保持它的 TCP 连接打开。

1. 比较 HTTP/1.0 与 HTTP/1.1 的默认行为。
2. 将 Connection 首部视为由逗号分隔的选项列表。
3. 实现不区分大小写的完整 token 匹配；不要使用子串匹配。
4. 按照对应的 Connection 策略序列化响应。
5. 测试显式 close、keep-alive 以及选项冲突的情况。
6. 确认本课尚未改变服务器本身的生命周期。

**理解模型：**连接策略是根据请求得出的决策。序列化器负责传达这一决策；下一课再将它应用到实际的 socket 生命周期。


## 目标

根据 HTTP 请求确定连接是否持久化，并让响应序列化器输出对应的策略。本阶段为协议层支持 Keep-Alive 做准备，暂不改变 TCP 服务器的生命周期。

## 连接策略

实现将 Connection 字段识别为由逗号分隔、大小写不敏感的完整选项列表。每个选项周围的可选空白都会被忽略。

| 请求版本 | Connection 选项 | 结果 |
|---|---|---|
| HTTP/1.1 | 缺省，或没有 close 选项 | Keep-Alive |
| HTTP/1.1 | 包含 close | Close |
| HTTP/1.0 | 缺省，或没有 keep-alive 选项 | Close |
| HTTP/1.0 | 包含 keep-alive，且没有 close | Keep-Alive |
| 任一受支持版本 | 同时包含 keep-alive 和 close | Close |
| 不受支持的版本 | 任意 | Close |

未知的连接选项不会改变默认行为。匹配依据是完整 token，而不是子串。close 选项具有优先权。

解析器已经将解析后的首部名称统一转换为小写。策略辅助函数也以不区分大小写的方式比较名称，使手动构造的请求具有确定的行为。

## API 接口

- `ConnectionPolicy::Close` 和 `ConnectionPolicy::KeepAlive` 表示策略决策。
- `request_connection_policy(const HttpRequest&)` 计算策略，不修改请求。
- `serialize_response(response, policy)` 恰好写入一个作为唯一依据的 Connection 首部，默认在状态行中使用 HTTP/1.1，并根据正文的字节数计算 Content-Length。
- `serialize_response(response, policy, response_version)` 在状态行中保留 HTTP/1.0 或 HTTP/1.1；不受支持的版本会被拒绝。
- 为兼容 W1–W8-D5，原有的单参数 `serialize_response(response)` 重载仍在响应后关闭连接。

调用方提供的 Connection 和 Content-Length 值不会被直接转发；这两个字段都由序列化器负责。

## 重要边界

D6-A **尚不**复用 TCP 连接。D5 服务器继续调用原有的单参数序列化器，并在一次响应后关闭连接。D6-B 将把请求策略接入 Reactor 的连接生命周期，处理已缓冲的后续请求，并保持响应顺序。

在完成生命周期改造之前，不要在服务器中启用 Keep-Alive。

## 故障排查

使用本任务的固定检查来验证这些库函数；此工作区没有独立的服务器命令。

- **HTTP/1.0 和 HTTP/1.1 选择了相同的默认行为：**在计算选项之前检查请求版本。重新检查没有首部的情况，以及 HTTP/1.0 显式启用的情况；不受支持的版本必须选择 Close。
- **`x-close` 导致连接关闭，或混合大小写的选项未被识别：**检查逗号分隔的 token、空白处理和大小写处理。重新检查完整 token 匹配的用例，并确保真正的 `close` 选项优先于 `keep-alive`。
- **Keep-Alive 响应仍声明 close：**检查测试调用了哪个 `serialize_response` 重载。明确验证支持策略的重载，同时保持原有单参数重载在响应后关闭连接的行为。
- **传输内容中出现重复的策略首部，或版本错误：**检查调用方提供的首部，以及选用的响应版本重载。重新检查是否只有一个作为唯一依据的 Connection 首部、Content-Length 是否根据正文字节数计算，以及状态行是否使用受支持的 HTTP/1.0 或 HTTP/1.1。

## 验收要求

专用固定测试夹具会编译并测试两份持续维护的协议实现：

- `labs/w8/http-protocol/reference`
- `labs/w8/router/reference`，这是当前 HTTP 服务器链接的协议副本

每份实现都必须通过相同的 14 项测试，覆盖 HTTP/1.1 默认持久化、HTTP/1.0 显式启用、close 优先级、token 解析、大小写处理、空白处理、序列化器首部唯一性、Content-Length 计算以及向后兼容性。

在仓库根目录本地运行：

```bash
cmake -S course/fixtures/w8/http-connection-policy -B build-http-connection-policy \
  -DCPPAPPLAB_HTTP_PROTOCOL_SOURCE_DIR="$PWD/labs/w8/http-protocol/reference" \
  -DCPPAPPLAB_ROUTER_SOURCE_DIR="$PWD/labs/w8/router/reference" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-http-connection-policy --parallel
ctest --test-dir build-http-connection-policy --output-on-failure
```

## 解释你的设计

1. 为什么 HTTP/1.1 默认使用持久连接，而 HTTP/1.0 不这样做？
2. 为什么必须将 Connection 选项解析为逗号分隔的 token，而不能搜索子串？
3. 当 close 和 keep-alive 同时出现时，为什么 close 具有优先权？
4. 为什么 Connection 和 Content-Length 应由序列化器负责，而不能信任处理器提供的值？
5. 为什么仅添加 Keep-Alive 响应首部，并不能让服务器安全地复用 socket？

## 学习任务 — `w8.http-connection-policy`

**工作区文件**

- `include/http_message.hpp`
- `src/http_message.cpp`

实现 `ConnectionPolicy` 枚举、`request_connection_policy(const HttpRequest&)`，以及上文描述的两个支持策略的 `serialize_response` 重载。保持现有单参数序列化器与 W8-D5 兼容。

**固定验收：**`course/fixtures/w8/http-connection-policy` 中由平台维护的 14 项 GoogleTests。它们验证默认行为、HTTP/1.0 显式启用、完整 token 匹配、大小写/空白处理、close 优先级、首部的唯一权威来源以及向后兼容性。

在 `w8.http-server` 当前结果为 PASS 后开始。任务工作区从 D5 协议基线开始；D6-A 参考副本不会放入学习者的工作区。

```bash
./build/cppapplab start w8.http-connection-policy "$HOME/cppapplab-work/w8-connection-policy"
./build/cppapplab lesson "$HOME/cppapplab-work/w8-connection-policy"
./build/cppapplab hint "$HOME/cppapplab-work/w8-connection-policy" 1
./build/cppapplab check "$HOME/cppapplab-work/w8-connection-policy"
```
