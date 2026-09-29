#include "detail/linux_desktop_services.hpp"

#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

namespace {

using namespace ui;
using namespace ui::detail;

class RecordingBus final : public LinuxPortalBus {
public:
    [[nodiscard]] std::string unique_name() const override { return ":1.42"; }

    [[nodiscard]] LinuxDbusSubscriptionResult subscribe_signal(
        Dispatcher,
        const LinuxDbusSignalMatch&,
        LinuxDbusSignalCallback) override {
        return {LinuxDbusErrorCode::None, 1};
    }

    [[nodiscard]] bool unsubscribe_signal(LinuxDbusSubscriptionId) override {
        return true;
    }

    [[nodiscard]] LinuxDbusRequestStartResult call_method(
        Dispatcher,
        const LinuxDbusMethodCall& call,
        LinuxDbusCompletionCallback) override {
        calls.push_back(call);
        return {LinuxDbusErrorCode::None, 1};
    }

    [[nodiscard]] bool cancel_request(LinuxDbusRequestId) override { return true; }

    std::vector<LinuxDbusMethodCall> calls;
};

[[nodiscard]] LinuxDbusMethodCall portal_call(std::string interface,
                                              std::string member) {
    LinuxDbusMethodCall call{
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        std::move(interface),
        std::move(member),
    };
    call.arguments = {
        LinuxDbusValue::string(""),
        LinuxDbusValue::string("payload"),
        LinuxDbusValue::dictionary({}),
    };
    return call;
}

} // namespace

int main() {
    if (!linux_x11_portal_parent_window(0).empty()) return EXIT_FAILURE;
    if (linux_x11_portal_parent_window(0xabc) != "x11:abc") return EXIT_FAILURE;

    auto raw = std::make_shared<RecordingBus>();
    LinuxPortalParentBus parented{raw, linux_x11_portal_parent_window(0x1234)};
    Dispatcher dispatcher;

    auto chooser = portal_call("org.freedesktop.portal.FileChooser", "OpenFile");
    if (!parented.call_method(dispatcher, chooser, [](LinuxDbusCompletion) {}).ok()) {
        return EXIT_FAILURE;
    }
    auto open_uri = portal_call("org.freedesktop.portal.OpenURI", "OpenURI");
    if (!parented.call_method(dispatcher, open_uri, [](LinuxDbusCompletion) {}).ok()) {
        return EXIT_FAILURE;
    }

    LinuxDbusMethodCall close{
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop/request/1_42/nativeui_7_1",
        "org.freedesktop.portal.Request",
        "Close",
    };
    if (!parented.call_method(dispatcher, close, [](LinuxDbusCompletion) {}).ok()) {
        return EXIT_FAILURE;
    }

    if (raw->calls.size() != 3) return EXIT_FAILURE;
    for (std::size_t index = 0; index < 2; ++index) {
        if (raw->calls[index].arguments.empty() ||
            raw->calls[index].arguments.front() != LinuxDbusValue::string("x11:1234")) {
            return EXIT_FAILURE;
        }
    }
    if (!raw->calls[2].arguments.empty()) return EXIT_FAILURE;

    // The decorator must not mutate the caller-owned call object.
    if (chooser.arguments.front() != LinuxDbusValue::string("")) return EXIT_FAILURE;
    if (open_uri.arguments.front() != LinuxDbusValue::string("")) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
