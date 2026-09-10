#include "detail/linux_dbus.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

namespace {

bool drain_until(ui::detail::DispatcherOwner& owner,
                 const std::function<bool()>& done,
                 std::chrono::steady_clock::duration limit) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (!done() && std::chrono::steady_clock::now() < deadline) {
        (void)owner.checkpoint();
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    (void)owner.checkpoint();
    return done();
}

} // namespace

int main() {
    using namespace std::chrono_literals;
    using namespace ui::detail;

    LinuxDbusTransport transport;
    if (transport.start() != LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }

    DispatcherOwner owner;
    const auto dispatcher = owner.dispatcher();
    constexpr LinuxDbusClientId client = 1;

    bool success_done = false;
    LinuxDbusCompletion success;
    const auto success_id = transport.call_method(
        client,
        dispatcher,
        LinuxDbusMethodCall{
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "GetId",
            2s,
        },
        [&](LinuxDbusCompletion result) {
            success = std::move(result);
            success_done = true;
        });
    if (success_id == kInvalidLinuxDbusRequestId ||
        !drain_until(owner, [&] { return success_done; }, 2s) ||
        success.code != LinuxDbusErrorCode::None ||
        transport.pending_request_count() != 0 ||
        transport.cancel_request(client, success_id)) {
        return EXIT_FAILURE;
    }

    LinuxDbusMethodCall argument_call{
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "GetNameOwner",
        2s,
    };
    argument_call.arguments.push_back(LinuxDbusValue::string("org.freedesktop.DBus"));

    bool argument_done = false;
    LinuxDbusCompletion argument_result;
    const auto argument_id = transport.call_method(
        client,
        dispatcher,
        argument_call,
        [&](LinuxDbusCompletion result) {
            argument_result = std::move(result);
            argument_done = true;
        });
    if (argument_id == kInvalidLinuxDbusRequestId ||
        !drain_until(owner, [&] { return argument_done; }, 2s) ||
        argument_result.code != LinuxDbusErrorCode::None ||
        argument_result.values.size() != 1 ||
        argument_result.values.front().kind != LinuxDbusValueKind::String ||
        argument_result.values.front().text.empty() ||
        transport.pending_request_count() != 0) {
        return EXIT_FAILURE;
    }

    bool error_done = false;
    LinuxDbusCompletion remote_error;
    const auto error_id = transport.call_method(
        client,
        dispatcher,
        LinuxDbusMethodCall{
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "NativeUI_Method_That_Does_Not_Exist",
            2s,
        },
        [&](LinuxDbusCompletion result) {
            remote_error = std::move(result);
            error_done = true;
        });
    if (error_id == kInvalidLinuxDbusRequestId ||
        !drain_until(owner, [&] { return error_done; }, 2s) ||
        remote_error.code != LinuxDbusErrorCode::RemoteError ||
        remote_error.remote_error_name.empty() || transport.pending_request_count() != 0) {
        return EXIT_FAILURE;
    }

    bool cancelled_done = false;
    LinuxDbusCompletion cancelled;
    const auto cancelled_id = transport.call_method(
        client,
        dispatcher,
        LinuxDbusMethodCall{
            "org.freedesktop.DBus",
            "/org/freedesktop/DBus",
            "org.freedesktop.DBus",
            "ListNames",
            2s,
        },
        [&](LinuxDbusCompletion result) {
            cancelled = std::move(result);
            cancelled_done = true;
        });
    if (cancelled_id == kInvalidLinuxDbusRequestId ||
        !transport.cancel_request(client, cancelled_id) ||
        !drain_until(owner, [&] { return cancelled_done; }, 2s) ||
        cancelled.code != LinuxDbusErrorCode::Cancelled ||
        transport.pending_request_count() != 0) {
        return EXIT_FAILURE;
    }

    if (transport.call_method(
            client,
            dispatcher,
            LinuxDbusMethodCall{"not a bus name", "/", "org.example.Valid", "Ping", 2s},
            [](LinuxDbusCompletion) {}) != kInvalidLinuxDbusRequestId ||
        transport.call_method(
            client,
            dispatcher,
            LinuxDbusMethodCall{"org.freedesktop.DBus", "not/a/path", "org.example.Valid", "Ping", 2s},
            [](LinuxDbusCompletion) {}) != kInvalidLinuxDbusRequestId ||
        transport.call_method(
            client,
            dispatcher,
            LinuxDbusMethodCall{"org.freedesktop.DBus", "/", "not an interface", "Ping", 2s},
            [](LinuxDbusCompletion) {}) != kInvalidLinuxDbusRequestId ||
        transport.call_method(
            client,
            dispatcher,
            LinuxDbusMethodCall{"org.freedesktop.DBus", "/", "org.example.Valid", "not a member", 2s},
            [](LinuxDbusCompletion) {}) != kInvalidLinuxDbusRequestId) {
        return EXIT_FAILURE;
    }

    transport.stop();
    return EXIT_SUCCESS;
}
