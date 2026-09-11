#include "detail/linux_dbus_codec.hpp"

#include <dbus/dbus.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

int main() {
    using namespace ui::detail;

    const std::vector<LinuxDbusValue> values{
        LinuxDbusValue::boolean(true),
        LinuxDbusValue::byte(0x7fu),
        LinuxDbusValue::int16(-1234),
        LinuxDbusValue::uint16(54321),
        LinuxDbusValue::int32(-1234567),
        LinuxDbusValue::uint32(3456789012u),
        LinuxDbusValue::int64(-4'000'000'000ll),
        LinuxDbusValue::uint64(9'000'000'000ull),
        LinuxDbusValue::floating(1.25),
        LinuxDbusValue::string("NativeUI"),
        LinuxDbusValue::object_path("/org/nativeui/Test"),
        LinuxDbusValue::signature("a{sv}"),
        LinuxDbusValue::array(
            "s", {LinuxDbusValue::string("one"), LinuxDbusValue::string("two")}),
        LinuxDbusValue::array("u", {}),
        LinuxDbusValue::dictionary({
            {"enabled", LinuxDbusValue::variant(LinuxDbusValue::boolean(true))},
            {"count", LinuxDbusValue::variant(LinuxDbusValue::uint32(7))},
        }),
        LinuxDbusValue::variant(LinuxDbusValue::string("boxed")),
        LinuxDbusValue::structure({
            LinuxDbusValue::string("tuple"), LinuxDbusValue::uint32(9),
        }),
    };

    DBusMessage* message = dbus_message_new_signal(
        "/org/nativeui/Test", "org.nativeui.Test", "Values");
    if (message == nullptr) {
        return EXIT_FAILURE;
    }

    std::string error;
    if (!linux_dbus_append_values(message, values, error) || !error.empty()) {
        dbus_message_unref(message);
        return EXIT_FAILURE;
    }

    std::vector<LinuxDbusValue> decoded;
    if (!linux_dbus_decode_values(message, decoded, error) || !error.empty() ||
        decoded != values) {
        dbus_message_unref(message);
        return EXIT_FAILURE;
    }
    dbus_message_unref(message);

    const std::vector<LinuxDbusValue> invalid_values{
        LinuxDbusValue::floating(std::nan("")),
        LinuxDbusValue::object_path("not/a/path"),
        LinuxDbusValue::signature("{"),
        LinuxDbusValue::array(
            "s", {LinuxDbusValue::string("ok"), LinuxDbusValue::uint32(1)}),
        LinuxDbusValue::variant_many({}),
        LinuxDbusValue::variant_many(
            {LinuxDbusValue::string("one"), LinuxDbusValue::string("two")}),
        LinuxDbusValue::structure({}),
    };

    for (const auto& invalid : invalid_values) {
        DBusMessage* invalid_message = dbus_message_new_signal(
            "/org/nativeui/Test", "org.nativeui.Test", "Invalid");
        if (invalid_message == nullptr) {
            return EXIT_FAILURE;
        }
        error.clear();
        if (linux_dbus_append_values(invalid_message, {invalid}, error) || error.empty()) {
            dbus_message_unref(invalid_message);
            return EXIT_FAILURE;
        }
        dbus_message_unref(invalid_message);
    }

    return EXIT_SUCCESS;
}
