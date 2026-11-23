# W8-D1 · HTTP/1.1 协议基础

## 从这里开始

先从一条文本形式的 HTTP 请求入手，不要一开始就实现完整服务器。

1. 阅读示例请求，标出请求行、首部、空行和消息体。
2. 从请求行中解析方法、目标和协议版本。
3. 解析一个首部，并解释为什么比较首部名称时不区分大小写。
4. 将一个包含状态行、首部和消息体的简单响应序列化。
5. 验证 Content-Length 统计的是消息体的字节数，而不是字符数。
6. 比较 HTTP 消息边界与 TCP 接收边界。

**核心理解：**TCP 负责传输字节，HTTP 为这些字节赋予含义。一条请求可能分多次 TCP 读取才到达，因此绝不能把单次 `recv` 当作一条完整的 HTTP 消息。


## 目标

在 W7 已完成的 TCP 字节流之上构建应用协议层。本课涵盖 HTTP/1.1 消息结构、请求行与响应行、首部、状态码、按字节精确计算的 Content-Length、CRLF 定界，以及协议消息与一次 TCP 接收操作之间的区别。

本任务暂不实现完整的 HTTP 请求解析器、TLS、HTTP/2 或 HTTP/3。增量请求解析属于 W8 的下一阶段。

## 课程材料中的主题

### 请求结构

一条 HTTP 请求由以下部分组成：

1. 请求行：`METHOD target VERSION`
2. 采用 `Name: Value` 格式的首部
3. 一个空行，在最后一行首部之后表现为 `\r\n\r\n`
4. 可选的消息体

示例：

```http
GET /hello HTTP/1.1
Host: 127.0.0.1:8080
Accept: */*

```

```http
POST /echo HTTP/1.1
Content-Type: text/plain
Content-Length: 5

hello
```

### 响应结构

一条响应由状态行、首部、一个空行和可选的消息体组成：

```http
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 13
Connection: close

Hello, world!
```

请求行的格式是 `METHOD target VERSION`；响应状态行的格式是 `VERSION status reason`。

### 状态码

- 200 OK, 201 Created, 204 No Content
- 301 Moved Permanently, 302 Found
- 400 Bad Request, 401 Unauthorized, 403 Forbidden
- 404 Not Found, 405 Method Not Allowed
- 500 Internal Server Error, 503 Service Unavailable

合法请求访问不存在的资源时返回 404；请求行格式错误时返回 400。401 表示缺少有效的身份认证或认证无效；403 表示身份已知，但没有相应权限。

### 消息定界与连接行为

HTTP/1.1 文本消息使用 CRLF（`\r\n`）定界，而不是单独的 LF。首部结束标记是 `\r\n\r\n`。Content-Length 统计的是消息体字节数，而不是 Unicode 字符数。一次 `recv()` 不保证包含一条完整的 HTTP 请求。

HTTP/1.1 通常支持持久连接。为简化连接生命周期管理，本阶段实现显式输出 `Connection: close`；keep-alive 将在后续阶段实现。

## 实现文件

- `include/http_message.hpp`：`HttpRequest` 和 `HttpResponse`
- `src/http_message.cpp`：请求行解析与响应序列化
- `src/main.cpp`：简单的协议演示 CLI
- `tests/http_message_test.cpp`：由学员编写的十个测试

## 必需行为

- 从请求行中解析且仅解析三个字段：方法、目标和版本。
- 拒绝字段缺失、多余字段、空输入以及内嵌换行符。
- 仅解析请求行时，保留已有的首部和消息体。
- 序列化 HTTP/1.1 状态行、首部、CRLFCRLF 和消息体。
- 根据最终消息体的字节数计算 Content-Length。
- 在服务器的这一初始阶段输出 `Connection: close`。
- 首部名称不区分大小写；拒绝仅大小写不同的重复名称。
- 拒绝格式错误的首部名称，以及首部值或原因短语中的 CR/LF 注入。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/http-protocol parse-line "GET /hello HTTP/1.1"
./build/http-protocol response
```

在有可用服务器时，可以用 `curl -v http://127.0.0.1:8080/` 查看真实 HTTP 交互的细节，也可以通过 `nc 127.0.0.1 8080` 手动输入请求。别忘了最后的空行。

## 平台检查内容

固定测试夹具检查请求行解析、格式错误的输入、请求字段保留、状态行格式、首部处理、按字节精确计算的 Content-Length、强制关闭连接的语义、确定的首部顺序以及首部注入防护。平台测试夹具自行提供声明头文件、测试和 CMake 配置。

## 解释你的设计

1. HTTP 与 TCP 是什么关系？
2. HTTP 请求行由哪三个字段组成？
3. 响应状态行由哪三个字段组成？
4. 首部的基本格式是什么？
5. Content-Length 度量的是什么？
6. 为什么 HTTP/1.1 使用 CRLFCRLF 标记首部结束？
7. 为什么不能把一次 recv() 当作一条完整请求？
8. 200、400、404 和 500 分别表示什么？
9. 401 与 403 有什么区别？
10. Connection: close 与 keep-alive 有什么区别？
11. 为什么完整的 HTTP 解析器需要缓冲区和解析状态？
12. 为什么要将 HttpRequest 和 HttpResponse 与传输代码分离？

## 故障排查

- **`parse-line` 拒绝输入：**检查参数是否缺少字段、包含多余字段或内嵌 CR/LF。只传入请求行，然后检查解析后是否完整保留了已有的首部和消息体。
- **非 ASCII 文本的 Content-Length 不正确：**对照序列化后的消息体检查其字节数，而不是显示出来的字符数。重新检查 UTF-8 测试夹具，以及调用方提供错误 Content-Length 的情形。
- **响应看起来正确，但字节比较失败：**检查 CRLF 分隔符、最后的空行和首部顺序。逐字节比较空响应的测试情形，包括本阶段要求的 `Connection: close`。
- **重复首部或注入首部被接受：**检查首部名称是否按不区分大小写的方式处理，并检查首部值或原因短语中的 CR/LF。在修改合法响应的序列化逻辑之前，先重新检查应被拒绝的测试情形。

## 验收要求

- [ ] 请求行能解析为方法、目标和版本。
- [ ] 拒绝字段缺失和多余字段。
- [ ] 请求行解析后，已有首部和消息体保持不变。
- [ ] 响应使用 HTTP/1.1 状态行和 CRLF 定界。
- [ ] Content-Length 统计字节数。
- [ ] 输出 Connection: close。
- [ ] 首部名称按不区分大小写的方式处理。
- [ ] 拒绝首部和原因短语中的注入。
- [ ] 全部 33 项验收检查通过。
