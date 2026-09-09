#!/usr/bin/env python3
"""Static contract checks for the NativeUI v0.1 developer-preview gate.

T052 is deliberately an aggregate validation ticket.  This test keeps the
release contract auditable without embedding GitHub API state in production
code: current-head workflow results are still recorded by the completion
review, while this file verifies that the required gate surfaces and release
claims cannot silently disappear.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def text(path: str) -> str:
    file = ROOT / path
    require(file.is_file(), f"missing required T052 file: {path}")
    return file.read_text(encoding="utf-8")


def require_all(haystack: str, needles: tuple[str, ...], context: str) -> None:
    for needle in needles:
        require(needle in haystack, f"{context} is missing required contract text: {needle!r}")


def main() -> int:
    cmake = text("CMakeLists.txt")
    require(
        re.search(r"project\(NativeUI\s+VERSION\s+0\.1\.0\b", cmake) is not None,
        "T052 release candidate must identify the package as NativeUI 0.1.0",
    )

    normal_ci = text(".github/workflows/ci.yml")
    require_all(
        normal_ci,
        (
            "name: macOS",
            "name: Windows",
            "name: Linux X11",
            "name: Linux ASan + UBSan",
            "T047 external package contract",
            "T048 relocated external consumers",
            "macOS two-consumer Objective-C runtime isolation",
        ),
        "normal CI",
    )

    stress = text(".github/workflows/t042-stress.yml")
    require_all(
        stress,
        (
            "name: T042 Lifecycle Stress",
            "Linux X11",
            "Windows",
            "macOS",
            "Linux ASan + UBSan",
        ),
        "T042 lifecycle stress workflow",
    )

    benchmark = text(".github/workflows/t051-benchmarks.yml")
    require_all(
        benchmark,
        (
            "name: T051 Release Benchmarks",
            "Run full fixed protocol A",
            "Run full fixed protocol B on same runner",
            "t051-release-results",
        ),
        "T051 benchmark workflow",
    )

    release_gate = text(".github/workflows/t052-release-gate.yml")
    require_all(
        release_gate,
        (
            "name: T052 v0.1 Release Gate",
            "T052 release contract",
            "T052 clean bootstrap",
            "t052-release-evidence",
            "github.event.pull_request.head.sha || github.sha",
        ),
        "T052 release workflow",
    )

    release_notes = text("docs/releases/v0.1.0.md")
    release_notes_lower = release_notes.lower()
    require("developer preview" in release_notes_lower, "v0.1.0 must be identified as a developer preview")
    require("public api" in release_notes_lower and "may change" in release_notes_lower,
            "v0.1.0 must state that public APIs may change during 0.x")
    require_all(
        release_notes,
        (
            "macOS",
            "Windows",
            "Linux/X11",
            "find_package(NativeUI CONFIG REQUIRED)",
            "NativeUI::Core",
            "nativeui_attach_platform(",
        ),
        "v0.1.0 release notes",
    )

    print("PASS T052 v0.1 release-gate source contract")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"FAIL T052 release-gate contract: {error}", file=sys.stderr)
        raise SystemExit(1)
