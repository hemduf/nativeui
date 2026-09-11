#pragma once

#include "linux_dbus_client_operations.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

namespace ui::detail {

/// Application-owned Linux D-Bus client/transport lifetime seam.
///
/// Ownership mutation is confined to the owning Application/UI thread. The
/// first successful client registration creates exactly one typed client
/// operation surface, which in turn owns exactly one LinuxDbusTransport.
/// Subsequent clients reuse it. Releasing the last client keeps the surface and
/// transport alive until Application shutdown so later clients cannot create an
/// overflow/replacement connection for the same Application lifetime.
class LinuxDbusApplicationTransportOwner final {
public:
    LinuxDbusApplicationTransportOwner() = default;

    ~LinuxDbusApplicationTransportOwner() {
        shutdown();
    }

    LinuxDbusApplicationTransportOwner(const LinuxDbusApplicationTransportOwner&) = delete;
    LinuxDbusApplicationTransportOwner& operator=(const LinuxDbusApplicationTransportOwner&) = delete;
    LinuxDbusApplicationTransportOwner(LinuxDbusApplicationTransportOwner&&) = delete;
    LinuxDbusApplicationTransportOwner& operator=(LinuxDbusApplicationTransportOwner&&) = delete;

    [[nodiscard]] LinuxDbusClientId register_client() {
        if (closing_) {
            return kInvalidLinuxDbusClientId;
        }

        if (operations_) {
            const auto result = operations_->register_client();
            if (!result.ok()) {
                return kInvalidLinuxDbusClientId;
            }
            try {
                clients_.push_back(result.id);
            } catch (...) {
                (void)operations_->release_client(result.id);
                return kInvalidLinuxDbusClientId;
            }
            return result.id;
        }

        try {
            auto candidate = std::make_unique<LinuxDbusClientOperations>();
            if (candidate->start() != LinuxDbusErrorCode::None) {
                return kInvalidLinuxDbusClientId;
            }

            const auto result = candidate->register_client();
            if (!result.ok()) {
                candidate->stop();
                return kInvalidLinuxDbusClientId;
            }

            try {
                clients_.push_back(result.id);
            } catch (...) {
                (void)candidate->release_client(result.id);
                candidate->stop();
                return kInvalidLinuxDbusClientId;
            }

            operations_ = std::move(candidate);
            return result.id;
        } catch (...) {
            return kInvalidLinuxDbusClientId;
        }
    }

    void release_client(LinuxDbusClientId client) noexcept {
        if (closing_ || !operations_ || client == kInvalidLinuxDbusClientId) {
            return;
        }

        const auto found = std::find(clients_.begin(), clients_.end(), client);
        if (found == clients_.end()) {
            return;
        }

        (void)operations_->release_client(client);
        clients_.erase(found);
    }

    [[nodiscard]] std::size_t client_count() const noexcept {
        return clients_.size();
    }

    /// Canonical operation surface for T064/T068 clients. It preserves typed
    /// immediate failures such as ResourceLimit/InvalidArgument/Shutdown.
    [[nodiscard]] LinuxDbusClientOperations* operations_if_started() noexcept {
        return operations_.get();
    }

    [[nodiscard]] const LinuxDbusClientOperations* operations_if_started() const noexcept {
        return operations_.get();
    }

    /// Low-level validation seam retained for existing T072 transport tests.
    [[nodiscard]] LinuxDbusTransport* transport_if_started() noexcept {
        return operations_ ? &operations_->transport_for_testing() : nullptr;
    }

    [[nodiscard]] const LinuxDbusTransport* transport_if_started() const noexcept {
        return operations_ ? &operations_->transport_for_testing() : nullptr;
    }

    void shutdown() noexcept {
        if (closing_) {
            return;
        }

        closing_ = true;
        auto operations = std::move(operations_);
        if (operations) {
            // Application teardown closes logical clients first so their
            // pending Dispatcher completions/subscriptions are invalidated
            // before the shared transport begins stop/join/connection teardown.
            for (const auto client : clients_) {
                (void)operations->release_client(client);
            }
            clients_.clear();
            operations->stop();
        } else {
            clients_.clear();
        }
    }

private:
    bool closing_{};
    std::unique_ptr<LinuxDbusClientOperations> operations_;
    std::vector<LinuxDbusClientId> clients_;
};

} // namespace ui::detail
