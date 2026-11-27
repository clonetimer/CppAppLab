# C++20 · 采样窗口统计

实现 summarize：输入只读 double 采样窗，保留有限且在闭区间 [0,100] 的样本。没有有效样本返回 nullopt；否则返回计数、算术平均、最小值、最大值。不得修改输入。先检查空窗口、正常样本，再处理 NaN/Inf 与边界。建议使用 span 和 ranges::views::filter；测试验证行为而非强制内部语法。示例数据为合成数据，不是飞行数据。

Implement summarize over a read-only span. Keep finite samples in [0,100]; return nullopt when none are valid, otherwise count, arithmetic mean, minimum and maximum. Preserve the input. Try empty, nominal, NaN/Inf and boundary cases. Prefer a ranges filter; tests verify behavior, not internal syntax. Synthetic data only.

Fixed editable file: implementation.cpp (copied from starter.cpp, never reference.cpp). Read-only contract.hpp; 7 independent platform-owned GoogleTest cases. Reference is maintained for CI, not copied into the learner workspace. The workbench stages only the source snapshot and platform fixture into a disposable build directory. C++20, GCC 13, CMake, GoogleTest. No production or flight qualification.
