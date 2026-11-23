# W8-D5 · epoll + ThreadPool HTTP 服务器

## 从这里开始

服务器把你已经构建的模块连接起来。在加入并发之前，先跟踪一条请求。

1. 启动本地回环服务器，并请求它最简单的静态路由。
2. 跟踪生命周期：接受 socket 连接 → 注册到 epoll → 读取字节 → 解析请求。
3. 跟踪请求如何到达路由器，以及响应如何返回客户端。
4. 将 socket 的所有权保留在 Reactor 线程中；工作线程执行处理器任务，但不关闭 Reactor 拥有的客户端描述符。
5. 加入有界的工作任务提交，并处理队列饱和。
6. 测试停止、客户端断开连接、慢速读取客户端和重复请求。

**核心理解：**Reactor 监视 socket；ThreadPool 执行有界的应用任务；连接状态将两者衔接起来。修改代码之前，先画清所有权边界。


## 目标

将 W8-D1–D4 的 HTTP 应用处理流程变成真正的 Linux TCP 服务器。本阶段结合三项已经学习的能力：

- W6：有界 ThreadPool 和 future
- W7：非阻塞 socket、epoll 和 eventfd
- W8-D1–D4：HTTP 解析器、Router、静态文件处理器和响应序列化器

交付成果是一个单 Reactor HTTP 服务器。它监听真实的 TCP 端口，并提供现有的动态和静态路由服务。

## 核心所有权规则

**事件循环线程独占 socket、epoll 注册项、Connection 对象和读写缓冲区。工作线程绝不调用 recv、send、close 或 epoll_ctl，也绝不保留 Connection 指针或引用。**

事件循环将一个拥有独立所有权的 HttpRequest 值交给工作线程。工作线程通过完成队列返回一个 HttpResponse 值。eventfd 唤醒事件循环，使它能够将序列化后的字节追加到连接的写缓冲区。

## 架构

```text
TCP clients
    |
    v
non-blocking listener / epoll
    |
    +-- accept / recv / send / close
    |
    v
incremental HttpRequestParser
    |
    v
bounded W6 ThreadPool
    |
    +-- Router / dynamic handlers / static file I/O
    |
    v
HttpResponse
    |
    v
completion queue + eventfd
    |
    v
event-loop thread
    |
    v
serialize_response -> writeBuffer -> EPOLLOUT -> send
```

## 连接与过期结果的安全处理

文件描述符不能作为稳定的连接身份，因为 Linux 可能在 close 后立即复用它的整数值。因此，每个被接受的连接都会获得一个单调递增的 64 位代际令牌。

- epoll 事件通过 data.u64 携带代际令牌。
- 工作线程的完成结果同时携带 fd 和 generation。
- 只有对应的 generation 仍然存在且 fd 匹配时，才应用该完成结果。
- 已断开连接客户端的过期完成结果会被丢弃。

只有事件循环线程能修改连接映射。工作线程仅拥有请求值和响应值。由于处理器可能并发执行，自定义 Router 回调必须是线程安全的，不能在没有同步的情况下修改共享状态。

## 有界并发与过载

现有 W6 ThreadPool 使用阻塞式有界提交队列。仅限制连接数量还不够：客户端断开后，它的工作任务可能仍在运行，而新客户端又可能建立连接。

因此，事件循环维护一个 `outstanding_jobs` 计数器，覆盖排队中的任务、运行中的任务以及尚未取出处理的完成结果。它在提交前检查计数器，在达到 `task_queue_capacity` 时返回 503，在 submit 前递增，并在每取出一个完成结果时递减，包括过期的完成结果。这样，事件循环就不会为了等待队列空间而阻塞。`max_connections` 仍是独立的 socket/资源限制。

达到活动连接上限时，新接受的客户端会收到尽力发送的 503 Service Unavailable 响应，然后连接被关闭。达到未完成工作任务上限时，请求收到 503。请求和响应缓冲区也有大小限制。静态文件读取在为整个文件分配内存之前就检查上限；自定义动态处理器仍必须自行约束内部内存分配。

## D5 的协议边界

D3 响应序列化器有意输出 `Connection: close`。D5 保留这一契约，每条 TCP 连接只处理一条请求，并且只在完整响应发送后关闭连接。Keep-Alive 和 HTTP 流水线属于 D6 的工作；当序列化器仍强制关闭连接时，不得悄悄启用这些功能。

分块 Transfer-Encoding、TLS、URL 解码、查询参数解析、路由模式，以及生产级的规范路径/符号链接范围约束，仍不在本任务范围内。

## 错误映射

- 格式错误或不完整的 HTTP 请求：400 Bad Request
- 请求/首部/消息体超过配置上限或解析器上限：413 Content Too Large
- 处理器异常：500 Internal Server Error
- 达到活动连接上限或工作任务提交失败：503 Service Unavailable
- 未知路由/静态文件：沿用现有 404 行为
- 已知动态路由使用错误的方法：沿用现有 405 + Allow 行为

超过配置响应大小上限的响应会被替换为大小受限的 500 响应。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/http-server server 8080 www
```

启动服务器前先创建一个简单的文档根目录，让静态回退有文件可提供：

```bash
mkdir -p www
printf '<h1>CppAppLab</h1>\n' > www/index.html
```

试用真实网络服务器：

```bash
curl -i http://127.0.0.1:8080/hello
curl -i http://127.0.0.1:8080/health
curl -i http://127.0.0.1:8080/
curl -i -X POST --data-binary 'hello' http://127.0.0.1:8080/echo
```

使用 Ctrl+C 停止演示进程。库 API 还提供线程安全的 `request_stop()`，它通过写入 eventfd 唤醒阻塞中的 epoll_wait。调用方必须在销毁服务器之前 join 执行 `run()` 的线程。

## 平台检查内容

固定测试夹具使用真实的本地回环 TCP socket，以及真实的解析器 → 分发器 → 序列化器流程。它覆盖动态和静态路由、二进制 Echo、分片请求、部分写入、格式错误或过大的输入、过载、处理器异常、过期工作线程完成结果，以及 eventfd 关闭流程。平台提供的 CMake 和测试仍是验收依据；学员可控的构建文件不能削弱验收检查。

## 故障排查

- **curl 无法连接，或 `/` 返回 404：**检查服务器启动错误、所选端口和配置的文档根目录。对正在运行的监听服务重试 `/health`，然后验证文档根目录是否包含供静态路由使用的 `index.html`。
- **慢速读取客户端收到的响应被截断：**检查写偏移、剩余字节数，以及 `EAGAIN` 之后的 EPOLLOUT 注册。重新检查大响应测试夹具，继续发送尚未发送的字节，并只在完整响应写出后关闭这条 D5 连接。
- **队列压力导致 Reactor 卡住或工作槽位丢失：**检查尚未完成的排队/运行任务，以及未取出处理的完成结果，也要计入已断开客户端的任务。重新检查饱和及请求后断开连接的测试情形；任务提交必须有界，且不能阻塞 socket 的处理进度。
- **迟到的响应发送给了新连接的客户端：**将完成结果中的描述符和代际令牌同时与当前连接比较。重新检查过期完成结果的测试情形，并将所有 socket/连接状态修改保留在 Reactor 线程中。

## 验收要求

- [x] 真实的非阻塞 IPv4 监听 socket 和水平触发 epoll。
- [x] socket 和 Connection 状态仅由事件循环拥有。
- [x] 集成有界 W6 ThreadPool，并使用独立拥有的请求值和响应值。
- [x] 完成队列和 eventfd 唤醒。
- [x] 基于代际令牌安全拒绝过期事件和完成结果。
- [x] 增量解析器和有界读写缓冲区。
- [x] 仅在需要时通过 EPOLLOUT 继续部分写入。
- [x] 400 / 413 / 500 / 503 映射和干净关闭。
- [x] 全部 34 项验收检查通过。

## 解释你的设计

1. 为什么仅靠 epoll 无法阻止慢处理器阻塞其他客户端？
2. 为什么工作线程不能修改 Connection 或 socket 状态？
3. 为什么仅用 fd 不足以标识一个连接？
4. 代际令牌用于防范什么问题？
5. 为什么完成结果入队后要由 eventfd 唤醒事件循环？
6. 为什么完成队列的大小受未完成工作任务数约束，而且包含客户端已断开的任务？
7. 为什么事件循环必须先检查 outstanding_jobs，再调用 W6 的阻塞式 submit？
8. 为什么 D5 中每条连接同时只能有一条正在处理的请求？
9. 为什么 D5 在一条响应之后关闭连接？支持 Keep-Alive 之前必须做哪些改变？
10. 为什么只有 writeBuffer 非空时才应启用 EPOLLOUT？
11. 工作线程仍在处理时，如果客户端断开，会发生什么？
12. 为什么事件循环必须丢弃 generation 已不存在的完成结果？
