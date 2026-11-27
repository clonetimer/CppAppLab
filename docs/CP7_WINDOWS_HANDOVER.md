# CP7-G · Windows 原生与双环境交付手册（候选收口）

更新：2026-11-27。本文区分**真实 CI 验收**、**开发候选**、**实机反馈**和**公开再分发审批**。冻结 25 个正式 W1–W9 任务、原有 C++17 合同及 CP4 既有现代教学内容；不扩大课程范围。

## 1. 已验证交付版本

当前已合并的代码基线 #101：[3fef88433eb2a8b4f91f4cda4b212f65fc9249a5](https://github.com/clonetimer/CppAppLab/commit/3fef88433eb2a8b4f91f4cda4b212f65fc9249a5)。实际验收包来自该 PR 的测试合并提交 `136379506cdd2a3a4b71b1f0314b18184d5e2d88`，不要混称为正式发布或 main 构建包。

- [下载内部 Windows x64 CI 候选包](https://github.com/clonetimer/CppAppLab/actions/runs/46016954035/artifacts/15206159695)：需要访问权限，artifact 临时保存，当前到期时间 2026-12-11 14:39:12 UTC。
- 内层 ZIP：`CppAppLab-Windows-x64-v0.1.0-136379506cdd-candidate.zip`。
- 内层 ZIP SHA-256：`9c1b5ad60aaf8a8a1836caa2997d8ef7af0be6d505b7877d747d05f714e01481`。外层 GitHub artifact ZIP SHA-256：`a1c2adabfa31bd1d71d4ce00ec6755190f7b97c0933b2dfd0ea400fb54a0704a`，二者不可混用。
- [Windows 真实解压验收 46016954035](https://github.com/clonetimer/CppAppLab/actions/runs/46016954035)：40,619 文件逐项校验、63 项固定签名依赖、101 项基础合同、20 个现代对照变体、15 项现代工程测试。
- [完整主 CI 46016954066](https://github.com/clonetimer/CppAppLab/actions/runs/46016954066)：14/14 适用 jobs 通过（push-only job 不适用）。[Windows 基础 46016954268](https://github.com/clonetimer/CppAppLab/actions/runs/46016954268) 通过。
- [源码证据报告](https://github.com/clonetimer/CppAppLab/actions/runs/46016954035/artifacts/15206597625)：59/59 签名源码归档覆盖 63/63 二进制包；不是公开再分发许可。

#102 的源码复用不属于上述已验证 ZIP。此前 Linux 源码导入的 101 项重新验收在两版 Ubuntu 通过，但 Windows 导出因真实指纹格式解析错误返回 409。修正后的 [9726f572](https://github.com/clonetimer/CppAppLab/commit/9726f572744bb4ac9bed07b263bb0d2418f11336) 加入实际 Windows 导出 artifact → 两版 Ubuntu 的同工作流验证，当前仍待最终 CI。不得用旧 run 代替新 head 的验收。

## 2. 学员的使用路径

| 学习内容 | Windows 免安装包 | Ubuntu/WSL2 |
|---|---|---|
| W1–W4（5 个正式任务） | 原生 CI 已通过；普通用户实机待验证 | 两版 Ubuntu 保留 |
| 10 组 C++20/23 对照实验及两个独立工程 | 随包 GCC 16.2.0 配置已通过 | Ubuntu 24.04 GCC 13 配置保留 |
| W5–W9（20 个 Linux 任务） | 教材可读，禁止原生执行，按需进入已配置 WSL2 | 真 Linux APIs 和原合同 |
| 独立经典知识实验 | 只读，尚未确认原生执行 | 既有 Linux 能力 |

首次在 Windows 学习：取得内部候选 artifact，解开外层 ZIP；用 PowerShell `Get-FileHash -Algorithm SHA256 -LiteralPath '<内层ZIP完整路径>'` 对照上文或同包 SHA256SUMS；完整解压内层到普通用户可写的英文目录，如 `D:\CppAppLab`（可含空格）。双击包内 `Start-CppAppLab.cmd`，浏览器选择“继续学习”，按教材编辑、构建、运行和正式验收。不要求先安装 WSL、GCC、Python 或 CMake。W2–W4 需要创建真实文件符号链接的权限；无权限不能跳过验收。首次断网可使用基础编辑器。源码构建目录中同名启动器属于原有 WSL2 入口，请勿混用。

需要 W5–W9 时，提前手动准备 Ubuntu 22.04/24.04 WSL2、已构建且目录匹配的 `~/work/CppAppLab`，点击“进入 WSL2 学习”。启动仅按需，不自动安装或修改代码。Linux 前置课程必须获得 Linux 本地验收；现有 Linux 学习成果不覆盖，Windows PASS 不迁移。#102 通过后才能使用源码交换功能：从 Windows 显式导出源码 JSON，在 Linux 新的已解锁基础课程内逐课导入并重新正式验收。

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
| G07 | #102 最终通过后，导出真实学员源码；Linux 逐课导入、重验收、尝试重复导入 | 不导入 PASS、不覆盖；真实 Linux 101/101 后 W5 解锁 | NOT RUN |
| G08 | 真人首次使用及中断恢复，记录独立完成率、耗时、错误恢复、困难点 | 记录真实观察，不以 CI 结果代替学员理解；未定量阈值前仅报告描述性结果 | NOT RUN |

建议反馈格式：`G编号 / PASS、FAIL、NOT RUN / 包SHA-256 / 实际操作 / 预期与观察 / 截图或日志路径`。未执行不要标记 PASS。

## 4. 最终收口及发布门槛

技术收口：#102 最终 head 的 Windows ZIP/Chromium、两版 Ubuntu 和实际导出 artifact 跨平台重验收全通过，核实合并树与验收树一致，更新候选包身份，再对最终 main 做验收。此文档是交付准备，并非完成登记。

发布审批：仓库根目录目前缺少明确项目许可证；所有者决定项目许可，不擅自选择。第三方归档证据已采集，但对应源码完整性、版权文本与例外、嵌入依赖、长期源代码提供义务及分发方式须逐项确认。参见 [D3 证据与边界](CP7_WINDOWS_REDISTRIBUTION.md)。不得把内部候选直接标成 public Release。

真实 Windows 11 / WSL2 和真人学习效果按用户约定留给后期反馈。无实机结果只声明真实 CI 范围，不虚构全环境支持、完全断网实测或学习成效。
