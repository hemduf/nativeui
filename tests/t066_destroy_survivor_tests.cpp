#include <nativeui/nativeui.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace {

bool pump(ui::Application& app, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        if (!app.poll(0.0) && !app.quit_requested()) return false;
    }
    return true;
}

int fail(std::string_view message) {
    std::cerr << "FAIL t066_destroy_survivor_tests: " << message << '\n';
    return EXIT_FAILURE;
}

#if defined(_WIN32)
void diagnose_failed_resize(const ui::StandaloneWindow& window) {
    const auto native = window.native_handle();
    const auto hwnd = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(native));
    const BOOL valid = hwnd ? IsWindow(hwnd) : FALSE;

    RECT rect{};
    const BOOL have_rect = valid ? GetWindowRect(hwnd, &rect) : FALSE;
    BOOL raw_resize = FALSE;
    DWORD raw_error = ERROR_SUCCESS;
    if (have_rect) {
        SetLastError(ERROR_SUCCESS);
        const int width = std::max(1L, rect.right - rect.left + 20L);
        const int height = std::max(1L, rect.bottom - rect.top + 10L);
        raw_resize = SetWindowPos(
            hwnd,
            nullptr,
            0,
            0,
            width,
            height,
            SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
        raw_error = raw_resize ? ERROR_SUCCESS : GetLastError();
    }

    std::cerr << "T066 Windows survivor diagnostic: native_handle=" << native
              << " IsWindow=" << (valid ? 1 : 0)
              << " GetWindowRect=" << (have_rect ? 1 : 0)
              << " raw_SetWindowPos=" << (raw_resize ? 1 : 0)
              << " raw_error=" << raw_error
              << " should_close=" << (window.should_close() ? 1 : 0)
              << " is_closed=" << (window.is_closed() ? 1 : 0)
              << " last_error='" << window.last_error() << "'\n";
}
#endif

} // namespace

int main() {
    ui::Application app;
    if (!app.valid()) return fail("Application construction failed");

    ui::UI tree_a{ui::Label{"T066 direct-destroy A"}};
    ui::UI tree_b{ui::Label{"T066 direct-destroy B"}};
    auto a = std::make_unique<ui::StandaloneWindow>(
        app,
        tree_a,
        ui::WindowDesc{.title = "T066 direct-destroy A",
                       .size = {420.0f, 220.0f},
                       .resizable = true});
    auto b = std::make_unique<ui::StandaloneWindow>(
        app,
        tree_b,
        ui::WindowDesc{.title = "T066 direct-destroy B",
                       .size = {360.0f, 180.0f},
                       .resizable = true});
    if (!a->valid() || !b->valid()) return fail("window construction failed");
    if (!pump(app, 8)) return fail("Application stopped before direct destruction");

    a.reset();
    if (app.quit_requested()) return fail("destroying A requested quit while B remained live");
    if (!b->native_handle()) return fail("surviving B lost its native handle");
    if (b->should_close()) return fail("surviving B became closing after destroying A");

    // Keep resize as the first native mutation after sibling destruction. This
    // matches the T060 regression sequence exactly and prevents an unrelated
    // synchronous title update from masking a stale native-window condition.
    if (!b->set_size({380.0f, 190.0f})) {
#if defined(_WIN32)
        diagnose_failed_resize(*b);
#endif
        return fail("surviving B resize failed");
    }
    if (!b->set_title("T066 direct-destroy B survives")) {
        return fail("surviving B title update failed");
    }
    if (!pump(app, 4)) return fail("Application stopped while B remained live");

    b.reset();
    if (!app.quit_requested()) return fail("destroying final window did not request quit");

    std::cout << "PASS t066_destroy_survivor_tests\n";
    return EXIT_SUCCESS;
}
