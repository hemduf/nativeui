#!/usr/bin/env python3
"""Static contract for NativeUI GitHub Actions cadence.

This test intentionally validates only repository-owned workflow policy; it does
not attempt to emulate GitHub Actions itself.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORKFLOWS = ROOT / ".github" / "workflows"


def read(name: str) -> str:
    return (WORKFLOWS / name).read_text(encoding="utf-8")


def require(text: str, needle: str, name: str) -> None:
    if needle not in text:
        raise AssertionError(f"{name}: missing {needle!r}")


def forbid(text: str, needle: str, name: str) -> None:
    if needle in text:
        raise AssertionError(f"{name}: forbidden {needle!r}")


def main() -> None:
    workflow_files = sorted(
        path for path in WORKFLOWS.glob("*.yml") if path.is_file()
    )
    if not workflow_files:
        raise AssertionError("no workflow files found")

    # Every workflow that can react to PR churn must cancel superseded work.
    for path in workflow_files:
        text = path.read_text(encoding="utf-8")
        if "pull_request:" in text:
            require(text, "cancel-in-progress: true", path.name)

    # Whole-project qualification is final-candidate only.
    for name in ("t042-stress.yml", "t052-release-gate.yml"):
        text = read(name)
        require(text, "types: [ready_for_review]", name)
        require(text, "workflow_dispatch:", name)
        forbid(text, "  push:\n", name)

    # Dedicated subsystem contracts must be path scoped.
    for name in (
        "package-contract.yml",
        "t045-accessibility-semantics.yml",
        "t060-application.yml",
        "t065-dispatcher-contract.yml",
        "t067-virtual-list.yml",
    ):
        text = read(name)
        require(text, "    paths:\n", name)

    package_contract = read("package-contract.yml")
    require(
        package_contract,
        "      - 'cmake/NativeUIAttachPlatform.cmake'",
        "package-contract.yml",
    )
    require(
        package_contract,
        "      - 'cmake/NativeUIApplication.cmake'",
        "package-contract.yml",
    )
    require(
        package_contract,
        "      - 'cmake/NativeUIBinaryData.cmake'",
        "package-contract.yml",
    )
    require(
        package_contract,
        "tests/t054_application_contract_tests.cmake",
        "package-contract.yml",
    )
    require(
        package_contract,
        "CMAKE_OBJC_COMPILER_LAUNCHER: sccache",
        "package-contract.yml",
    )
    require(
        package_contract,
        "CMAKE_OBJCXX_COMPILER_LAUNCHER: sccache",
        "package-contract.yml",
    )

    dispatcher = read("t065-dispatcher-contract.yml")
    forbid(dispatcher, "include/nativeui/nativeui.hpp", "t065-dispatcher-contract.yml")
    forbid(dispatcher, "      - 'CMakeLists.txt'", "t065-dispatcher-contract.yml")

    # T067's implementation is split between the public wrapper and retained
    # detail headers. Path scoping must not omit those implementation files.
    virtual_list = read("t067-virtual-list.yml")
    require(
        virtual_list,
        "      - 'include/nativeui/detail/virtual_list_*'",
        "t067-virtual-list.yml",
    )

    ci = read("ci.yml")
    require(ci, "pull_request:\n    branches: [main]", "ci.yml")
    forbid(ci, "tests/t047_package_tests.cmake", "ci.yml")
    forbid(ci, "tests/t048_external_consumer_tests.cmake", "ci.yml")
    forbid(ci, "tests/t054_package_tests.cmake", "ci.yml")
    forbid(ci, "tests/t056_package_tests.cmake", "ci.yml")
    require(ci, "NATIVEUI_CTEST_EXCLUDE:", "ci.yml")
    for test_name in (
        "nativeui_t047_attach_contract_tests",
        "nativeui_t054_application_contract_tests",
        "nativeui_t054_argument_boundary_tests",
        "nativeui_t054_package_source_contract_tests",
    ):
        require(ci, test_name, "ci.yml")
    require(ci, "CMAKE_OBJC_COMPILER_LAUNCHER: sccache", "ci.yml")
    require(ci, "CMAKE_OBJCXX_COMPILER_LAUNCHER: sccache", "ci.yml")
    require(
        ci,
        "-DNATIVEUI_CI_SHARE_MACOS_EXAMPLE_BRIDGE=ON",
        "ci.yml",
    )

    consumer_platform = (ROOT / "cmake" / "NativeUIConsumerPlatform.cmake").read_text(
        encoding="utf-8"
    )
    require(
        consumer_platform,
        "NATIVEUI_CI_SHARE_MACOS_EXAMPLE_BRIDGE",
        "cmake/NativeUIConsumerPlatform.cmake",
    )
    require(
        consumer_platform,
        'NUI_TARGET MATCHES "^nativeui_example_"',
        "cmake/NativeUIConsumerPlatform.cmake",
    )

    # Keep the policy document wired into agent recovery instructions.
    agents = (ROOT / "AGENTS.md").read_text(encoding="utf-8")
    require(agents, "`CI_POLICY.md`", "AGENTS.md")

    policy = (ROOT / "CI_POLICY.md").read_text(encoding="utf-8")
    require(policy, "Draft -> Ready for review", "CI_POLICY.md")

    print(f"CI policy contract: PASS ({len(workflow_files)} workflows checked)")


if __name__ == "__main__":
    main()
