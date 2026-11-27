
# CppAppLab Windows x64 原生学习者包（CI 候选）

## 开始学习

1. 完整解压 ZIP。建议使用普通用户可写的英文目录，可含空格，例如 `D:\CppAppLab`；如果系统存在可验证的短文件名别名，程序也允许其他安装路径。用户名及学员工作区可以是中文。
2. 双击 `Start-CppAppLab.cmd`，在本机浏览器打开网页。保持启动窗口运行，Ctrl+C 停止服务。
3. 首次学习选择“继续学习”，完成 W1–W4。无需安装 WSL2、Ubuntu、MSYS2、Python、GCC、CMake、Ninja 或 GoogleTest；本候选将原生运行依赖随包交付。
4. W5–W9 保留教材阅读，实际运行需要 Ubuntu/WSL2。Windows 原生独立知识实验与 C++20/23 工程尚未开放执行。

**重要：** W2–W4 的正式验收包含真实文件符号链接测试。普通 Windows 账户可能需要由用户/管理员开启开发者模式或具备相应权限。启动程序只检查能力并给出提示，不会自行更改系统设置；也不会跳过测试。W1 不要求此能力。

断网时可使用内置基础编辑器。无需联网下载依赖进行基础课程构建或验收；实际完全断网的普通用户机器验收仍待补充。Windows 包路径中的工具链必须为 ASCII 路径或可验证的 ASCII 短路径；学习文件和学习记录保留中文路径。若系统临时目录不兼容，可设置 `CPPAPPLAB_NATIVE_BUILD_ROOT` 为用户可写的本地英文路径。

## 其他入口

- `Start-CppAppLab.cmd --port 8766`：使用另一回环端口。
- `Start-CppAppLab.cmd --verify-tools`：检查随包工具链的实际版本，并在临时目录中完成原生 C++17 头文件、链接和可执行文件运行探针；不改变学习进度。
- `cppapplab.cmd learn`：直接使用相同的原生 CLI。
- `Start-CppAppLab.cmd --no-browser`：只启动前台服务，手动打开终端显示的地址。

学习者工作区默认位于 `%USERPROFILE%\cppapplab-work`，进度位于 `%LOCALAPPDATA%\CppAppLab\native-windows\progress`。编译缓存不含正式学习进度；更新包不会覆盖工作区及进度。

## 包身份与再分发限制

`BUILD-INFO.txt`、`package-manifest.json` 和随归档提供的 `SHA256SUMS` 记录源码版本、Python 官方 ZIP 哈希、UCRT64 冻结的 63 项软件包版本、原始包元数据、上游实际归档 SHA-256 采集报告和逐文件哈希。CP7-D2b 已冻结 D2a 实际采集报告的全部 63 个归档身份，候选构建将仅安装经锁定 SHA-256 和可信签名核验的归档；仍须通过实际 Windows 重放及完整 ZIP 回归。工具链许可文本保存在 toolchain 的原始目录结构；嵌入式 Python 原始许可随包保留。第三方软件精确对应源码、许可证与再分发义务仍须在公开发布前完成核查。

**本包是内部 CI 验证候选，不是正式 GitHub Release。** 普通 Windows 11 实机及真人学习评估尚未完成；正式支持声明须以最终验收为准。
