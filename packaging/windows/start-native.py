"""Portable native Windows launcher. No installer, WSL or external tool lookup."""
from __future__ import annotations

import argparse
import ctypes
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def ascii_tool_path(path: Path) -> Path | None:
    def suitable(value: Path) -> bool:
        return all(32 <= ord(ch) <= 126 and ch != ";" for ch in str(value))

    if suitable(path):
        return path
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    short_name = kernel.GetShortPathNameW
    short_name.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_uint32]
    short_name.restype = ctypes.c_uint32
    needed = short_name(str(path), None, 0)
    if needed:
        buffer = ctypes.create_unicode_buffer(needed)
        copied = short_name(str(path), buffer, needed)
        if 0 < copied < needed:
            alias = Path(buffer.value)
            if suitable(alias) and alias.is_absolute() and os.path.samefile(path, alias):
                return alias
    return None


def configure(root: Path, web) -> tuple[Path, Path]:
    if os.name != "nt" or struct.calcsize("P") != 8:
        raise RuntimeError("This package requires 64-bit Windows.")
    binary = root / "bin/cppapplab.exe"
    runtime = root / "runtime/python.exe"
    prefix = root / "toolchain"
    for path in (root, binary, runtime, prefix, root / "course/tasks.tsv"):
        web._validate_native_path_chain(path)
    if not os.path.samefile(sys.executable, runtime):
        raise RuntimeError("Launch with the bundled runtime/python.exe.")
    for name in ("g++.exe", "gcc.exe", "cmake.exe", "ctest.exe", "ninja.exe", "gdb.exe"):
        if not (prefix / "bin" / name).is_file():
            raise RuntimeError(f"Incomplete bundled toolchain: {name}")
    tools = ascii_tool_path(prefix)
    if tools is None:
        raise RuntimeError("请将程序包解压到可写的英文目录（可含空格），例如 D:\\CppAppLab。"
                           "用户名、学员工作区和学习记录仍可使用中文。"
                           "MinGW toolchain needs an ASCII path or an existing verified short alias.")

    # Do not inherit unrelated host compilers, CMake packages, Python installs,
    # include/library search paths, or user-set compiler launcher flags.
    clear_exact = {
        "CC", "CXX", "CPP", "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS",
        "CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
        "INCLUDE", "LIB", "LIBPATH", "LIBRARY_PATH", "COMPILER_PATH",
        "GCC_EXEC_PREFIX", "MAKEFLAGS", "MFLAGS", "GTEST_ROOT",
        "GTest_DIR".upper(), "MSYSTEM", "MSYS", "MINGW_PREFIX"
    }
    for key in tuple(os.environ):
        upper = key.upper()
        if upper in clear_exact or upper.startswith(
                ("CMAKE_", "CTEST_", "GTEST_", "PYTHON", "PKG_CONFIG_", "MSYS2_")):
            os.environ.pop(key, None)
    system = Path(os.environ.get("SystemRoot", r"C:\Windows"))
    os.environ["PATH"] = os.pathsep.join((str(tools / "bin"), str(system / "System32"), str(system)))
    os.environ["CMAKE_PREFIX_PATH"] = str(tools)
    os.environ["CPPAPPLAB_DATA_DIR"] = str(root)
    os.environ["CPPAPPLAB_LAUNCH_COMMAND"] = str(root / "cppapplab.cmd")

    override = os.environ.get("CPPAPPLAB_NATIVE_BUILD_ROOT")
    cache = Path(override).absolute() if override else Path(tempfile.gettempdir()) / "CppAppLab-native-build"
    web._validate_native_path_chain(cache, allow_missing=True)
    cache.mkdir(parents=True, exist_ok=True)
    web._validate_native_path_chain(cache)
    ascii_cache = ascii_tool_path(cache)
    if ascii_cache is None and not override:
        # A writable package directory is the explicit second choice; never
        # create a global system folder or require elevation.
        cache = root / "build-cache"
        web._validate_native_path_chain(cache, allow_missing=True)
        cache.mkdir(exist_ok=True)
        web._validate_native_path_chain(cache)
        ascii_cache = ascii_tool_path(cache)
    if ascii_cache is None:
        raise RuntimeError("无法建立英文编译缓存目录，请设置 CPPAPPLAB_NATIVE_BUILD_ROOT "
                           "为当前用户可写的本地英文目录。")
    temp = ascii_cache / "tmp"
    web._validate_native_path_chain(temp, allow_missing=True)
    temp.mkdir(exist_ok=True)
    web._validate_native_path_chain(temp)
    os.environ["CPPAPPLAB_NATIVE_BUILD_ROOT"] = str(ascii_cache)
    os.environ["TEMP"] = os.environ["TMP"] = str(temp)
    # Reset Python's cached temp discovery after setting this child-only path.
    tempfile.tempdir = None
    return binary, tools


def symlink_capability() -> bool:
    with tempfile.TemporaryDirectory(prefix="cppapplab-link-check-") as directory:
        target = Path(directory) / "target.txt"
        link = Path(directory) / "link.txt"
        target.write_text("probe", encoding="ascii")
        try:
            os.symlink(target, link, target_is_directory=False)
            return link.is_symlink() and link.read_text(encoding="ascii") == "probe"
        except OSError:
            return False


def verify_tools(binary: Path, prefix: Path) -> None:
    target = subprocess.check_output([str(prefix / "bin/g++.exe"), "-dumpmachine"],
                                     text=True, timeout=10).strip()
    if target != "x86_64-w64-mingw32":
        raise RuntimeError("Bundled compiler target is not native Windows x64.")
    print("Compiler target:", target, flush=True)
    for command in [binary, *(prefix / "bin" / name for name in
                              ("g++.exe", "cmake.exe", "ctest.exe", "ninja.exe", "gdb.exe"))]:
        output = subprocess.check_output([str(command), "--version"], timeout=30)
        print(command.name + ": " + output.decode("utf-8", errors="replace").splitlines()[0],
              flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="CppAppLab native Windows portable candidate")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--no-browser", action="store_true")
    parser.add_argument("--verify-tools", action="store_true")
    parser.add_argument("--cli", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    root = Path(__file__).absolute().parent
    # Official embeddable Python uses isolated ._pth mode. Explicitly insert
    # only this package's existing server directory; do not enable site/pip.
    sys.path.insert(0, str(root / "web/server"))
    import cppapplab_web as web
    try:
        binary, prefix = configure(root, web)
        if args.cli is not None:
            return subprocess.call([str(binary), *args.cli], cwd=root)
        print("CppAppLab Windows 原生候选包", flush=True)
        if not symlink_capability():
            print("注意：当前账户不能创建真实文件符号链接。W1 可用；"
                  "W2–W4 完整验收需要 Windows 开发者模式或相应权限。"
                  "本程序不会修改系统设置。", flush=True)
        if args.verify_tools:
            verify_tools(binary, prefix)
            return 0
        options = ["--root", str(root), "--cppapplab", str(binary),
                   "--host", "127.0.0.1", "--port", str(args.port)]
        if not args.no_browser:
            options.append("--open-browser")
        return web.main(options)
    except (OSError, RuntimeError, web.ApiError, subprocess.SubprocessError) as exc:
        print(f"[FAIL] {exc}", file=sys.stderr, flush=True)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
