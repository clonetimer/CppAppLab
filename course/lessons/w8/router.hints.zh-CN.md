## 提示 1 — 存储可调用的处理器

使用 `std::function<HttpResponse(const HttpRequest&)>`。将方法和路径作为独立的查找维度。重复注册已有的方法/路径组合时，替换它的处理器。

## 提示 2 — 替换之前先验证

在修改已有注册项之前，先拒绝空或无效的方法 token、不以根路径开头或带有查询字符串的注册路径，以及空回调。方法和路径的匹配区分大小写。

## 提示 3 — 匹配请求并分发到处理器

### 匹配路径，保留目标

对于请求，查找使用 `request.target.substr(0, request.target.find('?'))`。将未修改的请求传给回调。不要把这一操作当作查询参数解析或 URL 解码。

### 区分 404 与 405

动态路径不存在时返回 404。路径已知但没有匹配的方法时，收集已注册的方法，排序后拼接为 Allow。返回 405；不要尝试同名静态文件。

### 三个处理器

Hello 返回课程中的问候语，并在末尾追加 LF。Echo 按字节数复制字符串消息体，包括内嵌 NUL。Health 返回固定的 JSON 文档，类型为 application/json。注册 GET /hello、POST /echo 和 GET /health。

### 按路径归属分发

提取路径后检查 `has_path`。对于动态路径，即使处理器响应 404，也返回 `router.handle(request)`。只有动态路径不存在时，才使用 `static_files.handle(request)`。序列化和网络 I/O 仍放在 Router 之外。
