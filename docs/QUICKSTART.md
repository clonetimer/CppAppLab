# First-use quick start

This guide is for a learner using Windows 11 + WSL2 + Ubuntu 22.04 LTS or Ubuntu 24.04 LTS. You do not need to know the task IDs or choose a workspace path to begin.

## 1. Open Ubuntu and get the project

In PowerShell, list installed distributions and start your existing Ubuntu 22.04 or Ubuntu 24.04 distribution:

~~~powershell
wsl -l -v
wsl -d Ubuntu
~~~

Use the exact distribution name shown by `wsl -l -v` (for example, `wsl -d Ubuntu-22.04` or `wsl -d Ubuntu-24.04`). The distribution must report WSL version 2.

Inside Ubuntu:

~~~bash
mkdir -p ~/work
cd ~/work
git clone https://github.com/clonetimer/CppAppLab.git
cd CppAppLab
~~~

## 2. Run the one-command setup

~~~bash
bash scripts/quickstart.sh
~~~

The script:

1. Checks that Ubuntu 22.04 or 24.04 and the required toolchain are available.
2. Offers to install missing packages with apt (you approve before installation).
3. Configures and builds CppAppLab.
4. Runs the platform's CTest suite.
5. Opens the interactive learning home, where you can continue, search for a task or review progress.

The script does not delete or replace learner workspaces. It uses the default CppAppLab build directory and may reuse its build cache.

### Windows one-click launcher

If your checkout is inside WSL, run explorer.exe . from the repository root in Ubuntu. Windows Explorer opens the project folder; double-click Start-CppAppLab.cmd there. Alternatively, keep a Windows-accessible checkout of the private repository and double-click the same file from its root.

You can also run the launcher from PowerShell:

~~~powershell
.\Start-CppAppLab.cmd
~~~

The launcher detects installed Ubuntu distributions, verifies WSL2 and supports Ubuntu 22.04 and 24.04 source checkouts. If ~/work/CppAppLab does not exist inside WSL, it clones the private repository there and runs the first-time setup. If more than one Ubuntu distribution is installed, it asks you to choose one. The bootstrap is transferred through a temporary file rather than stdin, so the first-use package confirmation and sudo password prompt remain connected to the WSL terminal.

This Windows launcher uses the source checkout and builds it inside the selected distribution, so it supports both Ubuntu 22.04 and 24.04. The published prebuilt learner package and offline dependency bundle are built separately for the selected Ubuntu 22.04 or Ubuntu 24.04 target. Use the archive whose filename matches your WSL distribution.

The first clone requires GitHub authentication inside WSL. Configure Git credentials there; for example, install GitHub CLI and run gh auth login inside Ubuntu before retrying. The launcher checks the checkout origin and refuses to run an unrelated repository.

After pulling project updates inside WSL, rebuild and rerun acceptance checks with:

~~~powershell
.\Start-CppAppLab.cmd -Setup
~~~

The launcher deliberately does not run git pull; local changes are preserved. It can also be pointed at another Linux checkout with -RepoPath "~/projects/CppAppLab" or a specific distro with -Distro Ubuntu-22.04.

## 3. Start learning

For an interactive home screen, run:

~~~bash
./build/cppapplab
~~~

Choose Continue learning, browse tasks by number, or search by task ID or title keyword. The task list shows PASSED, IN PROGRESS, NEEDS WORK, STALE, AVAILABLE and LOCKED states. If you prefer to skip the menu, run the guided command directly:

~~~bash
./build/cppapplab learn
~~~

It automatically chooses the next unlocked task, creates a workspace under ~/cppapplab-work/, displays the task instructions, and prints the exact commands to request a hint and run the acceptance check.

To resume an incomplete task later, run the same command again. After the current task passes, the command automatically advances to the next unlocked task:

~~~bash
./build/cppapplab learn
~~~

To start or revisit a specific task:

~~~bash
./build/cppapplab learn w1.calculator
~~~

A task with an unmet prerequisite remains locked. The command reports which prerequisite must pass first.

## 4. The normal learning loop

1. Open the displayed workspace in your editor (VS Code users can use the printed code command).
2. Edit the files listed by the task guide.
3. If stuck, request a progressive hint (levels 1–3).
4. Run `cppapplab build <workspace>` to compile and inspect compiler diagnostics.
5. Run `cppapplab run <workspace>` to execute the direct learner program or the registered CTest suite. This action does not record formal evidence.
6. Run `cppapplab check <workspace>` for the platform-owned acceptance contract. A PASS is recorded only by this command.
7. After PASS, run ./build/cppapplab learn to continue, or return to the home menu with ./build/cppapplab.

Your source files, progress and check history are kept between sessions. A changed source file makes the previous PASS evidence stale; rerun the check after editing.

## 5. Useful commands

| Command | Purpose |
|---|---|
| ./build/cppapplab learn | Start or resume the next unlocked task |
| ./build/cppapplab learn <task-id> | Start or resume a selected task |
| ./build/cppapplab progress | Review all task states and prerequisite locks |
| ./build/cppapplab list | Browse the complete task catalog |
| ./build/cppapplab history <task-id> | Review previous acceptance attempts |
| ./build/cppapplab build <workspace> | Compile with the task's fixed build profile |
| ./build/cppapplab run <workspace> | Run the learner program or fixed test suite (no evidence) |
| ./build/cppapplab check <workspace> | Run formal acceptance and record evidence |
| bash scripts/check_environment.sh | Recheck the development environment |

## Troubleshooting

- Missing packages: rerun bash scripts/quickstart.sh and approve the package installation prompt in the terminal. The prompt reads from the controlling terminal even if stdin was redirected; if no terminal is attached, setup stops without installing anything.
- GoogleTest not found: install Ubuntu's libgtest-dev package; the quick-start script configures CMake to use the system package.
- A task is locked: run ./build/cppapplab progress, then complete the named prerequisite and obtain a current PASS.
- A workspace already exists: the guided command resumes a valid workspace. If the default path belongs to another task or is not a valid workspace, it chooses a numbered sibling path instead of overwriting it.
- WSL2 or Ubuntu version mismatch: follow Environment setup and verify the distribution version in PowerShell with wsl -l -v.

Hosted CI is not a substitute for target-machine acceptance. Windows 11 + WSL2 validation and offline dependency packaging remain separate project work.

## Browser learning workbench (P8-A / P8-B preview)

After building the existing CppAppLab project, start the local browser service from the repository root:

~~~bash
bash scripts/start-web-workbench.sh
~~~

Open http://127.0.0.1:8765/ in your Windows browser. The service binds to loopback only. It delegates workspace lifecycle, compilation profiles, test execution and formal acceptance to the existing CppAppLab learning engine. It does not expose an arbitrary shell endpoint.

Stop the service with:

~~~bash
bash scripts/start-web-workbench.sh stop
~~~

### Browser learning loop

1. Select a course task and start or resume its workspace.
2. Read the lesson and edit the task's declared source files in the multi-file editor.
3. Changes are saved automatically after a short pause; Ctrl+S also saves immediately.
4. Use **Build** to compile the current source.
5. Use **Run / Tests** to execute the learner program for direct-execution tasks or the registered CTest suite for CMake/GoogleTest tasks.
6. Use **Formal Acceptance** to run the platform-owned acceptance contract and record PASS/FAIL evidence.

Build and Run never record formal acceptance evidence. A source edit invalidates the previous build fingerprint; build again before running. Only Formal Acceptance updates the course evidence and prerequisite state.

The enhanced Monaco editor is loaded from the pinned Monaco 0.52.2 CDN package. If the CDN is unreachable, the page falls back to its local plain-text editor; the local Python service and course API do not require third-party Python packages or Node.js.

The browser workbench remains a preview while P8-B CI and browser visual acceptance complete. The existing CLI remains supported and authoritative.
