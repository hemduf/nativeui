#!/usr/bin/env python3
"""Materialize the exact v0.1 release-note CMake snippet as a buildable consumer."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def release_cmake_snippet() -> str:
    notes = (ROOT / "docs/releases/v0.1.0.md").read_text(encoding="utf-8")
    match = re.search(r"```cmake\n(.*?)\n```", notes, flags=re.DOTALL)
    if match is None:
        raise RuntimeError("v0.1.0 release notes contain no CMake code block")
    snippet = match.group(1).strip()
    required = (
        "find_package(NativeUI CONFIG REQUIRED)",
        "add_executable(MyApplication main.cpp)",
        "target_link_libraries(MyApplication PRIVATE NativeUI::Core)",
        "nativeui_attach_platform(",
        "TARGET MyApplication",
        "CONSUMER_ID com.example.my-application",
    )
    for token in required:
        if token not in snippet:
            raise RuntimeError(f"release-note CMake snippet is missing {token!r}")
    return snippet


def prepare(output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    snippet = release_cmake_snippet()
    (output / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.24)\n"
        "project(T052ReleaseDocConsumer LANGUAGES CXX)\n\n"
        + snippet
        + "\n",
        encoding="utf-8",
    )
    (output / "main.cpp").write_text(
        "#include <nativeui/window.hpp>\n\n"
        "int main() {\n"
        "    return 0;\n"
        "}\n",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prepare(args.output.resolve())
    print(f"PASS prepared release-doc consumer at {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
