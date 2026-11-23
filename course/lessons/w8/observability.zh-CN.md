# W8-D6-D2 · 结构化日志与运行时指标

## 从这里开始

通过一个请求了解服务器记录了什么。

1. 发送一个成功请求，并检查 JSON-lines 访问日志。
2. 发送一个返回 HTTP 错误的请求，并检查状态字段。
3. 触发处理器或解析器错误，并检查错误日志。
4. 比较请求前后的计数器。
5. 验证例行日志中没有写入机密信息和敏感请求值。
6. 分别禁用访问和错误通道，确认配置生效。

**理解模型：**日志解释单个事件；指标汇总一段时间内的行为。可观测性不能改变请求结果，也不能泄露机密信息。


## 目标

让 HTTP 服务器具有可观测性，同时避免工作线程修改 Reactor 拥有的状态，也避免在例行日志中暴露请求中的机密信息。

## 结构化 JSON Lines

默认情况下，服务器向 stderr 写入日志，每行一个 JSON 对象。访问和错误通道可以分别禁用：

- `access_log_enabled`：记录成功及错误 HTTP 响应的完成事件。
- `error_log_enabled`：记录处理器失败、解析器/HTTP 错误、连接拒绝、超时以及服务器生命周期事件。
- `log_sink`：可选回调，在 Reactor 线程上接收临时的 `std::string_view`。如果日志接收器要保留该行，必须复制这个视图中的数据，而且不能阻塞 Reactor。

默认日志接收器为 `std::clog`。回调抛出的异常会被隔离并计数，不会终止请求处理。

### 访问事件

`http_access` 事件包含：

- 墙上时钟时间 `timestamp_ms`、严重级别 `level` 和事件名称
- 连接代次 ID
- 请求方法和路径
- HTTP 状态
- 从请求开始到响应完全写出的端到端耗时，单位为毫秒
- 已完全写出的响应字节数
- 连接将关闭还是保持持久连接

查询部分会被有意隐去，日志改为记录 `query_present`。请求首部和正文绝不会被记录。

### 错误与生命周期事件

事件流包含 `http_error`、`handler_failure`、`worker_queue_rejected`、`connection_rejected`、`idle_timeout`、`header_timeout`、`body_timeout`、`server_started`、`server_shutdown_started`、`shutdown_deadline_expired`、`server_loop_failure` 和 `server_stopped`。

处理器异常会映射为固定的诊断事件。不会输出原始异常文本，因为处理器可能在异常消息中包含凭据或请求内容。

## 运行时指标 API

`HttpServer::metrics()` 返回线程安全的 `HttpServerMetrics` 快照。计数器由原子变量支持，因此可在 Reactor 和工作线程池运行时调用该 API。

| 指标 | 语义 |
|---|---|
| `accepted_connections` | 成功注册到 epoll 的 socket 数 |
| `rejected_connections` | 被 `max_connections` 拒绝的新 socket 数 |
| `active_connections` | 当前由 Reactor 拥有的已接受 socket 数 |
| `closed_connections` | 由 Reactor 关闭并移除的已接受 socket 数 |
| `persistent_connections` | 当前占用 Keep-Alive 配额槽位的 socket 数 |
| `requests_received` | 被接受并准备派发的完整 HTTP 请求数 |
| `responses_completed` | 已向对端完全写出的响应数 |
| `response_bytes` | 已完全写出的 HTTP 响应总字节数 |
| `client_errors` | 已完全写出的 4xx 响应数 |
| `server_errors` | 已完全写出的 5xx 响应数 |
| `request_timeouts` | 首部/正文截止时间到期次数 |
| `idle_timeouts` | 空闲 keep-alive/已接受 socket 的到期次数 |
| `handler_failures` | 路由/静态文件处理器抛出的异常数 |
| `queue_rejections` | 因工作线程队列已满或提交失败而被拒绝的请求数 |
| `active_jobs` | 当前已提交但尚未完成的工作线程任务数 |
| `graceful_shutdowns` | 已启动的关闭流程次数 |
| `forced_shutdown_connections` | 关闭排空截止时间到达时被关闭的 socket 数 |
| `log_sink_failures` | 用户日志回调抛出异常的次数 |

除表示当前值的指标（`active_connections`、`persistent_connections`、`active_jobs`）外，其余指标均为累计值。只有整个序列化响应完全写出后，响应计数器才递增；对端在部分写入期间断开连接，不计为已完成响应。被拒绝的连接不是已接受的 socket，因此不会使 `closed_connections` 递增。

快照的每个字段都具有线程安全性，但它不是跨计数器的事务性快照：各次原子读取之间，数值可能继续变化。

## 故障排查

使用固定回环检查及其捕获日志的接收器，检查本库任务的输出。

- **传输尚未完成，响应计数器就递增：**检查写入偏移在何处达到序列化响应的长度。重新检查已完成响应的记账；工作线程完成或部分写入，还不代表响应已完成。
- **日志包含查询值或异常细节：**检查路径提取和处理器失败事件的格式化。使用带查询字符串的请求及抛出异常的处理器重新检查脱敏；保留 `query_present`，省略首部/正文，并避免记录原始异常文本。
- **自定义日志接收器损坏日志或中断请求：**检查临时 `std::string_view` 的生命周期及异常路径。复制需要保留的行，确保回调不阻塞，并重新检查：抛出异常的接收器应使 `log_sink_failures` 递增，但不能改变响应。
- **连接或任务的当前值指标始终不归零：**检查实际的 socket 移除与完成结果处理，包括被拒绝或已经过期的工作。活动稳定结束后，重新检查 Keep-Alive 和队列压力用例；累计计数器不应随当前值指标一起重置。

## 验收要求

固定回环测试夹具覆盖：

1. JSONL 访问记录字段和已完成响应计数器。
2. 同一个 Keep-Alive socket 上处理两个请求期间的活动/持久连接当前值指标。
3. 处理器异常事件、500 计数器，以及不记录原始异常文本。
4. 独立禁用错误日志，以及查询字符串脱敏。
5. 隔离并计数日志接收器异常，同时不打断请求处理。
6. 空闲和不完整首部的超时计数器。
7. 最大连接数拒绝计数器。
8. 工作线程队列拒绝计数器，以及活动任务数恢复。

该夹具在 GCC、ASan/UBSan 和 clang-tidy 下运行。D5、D6-B、D6-C 和 D6-D1 固定夹具仍作为独立回归门槛。

## 解释你的设计

1. 为什么请求指标由 Reactor 负责，而工作线程异常通过完成队列返回？
2. 为什么只有全部序列化字节写出后，才对响应计数？
3. 为什么活动连接数和活动任务数是当前值指标，而不是累计计数器？
4. 为什么默认访问日志要隐去查询字符串，并省略请求首部/正文？
5. 为什么自定义日志接收器必须不阻塞，并且在需要保留传入视图时复制其数据？
6. 为什么指标快照是线程安全的，却不保证所有字段之间的事务一致性？

## 学习任务 — `w8.http-observability`

**工作区文件**

- `include/http_server.hpp`
- `src/http_server.cpp`

从 D6-D1 开始，添加公开的日志和指标 API、JSONL 访问/错误/生命周期事件、查询字符串脱敏、自定义日志接收器、异常隔离，以及原子计数器/当前值指标。保持连接状态和指标更新都由 Reactor 负责。

**固定验收：**`course/fixtures/w8/http-observability` 中由平台维护的 8 项回环 GoogleTests。它们验证日志字段、脱敏、指标、处理器失败、接收器异常隔离、超时计数器、连接拒绝以及队列压力。

```bash
./build/cppapplab start w8.http-observability "$HOME/cppapplab-work/w8-observability"
./build/cppapplab lesson "$HOME/cppapplab-work/w8-observability"
./build/cppapplab hint "$HOME/cppapplab-work/w8-observability" 1
./build/cppapplab check "$HOME/cppapplab-work/w8-observability"
```
