#include "detail/linux_desktop_services.hpp"

#include <cstdlib>
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

    [[nodiscard]] std::string unique_name() const override { return ":1.42"; }

    [[nodiscard]] LinuxDbusSubscriptionResult subscribe_signal(
        Dispatcher,
        const LinuxDbusSignalMatch& match,
        LinuxDbusSignalCallback callback) override {
        const auto id = next_subscription_++;
        subscriptions_.push_back({id, match, std::move(callback)});
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
        if (next_call_result_.code != LinuxDbusErrorCode::None) {
            const auto result = next_call_result_;
            next_call_result_ = {LinuxDbusErrorCode::None, 0};
            return result;
        }
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

    void emit_response(LinuxDbusSubscriptionId id, std::uint32_t response) {
        for (auto& subscription : subscriptions_) {
            if (subscription.id != id || !subscription.active || !subscription.callback) continue;
            LinuxDbusSignal signal;
            signal.path = subscription.match.path;
            signal.interface = "org.freedesktop.portal.Request";
            signal.member = "Response";
            signal.arguments = {
                LinuxDbusValue::uint32(response),
                LinuxDbusValue::dictionary({}),
            };
            subscription.callback(std::move(signal));
            return;
        }
    }

    void fail_next_call(LinuxDbusErrorCode code) {
        next_call_result_ = {code, 0};
    }

    struct Subscription final {
        LinuxDbusSubscriptionId id{};
        LinuxDbusSignalMatch match;
        LinuxDbusSignalCallback callback;
        bool active{true};
    };

    std::vector<PendingCall> calls_;
    std::vector<Subscription> subscriptions_;
    std::vector<LinuxDbusRequestId> cancelled_requests_;

private:
    LinuxDbusRequestId next_request_{100};
    LinuxDbusSubscriptionId next_subscription_{200};
    LinuxDbusRequestStartResult next_call_result_{LinuxDbusErrorCode::None, 0};
};

[[nodiscard]] bool valid_handle_token(const LinuxDbusValue& options,
                                      const std::string& expected) {
    if (options.kind != LinuxDbusValueKind::Dictionary || options.entries.size() != 1) return false;
    const auto& [key, variant] = options.entries.front();
    return key == "handle_token" && variant.kind == LinuxDbusValueKind::Variant &&
           variant.elements.size() == 1 &&
           variant.elements.front() == LinuxDbusValue::string(expected);
}

} // namespace

int main() {
    auto bus = std::make_shared<FakePortalBus>();
    Dispatcher dispatcher;
    auto backend = make_linux_portal_desktop_services_backend(bus, dispatcher, 7);
    if (!backend) return EXIT_FAILURE;

    DesktopServiceStatus status = DesktopServiceStatus::Error;
    std::size_t callback_count = 0;
    if (backend->start_open_url(
            11, "https://example.invalid/nativeui",
            [&](DesktopServiceStatus value) {
                status = value;
                ++callback_count;
            }) != DesktopServiceStatus::Accepted) {
        return EXIT_FAILURE;
    }

    const std::string token = "nativeui_7_11";
    const std::string expected_handle =
        "/org/freedesktop/portal/desktop/request/1_42/" + token;
    if (bus->subscriptions_.size() != 1 || bus->calls_.size() != 1) return EXIT_FAILURE;
    if (bus->subscriptions_[0].match.path != expected_handle ||
        bus->subscriptions_[0].match.interface != "org.freedesktop.portal.Request" ||
        bus->subscriptions_[0].match.member != "Response") {
        return EXIT_FAILURE;
    }

    const auto& open = bus->calls_[0].call;
    if (open.destination != "org.freedesktop.portal.Desktop" ||
        open.path != "/org/freedesktop/portal/desktop" ||
        open.interface != "org.freedesktop.portal.OpenURI" ||
        open.member != "OpenURI" || open.arguments.size() != 3 ||
        open.arguments[0] != LinuxDbusValue::string("") ||
        open.arguments[1] != LinuxDbusValue::string("https://example.invalid/nativeui") ||
        !valid_handle_token(open.arguments[2], token)) {
        return EXIT_FAILURE;
    }

    LinuxDbusCompletion open_reply;
    open_reply.values = {LinuxDbusValue::object_path(expected_handle)};
    bus->complete_call(0, std::move(open_reply));
    if (callback_count != 0) return EXIT_FAILURE;
    bus->emit_response(bus->subscriptions_[0].id, 0);
    if (callback_count != 1 || status != DesktopServiceStatus::Accepted) return EXIT_FAILURE;
    if (backend->cancel(11)) return EXIT_FAILURE;

    DesktopServiceStatus cancelled = DesktopServiceStatus::Error;
    if (backend->start_open_url(12, "https://example.invalid/cancel",
                                [&](DesktopServiceStatus value) { cancelled = value; }) !=
        DesktopServiceStatus::Accepted) {
        return EXIT_FAILURE;
    }
    if (!backend->cancel(12) || cancelled != DesktopServiceStatus::Cancelled) {
        return EXIT_FAILURE;
    }
    if (bus->calls_.size() != 3 || bus->calls_[2].call.interface != "org.freedesktop.portal.Request" ||
        bus->calls_[2].call.member != "Close") {
        return EXIT_FAILURE;
    }

    DesktopServiceStatus missing = DesktopServiceStatus::Error;
    if (backend->start_open_url(13, "https://example.invalid/missing",
                                [&](DesktopServiceStatus value) { missing = value; }) !=
        DesktopServiceStatus::Accepted) {
        return EXIT_FAILURE;
    }
    LinuxDbusCompletion service_missing{
        LinuxDbusErrorCode::RemoteError,
        "org.freedesktop.DBus.Error.ServiceUnknown",
        "portal unavailable",
    };
    bus->complete_call(3, std::move(service_missing));
    if (missing != DesktopServiceStatus::Unsupported) return EXIT_FAILURE;

    DesktopServiceStatus user_cancel = DesktopServiceStatus::Error;
    if (backend->start_open_url(14, "https://example.invalid/user-cancel",
                                [&](DesktopServiceStatus value) { user_cancel = value; }) !=
        DesktopServiceStatus::Accepted) {
        return EXIT_FAILURE;
    }
    const auto last_subscription = bus->subscriptions_.back().id;
    LinuxDbusCompletion user_cancel_reply;
    user_cancel_reply.values = {
        LinuxDbusValue::object_path(bus->subscriptions_.back().match.path),
    };
    bus->complete_call(4, std::move(user_cancel_reply));
    bus->emit_response(last_subscription, 1);
    if (user_cancel != DesktopServiceStatus::Cancelled) return EXIT_FAILURE;

    // The normative T064 Linux ownership amendment requires hard T072 quota
    // exhaustion to remain distinguishable from this facade's own Busy limit.
    bus->fail_next_call(LinuxDbusErrorCode::ResourceLimit);
    if (backend->start_open_url(
            15, "https://example.invalid/resource-limit",
            [](DesktopServiceStatus) {}) != DesktopServiceStatus::ResourceLimit) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
