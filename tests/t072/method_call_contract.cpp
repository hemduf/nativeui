#include "detail/linux_dbus.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
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

void stage(const char* name) {
    std::cerr << "T072 method_call stage: " << name << '\n';
}

} // namespace

int main() {
    using namespace std::chrono_literals;
    using namespace ui::detail;

    stage("start");
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
        stage("basic-call-failed");
        return EXIT_FAILURE;
    }
    stage("basic-call-ok");

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
        stage("argument-call-failed");
        return EXIT_FAILURE;
    }
    stage("argument-call-ok");

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
        stage("remote-error-failed");
        return EXIT_FAILURE;
    }
    stage("remote-error-ok");

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
        stage("cancel-failed");
        return EXIT_FAILURE;
    }
    stage("cancel-ok");

    LinuxDbusTransport slow_peer;
    if (slow_peer.start() != LinuxDbusErrorCode::None) {
        stage("slow-peer-start-failed");
        return EXIT_FAILURE;
    }
    constexpr LinuxDbusClientId slow_peer_client = 2;
    const auto slow_path = slow_peer.register_object_path(
        slow_peer_client, "/org/nativeui/T072/Slow",
        [](const LinuxDbusMethodRequest&) {
            std::this_thread::sleep_for(100ms);
            return LinuxDbusMethodReply::method_return({LinuxDbusValue::string("late")});
        });
    if (slow_path == kInvalidLinuxDbusObjectRegistrationId) {
        stage("slow-path-register-failed");
        return EXIT_FAILURE;
    }
    stage("slow-path-ok");

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
    const bool timeout_observed =
        timeout_id != kInvalidLinuxDbusRequestId &&
        drain_until(owner, [&] { return timeout_callbacks == 1; }, 2s);
    std::cerr << "T072 method_call timeout: observed=" << timeout_observed
              << " callbacks=" << timeout_callbacks
              << " code=" << static_cast<int>(timeout_result.code)
              << " pending=" << transport.pending_request_count() << '\n';
    if (!timeout_observed || timeout_result.code != LinuxDbusErrorCode::Timeout ||
        transport.pending_request_count() != 0 ||
        transport.cancel_request(client, timeout_id)) {
        stage("timeout-contract-failed");
        return EXIT_FAILURE;
    }

    std::this_thread::sleep_for(150ms);
    (void)owner.checkpoint();
    if (timeout_callbacks != 1 ||
        !slow_peer.unregister_object_path(slow_peer_client, slow_path)) {
        stage("timeout-cleanup-failed");
        return EXIT_FAILURE;
    }
    slow_peer.stop();
    stage("timeout-ok");

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
        stage("invalid-input-failed");
        return EXIT_FAILURE;
    }

    transport.stop();
    stage("complete");
    return EXIT_SUCCESS;
}
