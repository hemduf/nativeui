#include "detail/linux_dbus.hpp"
#include "fd_probe.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

#include <dbus/dbus.h>

namespace {

using namespace std::chrono_literals;

constexpr const char* kAtSpiBusAddressVariable = "AT_SPI_BUS_ADDRESS";
constexpr const char* kServiceName = "org.a11y.Bus";
constexpr const char* kObjectPath = "/org/a11y/bus";
constexpr const char* kInterface = "org.a11y.Bus";
constexpr const char* kMember = "GetAddress";

enum class FakeReplyMode {
    Address,
    Empty,
    WrongType,
    ErrorReply,
    NoReply,
};

/// Minimal `org.a11y.Bus` launcher used as the deterministic test bus service.
/// It owns one private session connection, answers only the exact GetAddress
/// call and runs its dispatch loop on a worker thread so the discovery helper
/// can block on the caller thread while the reply is produced.
class FakeAccessibilityBusService final {
public:
    explicit FakeAccessibilityBusService(const std::string& session_address) {
        DBusError error;
        dbus_error_init(&error);
        connection_ = dbus_connection_open_private(session_address.c_str(), &error);
        dbus_error_free(&error);
        if (connection_ == nullptr) {
            return;
        }
        dbus_connection_set_exit_on_disconnect(connection_, FALSE);

        dbus_error_init(&error);
        const dbus_bool_t registered = dbus_bus_register(connection_, &error);
        dbus_error_free(&error);
        if (registered == FALSE) {
            close_connection();
            return;
        }

        dbus_error_init(&error);
        const int name_result = dbus_bus_request_name(
            connection_, kServiceName, DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
        dbus_error_free(&error);
        if (name_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER &&
            name_result != DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER) {
            close_connection();
            return;
        }

        static const DBusObjectPathVTable vtable = {
            nullptr,
            &FakeAccessibilityBusService::message_thunk,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
        };
        if (dbus_connection_register_object_path(connection_, kObjectPath, &vtable, this) ==
            FALSE) {
            close_connection();
            return;
        }
        path_registered_ = true;

        try {
            worker_ = std::thread([this] { dispatch_loop(); });
        } catch (...) {
            (void)dbus_connection_unregister_object_path(connection_, kObjectPath);
            (void)dbus_bus_release_name(connection_, kServiceName, nullptr);
            close_connection();
            return;
        }
        ready_ = true;
    }

    ~FakeAccessibilityBusService() {
        stop_.store(true, std::memory_order_release);
        if (worker_.joinable()) {
            worker_.join();
        }
        if (connection_ != nullptr) {
            if (path_registered_) {
                (void)dbus_connection_unregister_object_path(connection_, kObjectPath);
            }
            (void)dbus_bus_release_name(connection_, kServiceName, nullptr);
            dbus_connection_close(connection_);
            dbus_connection_unref(connection_);
        }
    }

    FakeAccessibilityBusService(const FakeAccessibilityBusService&) = delete;
    FakeAccessibilityBusService& operator=(const FakeAccessibilityBusService&) = delete;
    FakeAccessibilityBusService(FakeAccessibilityBusService&&) = delete;
    FakeAccessibilityBusService& operator=(FakeAccessibilityBusService&&) = delete;

    [[nodiscard]] bool ready() const noexcept {
        return ready_;
    }

    void set_mode(FakeReplyMode mode) noexcept {
        mode_.store(mode, std::memory_order_release);
    }

    void set_reply_address(std::string address) {
        std::lock_guard lock{mutex_};
        reply_address_ = std::move(address);
    }

    [[nodiscard]] int call_count() const noexcept {
        return calls_.load(std::memory_order_acquire);
    }

private:
    void close_connection() {
        if (connection_ != nullptr) {
            dbus_connection_close(connection_);
            dbus_connection_unref(connection_);
            connection_ = nullptr;
        }
    }

    void dispatch_loop() noexcept {
        while (!stop_.load(std::memory_order_acquire)) {
            if (dbus_connection_read_write_dispatch(connection_, 20) == FALSE) {
                return;
            }
        }
    }

    static DBusHandlerResult message_thunk(DBusConnection*,
                                           DBusMessage* message,
                                           void* data) noexcept {
        if (message == nullptr || data == nullptr) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        try {
            return static_cast<FakeAccessibilityBusService*>(data)->handle_message(message);
        } catch (...) {
            return DBUS_HANDLER_RESULT_NEED_MEMORY;
        }
    }

    DBusHandlerResult handle_message(DBusMessage* message) {
        if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL ||
            dbus_message_has_path(message, kObjectPath) == FALSE ||
            dbus_message_has_interface(message, kInterface) == FALSE ||
            dbus_message_has_member(message, kMember) == FALSE) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        calls_.fetch_add(1, std::memory_order_acq_rel);
        switch (mode_.load(std::memory_order_acquire)) {
        case FakeReplyMode::NoReply:
            // The call is deliberately left unanswered so the client-side
            // timeout path is exercised deterministically.
            return DBUS_HANDLER_RESULT_HANDLED;
        case FakeReplyMode::ErrorReply: {
            DBusMessage* reply = dbus_message_new_error(
                message, "org.nativeui.T072.TestError",
                "simulated accessibility launcher failure");
            return send_reply(reply);
        }
        case FakeReplyMode::Empty:
        case FakeReplyMode::WrongType:
        case FakeReplyMode::Address: {
            DBusMessage* reply = dbus_message_new_method_return(message);
            if (reply == nullptr) {
                return DBUS_HANDLER_RESULT_NEED_MEMORY;
            }
            if (mode_.load(std::memory_order_acquire) == FakeReplyMode::WrongType) {
                dbus_int32_t value = 7;
                (void)dbus_message_append_args(reply, DBUS_TYPE_INT32, &value,
                                               DBUS_TYPE_INVALID);
            } else {
                std::string text;
                if (mode_.load(std::memory_order_acquire) == FakeReplyMode::Empty) {
                    text.clear();
                } else {
                    std::lock_guard lock{mutex_};
                    text = reply_address_;
                }
                const char* raw = text.c_str();
                (void)dbus_message_append_args(reply, DBUS_TYPE_STRING, &raw,
                                               DBUS_TYPE_INVALID);
            }
            return send_reply(reply);
        }
        }
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    DBusHandlerResult send_reply(DBusMessage* reply) noexcept {
        if (reply == nullptr) {
            return DBUS_HANDLER_RESULT_NEED_MEMORY;
        }
        (void)dbus_connection_send(connection_, reply, nullptr);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    DBusConnection* connection_{};
    std::thread worker_;
    std::atomic<bool> stop_{false};
    std::atomic<FakeReplyMode> mode_{FakeReplyMode::Address};
    std::atomic<int> calls_{0};
    std::mutex mutex_;
    std::string reply_address_;
    bool path_registered_{};
    bool ready_{};
};

[[nodiscard]] const char* environment_value(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return nullptr;
    }
    return value;
}

[[nodiscard]] bool set_environment_value(const char* name, const char* value) {
    return setenv(name, value, 1) == 0;
}

[[nodiscard]] bool unset_environment_value(const char* name) {
    return unsetenv(name) == 0;
}

/// Waits until the fake service observed `expected` GetAddress calls, then
/// proves there was no extra/retried attempt.
[[nodiscard]] bool wait_for_exact_call_count(const FakeAccessibilityBusService& service,
                                             int expected,
                                             std::chrono::milliseconds limit) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (service.call_count() < expected && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    if (service.call_count() != expected) {
        return false;
    }
    // Give a retry loop time to show up before accepting "exactly one".
    std::this_thread::sleep_for(50ms);
    return service.call_count() == expected;
}

[[nodiscard]] bool discovery_matches(const ui::detail::LinuxDbusBusAddressDiscovery& discovery,
                                     ui::detail::LinuxDbusErrorCode expected_code,
                                     const std::string& expected_address) {
    using ui::detail::LinuxDbusErrorCode;
    if (discovery.code != expected_code) {
        return false;
    }
    if (expected_code == LinuxDbusErrorCode::None) {
        return discovery.available() && discovery.address == expected_address;
    }
    return !discovery.available() && discovery.address.empty();
}

} // namespace

int main() {
    using namespace std::chrono_literals;
    using namespace ui::detail;

    const char* address_env = environment_value("DBUS_SESSION_BUS_ADDRESS");
    if (address_env == nullptr) {
        return EXIT_FAILURE;
    }
    const std::string session_address{address_env};
    if (!linux_dbus_valid_bus_address(session_address)) {
        return EXIT_FAILURE;
    }

    FakeAccessibilityBusService service{session_address};
    if (!service.ready()) {
        return EXIT_FAILURE;
    }
    service.set_reply_address(session_address);
    service.set_mode(FakeReplyMode::Address);

    // The borrowed session transport is created through the explicit-address
    // path, so libdbus's process-global session address cache stays untouched
    // until the first temporary-connection call below.
    LinuxDbusTransport session_transport;
    if (session_transport.start(session_address) != LinuxDbusErrorCode::None ||
        !session_transport.running()) {
        return EXIT_FAILURE;
    }

    // Environment override precedence: a non-empty AT_SPI_BUS_ADDRESS wins with
    // no bus round-trip, even when a live session transport is available.
    const std::string first_override{"unix:path=/tmp/nativeui-t072-a11y-override"};
    if (!set_environment_value(kAtSpiBusAddressVariable, first_override.c_str())) {
        return EXIT_FAILURE;
    }
    if (!discovery_matches(
            LinuxDbusTransport::discover_accessibility_bus_address(&session_transport, 5s),
            LinuxDbusErrorCode::None, first_override) ||
        !discovery_matches(
            LinuxDbusTransport::discover_accessibility_bus_address(nullptr, 1ms),
            LinuxDbusErrorCode::None, first_override) ||
        !wait_for_exact_call_count(service, 0, 100ms)) {
        return EXIT_FAILURE;
    }

    // No process-global cache: a changed override is honored immediately.
    const std::string second_override{"unix:path=/tmp/nativeui-t072-a11y-override-two"};
    if (!set_environment_value(kAtSpiBusAddressVariable, second_override.c_str()) ||
        !discovery_matches(
            LinuxDbusTransport::discover_accessibility_bus_address(&session_transport, 5s),
            LinuxDbusErrorCode::None, second_override) ||
        !wait_for_exact_call_count(service, 0, 100ms)) {
        return EXIT_FAILURE;
    }

    // An empty override is not an override: exactly one GetAddress call is
    // issued through the borrowed transport.
    if (!set_environment_value(kAtSpiBusAddressVariable, "") ||
        !discovery_matches(
            LinuxDbusTransport::discover_accessibility_bus_address(&session_transport, 5s),
            LinuxDbusErrorCode::None, session_address) ||
        !wait_for_exact_call_count(service, 1, 2s)) {
        return EXIT_FAILURE;
    }
    if (!unset_environment_value(kAtSpiBusAddressVariable)) {
        return EXIT_FAILURE;
    }

    // Borrowed-transport proof: point the session address cache at an unusable
    // address. A temporary session connection would now fail while the live
    // borrowed transport still reaches the fake service.
    const std::string bogus_session_address =
        "unix:path=/nonexistent/nativeui-t072-no-session";
    if (!set_environment_value("DBUS_SESSION_BUS_ADDRESS", bogus_session_address.c_str()) ||
        !discovery_matches(
            LinuxDbusTransport::discover_accessibility_bus_address(&session_transport, 5s),
            LinuxDbusErrorCode::None, session_address) ||
        !wait_for_exact_call_count(service, 2, 2s)) {
        return EXIT_FAILURE;
    }
    if (!set_environment_value("DBUS_SESSION_BUS_ADDRESS", session_address.c_str())) {
        return EXIT_FAILURE;
    }

    session_transport.stop();

    // One-shot temporary session connection, opened only for the call and
    // closed before returning. The surrounding descriptor count proves the
    // connection does not leak on any of the following paths.
    const std::size_t fd_before = t072_test::open_file_descriptor_count();

    {
        const auto discovery =
            LinuxDbusTransport::discover_accessibility_bus_address(nullptr, 5s);
        if (!discovery_matches(discovery, LinuxDbusErrorCode::None, session_address) ||
            !wait_for_exact_call_count(service, 3, 2s)) {
            return EXIT_FAILURE;
        }
    }

    // A supplied but stopped transport is not available; the helper falls back
    // to its one-shot temporary connection.
    {
        const auto discovery = LinuxDbusTransport::discover_accessibility_bus_address(
            &session_transport, 5s);
        if (!discovery_matches(discovery, LinuxDbusErrorCode::None, session_address) ||
            !wait_for_exact_call_count(service, 4, 2s)) {
            return EXIT_FAILURE;
        }
    }

    // Malformed replies and remote failures each return a bounded error with
    // exactly one attempt and no leaked temporary connection.
    service.set_mode(FakeReplyMode::Empty);
    {
        const auto discovery =
            LinuxDbusTransport::discover_accessibility_bus_address(nullptr, 5s);
        if (!discovery_matches(discovery, LinuxDbusErrorCode::LocalProtocolError, {}) ||
            !wait_for_exact_call_count(service, 5, 2s)) {
            return EXIT_FAILURE;
        }
    }

    service.set_mode(FakeReplyMode::WrongType);
    {
        const auto discovery =
            LinuxDbusTransport::discover_accessibility_bus_address(nullptr, 5s);
        if (!discovery_matches(discovery, LinuxDbusErrorCode::LocalProtocolError, {}) ||
            !wait_for_exact_call_count(service, 6, 2s)) {
            return EXIT_FAILURE;
        }
    }

    service.set_mode(FakeReplyMode::ErrorReply);
    {
        const auto discovery =
            LinuxDbusTransport::discover_accessibility_bus_address(nullptr, 5s);
        if (!discovery_matches(discovery, LinuxDbusErrorCode::RemoteError, {}) ||
            !wait_for_exact_call_count(service, 7, 2s)) {
            return EXIT_FAILURE;
        }
    }

    service.set_mode(FakeReplyMode::NoReply);
    {
        const auto begin = std::chrono::steady_clock::now();
        const auto discovery =
            LinuxDbusTransport::discover_accessibility_bus_address(nullptr, 1ms);
        const auto elapsed = std::chrono::steady_clock::now() - begin;
        if (!discovery_matches(discovery, LinuxDbusErrorCode::Timeout, {}) ||
            !wait_for_exact_call_count(service, 8, 2s) || elapsed >= 5s) {
            return EXIT_FAILURE;
        }
    }

    // Recovery after the timeout: the next normal call still succeeds.
    service.set_mode(FakeReplyMode::Address);
    {
        const auto discovery =
            LinuxDbusTransport::discover_accessibility_bus_address(nullptr, 5s);
        if (!discovery_matches(discovery, LinuxDbusErrorCode::None, session_address) ||
            !wait_for_exact_call_count(service, 9, 2s)) {
            return EXIT_FAILURE;
        }
    }

    // Invalid timeouts are rejected before any bus round-trip.
    const std::chrono::milliseconds invalid_timeouts[] = {0ms, 301s, 300s + 1ms};
    for (const auto invalid : invalid_timeouts) {
        if (!discovery_matches(
                LinuxDbusTransport::discover_accessibility_bus_address(nullptr, invalid),
                LinuxDbusErrorCode::InvalidArgument, {})) {
            return EXIT_FAILURE;
        }
    }
    if (!wait_for_exact_call_count(service, 9, 100ms)) {
        return EXIT_FAILURE;
    }

    const std::size_t fd_after = t072_test::open_file_descriptor_count();
    if (fd_before != static_cast<std::size_t>(-1) && fd_after != fd_before) {
        return EXIT_FAILURE;
    }

    // End to end: the discovered address starts a real transport and registers
    // a client, exactly like the explicit-address mode contract.
    {
        service.set_mode(FakeReplyMode::Address);
        const auto discovery =
            LinuxDbusTransport::discover_accessibility_bus_address(nullptr, 5s);
        if (!discovery.available() || !wait_for_exact_call_count(service, 10, 2s)) {
            return EXIT_FAILURE;
        }
        LinuxDbusTransport accessibility;
        if (accessibility.start(discovery.address) != LinuxDbusErrorCode::None ||
            !accessibility.running() ||
            accessibility.register_client() == kInvalidLinuxDbusClientId) {
            return EXIT_FAILURE;
        }
        accessibility.stop();
    }

    session_transport.stop();
    return EXIT_SUCCESS;
}
