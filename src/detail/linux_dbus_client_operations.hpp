#pragma once

#include "linux_dbus.hpp"

#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ui::detail {

template <typename Id>
struct LinuxDbusImmediateResult final {
    LinuxDbusErrorCode code{LinuxDbusErrorCode::InvalidArgument};
    Id id{};

    [[nodiscard]] bool ok() const noexcept {
        return code == LinuxDbusErrorCode::None && id != Id{};
    }
};

using LinuxDbusClientRegistrationResult = LinuxDbusImmediateResult<LinuxDbusClientId>;
using LinuxDbusRequestStartResult = LinuxDbusImmediateResult<LinuxDbusRequestId>;
using LinuxDbusSubscriptionResult = LinuxDbusImmediateResult<LinuxDbusSubscriptionId>;
using LinuxDbusObjectPathResult = LinuxDbusImmediateResult<LinuxDbusObjectRegistrationId>;

/// Canonical typed client-facing operation surface for one T072 transport.
///
/// One instance owns one LinuxDbusTransport and serializes synchronous client
/// admission/resource acquisition so immediate rejection can preserve the #84
/// error model instead of collapsing ResourceLimit/InvalidArgument/Shutdown to
/// an invalid ID. The underlying transport remains responsible for I/O-thread
/// work, exactly-once asynchronous completion, client ownership and hard quotas.
///
/// Application ownership creates at most one of these surfaces. An independent
/// accessibility-enabled EmbeddedView may own one surface of its own. Do not
/// create multiple surfaces around the same transport or use the raw transport
/// as an overflow path.
class LinuxDbusClientOperations final {
public:
    LinuxDbusClientOperations() = default;
    ~LinuxDbusClientOperations();

    LinuxDbusClientOperations(const LinuxDbusClientOperations&) = delete;
    LinuxDbusClientOperations& operator=(const LinuxDbusClientOperations&) = delete;
    LinuxDbusClientOperations(LinuxDbusClientOperations&&) = delete;
    LinuxDbusClientOperations& operator=(LinuxDbusClientOperations&&) = delete;

    [[nodiscard]] LinuxDbusErrorCode start();
    void stop() noexcept;

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] std::string unique_name() const;

    [[nodiscard]] LinuxDbusClientRegistrationResult register_client();
    [[nodiscard]] bool release_client(LinuxDbusClientId client) noexcept;
    [[nodiscard]] std::size_t client_count() const noexcept;

    [[nodiscard]] LinuxDbusRequestStartResult call_method(
        LinuxDbusClientId client,
        ui::Dispatcher dispatcher,
        const LinuxDbusMethodCall& call,
        LinuxDbusCompletionCallback callback);
    [[nodiscard]] bool cancel_request(LinuxDbusClientId client,
                                      LinuxDbusRequestId id);
    [[nodiscard]] std::size_t pending_request_count() const noexcept;

    [[nodiscard]] LinuxDbusSubscriptionResult subscribe_signal(
        LinuxDbusClientId client,
        ui::Dispatcher dispatcher,
        const LinuxDbusSignalMatch& match,
        LinuxDbusSignalCallback callback);
    [[nodiscard]] bool unsubscribe_signal(LinuxDbusClientId client,
                                          LinuxDbusSubscriptionId id);
    [[nodiscard]] std::size_t subscription_count() const noexcept;

    [[nodiscard]] LinuxDbusObjectPathResult register_object_path(
        LinuxDbusClientId client,
        std::string path,
        LinuxDbusObjectPathHandler handler);
    [[nodiscard]] bool unregister_object_path(LinuxDbusClientId client,
                                              LinuxDbusObjectRegistrationId id);
    [[nodiscard]] std::size_t object_path_count() const noexcept;

    [[nodiscard]] LinuxDbusErrorCode send_signal(
        LinuxDbusClientId client,
        std::string_view path,
        std::string_view interface,
        std::string_view member,
        const std::vector<LinuxDbusValue>& arguments);

    /// Low-level validation seam only. Production T064/T068 clients should use
    /// the typed methods above so immediate failures keep their error code.
    [[nodiscard]] LinuxDbusTransport& transport_for_testing() noexcept {
        return transport_;
    }
    [[nodiscard]] const LinuxDbusTransport& transport_for_testing() const noexcept {
        return transport_;
    }

private:
    struct ObjectPathOwner final {
        LinuxDbusClientId client{};
        std::string path;
    };

    [[nodiscard]] bool has_client_locked(LinuxDbusClientId client) const noexcept;

    mutable std::mutex mutex_;
    LinuxDbusTransport transport_;
    std::unordered_set<LinuxDbusClientId> clients_;
    std::unordered_map<LinuxDbusObjectRegistrationId, ObjectPathOwner> object_paths_;
    std::unordered_set<std::string> object_path_names_;
    bool closing_{};
};

} // namespace ui::detail
