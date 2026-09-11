#include <nativeui/desktop_services.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

void drain(ui::detail::DispatcherOwner& owner) {
    while (owner.pending_task_count() != 0) {
        (void)owner.checkpoint();
    }
}

class HoldingBackend final : public ui::DesktopServicesBackend {
public:
    std::unordered_map<ui::DesktopRequestId, ui::FileDialogCallback> files;
    std::unordered_map<ui::DesktopRequestId, ui::StatusCallback> urls;

    ui::DesktopServiceStatus start_open_file(ui::DesktopRequestId id,
                                             const ui::OpenFileOptions&,
                                             ui::FileDialogCallback callback) override {
        files.emplace(id, std::move(callback));
        return ui::DesktopServiceStatus::Accepted;
    }

    ui::DesktopServiceStatus start_open_files(ui::DesktopRequestId id,
                                              const ui::OpenFileOptions&,
                                              ui::FileDialogCallback callback) override {
        files.emplace(id, std::move(callback));
        return ui::DesktopServiceStatus::Accepted;
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
        if (const auto it = files.find(id); it != files.end()) {
            auto callback = std::move(it->second);
            files.erase(it);
            callback(ui::FileDialogResult{
                .status = ui::DesktopServiceStatus::Cancelled,
                .paths = {},
                .error = {},
            });
            return true;
        }
        if (const auto it = urls.find(id); it != urls.end()) {
            auto callback = std::move(it->second);
            urls.erase(it);
            callback(ui::DesktopServiceStatus::Cancelled);
            return true;
        }
        return false;
    }

    void complete_file(ui::DesktopRequestId id, std::filesystem::path path) {
        const auto it = files.find(id);
        if (it == files.end()) return;
        auto callback = std::move(it->second);
        files.erase(it);
        callback(ui::FileDialogResult{
            .status = ui::DesktopServiceStatus::Accepted,
            .paths = {std::move(path)},
            .error = {},
        });
    }
};

} // namespace

int main() {
    ui::detail::DispatcherOwner owner_a;
    ui::detail::DispatcherOwner owner_b;
    auto backend_a = std::make_shared<HoldingBackend>();
    auto backend_b = std::make_shared<HoldingBackend>();
    ui::DesktopServices services_a{owner_a.dispatcher(), backend_a};
    ui::DesktopServices services_b{owner_b.dispatcher(), backend_b};

    std::size_t a_file_callbacks = 0;
    std::size_t b_file_callbacks = 0;

    const auto b_file = services_b.open_file({}, [&](ui::FileDialogResult result) {
        if (result.status == ui::DesktopServiceStatus::Accepted &&
            result.paths == std::vector<std::filesystem::path>{"b.wav"}) {
            ++b_file_callbacks;
        }
    });
    if (b_file == ui::kInvalidDesktopRequestId || services_a.cancel(b_file)) {
        return EXIT_FAILURE;
    }

    const auto a_file = services_a.open_file({}, [&](ui::FileDialogResult result) {
        if (result.status == ui::DesktopServiceStatus::Cancelled) {
            ++a_file_callbacks;
        }
    });
    if (a_file == ui::kInvalidDesktopRequestId || !services_a.cancel(a_file)) {
        return EXIT_FAILURE;
    }
    drain(owner_a);
    if (a_file_callbacks != 1 || b_file_callbacks != 0 ||
        backend_b->files.find(b_file) == backend_b->files.end()) {
        return EXIT_FAILURE;
    }

    backend_b->complete_file(b_file, "b.wav");
    drain(owner_b);
    if (b_file_callbacks != 1 || services_a.cancel(a_file) || services_b.cancel(b_file)) {
        return EXIT_FAILURE;
    }

    std::vector<ui::DesktopRequestId> a_urls;
    a_urls.reserve(ui::kDesktopServicesMaxActiveUrls);
    for (std::size_t i = 0; i < ui::kDesktopServicesMaxActiveUrls; ++i) {
        const auto id = services_a.open_url("https://owner-a.example", [](ui::DesktopServiceStatus) {});
        if (id == ui::kInvalidDesktopRequestId) return EXIT_FAILURE;
        a_urls.push_back(id);
    }

    bool a_busy = false;
    if (services_a.open_url("https://overflow.example", [&](ui::DesktopServiceStatus status) {
            a_busy = status == ui::DesktopServiceStatus::Busy;
        }) != ui::kInvalidDesktopRequestId || a_busy) {
        return EXIT_FAILURE;
    }

    const auto b_url = services_b.open_url("https://owner-b.example", [](ui::DesktopServiceStatus) {});
    if (b_url == ui::kInvalidDesktopRequestId || backend_b->urls.size() != 1) {
        return EXIT_FAILURE;
    }
    drain(owner_a);
    if (!a_busy || backend_a->urls.size() != ui::kDesktopServicesMaxActiveUrls) {
        return EXIT_FAILURE;
    }

    for (const auto id : a_urls) {
        if (!services_a.cancel(id)) return EXIT_FAILURE;
    }
    if (!services_b.cancel(b_url)) return EXIT_FAILURE;
    drain(owner_a);
    drain(owner_b);

    return EXIT_SUCCESS;
}
