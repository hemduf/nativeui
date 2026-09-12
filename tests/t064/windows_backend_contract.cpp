#include "detail/windows_desktop_services.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>

namespace {

using namespace std::chrono_literals;

bool wait_until(const std::function<bool()>& done,
                std::chrono::steady_clock::duration timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!done() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(10ms);
    }
    return done();
}

bool dialog_visible(std::wstring_view title) {
    const std::wstring value{title};
    const HWND window = FindWindowW(nullptr, value.c_str());
    return window != nullptr && IsWindowVisible(window) != FALSE;
}

template <typename Start>
bool smoke_dialog(ui::DesktopServicesBackend& backend,
                  ui::DesktopRequestId id,
                  std::wstring_view native_title,
                  Start&& start) {
    std::atomic<int> callbacks{0};
    std::atomic<ui::DesktopServiceStatus> completion_status{ui::DesktopServiceStatus::Error};
    const auto status = start([&](ui::FileDialogResult result) {
        completion_status.store(result.status, std::memory_order_release);
        callbacks.fetch_add(1, std::memory_order_acq_rel);
    });
    if (status != ui::DesktopServiceStatus::Accepted ||
        callbacks.load(std::memory_order_acquire) != 0) {
        return false;
    }
    if (!wait_until([&] { return dialog_visible(native_title); }, 5s)) return false;
    if (!backend.cancel(id)) return false;
    if (!wait_until([&] { return callbacks.load(std::memory_order_acquire) == 1; }, 5s)) {
        return false;
    }
    if (completion_status.load(std::memory_order_acquire) !=
        ui::DesktopServiceStatus::Cancelled) {
        return false;
    }
    std::this_thread::sleep_for(50ms);
    return callbacks.load(std::memory_order_acquire) == 1 && !backend.cancel(id);
}

bool run_native_smoke() {
    auto backend = ui::detail::make_windows_desktop_services_backend(std::uintptr_t{0});
    if (!backend) return false;

    ui::OpenFileOptions open_one;
    open_one.title = "NativeUI T064 Open File Smoke";
    open_one.filters = {{"Audio", {".wav", ".aiff"}}};
    if (!smoke_dialog(*backend, 1, L"NativeUI T064 Open File Smoke",
                      [&](ui::FileDialogCallback completion) {
                          return backend->start_open_file(1, open_one, std::move(completion));
                      })) {
        return false;
    }

    ui::OpenFileOptions open_many;
    open_many.title = "NativeUI T064 Open Files Smoke";
    open_many.filters = {{"Audio", {".wav"}}};
    if (!smoke_dialog(*backend, 2, L"NativeUI T064 Open Files Smoke",
                      [&](ui::FileDialogCallback completion) {
                          return backend->start_open_files(2, open_many, std::move(completion));
                      })) {
        return false;
    }

    ui::SaveFileOptions save;
    save.title = "NativeUI T064 Save File Smoke";
    save.suggested_filename = std::string{"nativeui-t064-\xC3\xA9.wav"};
    save.filters = {{"Wave", {".wav"}}};
    if (!smoke_dialog(*backend, 3, L"NativeUI T064 Save File Smoke",
                      [&](ui::FileDialogCallback completion) {
                          return backend->start_save_file(3, save, std::move(completion));
                      })) {
        return false;
    }

    ui::DirectoryOptions directory;
    directory.title = "NativeUI T064 Directory Smoke";
    if (!smoke_dialog(*backend, 4, L"NativeUI T064 Directory Smoke",
                      [&](ui::FileDialogCallback completion) {
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

    return true;
}

} // namespace

int main(int argc, char** argv) {
    using namespace ui::detail::windows_desktop_services_detail;

    if (argc == 2 && std::string_view{argv[1]} == "--native-smoke") {
        return run_native_smoke() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    auto backend = ui::detail::make_windows_desktop_services_backend(std::uintptr_t{0});
    if (!backend) return EXIT_FAILURE;

    bool completion_called = false;
    const auto status = backend->start_open_file(
        ui::kInvalidDesktopRequestId,
        {},
        [&completion_called](ui::FileDialogResult) { completion_called = true; });
    if (status != ui::DesktopServiceStatus::InvalidArgument || completion_called) {
        return EXIT_FAILURE;
    }
    if (backend->cancel(ui::kInvalidDesktopRequestId)) return EXIT_FAILURE;

    std::wstring unicode;
    if (!utf8_to_wide(std::string{"nativeui-\xC3\xA9.wav"}, unicode) ||
        unicode != L"nativeui-\u00E9.wav") {
        return EXIT_FAILURE;
    }

    RequestState cancel_wins;
    if (!claim_cancellation(cancel_wins) || claim_cancellation(cancel_wins)) {
        return EXIT_FAILURE;
    }
    ui::FileDialogResult accepted_after_cancel{
        .status = ui::DesktopServiceStatus::Accepted,
        .paths = {"ignored.wav"},
        .error = {},
    };
    arbitrate_worker_result(cancel_wins, accepted_after_cancel);
    if (accepted_after_cancel.status != ui::DesktopServiceStatus::Cancelled ||
        !accepted_after_cancel.paths.empty() ||
        !cancel_wins.worker_finished.load(std::memory_order_acquire)) {
        return EXIT_FAILURE;
    }

    RequestState worker_wins;
    ui::FileDialogResult accepted{
        .status = ui::DesktopServiceStatus::Accepted,
        .paths = {"selected.wav"},
        .error = {},
    };
    arbitrate_worker_result(worker_wins, accepted);
    if (accepted.status != ui::DesktopServiceStatus::Accepted ||
        accepted.paths.size() != 1 ||
        claim_cancellation(worker_wins) ||
        !worker_wins.worker_finished.load(std::memory_order_acquire)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
