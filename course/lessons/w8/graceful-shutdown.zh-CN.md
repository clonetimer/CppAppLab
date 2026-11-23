# W8-D6-D1 · HTTP 服务器的优雅关闭

## 从这里开始

关闭是需要协调的状态转换，不只是终止进程。

1. 启动服务器，并打开一个空闲客户端连接。
2. 请求关闭，观察 Reactor 如何被唤醒。
3. 验证监听 socket 停止接受新客户端。
4. 观察空闲连接和正在处理的请求分别如何处理。
5. 确认工作线程能完成工作，或受到关闭契约的约束。
6. 重复关闭操作，检查幂等性和描述符清理。

**理解模型：**Reactor 拥有 socket 和连接状态。关闭期间，工作线程不得自行关闭由 Reactor 拥有的描述符。


## 目标

停止接受新客户端，尽可能完成已经接受的工作，并以确定的方式关闭 socket。关闭流程必须由 Reactor 线程驱动；工作线程绝不能关闭客户端描述符或修改 Connection 状态。

## 关闭顺序

1. `request_stop()` 设置停止标志，并通过 eventfd 唤醒 Reactor。
2. Reactor 移除并关闭监听 socket，不再接受任何新客户端。
3. 立即关闭空闲 Keep-Alive 连接。
4. 对已收到请求字节但请求仍不完整的连接，返回 HTTP 503 Service Unavailable，并携带 Connection: close。
5. 允许已经在工作线程池中执行的请求完成。将它们待发送响应的策略改为 Close，使最终响应不再声明还可以复用连接处理另一个请求。
6. 允许正在写出的响应完成，然后关闭连接。如果尚未发送任何响应字节，则在写出前将 Connection 首部改为 close。
7. 使用单调关闭截止时间限制 socket 保持打开的时长。截止时间到达后，关闭剩余连接。
8. Reactor 继续取完工作线程的完成结果。所有连接都已关闭，且所有已提交的工作线程任务都已完成后，Reactor 才退出。

## 配置

`HttpServerConfig::shutdown_timeout` 默认为 5 秒，遵循与其他截止时间相同的校验规则：必须为正，且不大于 24 小时。

关闭截止时间限制的是连接排空过程，而不是任意用户处理器的执行。现有 ThreadPool 无法强制取消一个永不返回的 C++ 回调。因此，处理器必须具有执行边界并能协作结束；服务器会等待已提交的任务完成，然后 `run()` 才返回，再等待线程池退出。

## 响应语义

- 正在处理的请求收到其处理器的正常响应，并携带 Connection: close。
- 不完整请求收到 503，并关闭连接。
- 空闲 Keep-Alive 连接静默关闭。
- 已经部分写出的响应会在可行时继续完成，随后关闭 socket。一旦部分首部已经到达对端，服务器就无法安全地改写已发送的字节；TCP EOF 仍会终止连接。

## 故障排查

固定回环检查调用库的停止/等待退出生命周期；本任务不提供单独的服务器可执行程序。

- **请求停止后 Reactor 仍在休眠，或仍在接受连接：**检查 `request_stop()`、它通过 eventfd 发出的唤醒，以及 Reactor 对监听器的清理。重新检查空闲连接用例，确保由拥有 socket 的线程执行关闭状态转换。
- **不完整请求与正在处理的请求得到了相同的关闭结果：**检查解析是否已完成，以及请求是否已由工作线程持有。重新检查三种状态：空闲连接静默关闭，不完整请求字节收到 503，已接受的处理器工作可以正常完成。
- **尚未发送的响应仍声明可以复用连接：**检查关闭时该响应待采用的连接策略和写入偏移。重新检查发送字节之前的强制关闭处理；已部分发送的首部无法改写，因此应在可行时完成响应后关闭连接。
- **`run()` 的耗时超过 `shutdown_timeout`：**分别检查剩余 socket 和已提交的工作线程任务。使用执行有界的处理器重新检查截止时间用例；截止时间负责关闭 socket，而完成结果仍须取完，工作线程也必须完成后才能等待其退出。

## 验收要求

固定回环测试夹具覆盖：

1. 停止时关闭空闲 Keep-Alive 连接。
2. 等待正在执行的处理器完成，并返回携带 Connection: close 的响应。
3. 关闭期间对不完整请求返回 503。
4. 在配置的关闭截止时间到达时关闭连接，同时允许其执行有界的处理器完成。
5. 拒绝值为零的关闭超时。

该夹具在 GCC、ASan/UBSan 和 clang-tidy 下运行。现有 D5、D6-B 和 D6-C 夹具仍作为独立回归门槛。

## 解释你的设计

1. 为什么监听器/客户端描述符的关闭必须由 Reactor 负责？
2. 为什么正在处理的请求与空闲连接要采用不同的处理方式？
3. 为什么关闭截止时间应由与连接截止时间相同的单调 timerfd 跟踪？
4. 为什么服务器能限制 socket 排空的时间，却不能强制取消任意 C++ 处理器？
5. 为什么关闭超时连接后，服务器仍必须继续取完工作线程的完成结果？

## 学习任务 — `w8.http-graceful-shutdown`

**工作区文件**

- `include/http_server.hpp`
- `src/http_server.cpp`

从 D6-C 开始，实现明确的关闭截止时间，以及由 Reactor 负责的关闭状态转换。停止接受新客户端，关闭空闲 socket，为不完整请求返回 503，让正在处理的工作完成并携带 Connection: close，在单调截止时间到达时关闭剩余 socket。在 `run()` 返回前，继续取完工作线程的完成结果。

**固定验收：**`course/fixtures/w8/http-graceful-shutdown` 中由平台维护的 5 项回环 GoogleTests。

关闭截止时间限制的是 socket 排空；它无法强制取消任意 C++ 处理器。仍须安全地等待工作线程池退出。

```bash
./build/cppapplab start w8.http-graceful-shutdown "$HOME/cppapplab-work/w8-graceful-shutdown"
./build/cppapplab lesson "$HOME/cppapplab-work/w8-graceful-shutdown"
./build/cppapplab hint "$HOME/cppapplab-work/w8-graceful-shutdown" 1
./build/cppapplab check "$HOME/cppapplab-work/w8-graceful-shutdown"
```
