## 提示 1 — 请求行

使用 `std::istringstream` 提取方法、目标和版本。然后尝试提取第四个字段；如果存在，就拒绝该行。先解析到局部变量中，避免解析失败时只覆盖请求的一部分字段。

## 提示 2 — 响应状态行

以 `HTTP/1.1 <status_code> <reason_phrase>\r\n` 开头。拒绝不在 100–599 范围内的状态码，并拒绝原因短语中的 CR 或 LF。

## 提示 3 — 验证首部并序列化消息体

### 首部处理

验证首部名称是否仅由合法的 HTTP token 字符组成。首部名称不区分大小写，因此检测重复名称以及识别特殊的 Content-Length 和 Connection 字段时，应先统一名称的大小写。

### Content-Length

使用 `response.body.size()`。该值是字节数。对于 UTF-8 文本，字节长度可能大于显示的字符数量。

### 安全序列化

在这一初始任务中，忽略调用方提供的 Content-Length 和 Connection 值。只输出一次计算得到的 Content-Length 和 `Connection: close`。用 `\r\n\r\n` 结束首部，然后原样追加消息体。
