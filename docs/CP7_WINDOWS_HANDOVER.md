# CP7-G · Windows 原生与双环境交付手册（候选收口）

更新：2026-11-27。本文区分**真实 CI 验收**、**开发候选**、**实机反馈**和**公开再分发审批**。冻结 25 个正式 W1–W9 任务、原有 C++17 合同及 CP4 既有现代教学内容；不扩大课程范围。

## 1. 已验证交付版本

已验证功能基线 #102 已合并：[9ce07bd25d7a26800275ab286c10e8f95394e08b](https://github.com/clonetimer/CppAppLab/commit/9ce07bd25d7a26800275ab286c10e8f95394e08b)。以下是可追溯的、已实际验证的固定下载基线，不是未来构建的自引用哈希，也不是正式 Release。若正在使用更新候选包，以该包随附的 SHA256SUMS、BUILD-INFO.txt 及 package-manifest.json 为准。

- [下载内部 Windows x64 CI 候选包](https://github.com/clonetimer/CppAppLab/actions/runs/46040159740/artifacts/15218809597)：需要访问权限，artifact 临时保存，到期时间 2026-12-11 17:21:08 UTC。
- 实际测试合并提交 `52b11e0e46a61b8e968f3c446370d6f573c5746b`；内层 ZIP：`CppAppLab-Windows-x64-v0.1.0-52b11e0e46a6-candidate.zip`。
- 内层 ZIP SHA-256：`2191860f0312e11fef047e9e2841c717729afa19532e92c06eed7ad0f36f8486`。外层 GitHub artifact ZIP SHA-256：`a378c120de7fe19bf7e0945a8f624d46a1797428617f66093bd618c694ea9228`，二者不可混用。
- [Windows 真实解压及跨平台验收 46040159740](https://github.com/clonetimer/CppAppLab/actions/runs/46040159740)：40,621 文件逐项校验，63 项固定签名依赖，101 项基础合同、20 个现代对照变体、15 项现代工程测试；Windows 实际导出文件在两版 Ubuntu 各通过 101 项真实重新验收。无 PASS 转移。
- [完整主 CI 46040159768](https://github.com/clonetimer/CppAppLab/actions/runs/46040159768)：14/14 适用 jobs 通过（push-only job 不适用）。[Windows 基础 46040159690](https://github.com/clonetimer/CppAppLab/actions/runs/46040159690) 通过。
- [实际源码交换证据](https://github.com/clonetimer/CppAppLab/actions/runs/46040159740/artifacts/15218844761)：合成 CI 学员的 5 课 13 个声明源码文件；内层 JSON SHA-256 `427377623cdfaa42cd874381e9592cfd73160639a635b1981f0eecbe7a3cb31a`。两版 Ubuntu 日志确认相同哈希，独立下载复核全部文件哈希与 ZIP CRC。它不是个人学员数据或许可凭证。
- [源码/许可证据报告](https://github.com/clonetimer/CppAppLab/actions/runs/46040159740/artifacts/15220565781)：59/59 签名源码归档覆盖 63/63 二进制包，63 项均有声明许可，14 项没有检出包自有 notice（需要人工核实共享许可或补充文本，不等于已证实违法），嵌入式 Python 对应源码哈希通过。报告外层 ZIP 已独立核对；不是公开再分发许可。

#103 首个文档 head 的三套工作流也通过（46040372883、46040372797、46040372788）。本轮只更新已验证身份、交付文档及随包声明，不修改教学合同；更新后的最终打包和 main 验收尚需通过。真实 WSL2 启动、普通账户断网及真人评价仍 NOT RUN。

## 2. 学员的使用路径

| 学习内容 | Windows 免安装包 | Ubuntu/WSL2 |
|---|---|---|
| W1–W4（5 个正式任务） | 原生 CI 已通过；普通用户实机待验证 | 两版 Ubuntu 保留 |
| 10 组 C++20/23 对照实验及两个独立工程 | 随包 GCC 16.2.0 配置已通过 | Ubuntu 24.04 GCC 13 配置保留 |
| W5–W9（20 个 Linux 任务） | 教材可读，禁止原生执行，按需进入已配置 WSL2 | 真 Linux APIs 和原合同 |
| 独立经典知识实验 | 只读，尚未确认原生执行 | 既有 Linux 能力 |

首次在 Windows 学习：取得内部候选 artifact，解开外层 ZIP；用 PowerShell `Get-FileHash -Algorithm SHA256 -LiteralPath '<内层ZIP完整路径>'` 对照上文或同包 SHA256SUMS；完整解压内层到普通用户可写的英文目录，如 `D:\CppAppLab`（可含空格）。双击包内 `Start-CppAppLab.cmd`，浏览器选择“继续学习”，按教材编辑、构建、运行和正式验收。不要求先安装 WSL、GCC、Python 或 CMake。W2–W4 需要创建真实文件符号链接的权限；无权限不能跳过验收。首次断网可使用基础编辑器。源码构建目录中同名启动器属于原有 WSL2 入口，请勿混用。

需要 W5–W9 时，提前手动准备 Ubuntu 22.04/24.04 WSL2、已构建且目录匹配的 `~/work/CppAppLab`，点击“进入 WSL2 学习”。启动仅按需，不自动安装或修改代码。Linux 前置课程必须获得 Linux 本地验收；现有 Linux 学习成果不覆盖，Windows PASS 不迁移。已通过 CI 的源码交换功能：从 Windows 显式导出源码 JSON，在 Linux 新的已解锁基础课程内逐课导入并重新正式验收。

## 3. 实机验收反馈表（目前全部 NOT RUN）

在普通用户 Windows 11 x64 上使用临时测试账户或备份后的独立学习目录。真实学员数据不要上传到仓库公开 issue；截图或日志应去除敏感路径/用户名。分别记录包的源码提交、内层 ZIP 哈希、Windows 版本、账户权限、开发者模式、网络和 WSL 版本。测试用例不要求删除已安装的 WSL 或降低系统安全设置；最好使用没有 WSL 的独立目标机。

| 编号 | 操作与证据 | 合格条件 | 实机结果 |
|---|---|---|---|
| G01 | 哈希校验；完整解压；双击；--verify-tools | 无外部学习工具；网页启动；真实 GCC 编译/链接/运行探针 | NOT RUN |
| G02 | 普通账户、无 WSL 或未启用 WSL；实际断网；W1 编译与测试 | 本地编辑、真实运行、正式 PASS；不触发 WSL | NOT RUN |
| G03 | 真实符号链接权限不足时进行 W2–W4，再由用户按需配置允许的权限 | 明确诊断；不跳过安全测试；具备权限后 19/31/41 固定合同通过 | NOT RUN |
| G04 | 安装目录有空格，中文学员路径；关闭/重开包、修改通过源码 | 真实进度恢复；修改后 STALE；旧历史保留；不覆盖工作区 | NOT RUN |
| G05 | 随包 10 组现代实验及两工程，至少记录各标准真实失败/修复、正式验收 | 对照运行真实诊断，工程 7/7、8/8；经典记录不变 | NOT RUN |
| G06 | 已准备 Ubuntu WSL2，从 Windows 选择 W5，观察目标实例、课程及浏览器回退 | 同一 Linux 服务、前置正确、返回原生；无错误端口导航 | NOT RUN |
| G07 | 导出真实学员源码；Linux 逐课导入、重验收、尝试重复导入 | 不导入 PASS、不覆盖；真实 Linux 101/101 后 W5 解锁 | NOT RUN |
| G08 | 真人首次使用及中断恢复，记录独立完成率、耗时、错误恢复、困难点 | 记录真实观察，不以 CI 结果代替学员理解；未定量阈值前仅报告描述性结果 | NOT RUN |

建议反馈格式：`G编号 / PASS、FAIL、NOT RUN / 包SHA-256 / 实际操作 / 预期与观察 / 截图或日志路径`。未执行不要标记 PASS。

## 4. 最终收口及发布门槛

技术收口：#102 的 Windows ZIP/Chromium、双 Ubuntu 和同一实际导出 artifact 重验收全通过，已合并且合并树与已验收 head 一致。#103 本次交付文档与随包元数据更新仍需最终 CI 和 main 验收。本手册引用固定已验证基线，不需要为每次文档构建追逐其自身生成的 ZIP 哈希。

发布审批：仓库根目录目前缺少明确项目许可证；所有者决定项目许可，不擅自选择。第三方归档证据已采集，但对应源码完整性、版权文本与例外、嵌入依赖、长期源代码提供义务及分发方式须逐项确认。参见 [D3 证据与边界](CP7_WINDOWS_REDISTRIBUTION.md)。不得把内部候选直接标成 public Release。

真实 Windows 11 / WSL2 和真人学习效果按用户约定留给后期反馈。无实机结果只声明真实 CI 范围，不虚构全环境支持、完全断网实测或学习成效。
