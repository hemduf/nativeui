#include "detail/linux_dbus.hpp"

#include <dbus/dbus.h>

namespace ui::detail {

bool linux_dbus_library_probe() noexcept {
    DBusError error;
    dbus_error_init(&error);
    const auto valid = dbus_validate_path("/", &error) != FALSE;
    dbus_error_free(&error);
    return valid;
}

} // namespace ui::detail
