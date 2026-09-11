#include "detail/linux_dbus.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <new>
#include <thread>
#include <vector>

namespace {

thread_local std::ptrdiff_t g_fail_allocation_after = -1;

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
    std::cerr << "T072 signal stage: " << name << '\n';
}

bool signal_setup_allocation_failures_are_atomic(
    ui::detail::LinuxDbusTransport& transport,
    ui::Dispatcher dispatcher,
    ui::detail::LinuxDbusClientId client,
    const ui::detail::LinuxDbusSignalMatch& match) {
    using namespace ui::detail;

    bool observed_failure = false;
    for (std::ptrdiff_t fail_after = 0; fail_after < 64; ++fail_after) {
        g_fail_allocation_after = fail_after;
        const auto id = transport.subscribe_signal(
            client, dispatcher, match, [](LinuxDbusSignal) {});
        g_fail_allocation_after = -1;

        if (id == kInvalidLinuxDbusSubscriptionId) {
            observed_failure = true;
            if (transport.subscription_count() != 0) {
                return false;
            }
            continue;
        }

        if (!observed_failure || transport.subscription_count() != 1 ||
            !transport.unsubscribe_signal(client, id) ||
            transport.subscription_count() != 0) {
            return false;
        }
        return true;
    }

    g_fail_allocation_after = -1;
    return false;
}

} // namespace

void* operator new(std::size_t size) {
    if (g_fail_allocation_after == 0) {
        throw std::bad_alloc{};
    }
    if (g_fail_allocation_after > 0) {
        --g_fail_allocation_after;
    }
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

int main() {
    using namespace std::chrono_literals;
    using namespace ui::detail;

    stage("start");
    LinuxDbusTransport transport;
    if (transport.start() != LinuxDbusErrorCode::None) return EXIT_FAILURE;

    DispatcherOwner owner;
    const auto dispatcher = owner.dispatcher();
    const auto ui_thread = std::this_thread::get_id();
    const auto client = transport.register_client();
    const auto sibling_client = transport.register_client();
    if (client == kInvalidLinuxDbusClientId ||
        sibling_client == kInvalidLinuxDbusClientId || client == sibling_client) {
        return EXIT_FAILURE;
    }

    std::size_t callback_count = 0;
    std::thread::id callback_thread;
    LinuxDbusSignal received;
    LinuxDbusSignalMatch match;
    match.sender = "org.freedesktop.DBus";
    match.path = "/org/freedesktop/DBus";
    match.interface = "org.freedesktop.DBus";
    match.member = "NameOwnerChanged";

    if (!signal_setup_allocation_failures_are_atomic(
            transport, dispatcher, client, match)) {
        return EXIT_FAILURE;
    }
    stage("allocation-failure-rollback-ok");

    const auto subscription = transport.subscribe_signal(client, dispatcher, match,
        [&](LinuxDbusSignal signal) {
            callback_thread = std::this_thread::get_id();
            received = std::move(signal);
            ++callback_count;
        });
    if (subscription == kInvalidLinuxDbusSubscriptionId ||
        transport.subscription_count() != 1 ||
        transport.unsubscribe_signal(sibling_client, subscription)) return EXIT_FAILURE;
    stage("first-subscription-ok");

    LinuxDbusTransport peer;
    if (peer.start() != LinuxDbusErrorCode::None) return EXIT_FAILURE;
    const auto peer_client = peer.register_client();
    if (peer_client == kInvalidLinuxDbusClientId) return EXIT_FAILURE;
    const auto peer_name = peer.unique_name();
    if (!drain_until(owner, [&] { return callback_count == 1; }, 2s) ||
        callback_thread != ui_thread || received.path != "/org/freedesktop/DBus" ||
        received.interface != "org.freedesktop.DBus" || received.member != "NameOwnerChanged" ||
        received.arguments.size() != 3 ||
        received.arguments[0].kind != LinuxDbusValueKind::String ||
        received.arguments[0].text != peer_name ||
        received.arguments[1].kind != LinuxDbusValueKind::String ||
        received.arguments[2].kind != LinuxDbusValueKind::String) return EXIT_FAILURE;
    stage("first-delivery-ok");

    if (!transport.unsubscribe_signal(client, subscription) ||
        transport.subscription_count() != 0 ||
        transport.unsubscribe_signal(client, subscription)) return EXIT_FAILURE;
    stage("first-unsubscribe-ok");

    LinuxDbusTransport later_peer;
    if (later_peer.start() != LinuxDbusErrorCode::None) return EXIT_FAILURE;
    const auto quiet_deadline = std::chrono::steady_clock::now() + 100ms;
    while (std::chrono::steady_clock::now() < quiet_deadline) {
        (void)owner.checkpoint();
        std::this_thread::sleep_for(1ms);
    }
    if (callback_count != 1) return EXIT_FAILURE;
    stage("post-unsubscribe-quiet-ok");

    LinuxDbusSignalMatch invalid = match;
    invalid.path = "not/a/path";
    if (transport.subscribe_signal(client, dispatcher, invalid, [](LinuxDbusSignal) {}) !=
        kInvalidLinuxDbusSubscriptionId) return EXIT_FAILURE;

    LinuxDbusSignalMatch never_emitted;
    never_emitted.interface = "org.nativeui.T072.Capacity";
    never_emitted.member = "NeverEmitted";
    std::vector<LinuxDbusSubscriptionId> capacity_subscriptions;
    capacity_subscriptions.reserve(kLinuxDbusMaxSubscriptions);
    stage("capacity-subscribe-begin");
    for (std::size_t i = 0; i < kLinuxDbusMaxSubscriptions; ++i) {
        const auto id = transport.subscribe_signal(client, dispatcher, never_emitted,
                                                   [](LinuxDbusSignal) {});
        if (id == kInvalidLinuxDbusSubscriptionId) {
            std::cerr << "T072 signal capacity subscribe failed at index=" << i << '\n';
            return EXIT_FAILURE;
        }
        capacity_subscriptions.push_back(id);
    }
    stage("capacity-subscribe-complete");
    if (transport.subscription_count() != kLinuxDbusMaxSubscriptions ||
        transport.subscribe_signal(client, dispatcher, never_emitted, [](LinuxDbusSignal) {}) !=
            kInvalidLinuxDbusSubscriptionId) return EXIT_FAILURE;
    stage("capacity-overflow-ok");
    for (std::size_t i = 0; i < capacity_subscriptions.size(); ++i) {
        if (!transport.unsubscribe_signal(client, capacity_subscriptions[i])) {
            std::cerr << "T072 signal capacity unsubscribe failed at index=" << i << '\n';
            return EXIT_FAILURE;
        }
    }
    stage("capacity-unsubscribe-complete");
    if (transport.subscription_count() != 0) return EXIT_FAILURE;

    std::size_t emitted_callback_count = 0;
    LinuxDbusSignal emitted_signal;
    LinuxDbusSignalMatch emitted_match;
    emitted_match.path = "/org/nativeui/T072/Signal";
    emitted_match.interface = "org.nativeui.T072.Test";
    emitted_match.member = "Changed";
    const auto emitted_subscription = peer.subscribe_signal(
        peer_client, dispatcher, emitted_match,
        [&](LinuxDbusSignal signal) {
            emitted_signal = std::move(signal);
            ++emitted_callback_count;
        });
    if (emitted_subscription == kInvalidLinuxDbusSubscriptionId) return EXIT_FAILURE;

    const std::vector<LinuxDbusValue> emitted_arguments{
        LinuxDbusValue::string("payload"), LinuxDbusValue::uint32(7)};
    if (!transport.send_signal("/org/nativeui/T072/Signal", "org.nativeui.T072.Test",
                               "Changed", emitted_arguments) ||
        !drain_until(owner, [&] { return emitted_callback_count == 1; }, 2s) ||
        emitted_signal.sender != transport.unique_name() ||
        emitted_signal.path != "/org/nativeui/T072/Signal" ||
        emitted_signal.interface != "org.nativeui.T072.Test" ||
        emitted_signal.member != "Changed" || emitted_signal.arguments != emitted_arguments ||
        transport.send_signal("relative/path", "org.nativeui.T072.Test", "Changed", {}) ||
        transport.send_signal("/org/nativeui/T072/Signal", "not an interface", "Changed", {}) ||
        transport.send_signal("/org/nativeui/T072/Signal", "org.nativeui.T072.Test", "bad.member", {}))
        return EXIT_FAILURE;
    stage("emitted-signal-ok");
    if (!peer.unsubscribe_signal(peer_client, emitted_subscription)) return EXIT_FAILURE;

    transport.release_client(client);
    transport.release_client(sibling_client);
    peer.release_client(peer_client);
    later_peer.stop();
    peer.stop();
    transport.stop();
    stage("complete");
    return EXIT_SUCCESS;
}
