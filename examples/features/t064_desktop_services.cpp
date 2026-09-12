#include "example_support.hpp"

#include <nativeui/nativeui.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string status_text(ui::DesktopServiceStatus status) {
    switch (status) {
    case ui::DesktopServiceStatus::Accepted: return "Accepted";
    case ui::DesktopServiceStatus::Cancelled: return "Cancelled";
    case ui::DesktopServiceStatus::Busy: return "Busy";
    case ui::DesktopServiceStatus::ResourceLimit: return "ResourceLimit";
    case ui::DesktopServiceStatus::Unsupported: return "Unsupported";
    case ui::DesktopServiceStatus::InvalidArgument: return "InvalidArgument";
    case ui::DesktopServiceStatus::Error: return "Error";
    }
    return "Unknown";
}

class FakeDesktopServicesBackend final : public ui::DesktopServicesBackend {
public:
    ui::DesktopServiceStatus start_open_file(
        ui::DesktopRequestId,
        const ui::OpenFileOptions&,
        ui::FileDialogCallback completion) override {
        completion(ui::FileDialogResult{
            .status = ui::DesktopServiceStatus::Accepted,
            .paths = {std::filesystem::path{"fake-open.wav"}},
            .error = {},
        });
        return ui::DesktopServiceStatus::Accepted;
    }

    ui::DesktopServiceStatus start_open_files(
        ui::DesktopRequestId,
        const ui::OpenFileOptions&,
        ui::FileDialogCallback completion) override {
        completion(ui::FileDialogResult{
            .status = ui::DesktopServiceStatus::Accepted,
            .paths = {
                std::filesystem::path{"fake-a.wav"},
                std::filesystem::path{"fake-b.wav"},
            },
            .error = {},
        });
        return ui::DesktopServiceStatus::Accepted;
    }

    ui::DesktopServiceStatus start_save_file(
        ui::DesktopRequestId,
        const ui::SaveFileOptions&,
        ui::FileDialogCallback completion) override {
        completion(ui::FileDialogResult{
            .status = ui::DesktopServiceStatus::Accepted,
            .paths = {std::filesystem::path{"fake-save.wav"}},
            .error = {},
        });
        return ui::DesktopServiceStatus::Accepted;
    }

    ui::DesktopServiceStatus start_select_directory(
        ui::DesktopRequestId,
        const ui::DirectoryOptions&,
        ui::FileDialogCallback completion) override {
        completion(ui::FileDialogResult{
            .status = ui::DesktopServiceStatus::Accepted,
            .paths = {std::filesystem::path{"fake-directory"}},
            .error = {},
        });
        return ui::DesktopServiceStatus::Accepted;
    }

    ui::DesktopServiceStatus start_open_url(
        ui::DesktopRequestId,
        std::string,
        ui::StatusCallback completion) override {
        completion(ui::DesktopServiceStatus::Accepted);
        return ui::DesktopServiceStatus::Accepted;
    }

    bool cancel(ui::DesktopRequestId) override { return false; }
};

int self_test() {
    ui::Application application;
    application.set_quit_policy(ui::QuitPolicy::ExplicitOnly);
    if (!application.valid()) {
        return example::fail(application.last_error().empty()
                                 ? "Application initialization failed"
                                 : application.last_error());
    }

    ui::UI tree{ui::Label{"T064 DesktopServices fake self-test"}};
    ui::StandaloneWindow window{
        application,
        tree,
        ui::WindowDesc{.title = "NativeUI T064 self-test",
                       .size = {360.0f, 120.0f},
                       .resizable = true}};
    if (!window.valid()) {
        return example::fail(window.last_error().empty()
                                 ? "window creation failed"
                                 : window.last_error());
    }

    auto backend = std::make_shared<FakeDesktopServicesBackend>();
    ui::DesktopServices services{window.dispatcher(), backend};

    bool file_called = false;
    const auto file_id = services.open_file({}, [&](ui::FileDialogResult result) {
        file_called = result.status == ui::DesktopServiceStatus::Accepted &&
                      result.paths ==
                          std::vector<std::filesystem::path>{"fake-open.wav"};
    });
    if (file_id == ui::kInvalidDesktopRequestId || file_called) {
        return example::fail("fake open_file did not preserve async completion");
    }
    if (!application.poll(0.0) || !file_called || services.cancel(file_id)) {
        return example::fail("fake open_file completion contract failed");
    }

    bool url_called = false;
    const auto url_id = services.open_url(
        "https://example.com",
        [&](ui::DesktopServiceStatus status) {
            url_called = status == ui::DesktopServiceStatus::Accepted;
        });
    if (url_id == ui::kInvalidDesktopRequestId || url_called) {
        return example::fail("fake open_url did not preserve async completion");
    }
    if (!application.poll(0.0) || !url_called || services.cancel(url_id)) {
        return example::fail("fake open_url completion contract failed");
    }

    return 0;
}

ui::OpenFileOptions audio_open_options() {
    return ui::OpenFileOptions{
        .title = "Open audio",
        .initial_directory = std::nullopt,
        .filters = {ui::FileFilter{.description = "Audio", .extensions = {".wav", ".aiff"}}},
    };
}

int interactive() {
    ui::Application application;
    if (!application.valid()) {
        return example::fail(application.last_error().empty()
                                 ? "Application initialization failed"
                                 : application.last_error());
    }

    ui::State<std::string> status{"Choose an action"};
    ui::DesktopServices* services = nullptr;

    ui::UI tree{
        ui::Column{
            ui::Header{"T064 — DESKTOP SERVICES"},
            ui::Row{
                ui::Button{"Open file", [&] {
                    if (!services) return;
                    (void)services->open_file(
                        audio_open_options(),
                        [&status](ui::FileDialogResult result) {
                            status.set("Open file: " + status_text(result.status));
                        });
                }},
                ui::Button{"Open files", [&] {
                    if (!services) return;
                    (void)services->open_files(
                        audio_open_options(),
                        [&status](ui::FileDialogResult result) {
                            status.set("Open files: " + status_text(result.status));
                        });
                }},
                ui::Button{"Save file", [&] {
                    if (!services) return;
                    ui::SaveFileOptions options;
                    options.title = "Save audio";
                    options.suggested_filename = "example.wav";
                    options.filters = {
                        ui::FileFilter{.description = "Wave", .extensions = {".wav"}},
                    };
                    (void)services->save_file(
                        std::move(options),
                        [&status](ui::FileDialogResult result) {
                            status.set("Save file: " + status_text(result.status));
                        });
                }},
            }.gap(8.0f),
            ui::Row{
                ui::Button{"Select directory", [&] {
                    if (!services) return;
                    (void)services->select_directory(
                        ui::DirectoryOptions{
                            .title = "Select directory",
                            .initial_directory = std::nullopt,
                        },
                        [&status](ui::FileDialogResult result) {
                            status.set("Directory: " + status_text(result.status));
                        });
                }},
                ui::Button{"Open https://example.com", [&] {
                    if (!services) return;
                    (void)services->open_url(
                        "https://example.com",
                        [&status](ui::DesktopServiceStatus result) {
                            status.set("URL: " + status_text(result));
                        });
                }},
            }.gap(8.0f),
            ui::TextInput{"Status", status},
            ui::Label{
                "Native dialogs and URL launch are requested only by the buttons above."
            }.size(12.0f).color(ui::colors::textMuted),
        }.padding(18.0f).gap(12.0f)};

    ui::StandaloneWindow window{
        application,
        tree,
        ui::WindowDesc{.title = "NativeUI T064 — Desktop Services",
                       .size = {720.0f, 250.0f},
                       .resizable = true}};
    if (!window.valid()) {
        return example::fail(window.last_error().empty()
                                 ? "window creation failed"
                                 : window.last_error());
    }
    services = &window.desktop_services();
    return application.run();
}

} // namespace

int main(int argc, char** argv) {
    return example::self_test_requested(argc, argv) ? self_test() : interactive();
}