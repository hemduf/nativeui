#include "detail/linux_dbus.hpp"

#include <dbus/dbus.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <utility>

namespace ui::detail {
namespace {

std::once_flag g_dbus_threads_once;
bool g_dbus_threads_initialized = false;

[[nodiscard]] bool is_path_element_char(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '_';
}

} // namespace

bool linux_dbus_initialize_threads() noexcept {
    std::call_once(g_dbus_threads_once, [] {
        g_dbus_threads_initialized = dbus_threads_init_default() != FALSE;
    });
    return g_dbus_threads_initialized;
}

bool linux_dbus_valid_timeout(std::chrono::milliseconds timeout) noexcept {
    return timeout >= kLinuxDbusMinTimeout && timeout <= kLinuxDbusMaxTimeout;
}

bool linux_dbus_valid_object_path(std::string_view path) noexcept {
    if (path == "/") {
        return true;
    }
    if (path.size() < 2 || path.front() != '/' || path.back() == '/') {
        return false;
    }

    bool element_has_character = false;
    for (std::size_t i = 1; i < path.size(); ++i) {
        const char c = path[i];
        if (c == '/') {
            if (!element_has_character) {
                return false;
            }
            element_has_character = false;
            continue;
        }
        if (!is_path_element_char(c)) {
            return false;
        }
        element_has_character = true;
    }
    return element_has_character;
}

bool linux_dbus_library_probe() noexcept {
    if (!linux_dbus_initialize_threads()) {
        return false;
    }

    DBusError error;
    dbus_error_init(&error);
    const auto valid = dbus_validate_path("/", &error) != FALSE;
    dbus_error_free(&error);
    return valid;
}

struct LinuxDbusTransport::Impl final {
    mutable std::mutex lifecycle_mutex;
    DBusConnection* connection{};
    std::thread io_thread;
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> running{false};
    std::string unique_name;
};

LinuxDbusTransport::LinuxDbusTransport()
    : impl_(std::make_unique<Impl>()) {}

LinuxDbusTransport::~LinuxDbusTransport() {
    stop();
}

LinuxDbusErrorCode LinuxDbusTransport::start() {
    std::lock_guard lock{impl_->lifecycle_mutex};
    if (impl_->running.load(std::memory_order_acquire)) {
        return LinuxDbusErrorCode::None;
    }

    if (!linux_dbus_initialize_threads()) {
        return LinuxDbusErrorCode::InitializationFailed;
    }

    DBusError error;
    dbus_error_init(&error);
    DBusConnection* connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    if (connection == nullptr) {
        dbus_error_free(&error);
        return LinuxDbusErrorCode::BusUnavailable;
    }
    dbus_error_free(&error);

    dbus_connection_set_exit_on_disconnect(connection, FALSE);
    const char* unique_name = dbus_bus_get_unique_name(connection);
    if (unique_name == nullptr || *unique_name == '\0') {
        dbus_connection_close(connection);
        dbus_connection_unref(connection);
        return LinuxDbusErrorCode::LocalProtocolError;
    }

    impl_->connection = connection;
    impl_->unique_name = unique_name;
    impl_->stop_requested.store(false, std::memory_order_release);
    impl_->running.store(true, std::memory_order_release);

    try {
        Impl* state = impl_.get();
        impl_->io_thread = std::thread([state] {
            while (!state->stop_requested.load(std::memory_order_acquire)) {
                if (dbus_connection_read_write_dispatch(state->connection, 100) == FALSE) {
                    break;
                }
            }
            state->running.store(false, std::memory_order_release);
        });
    } catch (...) {
        impl_->running.store(false, std::memory_order_release);
        impl_->connection = nullptr;
        impl_->unique_name.clear();
        dbus_connection_close(connection);
        dbus_connection_unref(connection);
        return LinuxDbusErrorCode::InitializationFailed;
    }

    return LinuxDbusErrorCode::None;
}

void LinuxDbusTransport::stop() noexcept {
    std::lock_guard lock{impl_->lifecycle_mutex};
    impl_->stop_requested.store(true, std::memory_order_release);

    if (impl_->io_thread.joinable()) {
        impl_->io_thread.join();
    }

    if (impl_->connection != nullptr) {
        dbus_connection_close(impl_->connection);
        dbus_connection_unref(impl_->connection);
        impl_->connection = nullptr;
    }

    impl_->running.store(false, std::memory_order_release);
}

bool LinuxDbusTransport::running() const noexcept {
    return impl_->running.load(std::memory_order_acquire);
}

std::string LinuxDbusTransport::unique_name() const {
    std::lock_guard lock{impl_->lifecycle_mutex};
    return impl_->unique_name;
}

} // namespace ui::detail
