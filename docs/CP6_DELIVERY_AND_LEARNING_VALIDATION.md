# CP6 — 交付与学习验证清单

日期：2026-11-27  
基线：main `41feb08eadbe6464b7092ca4e3141258a4a22d88`（PR #81 merge）

## 范围冻结

- 不新增课程任务，不扩展 W10–W12，不重开 Windows 原生迁移。
- 不修改 C++17 课程的固定验收、参考实现隔离、源码指纹、历史记录或学员数据。
- 保留真实 GCC、CMake、GoogleTest/CTest、ASan/UBSan 与 clang-tidy 验收。
- 仓库 CP0/CP1 冻结的是原始 23 个任务；当前 main 的 `course/tasks.tsv` 已因 W9-A4/A5 实际扩展为 25 项。报告必须同时保留这两个事实，不得把当前 25 项说成 23 项，也不得掩盖范围变化。
- PR #82（W10 TLS）属于课程扩展，不属于 CP6；本清单不推进、不合并它。

## 证据分层

| 维度 | 已有证据 | 尚缺证据 | 通过条件 |
|---|---|---|---|
| 工程回归 | CP0–CP5 合并记录；PR #81 记录 14 项适用 CI PASS、双 Ubuntu 六配置 21/21、10,000 次实际有界 fuzz | 需要按最终发布候选重新汇总适用 CI 与产物 SHA-256 | 版本、提交、CI、产物校验值可相互追溯 |
| 学习者包 | 已有可重定位包与包内 CLI smoke test 历史 | 当前 main 对应的包需重新生成、下载、校验、解压并运行 | 新目录启动成功；课程、指南、验收命令可用；不覆盖旧工作区 |
| 离线依赖 | 已有离线依赖包生成与 no-install 验证历史 | 对当前候选版本重新核验清单、SHA-256、目标 Ubuntu 版本和 dry-run | 只使用包内依赖完成校验；不得声称目标机已安装成功 |
| Windows/WSL2 目标机 | PowerShell 5.1/7 语法和 launcher handoff 的 hosted CI | 用户真实 Windows 11 + WSL2 + Ubuntu 22.04/24.04 启动、浏览器可见和状态持久化 | 运行 `tests/windows-target-machine-acceptance.ps1`，保留 JSON 与 transcript；用户确认页面可见 |
| 首次学习理解 | 有 Chromium 课程目录、导航、双语、编辑/构建/运行/验收自动化 | 真实零基础学员独立完成任务的观察记录 | 记录卡点、求助次数、错误恢复路径和是否能解释所学；未观察记为未评估 |
| 学习迁移 | 有知识点索引、固定失败/修复实验、C++20/23 独立工程 | 学员在未见过的新小问题中独立迁移概念的结果 | 使用未公开提示的新题；保留原始作答与评分依据；不得用参考答案代替学员表现 |

## 执行顺序

### CP6-A — 冻结发布候选

1. 从 main 选定唯一候选提交。
2. 运行完整适用 CI；标记 PASS、FAIL、SKIPPED，跳过项不得计为通过。
3. 生成源码包、学习者包和离线依赖包。
4. 保存提交 SHA、工具链版本、包名、大小、SHA-256 和 CI run URL。
5. 核对 README、课程导航、注册表数量和状态一致。

### CP6-B — 包与启动验收

1. 在全新临时目录解压学习者包，确认不依赖源码仓库路径。
2. 运行首页、指南、任务浏览、开始/继续学习、Build、Run/Tests、Formal Acceptance。
3. 验证失败后可修复、PASS 后状态更新、源码变化使旧 PASS 失效。
4. 在 Ubuntu 22.04 与 24.04 分别核对对应包，不交叉使用不同发行版二进制。
5. 验证离线依赖包的 checksum、verify-only 和 APT dry-run；不在未授权环境安装系统包。

### CP6-C — Windows 11 + WSL2 目标机

在真实 Windows 主机执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\windows-target-machine-acceptance.ps1
```

如需指定发行版，按脚本帮助选择 Ubuntu 22.04 或 24.04。保留生成的 JSON 证据与 transcript；必须由用户确认浏览器欢迎页确实可见。Hosted Windows CI 只证明脚本语法/命令交接，不等于目标机 PASS。

### CP6-D — 真人学习评估

采用小规模形成性评估，不预设“通过率”或编造参与者数据：

- 首次使用：让参与者从首页独立找到下一关并打开工作区。
- 错误恢复：提供一个真实编译错误，观察能否定位知识点、理解原因并修复。
- 独立实现：选择已发布课程中的一个任务，不提供参考答案。
- 迁移题：提供一个未在教材中出现、但使用同一核心概念的新问题。
- 访谈：让参与者用自己的话解释“学会了什么、证据是什么、还不会什么”。

记录参与者背景（仅必要的非敏感学习经验）、任务耗时、独立完成情况、提示/求助次数、错误类型、原始产物和评分规则。不得把 AI 自评或自动化通过冒充真人评估。

## 交付状态

- CP0–CP4：按已合并记录收口。
- CP5：W9-A4/A5 已合并；A5 本轮 10,000 次有界 fuzz 未发现问题，不构成安全证明。
- CP6-A/B：待对当前候选版本重新生成并核验包与 CI。
- CP6-C：待真实 Windows 11 + WSL2 目标机执行。
- CP6-D：待真实学习者参与；目前不得声明学习效果已验证。
- Release：未发布前不得称正式稳定版。

## 经验记录

- CI 通过、浏览器自动化通过、包可生成、目标机可启动、真人能学会是五种不同证据。
- 跳过的 job 必须单独标记，不能折算为 PASS。
- 课程目录覆盖不等于学员掌握；固定夹具通过不等于迁移能力通过。
- 对 fuzz 结果只报告实际次数、工具链、语料和是否发现问题，不把有限无发现扩大为安全结论。

## CP6-B packaged browser candidate

#83 was merged as `ffea39d984934c3b9d624a62eedce2266f5a3418` after run 45945570547 passed all 14 applicable jobs (one push-only job skipped). Its prebuilt learner packages provided CLI only. This candidate ships the existing web service and static UI beside the prebuilt CLI, with a single foreground loopback entry point. Both Ubuntu package jobs must now run real HTTP W1 starter FAIL, build/run, formal PASS, source staleness and restart persistence from a relocated package, without building the platform again. The existing source Chromium gate remains separate; the package API test is not a new human or browser-DOM qualification. No new course or acceptance contract, no new Node runtime, no native Windows claim. Target-machine and human learning remain NOT RUN.
