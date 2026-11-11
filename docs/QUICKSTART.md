# First-use quick start

This guide is for a learner using Windows 11 + WSL2 + Ubuntu 24.04. You do not need to know the task IDs or choose a workspace path to begin.

## 1. Open Ubuntu and get the project

In PowerShell, start Ubuntu:

```powershell
wsl -d Ubuntu
```

Inside Ubuntu:

```bash
mkdir -p ~/work
cd ~/work
git clone https://github.com/clonetimer/CppAppLab.git
cd CppAppLab
```

## 2. Run the one-command setup

```bash
bash scripts/quickstart.sh
```

The script:

1. Checks that Ubuntu 24.04 and the required toolchain are available.
2. Offers to install missing packages with `apt` (you approve before installation).
3. Configures and builds CppAppLab.
4. Runs the platform's CTest suite.
5. Starts the guided learning entry point.

The script does not delete or replace learner workspaces. It uses the default CppAppLab build directory and may reuse its build cache.

## 3. Start learning

The guided entry point is:

```bash
./build/cppapplab learn
```

It automatically chooses the next unlocked task, creates a workspace under `~/cppapplab-work/`, displays the task instructions, and prints the exact commands to request a hint and run the acceptance check.

To resume an incomplete task later, run the same command again. After the current task passes, the command automatically advances to the next unlocked task:

```bash
./build/cppapplab learn
```

To start or revisit a specific task:

```bash
./build/cppapplab learn w1.calculator
```

A task with an unmet prerequisite remains locked. The command reports which prerequisite must pass first.

## 4. The normal learning loop

1. Open the displayed workspace in your editor (VS Code users can use the printed `code` command).
2. Edit the files listed by the task guide.
3. If stuck, request a progressive hint (levels 1–3).
4. Run the printed `cppapplab check` command.
5. After PASS, run `./build/cppapplab learn` to continue.

Your source files, progress and check history are kept between sessions. A changed source file makes the previous PASS evidence stale; rerun the check after editing.

## 5. Useful commands

| Command | Purpose |
|---|---|
| `./build/cppapplab learn` | Start or resume the next unlocked task |
| `./build/cppapplab learn <task-id>` | Start or resume a selected task |
| `./build/cppapplab progress` | Review all task states and prerequisite locks |
| `./build/cppapplab list` | Browse the complete task catalog |
| `./build/cppapplab history <task-id>` | Review previous acceptance attempts |
| `bash scripts/check_environment.sh` | Recheck the development environment |

## Troubleshooting

- **Missing packages:** rerun `bash scripts/quickstart.sh` and approve the package installation prompt, or install the listed packages manually.
- **GoogleTest not found:** install Ubuntu's `libgtest-dev` package; the quick-start script configures CMake to use the system package.
- **A task is locked:** run `./build/cppapplab progress`, then complete the named prerequisite and obtain a current PASS.
- **A workspace already exists:** the guided command resumes a valid workspace. If the default path belongs to another task or is not a valid workspace, it chooses a numbered sibling path instead of overwriting it.
- **WSL2 or Ubuntu version mismatch:** follow [Environment setup](ENVIRONMENT_WSL2_UBUNTU.md) and verify the distribution version in PowerShell with `wsl -l -v`.

Hosted CI is not a substitute for target-machine acceptance. Windows 11 + WSL2 validation and offline dependency packaging remain separate project work.
