# CppAppLab learner package

This package is a relocatable, prebuilt learner distribution for Ubuntu @TARGET_UBUNTU_VERSION@ LTS x86_64, including Ubuntu @TARGET_UBUNTU_VERSION@ under WSL2.

## Start learning

From this directory:

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
sudo apt install -y build-essential cmake git gdb libgtest-dev
~~~

Then run the environment check:

~~~bash
bash scripts/check_environment.sh
~~~

The prebuilt CppAppLab CLI is included. GCC, CMake and GoogleTest are still required because learner acceptance fixtures compile the learner's C++ source on the local machine. GDB is used by the environment and debugging lessons.

## What is included

- Prebuilt CppAppLab CLI for Ubuntu @TARGET_UBUNTU_VERSION@ x86_64
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
