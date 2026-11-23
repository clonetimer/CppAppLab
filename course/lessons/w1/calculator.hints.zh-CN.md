## 提示 1

从 \`switch (operation)\` 语句开始。每个枚举值代表一种算术运算。返回一个同时包含数值和 \`Error\` 值的 \`Result\`。

## 提示 2

四种正常运算的结果都应使用 \`Error::None\`。对于除法，在执行运算之前检查是否满足 \`right == 0.0\`。不要先做除法，再尝试修补结果。

## 提示 3

运算成功时，分别返回 \`{left + right, Error::None}\`、\`{left - right, Error::None}\`、\`{left * right, Error::None}\` 或 \`{left / right, Error::None}\`。除数为零时返回 \`{0.0, Error::DivisionByZero}\`；在 switch 之后为无法识别的枚举值返回 \`{0.0, Error::InvalidOperation}\`。
