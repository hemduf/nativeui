#!/usr/bin/env python3

"""Run the NativeUI WebAssembly lifecycle smoke in headless Chromium."""

from __future__ import annotations

import argparse
import functools
import http.server
import pathlib
import threading
import urllib.parse

from playwright.sync_api import Error as PlaywrightError
from playwright.sync_api import TimeoutError as PlaywrightTimeoutError
from playwright.sync_api import sync_playwright


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("page", type=pathlib.Path)
    parser.add_argument(
        "--embedded",
        action="store_true",
        help="Exercise the EmbeddedView/PUGL_MODULE host path instead of standalone windows.",
    )
    parser.add_argument(
        "--diagnostics-dir",
        type=pathlib.Path,
        default=pathlib.Path("browser-diagnostics"),
    )
    return parser.parse_args()


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, _format: str, *args: object) -> None:
        del args


def write_diagnostics(page: object, directory: pathlib.Path) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    try:
        directory.joinpath("page.html").write_text(page.content(), encoding="utf-8")
        page.screenshot(path=str(directory / "page.png"), full_page=True)
    except PlaywrightError as error:
        directory.joinpath("diagnostics-error.txt").write_text(
            f"{error}\n", encoding="utf-8"
        )


def require_count(locator: object, expected: int, label: str) -> None:
    actual = locator.count()
    if actual != expected:
        raise RuntimeError(f"Expected {expected} {label}, got {actual}")


def main() -> int:
    args = parse_args()
    test_page = args.page.resolve()
    if not test_page.is_file():
        print(f"Missing browser test page: {test_page}")
        return 2

    handler = functools.partial(QuietHandler, directory=str(test_page.parent))
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()

    console_errors: list[str] = []
    page_errors: list[str] = []
    url = (
        f"http://127.0.0.1:{server.server_address[1]}/"
        f"{urllib.parse.quote(test_page.name)}"
    )

    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(headless=True)
            context = browser.new_context(viewport={"width": 800, "height": 600})
            page = context.new_page()
            page.on(
                "console",
                lambda message: console_errors.append(message.text)
                if message.type == "error"
                else None,
            )
            page.on("pageerror", lambda error: page_errors.append(str(error)))

            try:
                page.goto(url, wait_until="load", timeout=15000)
                page.wait_for_function(
                    "document.body && document.body.dataset.nativeuiReady === 'true'",
                    timeout=10000,
                )

                initial = page.locator("body").get_attribute("data-nativeui-test")
                if initial == "fail":
                    reason = page.locator("body").get_attribute("data-nativeui-reason")
                    raise RuntimeError(reason or "Lifecycle initialization failed")

                canvases = page.locator("canvas[id^='pugl-view-']")
                inputs = page.locator("textarea[data-pugl-text-input]")
                primary = page.locator("canvas.nativeui-primary-surface")
                require_count(canvases, 2, "Pugl canvases")
                require_count(inputs, 2, "Pugl text-input bridges")
                require_count(
                    primary,
                    0 if args.embedded else 1,
                    "primary NativeUI surfaces",
                )

                destroy_export = (
                    "_nativeuiWasmEmbeddedDestroyFirst"
                    if args.embedded
                    else "_nativeuiWasmLifecycleDestroyFirst"
                )
                destroy_result = page.evaluate(
                    """
                    (name) => {
                      const destroy = Module && Module[name];
                      if (typeof destroy !== 'function') {
                        throw new Error('Missing destroy export: ' + name);
                      }
                      return destroy();
                    }
                    """,
                    destroy_export,
                )
                if destroy_result != 0:
                    raise RuntimeError(f"Destroy-first returned {destroy_result}")

                page.wait_for_function(
                    "document.querySelectorAll(\"canvas[id^='pugl-view-']\").length === 1",
                    timeout=5000,
                )
                require_count(
                    page.locator("textarea[data-pugl-text-input]"),
                    1,
                    "surviving text-input bridges",
                )
                require_count(
                    page.locator("canvas.nativeui-primary-surface"),
                    0 if args.embedded else 1,
                    "surviving primary surfaces",
                )

                remaining_input = page.locator("textarea[data-pugl-text-input]").first
                remaining_canvas = page.locator("canvas[id^='pugl-view-']").first
                remaining_input.focus()
                box = remaining_canvas.bounding_box()
                if not box:
                    raise RuntimeError("Surviving canvas has no bounding box")
                page.mouse.click(
                    box["x"] + box["width"] / 2.0,
                    box["y"] + box["height"] / 2.0,
                )
                focused = page.evaluate(
                    """
                    () => {
                      const input = document.querySelector('textarea[data-pugl-text-input]');
                      return !!input && document.activeElement === input;
                    }
                    """
                )
                if not focused:
                    raise RuntimeError(
                        "Pointer interaction stole surviving view keyboard focus"
                    )

                finish_export = (
                    "_nativeuiWasmEmbeddedFinish"
                    if args.embedded
                    else "_nativeuiWasmLifecycleFinish"
                )
                finish_result = page.evaluate(
                    """
                    (name) => {
                      const finish = Module && Module[name];
                      if (typeof finish !== 'function') {
                        throw new Error('Missing finish export: ' + name);
                      }
                      return finish();
                    }
                    """,
                    finish_export,
                )
                if finish_result != 0:
                    raise RuntimeError(f"Lifecycle finish returned {finish_result}")

                page.wait_for_function(
                    "document.body.dataset.nativeuiTest !== 'pending'",
                    timeout=5000,
                )
                result = page.locator("body").get_attribute("data-nativeui-test")
                reason = page.locator("body").get_attribute("data-nativeui-reason")
                if result != "pass" or console_errors or page_errors:
                    raise RuntimeError(
                        reason or f"Unexpected lifecycle result: {result!r}"
                    )

                mode = "embedded multi-instance" if args.embedded else "lifecycle"
                print(f"NativeUI WebAssembly {mode} browser smoke passed")
                return 0
            except (PlaywrightError, PlaywrightTimeoutError, RuntimeError) as error:
                print(f"Browser lifecycle smoke failed: {error}")
                for message in console_errors:
                    print(f"Console error: {message}")
                for message in page_errors:
                    print(f"Page error: {message}")
                write_diagnostics(page, args.diagnostics_dir)
                return 1
            finally:
                context.close()
                browser.close()
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5.0)


if __name__ == "__main__":
    raise SystemExit(main())
