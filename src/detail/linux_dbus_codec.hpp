#pragma once

#include "detail/linux_dbus.hpp"

#include <dbus/dbus.h>

#include <string>
#include <vector>

namespace ui::detail {

[[nodiscard]] bool linux_dbus_value_signature(const LinuxDbusValue& value,
                                              std::string& signature,
                                              std::string& error);
[[nodiscard]] bool linux_dbus_append_values(DBusMessage* message,
                                            const std::vector<LinuxDbusValue>& values,
                                            std::string& error);
[[nodiscard]] bool linux_dbus_decode_values(DBusMessage* message,
                                            std::vector<LinuxDbusValue>& values,
                                            std::string& error);

} // namespace ui::detail
