#pragma once

#include "linux_dbus.hpp"

#include <cstddef>
#include <memory>

namespace ui::detail {

/// Application-owned Linux D-Bus transport lifetime seam.
///
/// Ownership mutation is confined to the owning Application/UI thread. The
/// contained LinuxDbusTransport remains the thread-safe boundary for request,
/// subscription and object-path operations after a client has been registered.
/// The first successful client registration creates and starts exactly one
/// transport; subsequent clients reuse it. Releasing the last client keeps the
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

        if (transport_) {
            return transport_->register_client();
        }

        try {
            auto candidate = std::make_unique<LinuxDbusTransport>();
            if (candidate->start() != LinuxDbusErrorCode::None) {
                return kInvalidLinuxDbusClientId;
            }

            const auto client = candidate->register_client();
            if (client == kInvalidLinuxDbusClientId) {
                candidate->stop();
                return kInvalidLinuxDbusClientId;
            }

            transport_ = std::move(candidate);
            return client;
        } catch (...) {
            return kInvalidLinuxDbusClientId;
        }
    }

    void release_client(LinuxDbusClientId client) noexcept {
        if (closing_ || !transport_ || client == kInvalidLinuxDbusClientId) {
            return;
        }
        transport_->release_client(client);
    }

    [[nodiscard]] std::size_t client_count() const noexcept {
        return transport_ ? transport_->client_count() : 0;
    }

    [[nodiscard]] LinuxDbusTransport* transport_if_started() noexcept {
        return transport_.get();
    }

    [[nodiscard]] const LinuxDbusTransport* transport_if_started() const noexcept {
        return transport_.get();
    }

    void shutdown() noexcept {
        if (closing_) {
            return;
        }

        closing_ = true;
        auto transport = std::move(transport_);
        if (transport) {
            transport->stop();
        }
    }

private:
    bool closing_{};
    std::unique_ptr<LinuxDbusTransport> transport_;
};

} // namespace ui::detail
