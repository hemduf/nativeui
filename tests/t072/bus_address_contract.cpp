#include "detail/linux_dbus.hpp"
#include "detail/linux_dbus_client_operations.hpp"
#include "fd_probe.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

constexpr const char* kNoSuchBusAddress =
    "unix:path=/nonexistent/nativeui-t072-no-such-socket";

[[nodiscard]] const char* environment_bus_address() {
    const char* address = std::getenv("DBUS_SESSION_BUS_ADDRESS");
    if (address == nullptr || *address == '\0') {
        return nullptr;
    }
    return address;
}

/// Deterministic `dbus_bus_register()` fault injection: a listening Unix socket
/// accepts every connection and closes it immediately. The transport's
/// connection-open step therefore succeeds while the following registration
/// Hello sees a disconnected peer and fails.
class ClosingUnixListener final {
public:
    explicit ClosingUnixListener(std::string path) : path_{std::move(path)} {
        descriptor_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (descriptor_ < 0) {
            return;
        }

        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        if (path_.size() + 1 > sizeof(address.sun_path)) {
            close_descriptor();
            return;
        }
        std::memcpy(address.sun_path, path_.c_str(), path_.size() + 1);
        (void)::unlink(path_.c_str());

        if (::bind(descriptor_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
            ::listen(descriptor_, 1) != 0) {
            (void)::unlink(path_.c_str());
            close_descriptor();
            return;
        }

        try {
            worker_ = std::thread([this] { accept_and_close_loop(); });
        } catch (...) {
            (void)::unlink(path_.c_str());
            close_descriptor();
            return;
        }
        ready_ = true;
    }

    ~ClosingUnixListener() {
        stop_.store(true, std::memory_order_release);
        if (worker_.joinable()) {
            wake_accept();
            worker_.join();
        }
        close_descriptor();
        if (!path_.empty()) {
            (void)::unlink(path_.c_str());
        }
    }

    ClosingUnixListener(const ClosingUnixListener&) = delete;
    ClosingUnixListener& operator=(const ClosingUnixListener&) = delete;
    ClosingUnixListener(ClosingUnixListener&&) = delete;
    ClosingUnixListener& operator=(ClosingUnixListener&&) = delete;

    [[nodiscard]] bool ready() const noexcept {
        return ready_;
    }

    [[nodiscard]] std::string address() const {
        return "unix:path=" + path_;
    }

private:
    void accept_and_close_loop() noexcept {
        while (!stop_.load(std::memory_order_acquire)) {
            const int accepted = ::accept(descriptor_, nullptr, nullptr);
            if (accepted < 0) {
                return;
            }
            (void)::close(accepted);
        }
    }

    void wake_accept() noexcept {
        if (path_.empty() || path_.size() + 1 > sizeof(sockaddr_un{}.sun_path)) {
            return;
        }
        const int wake = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (wake < 0) {
            return;
        }
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, path_.c_str(), path_.size() + 1);
        (void)::connect(wake, reinterpret_cast<sockaddr*>(&address), sizeof(address));
        (void)::close(wake);
    }

    void close_descriptor() noexcept {
        if (descriptor_ >= 0) {
            (void)::close(descriptor_);
            descriptor_ = -1;
        }
    }

    std::string path_;
    int descriptor_{-1};
    std::thread worker_;
    std::atomic<bool> stop_{false};
    bool ready_{};
};

[[nodiscard]] std::string register_failure_socket_path() {
    return "/tmp/nativeui-t072-register-failure-" + std::to_string(::getpid()) + ".sock";
}

} // namespace

int main() {
    using namespace ui::detail;

    // Pure validation seam: syntax-only rejection must not need any bus.
    if (linux_dbus_valid_bus_address("") ||
        !linux_dbus_valid_bus_address("unix:path=/tmp/nativeui-t072-bus") ||
        !linux_dbus_valid_bus_address(
            "unix:abstract=/tmp/nativeui-t072-bus,guid=0f8dbe5b4e2a1c7d9e3f5566778899aa") ||
        !linux_dbus_valid_bus_address("tcp:host=127.0.0.1,port=65535") ||
        linux_dbus_valid_bus_address("missing-colon") ||
        linux_dbus_valid_bus_address("unix:path") ||
        linux_dbus_valid_bus_address("unix:path=") ||
        linux_dbus_valid_bus_address(":") ||
        linux_dbus_valid_bus_address("unix:=/tmp/nativeui-t072-bus") ||
        linux_dbus_valid_bus_address(";")) {
        return EXIT_FAILURE;
    }

    const std::string embedded_nul =
        std::string{"unix:path=/tmp/nativeui-t072-bus"} + '\0' + "suffix";
    if (linux_dbus_valid_bus_address(embedded_nul)) {
        return EXIT_FAILURE;
    }

    // Rejected explicit starts acquire nothing: no connection, no I/O thread,
    // no client/request/subscription/object-path slot and no file descriptor.
    const std::size_t fd_before = t072_test::open_file_descriptor_count();
    {
        LinuxDbusTransport transport;
        if (transport.start("") != LinuxDbusErrorCode::InvalidArgument ||
            transport.running() || !transport.unique_name().empty() ||
            transport.client_count() != 0 ||
            transport.register_client() != kInvalidLinuxDbusClientId) {
            return EXIT_FAILURE;
        }
        if (transport.start(embedded_nul) != LinuxDbusErrorCode::InvalidArgument ||
            transport.running()) {
            return EXIT_FAILURE;
        }
        if (transport.start("missing-colon") != LinuxDbusErrorCode::InvalidArgument ||
            transport.running() || transport.pending_request_count() != 0 ||
            transport.subscription_count() != 0 || transport.object_path_count() != 0) {
            return EXIT_FAILURE;
        }

        // A rejected address is rejected even when the transport is already
        // running: validation is not skipped by the running-state short circuit.
        const char* env_address = environment_bus_address();
        if (env_address == nullptr) {
            return EXIT_FAILURE;
        }
        if (transport.start(env_address) != LinuxDbusErrorCode::None ||
            !transport.running()) {
            return EXIT_FAILURE;
        }
        if (transport.start("") != LinuxDbusErrorCode::InvalidArgument ||
            !transport.running() || transport.unique_name().empty()) {
            return EXIT_FAILURE;
        }

        transport.stop();
        transport.stop(); // idempotent after a rejected start
        if (transport.running()) {
            return EXIT_FAILURE;
        }
    }
    const std::size_t fd_after = t072_test::open_file_descriptor_count();
    if (fd_before != static_cast<std::size_t>(-1) && fd_after != fd_before) {
        return EXIT_FAILURE;
    }

    const char* env_address = environment_bus_address();
    if (env_address == nullptr || !linux_dbus_valid_bus_address(env_address)) {
        return EXIT_FAILURE;
    }
    const std::string address{env_address};

    // Open failure after a syntactically valid address returns BusUnavailable,
    // leaks nothing and leaves the transport reusable for a normal start.
    {
        LinuxDbusTransport transport;
        if (transport.start(kNoSuchBusAddress) != LinuxDbusErrorCode::BusUnavailable ||
            transport.running() || transport.client_count() != 0 ||
            transport.pending_request_count() != 0 || transport.subscription_count() != 0 ||
            transport.object_path_count() != 0 || !transport.unique_name().empty()) {
            return EXIT_FAILURE;
        }
        transport.stop(); // no-throw and idempotent after a failed start
        if (transport.running()) {
            return EXIT_FAILURE;
        }

        if (transport.start(address) != LinuxDbusErrorCode::None || !transport.running()) {
            return EXIT_FAILURE; // recovery proves nothing was latched
        }
        transport.stop();
    }

    // Deterministic dbus_bus_register() failure after a successful connection
    // open: every attempt returns BusUnavailable, leaks nothing and leaves the
    // transport reusable. A subsequent normal start proves recovery.
    {
        ClosingUnixListener listener{register_failure_socket_path()};
        if (!listener.ready()) {
            return EXIT_FAILURE;
        }

        const std::size_t fd_before_register = t072_test::open_file_descriptor_count();
        for (int attempt = 0; attempt < 2; ++attempt) {
            LinuxDbusTransport transport;
            if (transport.start(listener.address()) != LinuxDbusErrorCode::BusUnavailable ||
                transport.running() || transport.client_count() != 0 ||
                transport.pending_request_count() != 0 ||
                transport.subscription_count() != 0 ||
                transport.object_path_count() != 0 || !transport.unique_name().empty()) {
                return EXIT_FAILURE;
            }
            transport.stop(); // idempotent after the partial start
            if (transport.running()) {
                return EXIT_FAILURE;
            }
        }
        const std::size_t fd_after_register = t072_test::open_file_descriptor_count();
        if (fd_before_register != static_cast<std::size_t>(-1) &&
            fd_after_register != fd_before_register) {
            return EXIT_FAILURE;
        }

        LinuxDbusTransport recovered;
        if (recovered.start(address) != LinuxDbusErrorCode::None || !recovered.running()) {
            return EXIT_FAILURE;
        }
        recovered.stop();
    }

    // Two transports on different modes, plus one failing neighbor, are fully
    // isolated from each other.
    {
        LinuxDbusTransport session_mode;
        LinuxDbusTransport explicit_mode;
        if (session_mode.start() != LinuxDbusErrorCode::None ||
            explicit_mode.start(address) != LinuxDbusErrorCode::None) {
            return EXIT_FAILURE;
        }

        const std::string session_name = session_mode.unique_name();
        const std::string explicit_name = explicit_mode.unique_name();
        if (session_name.empty() || explicit_name.empty() || session_name == explicit_name) {
            return EXIT_FAILURE;
        }

        LinuxDbusTransport failing;
        if (failing.start(kNoSuchBusAddress) != LinuxDbusErrorCode::BusUnavailable ||
            failing.running() || !session_mode.running() || !explicit_mode.running() ||
            session_mode.unique_name() != session_name ||
            explicit_mode.unique_name() != explicit_name) {
            return EXIT_FAILURE;
        }

        const auto session_client = session_mode.register_client();
        const auto explicit_client = explicit_mode.register_client();
        if (session_client == kInvalidLinuxDbusClientId ||
            explicit_client == kInvalidLinuxDbusClientId ||
            session_mode.client_count() != 1 || explicit_mode.client_count() != 1) {
            return EXIT_FAILURE;
        }

        session_mode.release_client(session_client);
        if (session_mode.client_count() != 0 || explicit_mode.client_count() != 1) {
            return EXIT_FAILURE;
        }

        session_mode.stop();
        if (session_mode.running() || !explicit_mode.running() ||
            explicit_mode.client_count() != 1) {
            return EXIT_FAILURE;
        }
        explicit_mode.stop();
    }

    // The explicit address is not process-global state: an explicit start/stop
    // pair never changes what a fresh transport does on the default session
    // path, and the typed operation surface forwards the same contract.
    {
        LinuxDbusTransport explicit_mode;
        if (explicit_mode.start(address) != LinuxDbusErrorCode::None) {
            return EXIT_FAILURE;
        }
        explicit_mode.stop();

        LinuxDbusTransport fresh;
        if (fresh.start() != LinuxDbusErrorCode::None || !fresh.running() ||
            fresh.unique_name().empty()) {
            return EXIT_FAILURE;
        }
        fresh.stop();
    }

    {
        LinuxDbusClientOperations operations;
        if (operations.start("bad-address") != LinuxDbusErrorCode::InvalidArgument ||
            operations.running() || operations.client_count() != 0) {
            return EXIT_FAILURE;
        }
        if (operations.start(address) != LinuxDbusErrorCode::None || !operations.running()) {
            return EXIT_FAILURE;
        }
        const auto registration = operations.register_client();
        if (!registration.ok() || operations.client_count() != 1) {
            return EXIT_FAILURE;
        }
        operations.stop();
        if (operations.running()) {
            return EXIT_FAILURE;
        }
        if (operations.start(address) != LinuxDbusErrorCode::Shutdown) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
