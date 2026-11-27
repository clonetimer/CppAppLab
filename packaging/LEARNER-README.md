# CppAppLab learner package

This package is a relocatable, prebuilt learner distribution for Ubuntu @TARGET_UBUNTU_VERSION@ LTS x86_64, including Ubuntu @TARGET_UBUNTU_VERSION@ under WSL2.

## 开始学习 / Start learning

在 WSL2 的 Ubuntu 或 Ubuntu 桌面终端中进入解压后的目录，执行：

~~~bash
./start-web-workbench
~~~

打开终端显示的 http://127.0.0.1:8765/，即可使用中文网页工作台。无需构建 CppAppLab 平台本身。保持终端运行，按 Ctrl+C 停止服务。若端口已被占用，先停止旧服务，或使用 `CPPAPPLAB_WEB_PORT=8766 ./start-web-workbench`。网页仅监听本机；不要将本地服务暴露到网络。断网时 Monaco 编辑器 CDN 不可用则使用内置基础编辑器。

Open the printed local URL in your browser. The server runs in the foreground; Ctrl+C stops it. The prebuilt engine, Python standard-library service and browser UI are included. No platform rebuild or Node.js installation is needed. GCC and the task-specific build tools are still needed for learner code. C++20/23 optional labs still require the separately supported GCC 13 profile.

For the CLI instead, from this directory:

~~~bash
./cppapplab
~~~

The interactive home lets you continue learning, search tasks by number/ID/title, and review progress. To start or resume a task directly:

~~~bash
./cppapplab learn
./cppapplab learn w1.hello-world
~~~

The package wrapper sets CppAppLab's data directory to this extracted folder, so the package can be moved without rebuilding the platform executable.

## System tools required for task acceptance

Install the Ubuntu packages once:

~~~bash
sudo apt update
sudo apt install -y build-essential cmake git gdb libgtest-dev python3
~~~

Then run the environment check:

~~~bash
bash scripts/check_environment.sh
~~~

The prebuilt CppAppLab CLI is included. GCC, CMake and GoogleTest are still required because learner acceptance fixtures compile the learner's C++ source on the local machine. GDB is used by the environment and debugging lessons.

## What is included

- Prebuilt CppAppLab engine for Ubuntu @TARGET_UBUNTU_VERSION@ x86_64, CLI and local browser workbench
- Course catalog, lessons, progressive hints and platform-owned acceptance fixtures
- Learner workspace templates and platform reference baselines required by multi-stage acceptance fixtures
- Environment checker and first-use documentation

Reference baselines remain separate from learner templates. The learn command copies only the task's declared learner template into a workspace; reference baselines are not copied into learner workspaces.

## Persistent data

- Learner workspaces: ~/cppapplab-work/
- Progress and acceptance history: the user's CppAppLab state directory under the home directory (or CPPAPPLAB_STATE_DIR when explicitly set)

These directories are outside the extracted package. Replacing the package does not overwrite learner workspaces or progress.

## Package identity

See BUILD-INFO.txt for the exact source commit and build target. Verify the archive against SHA256SUMS before extraction.
