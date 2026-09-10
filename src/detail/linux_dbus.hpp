#pragma once

#include <nativeui/dispatcher.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace ui::detail {

using LinuxDbusClientId = std::uint64_t;
using LinuxDbusRequestId = std::uint64_t;
using LinuxDbusSubscriptionId = std::uint64_t;
using LinuxDbusObjectRegistrationId = std::uint64_t;

inline constexpr LinuxDbusClientId kInvalidLinuxDbusClientId = 0;
inline constexpr LinuxDbusRequestId kInvalidLinuxDbusRequestId = 0;
inline constexpr LinuxDbusSubscriptionId kInvalidLinuxDbusSubscriptionId = 0;
inline constexpr LinuxDbusObjectRegistrationId kInvalidLinuxDbusObjectRegistrationId = 0;

inline constexpr std::size_t kLinuxDbusMaxPendingCalls = 1'024;
inline constexpr std::size_t kLinuxDbusMaxSubscriptions = 256;
inline constexpr std::size_t kLinuxDbusMaxObjectPaths = 256;

inline constexpr auto kLinuxDbusDefaultTimeout = std::chrono::seconds{30};
inline constexpr auto kLinuxDbusMinTimeout = std::chrono::milliseconds{1};
inline constexpr auto kLinuxDbusMaxTimeout = std::chrono::seconds{300};

enum class LinuxDbusErrorCode {
    None,
    InitializationFailed,
    BusUnavailable,
    Disconnected,
    ResourceLimit,
    InvalidArgument,
    Timeout,
    RemoteError,
    LocalProtocolError,
    Cancelled,
    Shutdown,
};

struct LinuxDbusCompletion final {
    LinuxDbusErrorCode code{LinuxDbusErrorCode::None};
    std::string remote_error_name;
    std::string message;
};

using LinuxDbusCompletionCallback = std::function<void(LinuxDbusCompletion)>;

[[nodiscard]] bool linux_dbus_library_probe() noexcept;
[[nodiscard]] bool linux_dbus_initialize_threads() noexcept;
[[nodiscard]] bool linux_dbus_valid_timeout(std::chrono::milliseconds timeout) noexcept;
[[nodiscard]] bool linux_dbus_valid_object_path(std::string_view path) noexcept;

/// Transport-local hard-limit ledger. It owns no libdbus objects and invokes no
/// callbacks; IDs are monotonically generated within each resource namespace.
/// Client ownership is checked on release so sibling Portal/accessibility
/// clients sharing one Application transport cannot release each other's slots.
class LinuxDbusResourceLedger final {
public:
    LinuxDbusResourceLedger();
    ~LinuxDbusResourceLedger();

    LinuxDbusResourceLedger(const LinuxDbusResourceLedger&) = delete;
    LinuxDbusResourceLedger& operator=(const LinuxDbusResourceLedger&) = delete;
    LinuxDbusResourceLedger(LinuxDbusResourceLedger&&) = delete;
    LinuxDbusResourceLedger& operator=(LinuxDbusResourceLedger&&) = delete;

    [[nodiscard]] LinuxDbusRequestId acquire_request(LinuxDbusClientId client);
    [[nodiscard]] bool release_request(LinuxDbusClientId client, LinuxDbusRequestId id);
    [[nodiscard]] std::size_t pending_request_count() const noexcept;

    [[nodiscard]] LinuxDbusSubscriptionId acquire_subscription(LinuxDbusClientId client);
    [[nodiscard]] bool release_subscription(LinuxDbusClientId client, LinuxDbusSubscriptionId id);
    [[nodiscard]] std::size_t subscription_count() const noexcept;

    [[nodiscard]] LinuxDbusObjectRegistrationId acquire_object_path(LinuxDbusClientId client);
    [[nodiscard]] bool release_object_path(LinuxDbusClientId client,
                                           LinuxDbusObjectRegistrationId id);
    [[nodiscard]] std::size_t object_path_count() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Internal terminal-state registry for outbound method calls. It owns no
/// DBusPendingCall objects yet: the libdbus send/reply layer binds to this seam
/// in the following TDD unit. Completion removes transport capacity before it
/// posts the copied result to the owning T065 Dispatcher. Dispatcher rejection
/// is terminal and never falls back to the D-Bus I/O thread.
class LinuxDbusPendingCallSet final {
public:
    explicit LinuxDbusPendingCallSet(LinuxDbusResourceLedger& ledger);
    ~LinuxDbusPendingCallSet();

    LinuxDbusPendingCallSet(const LinuxDbusPendingCallSet&) = delete;
    LinuxDbusPendingCallSet& operator=(const LinuxDbusPendingCallSet&) = delete;
    LinuxDbusPendingCallSet(LinuxDbusPendingCallSet&&) = delete;
    LinuxDbusPendingCallSet& operator=(LinuxDbusPendingCallSet&&) = delete;

    [[nodiscard]] LinuxDbusRequestId begin(LinuxDbusClientId client,
                                           ui::Dispatcher dispatcher,
                                           std::chrono::milliseconds timeout,
                                           LinuxDbusCompletionCallback callback);
    [[nodiscard]] bool complete(LinuxDbusClientId client,
                                LinuxDbusRequestId id,
                                LinuxDbusCompletion completion);
    [[nodiscard]] bool cancel(LinuxDbusClientId client, LinuxDbusRequestId id);
    void shutdown() noexcept;

    [[nodiscard]] std::size_t pending_count() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class LinuxDbusTransport final {
public:
    LinuxDbusTransport();
    ~LinuxDbusTransport();

    LinuxDbusTransport(const LinuxDbusTransport&) = delete;
    LinuxDbusTransport& operator=(const LinuxDbusTransport&) = delete;
    LinuxDbusTransport(LinuxDbusTransport&&) = delete;
    LinuxDbusTransport& operator=(LinuxDbusTransport&&) = delete;

    [[nodiscard]] LinuxDbusErrorCode start();
    void stop() noexcept;

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] std::string unique_name() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ui::detail
