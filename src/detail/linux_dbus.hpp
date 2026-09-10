#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

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

} // namespace ui::detail
