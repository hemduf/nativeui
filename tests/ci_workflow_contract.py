#!/usr/bin/env python3
"""Static guardrails for the shared GitHub Actions build/cache contract.

GitHub Actions parses workflow YAML when a workflow runs. These checks preserve
NativeUI-specific coverage and prevent unintentional full-matrix duplication.
"""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WORKFLOWS = ROOT / ".github" / "workflows"
CACHE_ACTION = ROOT / ".github" / "actions" / "setup-cpp-cache" / "action.yml"


def require(condition: bool, description: str) -> None:
    if not condition:
        raise AssertionError(description)


def read_workflow(name: str) -> str:
    return (WORKFLOWS / name).read_text(encoding="utf-8")


def verify() -> None:
    cache_action = CACHE_ACTION.read_text(encoding="utf-8")
    require("using: composite" in cache_action, "Cache setup must be a composite action")
    require(
        "mozilla-actions/sccache-action@fc920bf0ec8de6ee65d409111f7ec508035751ba"
        in cache_action,
        "Cache setup must pin the reviewed sccache action",
    )
    for required in (
        "version: v0.16.0",
        "SCCACHE_GHA_ENABLED=true",
        "CMAKE_C_COMPILER_LAUNCHER=sccache",
        "CMAKE_CXX_COMPILER_LAUNCHER=sccache",
    ):
        require(required in cache_action, f"Missing compiler cache setting: {required}")

    expected_steps = {
        "ci.yml": 2,
        "scalar-source-validation.yml": 2,
        "main-smoke.yml": 1,
        "pugl-integration.yml": 1,
        "package-contract.yml": 1,
        "advanced-rendering-docs-validation.yml": 1,
    }
    cache_step = "uses: ./.github/actions/setup-cpp-cache"
    for name, count in expected_steps.items():
        content = read_workflow(name)
        require(
            content.count(cache_step) == count,
            f"{name} must set up cache for all {count} C/C++ build jobs",
        )
        require(
            content.index(cache_step) < content.index("name: Configure"),
            f"{name} must initialize compiler cache before CMake configure",
        )
        require(
            "CMAKE_BUILD_PARALLEL_LEVEL: '3'" in content,
            f"{name} should use conservative shared build parallelism",
        )

    ci = read_workflow("ci.yml")
    for platform in (
        "macOS ARM64",
        "Windows x64",
        "Linux x64",
        "Linux ARM64",
    ):
        require(f"- name: {platform}" in ci, f"Canonical matrix is missing {platform}")
    require(
        "macOS Intel" not in ci and "macos-15-intel" not in ci,
        "Retired macOS Intel job must not return to canonical CI",
    )
    for workflow in WORKFLOWS.glob("*.yml"):
        source = workflow.read_text(encoding="utf-8")
        require(
            "macos-15-intel" not in source,
            f"{workflow.name} must not schedule the retired macOS Intel runner",
        )
    require("name: Linux ASan + UBSan" in ci, "Sanitizers must remain in canonical CI")
    require("name: Linux D-Bus contract" in ci, "Linux D-Bus must remain in canonical CI")
    require(
        ci.count("if: github.event_name != 'pull_request' || !github.event.pull_request.draft")
        == 3,
        "All full qualification jobs must retain Draft-to-Ready gating",
    )
    require(
        "CMAKE_BUILD_PARALLEL_LEVEL: '2'" in ci,
        "Sanitizers must retain their dedicated memory bound",
    )
    require(
        "- '.github/workflows/**'" in ci
        and "- '.github/actions/setup-cpp-cache/**'" in ci,
        "CI must qualify changes to shared workflow and cache configuration",
    )
    require("python3 tests/ci_workflow_contract.py" in ci, "Contract job is missing")
    require(
        "ctest_parallel_contract.cmake" in ci,
        "Configured CTest scheduling contract must run in full CI",
    )
    require(
        ci.count("--parallel 2") == 2,
        "Canonical Linux/macOS/Windows CTest must use two slots",
    )
    require(
        "ctest --test-dir build-sanitize --output-on-failure" in ci,
        "Memory-sensitive sanitizer suite must stay serial",
    )
    require(
        "--output-junit" in ci and "Upload CTest report" in ci,
        "CTest result artifacts must remain available for comparisons",
    )
    source = (ROOT / "tests" / "CMakeLists.txt").read_text(encoding="utf-8")
    require(
        source.count("RESOURCE_LOCK native_display PROCESSORS 2") >= 4,
        "Native window and GPU tests must reserve both slots under one lock",
    )

    scalar = read_workflow("scalar-source-validation.yml")
    trigger_block = scalar.split("\npermissions:", 1)[0]
    require(
        "pull_request:" not in trigger_block,
        "Feature-specific workflow must not rerun a full matrix on every PR",
    )
    require(
        "- feature/scalar-source-material-data" in trigger_block
        and "workflow_dispatch:" in trigger_block,
        "Keep the special exact-head qualification available",
    )
    require(
        "CMAKE_BUILD_PARALLEL_LEVEL: '2'" in scalar,
        "Manual sanitizer qualification must remain bounded",
    )

    print("Shared CI workflow contract: PASS")


if __name__ == "__main__":
    verify()
