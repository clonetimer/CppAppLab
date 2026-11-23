## 提示 1 — 复用现有模块

不要重写 HTTP 解析器、Router、静态文件处理器、序列化器或 W6 ThreadPool。将 W8-D4 分发器与 W6 线程池和 W7 socket/epoll 模式组合起来。

## 提示 2 — 将所有权集中在一个线程

事件循环拥有所有文件描述符、epoll 注册项、连接映射和缓冲区。工作线程接收一个 HttpRequest 值，并返回一个 HttpResponse 值。绝不要在工作任务中捕获 Connection 引用或拥有 socket 的指针。

## 提示 3 — 协调工作线程、I/O 与关闭流程

### 使用代际令牌

在每个 Connection 和 epoll_event.data.u64 中存储一个单调递增的 uint64 令牌。完成结果中同时放入 fd 和令牌。除非该令牌仍映射到同一个 fd，否则丢弃完成结果。

### 唤醒事件循环

在互斥锁保护下将完成结果入队，然后向非阻塞 eventfd 写入 uint64_t{1}。事件循环先读空 eventfd，再取出并处理完成队列中的所有结果。写入 eventfd 时出现 EAGAIN 表示已有唤醒待处理。

### 避免 ThreadPool 提交阻塞

现有 W6 submit 会等待有界队列腾出容量。每条连接仅允许一条正在处理的请求，并让 outstanding_jobs 覆盖排队、运行和完成后尚未取出处理的任务。调用 submit 之前，如果 outstanding_jobs 达到 task_queue_capacity，则返回 503；只有取出对应完成结果时才递减，即使其连接已过期也要递减。

### 增量接收与有界缓冲区

持续调用 recv，直到出现 EAGAIN/EWOULDBLOCK。将字节追加到 readBuffer，运行 HttpRequestParser，并区分 NeedMoreData、Complete 和 Error。将解析器/首部/消息体大小限制错误映射为 413，将格式错误的输入映射为 400。

### 部分写入

将 HttpResponse 一次性序列化到 writeBuffer。只有仍有字节待发送时才启用 EPOLLOUT。每次 send 成功后推进 writeOffset；遇到 EINTR 时重试；遇到 EAGAIN 时停止本轮发送。只有响应全部发送完成后才关闭连接。

### 保留 D5 的协议边界

现有响应序列化器始终输出 Connection: close。因此，D5 每条 TCP 连接只处理一条请求。Keep-Alive、流水线和对已缓冲请求的再次解析属于下一个任务，届时需要同时修改序列化器和连接策略。

### 错误与关闭路径

在工作线程中捕获处理器异常并返回 500。容量耗尽时返回 503。request_stop 设置停止标志并写入 eventfd；调用方在销毁 HttpServer 前 join 执行 run() 的线程。


### 并发处理器

Router 回调会在多个 W6 工作线程上运行。尽可能让处理器保持无状态。如果回调捕获共享的可变状态，使用互斥锁或合适的原子操作保护它；绝不要捕获 Connection 或文件描述符。
