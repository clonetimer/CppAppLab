# W8-D3 · HTTP 响应与静态文件

## 从这里开始

在学习完整路由器之前，先把一条已解析的请求与一条响应连接起来。

1. 构造一条 `GET /` 请求，并传给静态文件处理器。
2. 查看返回的状态、首部和消息体。
3. 序列化响应，并检查精确的 HTTP 字节内容。
4. 尝试访问一个已知文件、一个不存在的文件和一个目录索引。
5. 比较 GET 与 HEAD：本任务仅允许 GET，因此 HEAD 返回 405，并带有 `Allow: GET`。
6. 测试二进制内容和不安全的路径。

**核心理解：**解析将字节转换为请求对象；处理器将该对象转换为响应对象；序列化再将响应转换回字节。每一步都承担单一职责。


## 目标

将 W8-D2 解析得到的请求模型与 HTTP 响应及静态文件处理器连接起来：

```text
HttpRequest
    ↓
StaticFileHandler
    ↓
HttpResponse
    ↓
serialize_response()
    ↓
HTTP/1.1 bytes
```

本任务构建处理器和序列化器的演示程序，尚不打开监听 TCP socket；网络与事件循环的集成将在后续阶段完成。

## 课程材料规定的范围

- 对 `GET /` 返回 `index.html`。
- 使用 `std::ios::binary` 读取静态文件，精确保留每个字节。
- 根据扩展名选择 Content-Type：html、css、js、txt、json、png、jpg/jpeg 和 svg；其他类型使用 `application/octet-stream`。
- 文件不存在时返回 404。
- 仅允许 GET；其他方法返回 405 和 `Allow: GET`。
- 查找文件前去掉查询字符串。
- 对不安全的目标返回 400。
- 将文件访问和 MIME 选择保留在 `StaticFileHandler` 内，不放入 TCP/epoll 层。
- 复用 D1 响应序列化器，使 Content-Length 根据消息体字节数计算，并输出 Connection: close。

路径检查有意采用保守策略：教学实现会拒绝不是相对于根路径的目标，以及包含 `..` 或反斜杠的目标。这不是生产级的文档根目录范围约束机制。URL 解码、规范路径检查，以及防止符号链接越出根目录，仍是已知限制。

## 架构

- `HttpRequest` / `HttpResponse`：来自 D1 的协议数据模型。
- `serialize_response`：验证响应首部，并输出状态行、首部、CRLFCRLF、以字节为单位的 Content-Length 和消息体。
- `StaticFileHandler`：将请求映射为文件或 HTTP 错误响应。
- `main.cpp`：处理单条请求的命令行演示程序；它不是网络服务器。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/http-static-files --help
./build/http-static-files handle GET /
./build/http-static-files handle GET /hello.txt
./build/http-static-files handle POST /
```

命令行示例使用相对于当前工作目录的 `www/` 目录。

## 平台检查内容

固定测试夹具覆盖根路径到索引文件的映射、二进制文件读取、MIME 类型、未知扩展名、404、405/Allow、不安全的路径、查询处理、Content-Length 和 Connection: close。平台提供的头文件、测试和 CMake 界定验收范围。

## 解释你的设计

1. 为什么 `HttpResponse` 与 `HttpRequest` 是相互独立的模型？
2. 为什么应该由序列化器计算 Content-Length？
3. 为什么静态文件必须以二进制模式读取？
4. Content-Type 向客户端说明了什么？
5. 为什么 `/` 通常映射到 `/index.html`？
6. 处理器应在什么情况下返回 404 和 405？
7. 为什么不能将请求目标直接拼接到本地路径中？
8. 什么是路径遍历？
9. 为什么 epoll/网络代码不应该执行文件 I/O 和 MIME 选择？
10. 解析、静态文件处理、响应序列化和 socket I/O 分别由哪一层负责？

## 故障排查

- **`GET /` 返回 404：**检查当前工作目录和 `www/index.html`；演示 CLI 使用相对的文档根目录。先在该目录放入一个已知文件并重试 `handle GET /`，再排查 MIME 选择。
- **HEAD 或 POST 返回 405：**检查请求方法和 `Allow` 首部。本阶段仅支持 GET；与 `handle GET /` 进行比较，并保留既定的非 GET 请求拒绝行为。
- **二进制字节或 Content-Length 不一致：**检查文件打开模式，以及序列化之前消息体的字节长度。重新检查二进制文件和 UTF-8 测试情形，避免按行读取或按 C 字符串计算长度。
- **查询字符串改变了文件查找或触发路径遍历拒绝：**检查第一个 `?` 之前的路径。先将路径与查询字符串分离，再应用本阶段的保守路径检查；重新检查查询值中包含 `..` 的测试情形。

## 验收要求

- [ ] GET / 返回 index.html。
- [ ] 文本和二进制文件的字节内容得到精确保留。
- [ ] 支持的扩展名返回预期的 MIME 类型。
- [ ] 未知扩展名返回 application/octet-stream。
- [ ] 不存在的文件返回 404。
- [ ] 非 GET 请求返回 405，并带有 Allow: GET。
- [ ] 不安全的目标返回 400。
- [ ] 查询字符串不影响文件查找。
- [ ] Content-Length 统计消息体字节数，并输出 Connection: close。
- [ ] 全部 31 项验收检查通过。
