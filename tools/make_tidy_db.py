#!/usr/bin/env python3
"""Turn PlatformIO's compile_commands.json into one clang-tidy can parse.

PlatformIO's database is written for the xtensa GCC. clang rejects its machine flags, and without the
toolchain's C++ headers it cannot find the standard library. This drops the flags clang does not know, targets
xtensa, and adds the toolchain's C++, GCC and newlib include directories. `-nostdlibinc` keeps the host's own
headers (glibc on a Linux runner) out: newlib's limits.h chains to the next limits.h, which must be GCC's. `-Dfinal=` works around clang rejecting `final` on
BasePanel's function-pointer-returning declarations, which GCC accepts.

Usage: make_tidy_db.py <compile_commands.json in> <directory to write the database to>
"""
import glob
import json
import os
import shlex
import sys

UNSUPPORTED = (
    "-mlongcalls", "-mlong-calls", "-mtext-section", "-mdisable-hardware", "-mfix-esp32",
    "-fno-tree-switch", "-fstrict-volatile", "-freorder", "-fno-shrink", "-fno-jump-tables",
    "-ffunction-sections", "-fdata-sections", "-fno-rtti", "-fno-exceptions",
    "-Wno-frame", "-Wno-old-style", "-Wno-error=", "-Wno-unused-but", "-Wno-format-truncation",
    "-Wno-stringop", "-Wno-unused-const", "-Wno-enum-conversion", "-Wno-type-limits",
)


def toolchain_dir():
    core = os.environ.get("PLATFORMIO_CORE_DIR", os.path.expanduser("~/.platformio"))
    return os.path.join(core, "packages", "toolchain-xtensa-esp-elf")


def main():
    src, out_dir = sys.argv[1:3]
    root = toolchain_dir()
    cxx = sorted(glob.glob(os.path.join(root, "xtensa-esp-elf", "include", "c++", "*")))
    if not cxx:
        sys.exit("make_tidy_db: no xtensa C++ headers under " + root)
    version = cxx[-1]
    gcc_dirs = sorted(glob.glob(os.path.join(root, "lib", "gcc", "xtensa-esp-elf", "*")))
    gcc_includes = [d for g in gcc_dirs[-1:] for d in (os.path.join(g, "include"), os.path.join(g, "include-fixed"))]
    includes = [version, os.path.join(version, "xtensa-esp-elf")] + gcc_includes
    includes.append(os.path.join(root, "xtensa-esp-elf", "include"))

    entries = []
    for entry in json.load(open(src)):
        args = shlex.split(entry["command"]) if "command" in entry else list(entry["arguments"])
        args = [a for a in args if not a.startswith(UNSUPPORTED)]
        args[0] = "clang++"
        args += ["--target=xtensa", "-nostdinc++", "-nostdlibinc", "-Dfinal="]
        for inc in includes:
            args += ["-isystem", inc]
        args += ["-Wno-unknown-warning-option", "-Wno-unknown-attributes"]
        entries.append({"directory": entry["directory"], "file": entry["file"], "arguments": args})

    os.makedirs(out_dir, exist_ok=True)
    json.dump(entries, open(os.path.join(out_dir, "compile_commands.json"), "w"))


if __name__ == "__main__":
    main()
