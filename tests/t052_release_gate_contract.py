#!/usr/bin/env python3
"""Static and toolchain contract checks for the NativeUI v0.1 release gate."""

from __future__ import annotations

import re
import subprocess
import sys
import tempfile
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


def exercise_hash_mismatch_diagnostic() -> None:
    """Prove the CMake hash primitive used by CPM fails closed on bad bytes."""
    with tempfile.TemporaryDirectory(prefix="nativeui-t052-hash-") as directory:
        root = Path(directory)
        payload = root / "payload.bin"
        payload.write_bytes(b"NativeUI T052 checksum mismatch probe\n")
        destination = root / "downloaded.bin"
        script = root / "probe.cmake"
        script.write_text(
            "file(DOWNLOAD\n"
            f"  \"{payload.as_uri()}\"\n"
            f"  \"{destination.as_posix()}\"\n"
            "  EXPECTED_HASH \"SHA256=" + ("0" * 64) + "\"\n"
            ")\n",
            encoding="utf-8",
        )
        result = subprocess.run(
            ["cmake", "-P", str(script)],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        diagnostic = result.stdout + result.stderr
        require(result.returncode != 0, "CMake unexpectedly accepted a deliberately wrong SHA-256")
        require(
            "HASH mismatch" in diagnostic or "hash mismatch" in diagnostic.lower(),
            "checksum mismatch probe failed without an actionable hash-mismatch diagnostic",
        )


def main() -> int:
    cmake = text("CMakeLists.txt")
    require(
        re.search(r"project\(NativeUI\s+VERSION\s+0\.1\.0\b", cmake) is not None,
        "T052 release candidate must identify the package as NativeUI 0.1.0",
    )

    dependencies = text("cmake/Dependencies.cmake")
    require(
        re.search(r'set\(NATIVEUI_PUGL_COMMIT\s+\"[0-9a-f]{40}\"', dependencies) is not None,
        "Pugl must remain pinned to one exact commit",
    )
    require('set(NATIVEUI_SKIA_TAG "chrome/m149"' in dependencies, "Skia release tag must remain pinned")
    require('URL_HASH "${_skia_hash}"' in dependencies, "Skia CPM acquisition must enforce URL_HASH")
    require(len(re.findall(r'SHA256=[0-9a-f]{64}', dependencies)) >= 5,
            "supported Skia assets must carry explicit SHA-256 pins")
    require_all(
        dependencies,
        (
            "NativeUI/Pugl supports macOS, Windows and Linux/X11",
            "The pinned skia-builder release currently provides Linux x64 only",
            "NATIVEUI_SKIA_WINDOWS_CRT must be MD or MT",
            "publishes the macOS universal artifact as Release",
        ),
        "unsupported dependency diagnostics",
    )
    exercise_hash_mismatch_diagnostic()

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
        ("name: T042 Lifecycle Stress", "Linux X11", "Windows", "macOS", "Linux ASan + UBSan"),
        "T042 lifecycle stress workflow",
    )

    benchmark = text(".github/workflows/t051-benchmarks.yml")
    require_all(
        benchmark,
        ("name: T051 Release Benchmarks", "Run full fixed protocol A", "Run full fixed protocol B on same runner", "t051-release-results"),
        "T051 benchmark workflow",
    )

    release_gate = text(".github/workflows/t052-release-gate.yml")
    require_all(
        release_gate,
        (
            "name: T052 v0.1 Release Gate",
            "T052 release contract",
            "T052 clean bootstrap",
            "T052 exact-head T051 benchmark",
            "t052-release-evidence",
            "t052-v0.1-benchmark-baseline",
            "github.event.pull_request.head.sha || github.sha",
            "BASELINE_SHA: ${{ github.event.pull_request.base.sha || github.sha }}",
            "Checkout benchmark baseline",
            "Capture one approved-base run and two candidate runs",
            "Prepare release-doc consumer from exact snippet",
            "Build release-doc consumer",
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
            "v0.1.0",
            "T071",
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
