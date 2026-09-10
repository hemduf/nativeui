#include "detail/linux_dbus.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <thread>
#include <vector>

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

    LinuxDbusSignalMatch never_emitted;
    never_emitted.interface = "org.nativeui.T072.Capacity";
    never_emitted.member = "NeverEmitted";
    std::vector<LinuxDbusSubscriptionId> capacity_subscriptions;
    capacity_subscriptions.reserve(kLinuxDbusMaxSubscriptions);
    for (std::size_t i = 0; i < kLinuxDbusMaxSubscriptions; ++i) {
        const auto id = transport.subscribe_signal(
            client, dispatcher, never_emitted, [](LinuxDbusSignal) {});
        if (id == kInvalidLinuxDbusSubscriptionId) {
            return EXIT_FAILURE;
        }
        capacity_subscriptions.push_back(id);
    }
    if (transport.subscription_count() != kLinuxDbusMaxSubscriptions ||
        transport.subscribe_signal(client, dispatcher, never_emitted,
                                   [](LinuxDbusSignal) {}) !=
            kInvalidLinuxDbusSubscriptionId) {
        return EXIT_FAILURE;
    }
    for (const auto id : capacity_subscriptions) {
        if (!transport.unsubscribe_signal(client, id)) {
            return EXIT_FAILURE;
        }
    }
    if (transport.subscription_count() != 0) {
        return EXIT_FAILURE;
    }

    // T072 is shared by the Portal and AT-SPI clients. In addition to receiving
    // signals, the transport must be able to emit a bounded plain-value signal
    // without exposing libdbus objects to either client.
    std::size_t emitted_callback_count = 0;
    LinuxDbusSignal emitted_signal;
    LinuxDbusSignalMatch emitted_match;
    emitted_match.path = "/org/nativeui/T072/Signal";
    emitted_match.interface = "org.nativeui.T072.Test";
    emitted_match.member = "Changed";
    const auto emitted_subscription = peer.subscribe_signal(
        sibling_client,
        dispatcher,
        emitted_match,
        [&](LinuxDbusSignal signal) {
            emitted_signal = std::move(signal);
            ++emitted_callback_count;
        });
    if (emitted_subscription == kInvalidLinuxDbusSubscriptionId) {
        return EXIT_FAILURE;
    }

    const std::vector<LinuxDbusValue> emitted_arguments{
        LinuxDbusValue::string("payload"),
        LinuxDbusValue::uint32(7),
    };
    if (!transport.send_signal(
            "/org/nativeui/T072/Signal",
            "org.nativeui.T072.Test",
            "Changed",
            emitted_arguments) ||
        !drain_until(owner, [&] { return emitted_callback_count == 1; }, 2s) ||
        emitted_signal.sender != transport.unique_name() ||
        emitted_signal.path != "/org/nativeui/T072/Signal" ||
        emitted_signal.interface != "org.nativeui.T072.Test" ||
        emitted_signal.member != "Changed" ||
        emitted_signal.arguments != emitted_arguments ||
        transport.send_signal("relative/path", "org.nativeui.T072.Test", "Changed", {}) ||
        transport.send_signal("/org/nativeui/T072/Signal", "not an interface", "Changed", {}) ||
        transport.send_signal("/org/nativeui/T072/Signal", "org.nativeui.T072.Test", "bad.member", {})) {
        return EXIT_FAILURE;
    }
    if (!peer.unsubscribe_signal(sibling_client, emitted_subscription)) {
        return EXIT_FAILURE;
    }

    later_peer.stop();
    peer.stop();
    transport.stop();
    return EXIT_SUCCESS;
}
