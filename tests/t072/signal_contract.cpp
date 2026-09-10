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

    LinuxDbusTransport transport;
    if (transport.start() != LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }

    DispatcherOwner owner;
    const auto dispatcher = owner.dispatcher();
    const auto ui_thread = std::this_thread::get_id();
    constexpr LinuxDbusClientId client = 41;
    constexpr LinuxDbusClientId sibling_client = 42;

    std::size_t callback_count = 0;
    std::thread::id callback_thread;
    LinuxDbusSignal received;

    LinuxDbusSignalMatch match;
    match.sender = "org.freedesktop.DBus";
    match.path = "/org/freedesktop/DBus";
    match.interface = "org.freedesktop.DBus";
    match.member = "NameOwnerChanged";

    const auto subscription = transport.subscribe_signal(
        client,
        dispatcher,
        match,
        [&](LinuxDbusSignal signal) {
            callback_thread = std::this_thread::get_id();
            received = std::move(signal);
            ++callback_count;
        });
    if (subscription == kInvalidLinuxDbusSubscriptionId ||
        transport.subscription_count() != 1 ||
        transport.unsubscribe_signal(sibling_client, subscription)) {
        return EXIT_FAILURE;
    }

    LinuxDbusTransport peer;
    if (peer.start() != LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }
    const auto peer_name = peer.unique_name();

    if (!drain_until(owner, [&] { return callback_count == 1; }, 2s) ||
        callback_thread != ui_thread ||
        received.path != "/org/freedesktop/DBus" ||
        received.interface != "org.freedesktop.DBus" ||
        received.member != "NameOwnerChanged" ||
        received.arguments.size() != 3 ||
        received.arguments[0].kind != LinuxDbusValueKind::String ||
        received.arguments[0].text != peer_name ||
        received.arguments[1].kind != LinuxDbusValueKind::String ||
        received.arguments[2].kind != LinuxDbusValueKind::String) {
        return EXIT_FAILURE;
    }

    if (!transport.unsubscribe_signal(client, subscription) ||
        transport.subscription_count() != 0 ||
        transport.unsubscribe_signal(client, subscription)) {
        return EXIT_FAILURE;
    }

    LinuxDbusTransport later_peer;
    if (later_peer.start() != LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }
    const auto quiet_deadline = std::chrono::steady_clock::now() + 100ms;
    while (std::chrono::steady_clock::now() < quiet_deadline) {
        (void)owner.checkpoint();
        std::this_thread::sleep_for(1ms);
    }
    if (callback_count != 1) {
        return EXIT_FAILURE;
    }

    LinuxDbusSignalMatch invalid = match;
    invalid.path = "not/a/path";
    if (transport.subscribe_signal(client, dispatcher, invalid,
                                   [](LinuxDbusSignal) {}) !=
        kInvalidLinuxDbusSubscriptionId) {
        return EXIT_FAILURE;
    }

    later_peer.stop();
    peer.stop();
    transport.stop();
    return EXIT_SUCCESS;
}
