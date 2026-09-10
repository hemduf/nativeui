#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace ui::detail {

using LinuxDbusRequestId = std::uint64_t;
using LinuxDbusSubscriptionId = std::uint64_t;
using LinuxDbusObjectRegistrationId = std::uint64_t;

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

[[nodiscard]] bool linux_dbus_library_probe() noexcept;
[[nodiscard]] bool linux_dbus_initialize_threads() noexcept;
[[nodiscard]] bool linux_dbus_valid_timeout(std::chrono::milliseconds timeout) noexcept;
[[nodiscard]] bool linux_dbus_valid_object_path(std::string_view path) noexcept;

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
