#include <nativeui/desktop_services.hpp>
#include <nativeui/dispatcher.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>

namespace {
std::atomic<bool> fail_next_allocation{false};
}

void* operator new(std::size_t size) {
    if (fail_next_allocation.exchange(false, std::memory_order_acq_rel)) {
        throw std::bad_alloc{};
    }
    if (void* pointer = std::malloc(size == 0 ? 1 : size)) return pointer;
    throw std::bad_alloc{};
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

namespace {

int fail() { return EXIT_FAILURE; }

class FakeBackend final : public ui::DesktopServicesBackend {
public:
    ui::DesktopServiceStatus start_open_file(ui::DesktopRequestId id,
                                             const ui::OpenFileOptions&,
                                             ui::FileDialogCallback callback) override {
        files.emplace(id, std::move(callback));
        return ui::DesktopServiceStatus::Accepted;
    }

    ui::DesktopServiceStatus start_open_files(ui::DesktopRequestId id,
                                              const ui::OpenFileOptions& options,
                                              ui::FileDialogCallback callback) override {
        return start_open_file(id, options, std::move(callback));
    }

    ui::DesktopServiceStatus start_save_file(ui::DesktopRequestId id,
                                             const ui::SaveFileOptions&,
                                             ui::FileDialogCallback callback) override {
        files.emplace(id, std::move(callback));
        return ui::DesktopServiceStatus::Accepted;
    }

    ui::DesktopServiceStatus start_select_directory(ui::DesktopRequestId id,
                                                    const ui::DirectoryOptions&,
                                                    ui::FileDialogCallback callback) override {
        files.emplace(id, std::move(callback));
        return ui::DesktopServiceStatus::Accepted;
    }

    ui::DesktopServiceStatus start_open_url(ui::DesktopRequestId id,
                                            std::string,
                                            ui::StatusCallback callback) override {
        urls.emplace(id, std::move(callback));
        return ui::DesktopServiceStatus::Accepted;
    }

    bool cancel(ui::DesktopRequestId id) override {
        if (const auto found = files.find(id); found != files.end()) {
            auto callback = std::move(found->second);
            files.erase(found);
            callback(ui::FileDialogResult{
                .status = ui::DesktopServiceStatus::Cancelled,
                .paths = {},
                .error = {},
            });
            return true;
        }
        if (const auto found = urls.find(id); found != urls.end()) {
            auto callback = std::move(found->second);
            urls.erase(found);
            callback(ui::DesktopServiceStatus::Cancelled);
            return true;
        }
        return false;
    }

    void complete_file(ui::DesktopRequestId id, ui::FileDialogResult result) {
        const auto found = files.find(id);
        if (found == files.end()) return;
        auto callback = std::move(found->second);
        files.erase(found);
        callback(std::move(result));
    }

    void complete_url(ui::DesktopRequestId id, ui::DesktopServiceStatus status) {
        const auto found = urls.find(id);
        if (found == urls.end()) return;
        auto callback = std::move(found->second);
        urls.erase(found);
        callback(status);
    }

    std::unordered_map<ui::DesktopRequestId, ui::FileDialogCallback> files;
    std::unordered_map<ui::DesktopRequestId, ui::StatusCallback> urls;
};

ui::FileDialogResult accepted_file_result(std::filesystem::path path) {
    ui::FileDialogResult result;
    result.status = ui::DesktopServiceStatus::Accepted;
    result.paths.push_back(std::move(path));
    return result;
}

bool allocation_failure_is_terminal_for_file() {
    ui::detail::DispatcherOwner owner;
    auto backend = std::make_shared<FakeBackend>();
    ui::DesktopServices services{owner.dispatcher(), backend};

    bool called = false;
    const auto id = services.open_file({}, [&](ui::FileDialogResult) { called = true; });
    if (id == ui::kInvalidDesktopRequestId) return false;

    auto result = accepted_file_result("allocation-failure.wav");
    fail_next_allocation.store(true, std::memory_order_release);
    bool escaped = false;
    try {
        backend->complete_file(id, std::move(result));
    } catch (...) {
        escaped = true;
    }
    if (escaped || called || fail_next_allocation.load(std::memory_order_acquire) ||
        services.cancel(id)) {
        return false;
    }

    const auto replacement = services.open_file({}, [](ui::FileDialogResult) {});
    return replacement != ui::kInvalidDesktopRequestId && services.cancel(replacement);
}

bool allocation_failure_is_terminal_for_status() {
    ui::detail::DispatcherOwner owner;
    auto backend = std::make_shared<FakeBackend>();
    ui::DesktopServices services{owner.dispatcher(), backend};

    bool called = false;
    const auto id = services.open_url(
        "https://example.invalid/", [&](ui::DesktopServiceStatus) { called = true; });
    if (id == ui::kInvalidDesktopRequestId) return false;

    fail_next_allocation.store(true, std::memory_order_release);
    bool escaped = false;
    try {
        backend->complete_url(id, ui::DesktopServiceStatus::Accepted);
    } catch (...) {
        escaped = true;
    }
    if (escaped || called || fail_next_allocation.load(std::memory_order_acquire) ||
        services.cancel(id)) {
        return false;
    }

    const auto replacement = services.open_url(
        "https://replacement.invalid/", [](ui::DesktopServiceStatus) {});
    return replacement != ui::kInvalidDesktopRequestId && services.cancel(replacement);
}

bool dispatcher_rejection_is_terminal() {
    ui::detail::DispatcherOwner owner;
    const auto dispatcher = owner.dispatcher();
    auto backend = std::make_shared<FakeBackend>();
    ui::DesktopServices services{dispatcher, backend};

    bool file_called = false;
    bool status_called = false;
    const auto file_id = services.open_file(
        {}, [&](ui::FileDialogResult) { file_called = true; });
    const auto url_id = services.open_url(
        "https://example.invalid/", [&](ui::DesktopServiceStatus) { status_called = true; });
    if (file_id == ui::kInvalidDesktopRequestId || url_id == ui::kInvalidDesktopRequestId) {
        return false;
    }

    for (std::size_t i = 0; i < ui::kDispatcherMaxPendingTasks; ++i) {
        if (!dispatcher.post([] {})) return false;
    }
    if (dispatcher.post([] {})) return false;

    backend->complete_file(file_id, accepted_file_result("queue-full.wav"));
    backend->complete_url(url_id, ui::DesktopServiceStatus::Accepted);
    if (file_called || status_called || services.cancel(file_id) || services.cancel(url_id)) {
        return false;
    }

    const auto file_replacement = services.open_file({}, [](ui::FileDialogResult) {});
    const auto url_replacement = services.open_url(
        "https://replacement.invalid/", [](ui::DesktopServiceStatus) {});
    if (file_replacement == ui::kInvalidDesktopRequestId ||
        url_replacement == ui::kInvalidDesktopRequestId) {
        return false;
    }
    return services.cancel(file_replacement) && services.cancel(url_replacement);
}

bool cancel_completion_failure_is_terminal() {
    ui::detail::DispatcherOwner owner;
    auto backend = std::make_shared<FakeBackend>();
    ui::DesktopServices services{owner.dispatcher(), backend};

    bool called = false;
    const auto id = services.open_file({}, [&](ui::FileDialogResult) { called = true; });
    if (id == ui::kInvalidDesktopRequestId) return false;

    fail_next_allocation.store(true, std::memory_order_release);
    bool escaped = false;
    bool cancelled = false;
    try {
        cancelled = services.cancel(id);
    } catch (...) {
        escaped = true;
    }
    if (escaped || !cancelled || called ||
        fail_next_allocation.load(std::memory_order_acquire) || services.cancel(id)) {
        return false;
    }

    const auto replacement = services.open_file({}, [](ui::FileDialogResult) {});
    return replacement != ui::kInvalidDesktopRequestId && services.cancel(replacement);
}

} // namespace

int main() {
    if (!allocation_failure_is_terminal_for_file()) return fail();
    if (!allocation_failure_is_terminal_for_status()) return fail();
    if (!dispatcher_rejection_is_terminal()) return fail();
    if (!cancel_completion_failure_is_terminal()) return fail();
    return EXIT_SUCCESS;
}
