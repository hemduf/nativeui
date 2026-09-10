#include "detail/linux_dbus_codec.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <string_view>
#include <utility>

namespace ui::detail {
namespace {

constexpr std::size_t kMaxValueDepth = 32;

void set_error(std::string& error, std::string_view message) {
    if (error.empty()) {
        error.assign(message);
    }
}

[[nodiscard]] bool contains_nul(std::string_view text) noexcept {
    return text.find('\0') != std::string_view::npos;
}

[[nodiscard]] bool valid_utf8(std::string_view text) {
    if (contains_nul(text)) {
        return false;
    }
    DBusError dbus_error;
    dbus_error_init(&dbus_error);
    const bool valid = dbus_validate_utf8(text.data(), &dbus_error) != FALSE;
    dbus_error_free(&dbus_error);
    return valid;
}

[[nodiscard]] bool valid_signature_text(std::string_view text, bool single) {
    if (text.empty() || contains_nul(text)) {
        return false;
    }
    DBusError dbus_error;
    dbus_error_init(&dbus_error);
    const bool valid = single
        ? dbus_signature_validate_single(text.data(), &dbus_error) != FALSE
        : dbus_signature_validate(text.data(), &dbus_error) != FALSE;
    dbus_error_free(&dbus_error);
    return valid;
}

[[nodiscard]] bool signature_impl(const LinuxDbusValue& value,
                                  std::string& signature,
                                  std::string& error,
                                  std::size_t depth) {
    if (depth > kMaxValueDepth) {
        set_error(error, "D-Bus value nesting exceeds the supported depth");
        return false;
    }

    switch (value.kind) {
    case LinuxDbusValueKind::Boolean:
        signature = DBUS_TYPE_BOOLEAN_AS_STRING;
        return true;
    case LinuxDbusValueKind::Byte:
        signature = DBUS_TYPE_BYTE_AS_STRING;
        return true;
    case LinuxDbusValueKind::Int16:
        signature = DBUS_TYPE_INT16_AS_STRING;
        return true;
    case LinuxDbusValueKind::UInt16:
        signature = DBUS_TYPE_UINT16_AS_STRING;
        return true;
    case LinuxDbusValueKind::Int32:
        signature = DBUS_TYPE_INT32_AS_STRING;
        return true;
    case LinuxDbusValueKind::UInt32:
        signature = DBUS_TYPE_UINT32_AS_STRING;
        return true;
    case LinuxDbusValueKind::Int64:
        signature = DBUS_TYPE_INT64_AS_STRING;
        return true;
    case LinuxDbusValueKind::UInt64:
        signature = DBUS_TYPE_UINT64_AS_STRING;
        return true;
    case LinuxDbusValueKind::Double:
        if (!std::isfinite(value.double_value)) {
            set_error(error, "D-Bus double value must be finite");
            return false;
        }
        signature = DBUS_TYPE_DOUBLE_AS_STRING;
        return true;
    case LinuxDbusValueKind::String:
        if (!valid_utf8(value.text)) {
            set_error(error, "D-Bus string must be valid UTF-8 without NUL");
            return false;
        }
        signature = DBUS_TYPE_STRING_AS_STRING;
        return true;
    case LinuxDbusValueKind::ObjectPath: {
        if (contains_nul(value.text)) {
            set_error(error, "D-Bus object path is invalid");
            return false;
        }
        DBusError dbus_error;
        dbus_error_init(&dbus_error);
        const bool valid = dbus_validate_path(value.text.c_str(), &dbus_error) != FALSE;
        dbus_error_free(&dbus_error);
        if (!valid) {
            set_error(error, "D-Bus object path is invalid");
            return false;
        }
        signature = DBUS_TYPE_OBJECT_PATH_AS_STRING;
        return true;
    }
    case LinuxDbusValueKind::Signature:
        if (!valid_signature_text(value.text, false)) {
            set_error(error, "D-Bus signature value is invalid");
            return false;
        }
        signature = DBUS_TYPE_SIGNATURE_AS_STRING;
        return true;
    case LinuxDbusValueKind::Array: {
        if (!valid_signature_text(value.element_signature, true)) {
            set_error(error, "D-Bus array element signature is invalid");
            return false;
        }
        for (const auto& element : value.elements) {
            std::string element_signature;
            if (!signature_impl(element, element_signature, error, depth + 1)) {
                return false;
            }
            if (element_signature != value.element_signature) {
                set_error(error, "D-Bus array elements must match the declared element signature");
                return false;
            }
        }
        signature = "a";
        signature += value.element_signature;
        return true;
    }
    case LinuxDbusValueKind::Dictionary:
        for (const auto& [key, mapped] : value.entries) {
            if (!valid_utf8(key)) {
                set_error(error, "D-Bus dictionary key must be valid UTF-8 without NUL");
                return false;
            }
            if (mapped.kind != LinuxDbusValueKind::Variant || mapped.elements.size() != 1) {
                set_error(error, "D-Bus string dictionary values must be variants");
                return false;
            }
            std::string mapped_signature;
            if (!signature_impl(mapped, mapped_signature, error, depth + 1)) {
                return false;
            }
        }
        signature = "a{sv}";
        return true;
    case LinuxDbusValueKind::Variant: {
        if (value.elements.size() != 1) {
            set_error(error, "D-Bus variant must contain exactly one value");
            return false;
        }
        std::string child_signature;
        if (!signature_impl(value.elements.front(), child_signature, error, depth + 1)) {
            return false;
        }
        signature = DBUS_TYPE_VARIANT_AS_STRING;
        return true;
    }
    case LinuxDbusValueKind::Struct: {
        if (value.elements.empty()) {
            set_error(error, "D-Bus struct must contain at least one value");
            return false;
        }
        std::string result{"("};
        for (const auto& element : value.elements) {
            std::string element_signature;
            if (!signature_impl(element, element_signature, error, depth + 1)) {
                return false;
            }
            result += element_signature;
        }
        result += ')';
        if (!valid_signature_text(result, true)) {
            set_error(error, "D-Bus struct signature is invalid");
            return false;
        }
        signature = std::move(result);
        return true;
    }
    }

    set_error(error, "Unsupported D-Bus value kind");
    return false;
}

[[nodiscard]] bool append_value(DBusMessageIter& iter,
                                const LinuxDbusValue& value,
                                std::string& error,
                                std::size_t depth) {
    if (depth > kMaxValueDepth) {
        set_error(error, "D-Bus value nesting exceeds the supported depth");
        return false;
    }

    switch (value.kind) {
    case LinuxDbusValueKind::Boolean: {
        const dbus_bool_t native = value.boolean_value ? TRUE : FALSE;
        return dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &native) != FALSE;
    }
    case LinuxDbusValueKind::Byte: {
        const unsigned char native = value.byte_value;
        return dbus_message_iter_append_basic(&iter, DBUS_TYPE_BYTE, &native) != FALSE;
    }
    case LinuxDbusValueKind::Int16: {
        const dbus_int16_t native = value.int16_value;
        return dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT16, &native) != FALSE;
    }
    case LinuxDbusValueKind::UInt16: {
        const dbus_uint16_t native = value.uint16_value;
        return dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT16, &native) != FALSE;
    }
    case LinuxDbusValueKind::Int32: {
        const dbus_int32_t native = value.int32_value;
        return dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &native) != FALSE;
    }
    case LinuxDbusValueKind::UInt32: {
        const dbus_uint32_t native = value.uint32_value;
        return dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &native) != FALSE;
    }
    case LinuxDbusValueKind::Int64: {
        const dbus_int64_t native = value.int64_value;
        return dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT64, &native) != FALSE;
    }
    case LinuxDbusValueKind::UInt64: {
        const dbus_uint64_t native = value.uint64_value;
        return dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT64, &native) != FALSE;
    }
    case LinuxDbusValueKind::Double: {
        const double native = value.double_value;
        return dbus_message_iter_append_basic(&iter, DBUS_TYPE_DOUBLE, &native) != FALSE;
    }
    case LinuxDbusValueKind::String:
    case LinuxDbusValueKind::ObjectPath:
    case LinuxDbusValueKind::Signature: {
        const char* native = value.text.c_str();
        const int type = value.kind == LinuxDbusValueKind::String
            ? DBUS_TYPE_STRING
            : value.kind == LinuxDbusValueKind::ObjectPath ? DBUS_TYPE_OBJECT_PATH
                                                            : DBUS_TYPE_SIGNATURE;
        return dbus_message_iter_append_basic(&iter, type, &native) != FALSE;
    }
    case LinuxDbusValueKind::Array: {
        DBusMessageIter child;
        if (dbus_message_iter_open_container(
                &iter, DBUS_TYPE_ARRAY, value.element_signature.c_str(), &child) == FALSE) {
            set_error(error, "Unable to open D-Bus array container");
            return false;
        }
        for (const auto& element : value.elements) {
            if (!append_value(child, element, error, depth + 1)) {
                (void)dbus_message_iter_close_container(&iter, &child);
                return false;
            }
        }
        if (dbus_message_iter_close_container(&iter, &child) == FALSE) {
            set_error(error, "Unable to close D-Bus array container");
            return false;
        }
        return true;
    }
    case LinuxDbusValueKind::Dictionary: {
        DBusMessageIter array_iter;
        if (dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &array_iter) == FALSE) {
            set_error(error, "Unable to open D-Bus dictionary container");
            return false;
        }
        for (const auto& [key, mapped] : value.entries) {
            DBusMessageIter entry_iter;
            if (dbus_message_iter_open_container(
                    &array_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &entry_iter) == FALSE) {
                set_error(error, "Unable to open D-Bus dictionary entry");
                (void)dbus_message_iter_close_container(&iter, &array_iter);
                return false;
            }
            const char* native_key = key.c_str();
            if (dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &native_key) == FALSE ||
                !append_value(entry_iter, mapped, error, depth + 1) ||
                dbus_message_iter_close_container(&array_iter, &entry_iter) == FALSE) {
                set_error(error, "Unable to append D-Bus dictionary entry");
                (void)dbus_message_iter_close_container(&iter, &array_iter);
                return false;
            }
        }
        if (dbus_message_iter_close_container(&iter, &array_iter) == FALSE) {
            set_error(error, "Unable to close D-Bus dictionary container");
            return false;
        }
        return true;
    }
    case LinuxDbusValueKind::Variant: {
        std::string child_signature;
        if (!signature_impl(value.elements.front(), child_signature, error, depth + 1)) {
            return false;
        }
        DBusMessageIter child;
        if (dbus_message_iter_open_container(
                &iter, DBUS_TYPE_VARIANT, child_signature.c_str(), &child) == FALSE) {
            set_error(error, "Unable to open D-Bus variant container");
            return false;
        }
        if (!append_value(child, value.elements.front(), error, depth + 1) ||
            dbus_message_iter_close_container(&iter, &child) == FALSE) {
            set_error(error, "Unable to append D-Bus variant value");
            return false;
        }
        return true;
    }
    case LinuxDbusValueKind::Struct: {
        DBusMessageIter child;
        if (dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, nullptr, &child) == FALSE) {
            set_error(error, "Unable to open D-Bus struct container");
            return false;
        }
        for (const auto& element : value.elements) {
            if (!append_value(child, element, error, depth + 1)) {
                (void)dbus_message_iter_close_container(&iter, &child);
                return false;
            }
        }
        if (dbus_message_iter_close_container(&iter, &child) == FALSE) {
            set_error(error, "Unable to close D-Bus struct container");
            return false;
        }
        return true;
    }
    }

    set_error(error, "Unsupported D-Bus value kind");
    return false;
}

[[nodiscard]] bool current_complete_signature(DBusMessageIter& iter,
                                              std::string& signature,
                                              std::string& error) {
    char* remaining = dbus_message_iter_get_signature(&iter);
    if (remaining == nullptr) {
        set_error(error, "Unable to read D-Bus value signature");
        return false;
    }

    DBusSignatureIter signature_iter;
    dbus_signature_iter_init(&signature_iter, remaining);
    char* current = dbus_signature_iter_get_signature(&signature_iter);
    dbus_free(remaining);
    if (current == nullptr) {
        set_error(error, "Unable to isolate D-Bus value signature");
        return false;
    }
    signature.assign(current);
    dbus_free(current);
    return true;
}

[[nodiscard]] bool array_element_signature(DBusMessageIter& iter,
                                           std::string& signature,
                                           std::string& error) {
    char* remaining = dbus_message_iter_get_signature(&iter);
    if (remaining == nullptr) {
        set_error(error, "Unable to read D-Bus array signature");
        return false;
    }

    DBusSignatureIter outer;
    dbus_signature_iter_init(&outer, remaining);
    DBusSignatureIter element;
    dbus_signature_iter_recurse(&outer, &element);
    char* element_text = dbus_signature_iter_get_signature(&element);
    dbus_free(remaining);
    if (element_text == nullptr) {
        set_error(error, "Unable to read D-Bus array element signature");
        return false;
    }
    signature.assign(element_text);
    dbus_free(element_text);
    return true;
}

[[nodiscard]] bool decode_value(DBusMessageIter& iter,
                                LinuxDbusValue& value,
                                std::string& error,
                                std::size_t depth) {
    if (depth > kMaxValueDepth) {
        set_error(error, "D-Bus value nesting exceeds the supported depth");
        return false;
    }

    const int type = dbus_message_iter_get_arg_type(&iter);
    switch (type) {
    case DBUS_TYPE_BOOLEAN: {
        dbus_bool_t native = FALSE;
        dbus_message_iter_get_basic(&iter, &native);
        value = LinuxDbusValue::boolean(native != FALSE);
        return true;
    }
    case DBUS_TYPE_BYTE: {
        unsigned char native{};
        dbus_message_iter_get_basic(&iter, &native);
        value = LinuxDbusValue::byte(native);
        return true;
    }
    case DBUS_TYPE_INT16: {
        dbus_int16_t native{};
        dbus_message_iter_get_basic(&iter, &native);
        value = LinuxDbusValue::int16(native);
        return true;
    }
    case DBUS_TYPE_UINT16: {
        dbus_uint16_t native{};
        dbus_message_iter_get_basic(&iter, &native);
        value = LinuxDbusValue::uint16(native);
        return true;
    }
    case DBUS_TYPE_INT32: {
        dbus_int32_t native{};
        dbus_message_iter_get_basic(&iter, &native);
        value = LinuxDbusValue::int32(native);
        return true;
    }
    case DBUS_TYPE_UINT32: {
        dbus_uint32_t native{};
        dbus_message_iter_get_basic(&iter, &native);
        value = LinuxDbusValue::uint32(native);
        return true;
    }
    case DBUS_TYPE_INT64: {
        dbus_int64_t native{};
        dbus_message_iter_get_basic(&iter, &native);
        value = LinuxDbusValue::int64(native);
        return true;
    }
    case DBUS_TYPE_UINT64: {
        dbus_uint64_t native{};
        dbus_message_iter_get_basic(&iter, &native);
        value = LinuxDbusValue::uint64(native);
        return true;
    }
    case DBUS_TYPE_DOUBLE: {
        double native{};
        dbus_message_iter_get_basic(&iter, &native);
        if (!std::isfinite(native)) {
            set_error(error, "Decoded D-Bus double value is non-finite");
            return false;
        }
        value = LinuxDbusValue::floating(native);
        return true;
    }
    case DBUS_TYPE_STRING:
    case DBUS_TYPE_OBJECT_PATH:
    case DBUS_TYPE_SIGNATURE: {
        const char* native = nullptr;
        dbus_message_iter_get_basic(&iter, &native);
        if (native == nullptr) {
            set_error(error, "Decoded D-Bus text value is null");
            return false;
        }
        if (type == DBUS_TYPE_STRING) {
            value = LinuxDbusValue::string(native);
        } else if (type == DBUS_TYPE_OBJECT_PATH) {
            value = LinuxDbusValue::object_path(native);
        } else {
            value = LinuxDbusValue::signature(native);
        }
        std::string ignored_signature;
        return signature_impl(value, ignored_signature, error, depth);
    }
    case DBUS_TYPE_ARRAY: {
        std::string element_signature;
        if (!array_element_signature(iter, element_signature, error)) {
            return false;
        }
        DBusMessageIter child;
        dbus_message_iter_recurse(&iter, &child);
        if (element_signature == "{sv}") {
            std::vector<std::pair<std::string, LinuxDbusValue>> entries;
            while (dbus_message_iter_get_arg_type(&child) != DBUS_TYPE_INVALID) {
                if (dbus_message_iter_get_arg_type(&child) != DBUS_TYPE_DICT_ENTRY) {
                    set_error(error, "D-Bus string dictionary contains a non-entry value");
                    return false;
                }
                DBusMessageIter entry_iter;
                dbus_message_iter_recurse(&child, &entry_iter);
                if (dbus_message_iter_get_arg_type(&entry_iter) != DBUS_TYPE_STRING) {
                    set_error(error, "D-Bus dictionary key is not a string");
                    return false;
                }
                const char* key = nullptr;
                dbus_message_iter_get_basic(&entry_iter, &key);
                if (key == nullptr || !dbus_message_iter_next(&entry_iter) ||
                    dbus_message_iter_get_arg_type(&entry_iter) != DBUS_TYPE_VARIANT) {
                    set_error(error, "D-Bus dictionary entry does not contain one string/variant pair");
                    return false;
                }
                LinuxDbusValue mapped;
                if (!decode_value(entry_iter, mapped, error, depth + 1)) {
                    return false;
                }
                if (dbus_message_iter_next(&entry_iter)) {
                    set_error(error, "D-Bus dictionary entry contains extra values");
                    return false;
                }
                entries.emplace_back(key, std::move(mapped));
                dbus_message_iter_next(&child);
            }
            value = LinuxDbusValue::dictionary(std::move(entries));
            return true;
        }

        std::vector<LinuxDbusValue> elements;
        while (dbus_message_iter_get_arg_type(&child) != DBUS_TYPE_INVALID) {
            LinuxDbusValue element;
            if (!decode_value(child, element, error, depth + 1)) {
                return false;
            }
            std::string actual_signature;
            if (!signature_impl(element, actual_signature, error, depth + 1) ||
                actual_signature != element_signature) {
                set_error(error, "Decoded D-Bus array element does not match its declared signature");
                return false;
            }
            elements.push_back(std::move(element));
            dbus_message_iter_next(&child);
        }
        value = LinuxDbusValue::array(std::move(element_signature), std::move(elements));
        return true;
    }
    case DBUS_TYPE_VARIANT: {
        DBusMessageIter child;
        dbus_message_iter_recurse(&iter, &child);
        if (dbus_message_iter_get_arg_type(&child) == DBUS_TYPE_INVALID) {
            set_error(error, "D-Bus variant has no value");
            return false;
        }
        LinuxDbusValue inner;
        if (!decode_value(child, inner, error, depth + 1)) {
            return false;
        }
        if (dbus_message_iter_next(&child)) {
            set_error(error, "D-Bus variant contains multiple values");
            return false;
        }
        value = LinuxDbusValue::variant(std::move(inner));
        return true;
    }
    case DBUS_TYPE_STRUCT: {
        DBusMessageIter child;
        dbus_message_iter_recurse(&iter, &child);
        std::vector<LinuxDbusValue> elements;
        while (dbus_message_iter_get_arg_type(&child) != DBUS_TYPE_INVALID) {
            LinuxDbusValue element;
            if (!decode_value(child, element, error, depth + 1)) {
                return false;
            }
            elements.push_back(std::move(element));
            dbus_message_iter_next(&child);
        }
        if (elements.empty()) {
            set_error(error, "D-Bus struct contains no values");
            return false;
        }
        value = LinuxDbusValue::structure(std::move(elements));
        return true;
    }
    default:
        set_error(error, "D-Bus value type is outside the supported NativeUI set");
        return false;
    }
}

} // namespace

bool linux_dbus_value_signature(const LinuxDbusValue& value,
                                std::string& signature,
                                std::string& error) {
    try {
        signature.clear();
        error.clear();
        return signature_impl(value, signature, error, 0);
    } catch (const std::bad_alloc&) {
        signature.clear();
        set_error(error, "Unable to allocate D-Bus value signature");
        return false;
    }
}

bool linux_dbus_append_values(DBusMessage* message,
                              const std::vector<LinuxDbusValue>& values,
                              std::string& error) {
    error.clear();
    if (message == nullptr) {
        set_error(error, "D-Bus message is null");
        return false;
    }

    try {
        for (const auto& value : values) {
            std::string signature;
            if (!signature_impl(value, signature, error, 0)) {
                return false;
            }
        }

        DBusMessageIter iter;
        dbus_message_iter_init_append(message, &iter);
        for (const auto& value : values) {
            if (!append_value(iter, value, error, 0)) {
                set_error(error, "Unable to append D-Bus value");
                return false;
            }
        }
        return true;
    } catch (const std::bad_alloc&) {
        set_error(error, "Unable to allocate while encoding D-Bus values");
        return false;
    }
}

bool linux_dbus_decode_values(DBusMessage* message,
                              std::vector<LinuxDbusValue>& values,
                              std::string& error) {
    error.clear();
    if (message == nullptr) {
        set_error(error, "D-Bus message is null");
        return false;
    }

    try {
        std::vector<LinuxDbusValue> decoded;
        DBusMessageIter iter;
        if (dbus_message_iter_init(message, &iter) == FALSE) {
            values.clear();
            return true;
        }

        do {
            LinuxDbusValue value;
            if (!decode_value(iter, value, error, 0)) {
                return false;
            }
            decoded.push_back(std::move(value));
        } while (dbus_message_iter_next(&iter));

        values = std::move(decoded);
        return true;
    } catch (const std::bad_alloc&) {
        set_error(error, "Unable to allocate while decoding D-Bus values");
        return false;
    }
}

} // namespace ui::detail
