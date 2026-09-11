#include "detail/linux_dbus.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <cstdlib>
#include <functional>
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
    const auto client = transport.register_client();
    if (client == kInvalidLinuxDbusClientId) {
        return EXIT_FAILURE;
    }

    bool success_done = false;
    LinuxDbusCompletion success;
    const auto success_id = transport.call_method(
        client, dispatcher,
        LinuxDbusMethodCall{"org.freedesktop.DBus", "/org/freedesktop/DBus",
                            "org.freedesktop.DBus", "GetId", 2s},
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

    LinuxDbusMethodCall argument_call{"org.freedesktop.DBus", "/org/freedesktop/DBus",
                                      "org.freedesktop.DBus", "GetNameOwner", 2s};
    argument_call.arguments.push_back(LinuxDbusValue::string("org.freedesktop.DBus"));
    bool argument_done = false;
    LinuxDbusCompletion argument_result;
    const auto argument_id = transport.call_method(
        client, dispatcher, argument_call,
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
        client, dispatcher,
        LinuxDbusMethodCall{"org.freedesktop.DBus", "/org/freedesktop/DBus",
                            "org.freedesktop.DBus", "NativeUI_Method_That_Does_Not_Exist", 2s},
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
        client, dispatcher,
        LinuxDbusMethodCall{"org.freedesktop.DBus", "/org/freedesktop/DBus",
                            "org.freedesktop.DBus", "ListNames", 2s},
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

    LinuxDbusTransport slow_peer;
    if (slow_peer.start() != LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }
    const auto slow_peer_client = slow_peer.register_client();
    if (slow_peer_client == kInvalidLinuxDbusClientId) {
        return EXIT_FAILURE;
    }
    const auto slow_path = slow_peer.register_object_path(
        slow_peer_client, "/org/nativeui/T072/Slow",
        [](const LinuxDbusMethodRequest&) {
            std::this_thread::sleep_for(100ms);
            return LinuxDbusMethodReply::method_return({LinuxDbusValue::string("late")});
        });
    if (slow_path == kInvalidLinuxDbusObjectRegistrationId) {
        return EXIT_FAILURE;
    }

    std::size_t timeout_callbacks = 0;
    LinuxDbusCompletion timeout_result;
    const auto timeout_id = transport.call_method(
        client, dispatcher,
        LinuxDbusMethodCall{slow_peer.unique_name(), "/org/nativeui/T072/Slow",
                            "org.nativeui.T072.Test", "Wait", 10ms},
        [&](LinuxDbusCompletion result) {
            timeout_result = std::move(result);
            ++timeout_callbacks;
        });
    if (timeout_id == kInvalidLinuxDbusRequestId ||
        !drain_until(owner, [&] { return timeout_callbacks == 1; }, 2s) ||
        timeout_result.code != LinuxDbusErrorCode::Timeout ||
        transport.pending_request_count() != 0 ||
        transport.cancel_request(client, timeout_id)) {
        return EXIT_FAILURE;
    }

    std::this_thread::sleep_for(150ms);
    (void)owner.checkpoint();
    if (timeout_callbacks != 1) {
        return EXIT_FAILURE;
    }

    if (transport.call_method(client, dispatcher,
            LinuxDbusMethodCall{"not a bus name", "/", "org.example.Valid", "Ping", 2s},
            [](LinuxDbusCompletion) {}) != kInvalidLinuxDbusRequestId ||
        transport.call_method(client, dispatcher,
            LinuxDbusMethodCall{"org.freedesktop.DBus", "not/a/path", "org.example.Valid", "Ping", 2s},
            [](LinuxDbusCompletion) {}) != kInvalidLinuxDbusRequestId ||
        transport.call_method(client, dispatcher,
            LinuxDbusMethodCall{"org.freedesktop.DBus", "/", "not an interface", "Ping", 2s},
            [](LinuxDbusCompletion) {}) != kInvalidLinuxDbusRequestId ||
        transport.call_method(client, dispatcher,
            LinuxDbusMethodCall{"org.freedesktop.DBus", "/", "org.example.Valid", "not a member", 2s},
            [](LinuxDbusCompletion) {}) != kInvalidLinuxDbusRequestId) {
        return EXIT_FAILURE;
    }

    std::size_t shutdown_callbacks = 0;
    LinuxDbusCompletion shutdown_result;
    const auto shutdown_id = transport.call_method(
        client, dispatcher,
        LinuxDbusMethodCall{slow_peer.unique_name(), "/org/nativeui/T072/Slow",
                            "org.nativeui.T072.Test", "Wait", 2s},
        [&](LinuxDbusCompletion result) {
            shutdown_result = std::move(result);
            ++shutdown_callbacks;
        });
    if (shutdown_id == kInvalidLinuxDbusRequestId ||
        transport.pending_request_count() != 1) {
        return EXIT_FAILURE;
    }

    transport.stop();
    if (!drain_until(owner, [&] { return shutdown_callbacks == 1; }, 2s) ||
        shutdown_result.code != LinuxDbusErrorCode::Shutdown ||
        transport.pending_request_count() != 0) {
        return EXIT_FAILURE;
    }

    std::this_thread::sleep_for(150ms);
    (void)owner.checkpoint();
    if (shutdown_callbacks != 1 ||
        !slow_peer.unregister_object_path(slow_peer_client, slow_path)) {
        return EXIT_FAILURE;
    }

    slow_peer.release_client(slow_peer_client);
    slow_peer.stop();
    transport.stop();
    return EXIT_SUCCESS;
}
