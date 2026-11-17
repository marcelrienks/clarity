#!/usr/bin/env python3
"""Test event check (restructure.md 2.9): every '[T] ...' string a Wokwi scenario waits for must be printable by a
log_t() call in the firmware sources.

A wait-serial string matches a log_t format when the format, with each printf conversion standing for any text, can
produce a line that starts with the awaited text (Wokwi matches the start of a line).

Usage: tools/check_test_events.py [--ignore PATH ...]
  --ignore PATH  leave a source file out, to check that code about to be deleted is not the only emitter
"""

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE_DIRS = ("src", "include")
SCENARIOS = "test/wokwi/scenario_automated_*.yaml"

WAIT_SERIAL = re.compile(r"""^\s*-\s*wait-serial:\s*(['"])(.*)\1\s*$""")
STRING_LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')
CONVERSION = re.compile(r"%[-+ #0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|h|ll|l|z|j|t)?[diouxXeEfgGcsp]|%%")


def awaited_strings():
    """Returns {text: [(file, line)]} for every '[T] ' wait-serial in the scenarios, without the prefix."""
    found = {}
    for path in sorted(ROOT.glob(SCENARIOS)):
        for number, line in enumerate(path.read_text().splitlines(), 1):
            match = WAIT_SERIAL.match(line)
            if match and match.group(2).startswith("[T] "):
                text = match.group(2)[len("[T] "):]
                if match.group(1) == "'":
                    text = text.replace("''", "'")
                found.setdefault(text, []).append((path.relative_to(ROOT), number))
    return found


def strip_comments(source):
    """Removes // and /* */ comments (the logging header documents log_t with example calls), keeping literals."""
    token = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:[^"\\\n]|\\.)*"|\'(?:[^\'\\\n]|\\.)*\'', re.DOTALL)
    return token.sub(lambda match: " " if match.group().startswith("/") else match.group(), source)


def first_argument(source, start):
    """Returns the first argument of the call whose '(' is at 'start', skipping string and char literals."""
    depth = 0
    i = start
    while i < len(source):
        char = source[i]
        if char in "\"'":
            end = source.index(char, i + 1)
            while source[end - 1] == "\\" and source[end - 2] != "\\":
                end = source.index(char, end + 1)
            i = end
        elif char in "([{":
            depth += 1
        elif char in ")]}" or (char == "," and depth == 1):
            depth -= 1
            if depth == 0:
                return source[start + 1:i]
        i += 1
    return ""


def log_formats(ignored):
    """Returns [(format, file)] for every string literal (adjacent literals joined) in log_t()'s format argument."""
    formats = []
    for directory in SOURCE_DIRS:
        for path in sorted((ROOT / directory).rglob("*")):
            if path.suffix not in (".h", ".cpp") or path.relative_to(ROOT).as_posix() in ignored:
                continue
            source = strip_comments(path.read_text())
            for call in re.finditer(r"\blog_t\s*\(", source):
                arguments = first_argument(source, call.end() - 1)
                # Adjacent literals concatenate; anything else between them separates two formats (a ternary)
                pieces = re.split(r'"\s*"', arguments)
                for literal in STRING_LITERAL.findall('"'.join(pieces)):
                    formats.append((bytes(literal, "utf-8").decode("unicode_escape"), path.relative_to(ROOT)))
    return formats


def format_pattern(fmt):
    """Regex for the lines a printf format can produce."""
    parts = []
    last = 0
    for conversion in CONVERSION.finditer(fmt):
        parts.append(re.escape(fmt[last:conversion.start()]))
        parts.append("%" if conversion.group() == "%%" else ".*")
        last = conversion.end()
    parts.append(re.escape(fmt[last:]))
    return "".join(parts)


def can_start_with(fmt, text):
    """True if some output of 'fmt' starts with 'text'."""
    # 'text' may end anywhere in the output: try each prefix of the format that does not split a conversion and has
    # literal text (a bare leading "%s" would match every awaited string)
    inside_conversion = set()
    for conversion in CONVERSION.finditer(fmt):
        inside_conversion.update(range(conversion.start() + 1, conversion.end()))
    cuts = [cut for cut in range(1, len(fmt) + 1)
            if cut not in inside_conversion and CONVERSION.sub("", fmt[:cut])]
    return any(re.fullmatch(format_pattern(fmt[:cut]), text, re.DOTALL) for cut in cuts)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--ignore", action="append", default=[], help="source file to leave out (repo-relative)")
    args = parser.parse_args()

    awaited = awaited_strings()
    formats = log_formats(set(args.ignore))
    missing = {text: where for text, where in awaited.items() if not any(can_start_with(f, text) for f, _ in formats)}

    for text, where in sorted(missing.items()):
        locations = ", ".join(f"{path}:{line}" for path, line in where)
        print(f"check_test_events: no log_t for '[T] {text}' ({locations})", file=sys.stderr)
    if missing:
        return 1
    print(f"check_test_events: OK ({len(awaited)} awaited strings, {len(formats)} log_t formats)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
