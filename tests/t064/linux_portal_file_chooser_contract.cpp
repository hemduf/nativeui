#include "detail/linux_desktop_services.hpp"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace ui;
using namespace ui::detail;

class FakePortalBus final : public LinuxPortalBus {
public:
    struct PendingCall final {
        LinuxDbusRequestId id{};
        LinuxDbusMethodCall call;
        LinuxDbusCompletionCallback callback;
    };

    struct Subscription final {
        LinuxDbusSubscriptionId id{};
        LinuxDbusSignalMatch match;
        LinuxDbusSignalCallback callback;
        bool active{true};
    };

    [[nodiscard]] std::string unique_name() const override { return ":1.42"; }

    [[nodiscard]] LinuxDbusSubscriptionResult subscribe_signal(
        Dispatcher,
        const LinuxDbusSignalMatch& match,
        LinuxDbusSignalCallback callback) override {
        const auto id = next_subscription_++;
        subscriptions_.push_back({id, match, std::move(callback), true});
        return {LinuxDbusErrorCode::None, id};
    }

    [[nodiscard]] bool unsubscribe_signal(LinuxDbusSubscriptionId id) override {
        for (auto& subscription : subscriptions_) {
            if (subscription.id == id && subscription.active) {
                subscription.active = false;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] LinuxDbusRequestStartResult call_method(
        Dispatcher,
        const LinuxDbusMethodCall& call,
        LinuxDbusCompletionCallback callback) override {
        const auto id = next_request_++;
        calls_.push_back(PendingCall{id, call, std::move(callback)});
        return {LinuxDbusErrorCode::None, id};
    }

    [[nodiscard]] bool cancel_request(LinuxDbusRequestId id) override {
        cancelled_requests_.push_back(id);
        return true;
    }

    void complete_call(std::size_t index, LinuxDbusCompletion completion) {
        if (index < calls_.size() && calls_[index].callback) {
            auto callback = std::move(calls_[index].callback);
            callback(std::move(completion));
        }
    }

    void emit_response(LinuxDbusSubscriptionId id,
                       std::uint32_t response,
                       LinuxDbusValue results) {
        for (auto& subscription : subscriptions_) {
            if (subscription.id != id || !subscription.active || !subscription.callback) continue;
            LinuxDbusSignal signal;
            signal.path = subscription.match.path;
            signal.interface = "org.freedesktop.portal.Request";
            signal.member = "Response";
            signal.arguments = {
                LinuxDbusValue::uint32(response),
                std::move(results),
            };
            subscription.callback(std::move(signal));
            return;
        }
    }

    std::vector<PendingCall> calls_;
    std::vector<Subscription> subscriptions_;
    std::vector<LinuxDbusRequestId> cancelled_requests_;

private:
    LinuxDbusRequestId next_request_{100};
    LinuxDbusSubscriptionId next_subscription_{200};
};

[[nodiscard]] const LinuxDbusValue* option_value(const LinuxDbusValue& options,
                                                 std::string_view key) {
    if (options.kind != LinuxDbusValueKind::Dictionary) return nullptr;
    for (const auto& [candidate, variant] : options.entries) {
        if (candidate != key || variant.kind != LinuxDbusValueKind::Variant ||
            variant.elements.size() != 1) {
            continue;
        }
        return &variant.elements.front();
    }
    return nullptr;
}

[[nodiscard]] bool string_option(const LinuxDbusValue& options,
                                 std::string_view key,
                                 std::string_view expected) {
    const auto* value = option_value(options, key);
    return value && value->kind == LinuxDbusValueKind::String && value->text == expected;
}

[[nodiscard]] bool bool_option(const LinuxDbusValue& options,
                               std::string_view key,
                               bool expected) {
    const auto* value = option_value(options, key);
    return value && value->kind == LinuxDbusValueKind::Boolean &&
           value->boolean_value == expected;
}

[[nodiscard]] bool folder_option(const LinuxDbusValue& options,
                                 std::string_view expected) {
    const auto* value = option_value(options, "current_folder");
    if (!value || value->kind != LinuxDbusValueKind::Array ||
        value->element_signature != "y" ||
        value->elements.size() != expected.size() + 1) {
        return false;
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (value->elements[i].kind != LinuxDbusValueKind::Byte ||
            value->elements[i].byte_value != static_cast<std::uint8_t>(expected[i])) {
            return false;
        }
    }
    return value->elements.back().kind == LinuxDbusValueKind::Byte &&
           value->elements.back().byte_value == 0;
}

[[nodiscard]] bool filters_option(const LinuxDbusValue& options) {
    const auto* value = option_value(options, "filters");
    if (!value || value->kind != LinuxDbusValueKind::Array ||
        value->element_signature != "(sa(us))" || value->elements.size() != 1) {
        return false;
    }
    const auto& filter = value->elements.front();
    if (filter.kind != LinuxDbusValueKind::Struct || filter.elements.size() != 2 ||
        filter.elements[0] != LinuxDbusValue::string("Audio") ||
        filter.elements[1].kind != LinuxDbusValueKind::Array ||
        filter.elements[1].element_signature != "(us)" ||
        filter.elements[1].elements.size() != 2) {
        return false;
    }
    const auto expected_entry = [](const LinuxDbusValue& entry, std::string_view glob) {
        return entry.kind == LinuxDbusValueKind::Struct && entry.elements.size() == 2 &&
               entry.elements[0] == LinuxDbusValue::uint32(0) &&
               entry.elements[1] == LinuxDbusValue::string(std::string{glob});
    };
    return expected_entry(filter.elements[1].elements[0], "*.wav") &&
           expected_entry(filter.elements[1].elements[1], "*.aiff");
}

[[nodiscard]] LinuxDbusValue uri_results(std::vector<std::string> uris) {
    std::vector<LinuxDbusValue> values;
    values.reserve(uris.size());
    for (auto& uri : uris) values.push_back(LinuxDbusValue::string(std::move(uri)));
    return LinuxDbusValue::dictionary({
        {"uris", LinuxDbusValue::variant(LinuxDbusValue::array("s", std::move(values)))},
    });
}

[[nodiscard]] std::string request_handle(DesktopRequestId id) {
    return "/org/freedesktop/portal/desktop/request/1_42/nativeui_7_" +
           std::to_string(id);
}

[[nodiscard]] bool complete_initial(FakePortalBus& bus,
                                    std::size_t call_index,
                                    std::string handle) {
    LinuxDbusCompletion reply;
    reply.values = {LinuxDbusValue::object_path(std::move(handle))};
    bus.complete_call(call_index, std::move(reply));
    return true;
}

} // namespace

int main() {
    auto bus = std::make_shared<FakePortalBus>();
    Dispatcher dispatcher;
    auto backend = make_linux_portal_desktop_services_backend(bus, dispatcher, 7);
    if (!backend) return EXIT_FAILURE;

    OpenFileOptions open_options;
    open_options.title = "Open audio";
    open_options.initial_directory = std::filesystem::path{"/tmp/Native UI"};
    open_options.filters = {FileFilter{"Audio", {".wav", ".aiff"}}};

    FileDialogResult open_result;
    std::size_t open_callbacks = 0;
    if (backend->start_open_file(
            21, open_options,
            [&](FileDialogResult result) {
                open_result = std::move(result);
                ++open_callbacks;
            }) != DesktopServiceStatus::Accepted) {
        return EXIT_FAILURE;
    }
    if (bus->calls_.size() != 1 || bus->subscriptions_.size() != 1) return EXIT_FAILURE;
    const auto& open = bus->calls_[0].call;
    if (open.interface != "org.freedesktop.portal.FileChooser" ||
        open.member != "OpenFile" || open.arguments.size() != 3 ||
        open.arguments[0] != LinuxDbusValue::string("") ||
        open.arguments[1] != LinuxDbusValue::string("Open audio") ||
        !string_option(open.arguments[2], "handle_token", "nativeui_7_21") ||
        !folder_option(open.arguments[2], "/tmp/Native UI") ||
        !filters_option(open.arguments[2]) ||
        option_value(open.arguments[2], "multiple") != nullptr ||
        option_value(open.arguments[2], "directory") != nullptr) {
        return EXIT_FAILURE;
    }
    complete_initial(*bus, 0, request_handle(21));
    bus->emit_response(bus->subscriptions_[0].id, 0,
                       uri_results({"file:///tmp/Native%20UI/kick.wav"}));
    if (open_callbacks != 1 || open_result.status != DesktopServiceStatus::Accepted ||
        open_result.paths.size() != 1 ||
        open_result.paths[0].generic_string() != "/tmp/Native UI/kick.wav" ||
        !open_result.error.empty()) {
        return EXIT_FAILURE;
    }

    FileDialogResult multi_result;
    if (backend->start_open_files(
            22, open_options,
            [&](FileDialogResult result) { multi_result = std::move(result); }) !=
        DesktopServiceStatus::Accepted) {
        return EXIT_FAILURE;
    }
    if (bus->calls_.size() != 2 || !bool_option(bus->calls_[1].call.arguments[2], "multiple", true)) {
        return EXIT_FAILURE;
    }
    complete_initial(*bus, 1, request_handle(22));
    bus->emit_response(bus->subscriptions_[1].id, 0,
                       uri_results({"file:///tmp/a.wav", "file:///tmp/b.aiff"}));
    if (multi_result.status != DesktopServiceStatus::Accepted || multi_result.paths.size() != 2 ||
        multi_result.paths[0].generic_string() != "/tmp/a.wav" ||
        multi_result.paths[1].generic_string() != "/tmp/b.aiff") {
        return EXIT_FAILURE;
    }

    SaveFileOptions save_options;
    save_options.title = "Save audio";
    save_options.initial_directory = std::filesystem::path{"/tmp/Native UI"};
    save_options.suggested_filename = "mix.wav";
    save_options.filters = open_options.filters;
    FileDialogResult save_result;
    if (backend->start_save_file(
            23, save_options,
            [&](FileDialogResult result) { save_result = std::move(result); }) !=
        DesktopServiceStatus::Accepted) {
        return EXIT_FAILURE;
    }
    if (bus->calls_.size() != 3 || bus->calls_[2].call.member != "SaveFile" ||
        !string_option(bus->calls_[2].call.arguments[2], "current_name", "mix.wav") ||
        !folder_option(bus->calls_[2].call.arguments[2], "/tmp/Native UI") ||
        !filters_option(bus->calls_[2].call.arguments[2])) {
        return EXIT_FAILURE;
    }
    complete_initial(*bus, 2, request_handle(23));
    bus->emit_response(bus->subscriptions_[2].id, 0,
                       uri_results({"file:///tmp/Native%20UI/mix.wav"}));
    if (save_result.status != DesktopServiceStatus::Accepted || save_result.paths.size() != 1 ||
        save_result.paths[0].generic_string() != "/tmp/Native UI/mix.wav") {
        return EXIT_FAILURE;
    }

    DirectoryOptions directory_options;
    directory_options.title = "Choose folder";
    directory_options.initial_directory = std::filesystem::path{"/tmp"};
    FileDialogResult directory_result;
    if (backend->start_select_directory(
            24, directory_options,
            [&](FileDialogResult result) { directory_result = std::move(result); }) !=
        DesktopServiceStatus::Accepted) {
        return EXIT_FAILURE;
    }
    if (bus->calls_.size() != 4 || bus->calls_[3].call.member != "OpenFile" ||
        !bool_option(bus->calls_[3].call.arguments[2], "directory", true) ||
        !folder_option(bus->calls_[3].call.arguments[2], "/tmp")) {
        return EXIT_FAILURE;
    }
    complete_initial(*bus, 3, request_handle(24));
    bus->emit_response(bus->subscriptions_[3].id, 0,
                       uri_results({"file:///tmp/Native%20UI"}));
    if (directory_result.status != DesktopServiceStatus::Accepted ||
        directory_result.paths.size() != 1 ||
        directory_result.paths[0].generic_string() != "/tmp/Native UI") {
        return EXIT_FAILURE;
    }

    FileDialogResult invalid_uri_result;
    if (backend->start_open_file(
            25, OpenFileOptions{},
            [&](FileDialogResult result) { invalid_uri_result = std::move(result); }) !=
        DesktopServiceStatus::Accepted) {
        return EXIT_FAILURE;
    }
    complete_initial(*bus, 4, request_handle(25));
    bus->emit_response(bus->subscriptions_[4].id, 0,
                       uri_results({"file://remote.example/tmp/not-local.wav"}));
    if (invalid_uri_result.status != DesktopServiceStatus::Error ||
        !invalid_uri_result.paths.empty() || invalid_uri_result.error.empty()) {
        return EXIT_FAILURE;
    }

    FileDialogResult cancelled;
    if (backend->start_select_directory(
            26, DirectoryOptions{},
            [&](FileDialogResult result) { cancelled = std::move(result); }) !=
        DesktopServiceStatus::Accepted) {
        return EXIT_FAILURE;
    }
    if (!backend->cancel(26) || cancelled.status != DesktopServiceStatus::Cancelled ||
        !cancelled.paths.empty() || !cancelled.error.empty() || bus->calls_.size() != 7 ||
        bus->calls_.back().call.interface != "org.freedesktop.portal.Request" ||
        bus->calls_.back().call.member != "Close") {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
