#include "detail/linux_dbus.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <cstdlib>
#include <functional>
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

    constexpr LinuxDbusClientId client_a = 1;
    constexpr LinuxDbusClientId client_b = 2;

    // Client teardown must suppress work that has already been accepted by the
    // UI dispatcher without suppressing a sibling client's accepted work.
    {
        LinuxDbusResourceLedger ledger;
        LinuxDbusPendingCallSet calls{ledger};
        DispatcherOwner owner;
        std::size_t client_a_callbacks = 0;
        std::size_t client_b_callbacks = 0;

        const auto request_a = calls.begin(
            client_a, owner.dispatcher(), 1s,
            [&](LinuxDbusCompletion) { ++client_a_callbacks; });
        const auto request_b = calls.begin(
            client_b, owner.dispatcher(), 1s,
            [&](LinuxDbusCompletion) { ++client_b_callbacks; });
        if (request_a == kInvalidLinuxDbusRequestId ||
            request_b == kInvalidLinuxDbusRequestId ||
            !calls.complete(client_a, request_a, LinuxDbusCompletion{}) ||
            !calls.complete(client_b, request_b, LinuxDbusCompletion{})) {
            return EXIT_FAILURE;
        }

        calls.discard_client(client_a);
        (void)owner.checkpoint();
        if (client_a_callbacks != 0 || client_b_callbacks != 1 ||
            calls.pending_count() != 0 || ledger.pending_request_count() != 0) {
            return EXIT_FAILURE;
        }
    }

    LinuxDbusTransport transport;
    if (transport.start() != LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }

    DispatcherOwner owner_a;
    DispatcherOwner owner_b;

    const auto path_a = transport.register_object_path(
        client_a,
        "/org/nativeui/T072/ClientA",
        [](const LinuxDbusMethodRequest&) {
            return LinuxDbusMethodReply::method_return({LinuxDbusValue::string("a")});
        });
    const auto path_b = transport.register_object_path(
        client_b,
        "/org/nativeui/T072/ClientB",
        [](const LinuxDbusMethodRequest&) {
            return LinuxDbusMethodReply::method_return({LinuxDbusValue::string("b")});
        });

    const LinuxDbusSignalMatch match{
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "NameOwnerChanged",
    };
    const auto signal_a = transport.subscribe_signal(
        client_a, owner_a.dispatcher(), match, [](LinuxDbusSignal) {});
    const auto signal_b = transport.subscribe_signal(
        client_b, owner_b.dispatcher(), match, [](LinuxDbusSignal) {});

    if (path_a == kInvalidLinuxDbusObjectRegistrationId ||
        path_b == kInvalidLinuxDbusObjectRegistrationId ||
        signal_a == kInvalidLinuxDbusSubscriptionId ||
        signal_b == kInvalidLinuxDbusSubscriptionId ||
        transport.object_path_count() != 2 || transport.subscription_count() != 2) {
        return EXIT_FAILURE;
    }

    transport.release_client(client_a);
    transport.release_client(client_a); // idempotent teardown

    if (transport.object_path_count() != 1 || transport.subscription_count() != 1 ||
        transport.unregister_object_path(client_a, path_b) ||
        transport.unsubscribe_signal(client_a, signal_b)) {
        return EXIT_FAILURE;
    }

    bool reply_done = false;
    LinuxDbusCompletion reply;
    const auto request = transport.call_method(
        client_b,
        owner_b.dispatcher(),
        LinuxDbusMethodCall{
            transport.unique_name(),
            "/org/nativeui/T072/ClientB",
            "org.nativeui.T072.Test",
            "Ping",
            2s,
        },
        [&](LinuxDbusCompletion result) {
            reply = std::move(result);
            reply_done = true;
        });
    if (request == kInvalidLinuxDbusRequestId ||
        !drain_until(owner_b, [&] { return reply_done; }, 2s) ||
        reply.code != LinuxDbusErrorCode::None || reply.values.size() != 1 ||
        reply.values.front() != LinuxDbusValue::string("b")) {
        return EXIT_FAILURE;
    }

    if (!transport.unregister_object_path(client_b, path_b) ||
        !transport.unsubscribe_signal(client_b, signal_b) ||
        transport.object_path_count() != 0 || transport.subscription_count() != 0) {
        return EXIT_FAILURE;
    }

    transport.stop();
    return EXIT_SUCCESS;
}
