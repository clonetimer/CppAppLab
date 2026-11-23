## 提示 1 — 将 Connection 视为选项列表

Connection 字段可以包含由逗号分隔的选项，以及可选的空格或制表符。不要在原始字符串中搜索子串 "close"；"x-close" 是不同的 token。

## 提示 2 — 规范化 token，而不是整个请求

对字段名称和选项 token 使用 ASCII 大小写不敏感比较。只去除每个 token 周围的可选空白。检查每个 Connection 条目，因为手动构造的请求可能包含仅大小写不同的键。

## 提示 3 — 保持序列化器的决定权

HTTP/1.1 默认使用 Keep-Alive；HTTP/1.0 需要显式的 keep-alive 选项。close token 优先。添加支持策略的序列化器重载，过滤调用方提供的 Connection 和 Content-Length 字段，并保留原有单参数重载在响应后关闭连接的行为。
