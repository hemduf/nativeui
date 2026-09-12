#include "detail/windows_desktop_services.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {

using namespace std::chrono_literals;

constexpr wchar_t kSmokeParentClass[] = L"NativeUIT064SmokeParent";

LRESULT CALLBACK smoke_parent_proc(HWND window,
                                   UINT message,
                                   WPARAM wparam,
                                   LPARAM lparam) {
    return DefWindowProcW(window, message, wparam, lparam);
}

class SmokeParentWindow final {
public:
    SmokeParentWindow() {
        instance_ = GetModuleHandleW(nullptr);
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = &smoke_parent_proc;
        window_class.hInstance = instance_;
        window_class.lpszClassName = kSmokeParentClass;
        atom_ = RegisterClassW(&window_class);
        if (atom_ == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return;
        }
        owns_class_ = atom_ != 0;

        window_ = CreateWindowExW(
            0,
            kSmokeParentClass,
            L"NativeUI T064 Native Smoke Parent",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            360,
            240,
            nullptr,
            nullptr,
            instance_,
            nullptr);
        if (window_) {
            ShowWindow(window_, SW_SHOW);
            UpdateWindow(window_);
        }
    }

    ~SmokeParentWindow() {
        if (window_) {
            DestroyWindow(window_);
        }
        if (owns_class_) {
            UnregisterClassW(kSmokeParentClass, instance_);
        }
    }

    SmokeParentWindow(const SmokeParentWindow&) = delete;
    SmokeParentWindow& operator=(const SmokeParentWindow&) = delete;

    [[nodiscard]] HWND handle() const noexcept { return window_; }
    [[nodiscard]] bool valid() const noexcept { return window_ != nullptr; }

private:
    HINSTANCE instance_{};
    ATOM atom_{};
    HWND window_{};
    bool owns_class_{};
};

void pump_messages() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

bool wait_until(const std::function<bool()>& done,
                std::chrono::steady_clock::duration timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!done() && std::chrono::steady_clock::now() < deadline) {
        pump_messages();
        std::this_thread::sleep_for(10ms);
    }
    pump_messages();
    return done();
}

struct OwnedDialogQuery final {
    HWND owner{};
    std::wstring_view title;
    bool found{};
};

BOOL CALLBACK find_owned_dialog(HWND window, LPARAM parameter) {
    auto& query = *reinterpret_cast<OwnedDialogQuery*>(parameter);
    if (IsWindowVisible(window) == FALSE || GetWindow(window, GW_OWNER) != query.owner) {
        return TRUE;
    }

    wchar_t title[256]{};
    const int length = GetWindowTextW(window, title, static_cast<int>(std::size(title)));
    if (length > 0 && std::wstring_view{title, static_cast<std::size_t>(length)} == query.title) {
        query.found = true;
        return FALSE;
    }
    return TRUE;
}

bool dialog_visible(HWND parent, std::wstring_view title) {
    OwnedDialogQuery query{parent, title, false};
    EnumWindows(&find_owned_dialog, reinterpret_cast<LPARAM>(&query));
    if (query.found) {
        return true;
    }

    // Keep an exact-title fallback for hosted Windows configurations that do
    // not report the modal owner relationship through GW_OWNER.
    const std::wstring value{title};
    const HWND window = FindWindowW(nullptr, value.c_str());
    return window != nullptr && IsWindowVisible(window) != FALSE;
}

template <typename Start>
bool smoke_dialog(ui::DesktopServicesBackend& backend,
                  HWND parent,
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
        std::fprintf(stderr, "T064 Windows native smoke: request %llu was not accepted asynchronously\n",
                     static_cast<unsigned long long>(id));
        return false;
    }

    if (!wait_until([&] { return dialog_visible(parent, native_title); }, 5s)) {
        std::fprintf(stderr, "T064 Windows native smoke: request %llu dialog did not become visible\n",
                     static_cast<unsigned long long>(id));
        // Best-effort cleanup keeps a failed smoke from leaving a modal worker
        // alive until the outer CTest timeout. The failure is still reported.
        if (backend.cancel(id)) {
            (void)wait_until([&] { return callbacks.load(std::memory_order_acquire) == 1; }, 5s);
        }
        return false;
    }
    if (!backend.cancel(id)) {
        std::fprintf(stderr, "T064 Windows native smoke: request %llu cancellation was rejected\n",
                     static_cast<unsigned long long>(id));
        return false;
    }
    if (!wait_until([&] { return callbacks.load(std::memory_order_acquire) == 1; }, 5s)) {
        std::fprintf(stderr, "T064 Windows native smoke: request %llu produced no cancellation callback\n",
                     static_cast<unsigned long long>(id));
        return false;
    }
    if (completion_status.load(std::memory_order_acquire) !=
        ui::DesktopServiceStatus::Cancelled) {
        std::fprintf(stderr, "T064 Windows native smoke: request %llu completed with wrong status\n",
                     static_cast<unsigned long long>(id));
        return false;
    }
    std::this_thread::sleep_for(50ms);
    return callbacks.load(std::memory_order_acquire) == 1 && !backend.cancel(id);
}

bool run_native_smoke() {
    SmokeParentWindow parent;
    if (!parent.valid()) {
        std::fprintf(stderr, "T064 Windows native smoke: unable to create parent window\n");
        return false;
    }

    auto backend = ui::detail::make_windows_desktop_services_backend(
        reinterpret_cast<std::uintptr_t>(parent.handle()));
    if (!backend) return false;

    ui::OpenFileOptions open_one;
    open_one.title = "NativeUI T064 Open File Smoke";
    open_one.filters = {{"Audio", {".wav", ".aiff"}}};
    if (!smoke_dialog(*backend, parent.handle(), 1, L"NativeUI T064 Open File Smoke",
                      [&](ui::FileDialogCallback completion) {
                          return backend->start_open_file(1, open_one, std::move(completion));
                      })) {
        return false;
    }

    ui::OpenFileOptions open_many;
    open_many.title = "NativeUI T064 Open Files Smoke";
    open_many.filters = {{"Audio", {".wav"}}};
    if (!smoke_dialog(*backend, parent.handle(), 2, L"NativeUI T064 Open Files Smoke",
                      [&](ui::FileDialogCallback completion) {
                          return backend->start_open_files(2, open_many, std::move(completion));
                      })) {
        return false;
    }

    ui::SaveFileOptions save;
    save.title = "NativeUI T064 Save File Smoke";
    save.suggested_filename = std::string{"nativeui-t064-\xC3\xA9.wav"};
    save.filters = {{"Wave", {".wav"}}};
    if (!smoke_dialog(*backend, parent.handle(), 3, L"NativeUI T064 Save File Smoke",
                      [&](ui::FileDialogCallback completion) {
                          return backend->start_save_file(3, save, std::move(completion));
                      })) {
        return false;
    }

    ui::DirectoryOptions directory;
    directory.title = "NativeUI T064 Directory Smoke";
    if (!smoke_dialog(*backend, parent.handle(), 4, L"NativeUI T064 Directory Smoke",
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
