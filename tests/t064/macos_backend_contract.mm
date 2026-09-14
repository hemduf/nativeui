#include "detail/macos_desktop_services.hpp"

#import <AppKit/AppKit.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {

bool pump_until(const std::function<bool()>& done,
                std::chrono::steady_clock::duration timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!done() && std::chrono::steady_clock::now() < deadline) {
        [[NSRunLoop currentRunLoop]
            runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    }
    return done();
}

NSSavePanel* visible_panel(std::string_view title) {
    NSString* expected = [[NSString alloc]
        initWithBytes:title.data()
               length:title.size()
             encoding:NSUTF8StringEncoding];
    if (!expected) return nil;
    NSSavePanel* found = nil;
    for (NSWindow* window in [NSApp windows]) {
        if ([window isVisible] && [[window title] isEqualToString:expected] &&
            [window isKindOfClass:[NSSavePanel class]]) {
            found = static_cast<NSSavePanel*>(window);
            break;
        }
    }
    [expected release];
    return found;
}

bool panel_visible(std::string_view title) {
    return visible_panel(title) != nil;
}

template <typename Start>
bool smoke_dialog(ui::DesktopServicesBackend& backend,
                  ui::DesktopRequestId id,
                  std::string_view title,
                  Start&& start) {
    int callbacks = 0;
    ui::DesktopServiceStatus completion_status = ui::DesktopServiceStatus::Error;
    const auto status = start([&](ui::FileDialogResult result) {
        ++callbacks;
        completion_status = result.status;
    });
    if (status != ui::DesktopServiceStatus::Accepted || callbacks != 0) return false;
    if (!pump_until([&] { return panel_visible(title); }, std::chrono::seconds{3})) {
        return false;
    }
    if (!backend.cancel(id)) return false;
    if (!pump_until([&] { return callbacks == 1; }, std::chrono::seconds{2})) {
        return false;
    }
    if (completion_status != ui::DesktopServiceStatus::Cancelled) return false;
    [[NSRunLoop currentRunLoop]
        runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
    return callbacks == 1 && !backend.cancel(id);
}

bool completion_exception_is_contained_and_releases_panel() {
#if !defined(NATIVEUI_T064_FAULT_INJECTION)
    return false;
#else
    auto released_panels = std::make_shared<std::atomic<std::size_t>>(0);
    ui::detail::MacDesktopServicesFaultHooks hooks;
    hooks.before_result_processing = [] {
        throw std::runtime_error{"T064 injected macOS result-processing failure"};
    };
    hooks.released_panels = released_panels;

    auto backend = ui::detail::make_macos_desktop_services_backend_for_testing(
        std::uintptr_t{0}, std::move(hooks));
    if (!backend) return false;

    constexpr ui::DesktopRequestId request_id = 1001;
    ui::OpenFileOptions options;
    options.title = "NativeUI T064 Completion Fault Smoke";

    int callbacks = 0;
    const auto status = backend->start_open_file(
        request_id, options, [&](ui::FileDialogResult) { ++callbacks; });
    if (status != ui::DesktopServiceStatus::Accepted || callbacks != 0) return false;

    NSSavePanel* panel = nil;
    if (!pump_until(
            [&] {
                panel = visible_panel(options.title);
                return panel != nil;
            },
            std::chrono::seconds{3})) {
        return false;
    }

    // Trigger the native AppKit completion directly rather than calling the
    // backend cancellation path. The injected C++ exception occurs after the
    // active entry has been removed and must be contained by the block.
    [panel cancel:nil];
    if (!pump_until(
            [&] { return released_panels->load(std::memory_order_acquire) == 1; },
            std::chrono::seconds{2})) {
        return false;
    }

    [[NSRunLoop currentRunLoop]
        runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
    if (callbacks != 0 || backend->cancel(request_id)) return false;

    backend.reset();
    return released_panels->load(std::memory_order_acquire) == 1;
#endif
}

bool run_native_smoke() {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    [NSApp finishLaunching];

    auto backend = ui::detail::make_macos_desktop_services_backend(std::uintptr_t{0});
    if (!backend) return false;

    ui::OpenFileOptions open_one;
    open_one.title = "NativeUI T064 Open File Smoke";
    open_one.filters = {{"Audio", {".wav", ".aiff"}}};
    if (!smoke_dialog(*backend, 1, open_one.title, [&](ui::FileDialogCallback completion) {
            return backend->start_open_file(1, open_one, std::move(completion));
        })) {
        return false;
    }

    ui::OpenFileOptions open_many;
    open_many.title = "NativeUI T064 Open Files Smoke";
    open_many.filters = {{"Audio", {".wav"}}};
    if (!smoke_dialog(*backend, 2, open_many.title, [&](ui::FileDialogCallback completion) {
            return backend->start_open_files(2, open_many, std::move(completion));
        })) {
        return false;
    }

    ui::SaveFileOptions save;
    save.title = "NativeUI T064 Save File Smoke";
    save.suggested_filename = std::string{"nativeui-t064-\xC3\xA9.wav"};
    save.filters = {{"Wave", {".wav"}}};
    if (!smoke_dialog(*backend, 3, save.title, [&](ui::FileDialogCallback completion) {
            return backend->start_save_file(3, save, std::move(completion));
        })) {
        return false;
    }

    ui::DirectoryOptions directory;
    directory.title = "NativeUI T064 Directory Smoke";
    if (!smoke_dialog(*backend, 4, directory.title, [&](ui::FileDialogCallback completion) {
            return backend->start_select_directory(4, directory, std::move(completion));
        })) {
        return false;
    }

    for (const auto& [id, url] : {
             std::pair<ui::DesktopRequestId, std::string_view>{5, "https://example.invalid/"},
             std::pair<ui::DesktopRequestId, std::string_view>{6, "http://example.invalid/"},
         }) {
        int callbacks = 0;
        ui::DesktopServiceStatus completion = ui::DesktopServiceStatus::Error;
        const auto status = backend->start_open_url(
            id, std::string{url}, [&](ui::DesktopServiceStatus result) {
                ++callbacks;
                completion = result;
            });
        if (status != ui::DesktopServiceStatus::Accepted || callbacks != 1 ||
            completion != ui::DesktopServiceStatus::Accepted) {
            return false;
        }
    }

    backend.reset();
    return completion_exception_is_contained_and_releases_panel();
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--native-smoke") {
        @autoreleasepool {
            return run_native_smoke() ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }

    auto backend = ui::detail::make_macos_desktop_services_backend(std::uintptr_t{0});
    return backend ? EXIT_SUCCESS : EXIT_FAILURE;
}
