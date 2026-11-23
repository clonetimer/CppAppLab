## 提示 1 — 解析结果

不要返回 bool。使用 NeedMoreData、Complete 和 Error 三种状态。除非一条请求已完整解析，否则让 consumed 保持为零。

## 提示 2 — 首部边界

查找 `\r\n\r\n`。如果未找到且缓冲区未超过 16 KiB 限制，则返回 NeedMoreData。如果超过限制，则返回 Error。

## 提示 3 — 精确解析一条有大小限制的请求

### 首部解析

在第一个冒号处分割每一行首部。验证字段名称是合法的 HTTP token（冒号之前不能有空白）。去除值两端可选的空白，将名称转为小写，并拒绝重复首部。

### Content-Length

使用 `std::from_chars` 将值解析为 `std::size_t`。要求转换没有错误，且返回的指针恰好指向值的末尾。拒绝超过 1 MiB 的值。

### 消息体与已消耗字节数

消息体紧接 CRLFCRLF 结束标记。如果剩余字节数少于 Content-Length，则返回 NeedMoreData。否则精确复制该数量的字节，并将 consumed 设为首部字节数加消息体字节数。

### 流水线请求

不要消耗第一条完整请求之后的字节。调用方移除 `result.consumed` 个字节，再对剩余缓冲区调用解析器。
