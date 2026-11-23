## 提示 1 — 复用 D1

不要重写 HTTP 消息模型。沿用 D1 中已验证的 `HttpRequest`、`HttpResponse` 和 `serialize_response` 实现。Content-Length 和 Connection: close 由序列化器负责。

## 提示 2 — 读取字节

使用 `std::ifstream(path, std::ios::binary)` 打开文件。将文件内容通过流读入字符串，并检查读取是否失败。不要使用文本模式下的按行读取方式处理任意资源文件。

## 提示 3 — 解析目标并构造响应

### 状态处理

- 非 GET：405 Method Not Allowed，并带有 `Allow: GET`。
- 不安全的目标：400 Bad Request。
- 文件不存在或不可读：404 Not Found。
- 读取成功：200 OK。

### URL 目标

查找文件之前移除查询字符串。将 `/` 映射到 `/index.html`。在这一教学阶段，要求路径以斜杠开头，并拒绝 `..` 和反斜杠。

### MIME 类型

使用 `std::filesystem::path::extension()`。映射常见的 Web 类型，其他类型回退到 `application/octet-stream`。

### 保持职责分离

处理器返回一个 `HttpResponse`。它不应该向 socket 写入数据，也不应该手动构建 HTTP 传输字节；这些字节由序列化器生成。
