#pragma once

#include <nativeui/dispatcher.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

enum class LinuxDbusValueKind {
    Boolean,
    Byte,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Double,
    String,
    ObjectPath,
    Signature,
    Array,
    Dictionary,
    Variant,
    Struct,
};

/// Internal plain-value representation shared by the Portal/accessibility
/// clients. It contains no libdbus objects and owns all text/container data.
/// Container shape is validated before encoding; malformed values fail
/// atomically rather than being partially interpreted by callers.
struct LinuxDbusValue final {
    LinuxDbusValueKind kind{LinuxDbusValueKind::String};
    bool boolean_value{};
    std::uint8_t byte_value{};
    std::int16_t int16_value{};
    std::uint16_t uint16_value{};
    std::int32_t int32_value{};
    std::uint32_t uint32_value{};
    std::int64_t int64_value{};
    std::uint64_t uint64_value{};
    double double_value{};
    std::string text;
    std::string element_signature;
    std::vector<LinuxDbusValue> elements;
    std::vector<std::pair<std::string, LinuxDbusValue>> entries;

    [[nodiscard]] static LinuxDbusValue boolean(bool value) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::Boolean;
        result.boolean_value = value;
        return result;
    }
    [[nodiscard]] static LinuxDbusValue byte(std::uint8_t value) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::Byte;
        result.byte_value = value;
        return result;
    }
    [[nodiscard]] static LinuxDbusValue int16(std::int16_t value) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::Int16;
        result.int16_value = value;
        return result;
    }
    [[nodiscard]] static LinuxDbusValue uint16(std::uint16_t value) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::UInt16;
        result.uint16_value = value;
        return result;
    }
    [[nodiscard]] static LinuxDbusValue int32(std::int32_t value) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::Int32;
        result.int32_value = value;
        return result;
    }
    [[nodiscard]] static LinuxDbusValue uint32(std::uint32_t value) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::UInt32;
        result.uint32_value = value;
        return result;
    }
    [[nodiscard]] static LinuxDbusValue int64(std::int64_t value) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::Int64;
        result.int64_value = value;
        return result;
    }
    [[nodiscard]] static LinuxDbusValue uint64(std::uint64_t value) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::UInt64;
        result.uint64_value = value;
        return result;
    }
    [[nodiscard]] static LinuxDbusValue floating(double value) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::Double;
        result.double_value = value;
        return result;
    }
    [[nodiscard]] static LinuxDbusValue string(std::string value) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::String;
        result.text = std::move(value);
        return result;
    }
    [[nodiscard]] static LinuxDbusValue object_path(std::string value) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::ObjectPath;
        result.text = std::move(value);
        return result;
    }
    [[nodiscard]] static LinuxDbusValue signature(std::string value) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::Signature;
        result.text = std::move(value);
        return result;
    }
    [[nodiscard]] static LinuxDbusValue array(std::string element_type,
                                              std::vector<LinuxDbusValue> values) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::Array;
        result.element_signature = std::move(element_type);
        result.elements = std::move(values);
        return result;
    }
    [[nodiscard]] static LinuxDbusValue dictionary(
        std::vector<std::pair<std::string, LinuxDbusValue>> values) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::Dictionary;
        result.entries = std::move(values);
        return result;
    }
    [[nodiscard]] static LinuxDbusValue variant(LinuxDbusValue value) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::Variant;
        result.elements.push_back(std::move(value));
        return result;
    }
    [[nodiscard]] static LinuxDbusValue variant_many(std::vector<LinuxDbusValue> values) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::Variant;
        result.elements = std::move(values);
        return result;
    }
    [[nodiscard]] static LinuxDbusValue structure(std::vector<LinuxDbusValue> values) {
        LinuxDbusValue result;
        result.kind = LinuxDbusValueKind::Struct;
        result.elements = std::move(values);
        return result;
    }

    bool operator==(const LinuxDbusValue&) const = default;
};

struct LinuxDbusCompletion final {
    LinuxDbusErrorCode code{LinuxDbusErrorCode::None};
    std::string remote_error_name;
    std::string message;
    std::vector<LinuxDbusValue> values;

    LinuxDbusCompletion() = default;
    LinuxDbusCompletion(LinuxDbusErrorCode error_code,
                        std::string remote_name,
                        std::string diagnostic)
        : code(error_code),
          remote_error_name(std::move(remote_name)),
          message(std::move(diagnostic)) {}
};

using LinuxDbusCompletionCallback = std::function<void(LinuxDbusCompletion)>;

struct LinuxDbusMethodCall final {
    std::string destination;
    std::string path;
    std::string interface;
    std::string member;
    std::chrono::milliseconds timeout{kLinuxDbusDefaultTimeout};
    std::vector<LinuxDbusValue> arguments;

    LinuxDbusMethodCall() = default;
    LinuxDbusMethodCall(std::string destination_name,
                        std::string object_path,
                        std::string interface_name,
                        std::string member_name,
                        std::chrono::milliseconds method_timeout = kLinuxDbusDefaultTimeout)
        : destination(std::move(destination_name)),
          path(std::move(object_path)),
          interface(std::move(interface_name)),
          member(std::move(member_name)),
          timeout(method_timeout) {}
};

struct LinuxDbusSignalMatch final {
    std::string sender;
    std::string path;
    std::string interface;
    std::string member;
};

struct LinuxDbusSignal final {
    std::string sender;
    std::string path;
    std::string interface;
    std::string member;
    std::vector<LinuxDbusValue> arguments;
};

using LinuxDbusSignalCallback = std::function<void(LinuxDbusSignal)>;

struct LinuxDbusMethodRequest final {
    std::string sender;
    std::string path;
    std::string interface;
    std::string member;
    std::vector<LinuxDbusValue> arguments;
};

struct LinuxDbusMethodReply final {
    bool is_error{};
    std::string error_name;
    std::string error_message;
    std::vector<LinuxDbusValue> values;

    [[nodiscard]] static LinuxDbusMethodReply method_return(
        std::vector<LinuxDbusValue> return_values) {
        LinuxDbusMethodReply reply;
        reply.values = std::move(return_values);
        return reply;
    }

    [[nodiscard]] static LinuxDbusMethodReply error(std::string name,
                                                     std::string message) {
        LinuxDbusMethodReply reply;
        reply.is_error = true;
        reply.error_name = std::move(name);
        reply.error_message = std::move(message);
        return reply;
    }
};

using LinuxDbusObjectPathHandler =
    std::function<LinuxDbusMethodReply(const LinuxDbusMethodRequest&)>;

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
/// DBusPendingCall objects: the libdbus layer binds its pending objects to this
/// registry. Completion removes transport capacity before it posts the copied
/// result to the owning T065 Dispatcher. Dispatcher rejection is terminal and
/// never falls back to the D-Bus I/O thread.
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
    void discard_client(LinuxDbusClientId client) noexcept;
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

    /// Thread-safe non-blocking method call. Arguments and successful method
    /// return values are copied through the owned T072 value model. Invalid
    /// syntax/value shapes or inactive transport return 0.
    [[nodiscard]] LinuxDbusRequestId call_method(LinuxDbusClientId client,
                                                 ui::Dispatcher dispatcher,
                                                 const LinuxDbusMethodCall& call,
                                                 LinuxDbusCompletionCallback callback);
    [[nodiscard]] bool cancel_request(LinuxDbusClientId client, LinuxDbusRequestId id);
    [[nodiscard]] std::size_t pending_request_count() const noexcept;

    /// Subscribe one client to a bounded bus signal match. Signal payload is
    /// copied on the D-Bus thread and delivered only through the supplied T065
    /// Dispatcher. Unsubscribe invalidates already-posted-but-not-started work.
    [[nodiscard]] LinuxDbusSubscriptionId subscribe_signal(
        LinuxDbusClientId client,
        ui::Dispatcher dispatcher,
        const LinuxDbusSignalMatch& match,
        LinuxDbusSignalCallback callback);
    [[nodiscard]] bool unsubscribe_signal(LinuxDbusClientId client,
                                          LinuxDbusSubscriptionId id);
    [[nodiscard]] std::size_t subscription_count() const noexcept;

    /// Emit one validated signal through this transport's private connection.
    /// Values are encoded before the non-blocking libdbus send; no libdbus
    /// object escapes the transport and invalid/inactive sends fail atomically.
    [[nodiscard]] bool send_signal(std::string_view path,
                                   std::string_view interface,
                                   std::string_view member,
                                   const std::vector<LinuxDbusValue>& arguments);

    /// Register an internal D-Bus object path. Handlers execute only on the
    /// owned I/O thread and receive copied plain values, never libdbus objects.
    [[nodiscard]] LinuxDbusObjectRegistrationId register_object_path(
        LinuxDbusClientId client,
        std::string path,
        LinuxDbusObjectPathHandler handler);
    [[nodiscard]] bool unregister_object_path(LinuxDbusClientId client,
                                              LinuxDbusObjectRegistrationId id);
    [[nodiscard]] std::size_t object_path_count() const noexcept;

    /// Tear down every resource owned by one logical client. Pending and
    /// already-posted callbacks for that client are suppressed; sibling clients
    /// sharing the same Application transport remain live.
    void release_client(LinuxDbusClientId client) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ui::detail
