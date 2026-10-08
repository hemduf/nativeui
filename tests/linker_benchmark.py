#!/usr/bin/env python3
"""Compare Linux executable relinks with the default linker and mold.

A baseline build creates all object files. A timestamp-only library update then
forces a linker-only baseline pass; switching CMake's executable linker flag
forces the experimental pass without changing any compiler inputs.
"""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

LINK_STEP = re.compile(r"^\[\d+/\d+\] Linking (?:CXX|C) executable", re.MULTILINE)
COMPILE_STEP = re.compile(
    r"^\[\d+/\d+\] Building (?:CXX|C|OBJC|OBJCXX) object", re.MULTILINE
)


def classify(log: str) -> dict[str, int]:
    return {
        "executable_links": len(LINK_STEP.findall(log)),
        "compile_steps": len(COMPILE_STEP.findall(log)),
    }


def comparable(reference: dict[str, int], candidate: dict[str, int]) -> bool:
    return (
        reference["executable_links"] > 0
        and reference["executable_links"] == candidate["executable_links"]
        and reference["compile_steps"] == 0
        and candidate["compile_steps"] == 0
    )


def run(command: list[str], *, env: dict[str, str] | None = None) -> tuple[float, str]:
    print("+", " ".join(command), flush=True)
    started = time.perf_counter()
    completed = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        env=env,
        check=False,
    )
    elapsed = time.perf_counter() - started
    if completed.returncode != 0:
        print(completed.stdout, flush=True)
        raise RuntimeError(
            f"Command failed with status {completed.returncode}: {' '.join(command)}"
        )
    lines = completed.stdout.splitlines()
    print(f"Completed in {elapsed:.2f}s; output lines: {len(lines)}", flush=True)
    for line in lines[-5:]:
        print(line, flush=True)
    return elapsed, completed.stdout


def self_test() -> None:
    sample = "\n".join(
        [
            "[1/4] Building CXX object tests/a.cpp.o",
            "[2/4] Linking CXX executable nativeui_unit_a",
            "[3/4] Linking C executable nativeui_unit_b",
            "[4/4] Linking CXX static library libnativeui_core.a",
        ]
    )
    assert classify(sample) == {"executable_links": 2, "compile_steps": 1}
    valid = {"executable_links": 2, "compile_steps": 0}
    assert comparable(valid, valid)
    assert not comparable(valid, {"executable_links": 1, "compile_steps": 0})
    assert not comparable(valid, {"executable_links": 2, "compile_steps": 1})
    assert not comparable({"executable_links": 0, "compile_steps": 0}, valid)
    print("Linker benchmark classification contract: PASS")


def benchmark(build: Path, output: Path) -> None:
    build = build.resolve()
    output = output.resolve()
    if not (build / "build.ninja").is_file():
        raise RuntimeError("Configure the Ninja benchmark before running this script")

    initial_seconds, _ = run(["cmake", "--build", str(build)])
    archives = list(build.rglob("libnativeui_core.a"))
    if len(archives) != 1:
        raise RuntimeError(
            f"Expected a single nativeui_core archive, found {len(archives)}"
        )

    # Only the archive timestamp changes; previously built object files remain
    # byte-for-byte unchanged. All executables depending on Core must relink.
    archives[0].touch()
    serialized = dict(os.environ, CMAKE_BUILD_PARALLEL_LEVEL="1")
    baseline_seconds, baseline_output = run(
        ["cmake", "--build", str(build)], env=serialized
    )
    baseline = classify(baseline_output)

    # Reconfigure only the executable linker flag on the *same* object tree.
    # This compares fresh relink passes rather than a cold compile against a
    # warm link. Recompiled objects invalidate the comparison.
    reconfigure_seconds, _ = run(
        [
            "cmake",
            "-S",
            ".",
            "-B",
            str(build),
            "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=mold",
        ]
    )
    mold_seconds, mold_output = run(
        ["cmake", "--build", str(build)], env=serialized
    )
    mold_stats = classify(mold_output)
    valid = comparable(baseline, mold_stats)

    results = {
        "comparable": valid,
        "initial_build_seconds": round(initial_seconds, 3),
        "mold_reconfigure_seconds": round(reconfigure_seconds, 3),
        "default": {"seconds": round(baseline_seconds, 3), **baseline},
        "mold": {"seconds": round(mold_seconds, 3), **mold_stats},
        "relative_link_time": (
            round(mold_seconds / baseline_seconds, 4)
            if valid and baseline_seconds > 0
            else None
        ),
    }
    output.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")

    print(json.dumps(results, indent=2), flush=True)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with Path(summary).open("a", encoding="utf-8") as handle:
            handle.write(
                "\n## NativeUI Linux linker benchmark\n\n"
                f"- Comparable relink passes: **{'yes' if valid else 'no'}**\n"
                f"- Default linker: **{baseline_seconds:.2f}s**, "
                f"{baseline['executable_links']} executable links\n"
                f"- mold: **{mold_seconds:.2f}s**, "
                f"{mold_stats['executable_links']} executable links\n"
                f"- Unexpected C/C++ compilations: "
                f"{baseline['compile_steps']} / {mold_stats['compile_steps']}\n"
            )
            if valid:
                handle.write(
                    f"- mold/default relink wall-time ratio: "
                    f"**{mold_seconds / baseline_seconds:.3f}**\n"
                )

    if not valid:
        raise RuntimeError(
            "Relink passes differed in executable count or recompiled objects; "
            "no performance conclusion is valid"
        )

    # Preserve actual NativeUI behavioral qualification under the alternative.
    run(
        [
            "ctest",
            "--test-dir",
            str(build),
            "--output-on-failure",
            "-L",
            "^unit$",
        ]
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("build", nargs="?", type=Path)
    parser.add_argument("report", nargs="?", type=Path)
    parser.add_argument("--self-test", action="store_true")
    options = parser.parse_args()
    if options.self_test:
        self_test()
        return
    if not options.build or not options.report:
        parser.error("build directory and report path are required")
    benchmark(options.build, options.report)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, AssertionError) as error:
        print(f"Linker benchmark failed: {error}", file=sys.stderr)
        sys.exit(1)
