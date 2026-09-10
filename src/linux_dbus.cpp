#include "detail/linux_dbus.hpp"

#include <dbus/dbus.h>

#include <mutex>

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

} // namespace ui::detail
