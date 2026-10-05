#pragma once

#include <nativeui/scalar_source.hpp>

#include <variant>

namespace ui::detail {

struct ScalarSourceAccess {
    [[nodiscard]] static bool is_constant(
        const ScalarSource& source) noexcept {
        return std::holds_alternative<float>(source.value_);
    }

    [[nodiscard]] static float constant_value(
        const ScalarSource& source) noexcept {
        if (const auto* value = std::get_if<float>(&source.value_)) {
            return *value;
        }
        return 0.0f;
    }

    [[nodiscard]] static const Brush* brush(
        const ScalarSource& source) noexcept {
        if (const auto* value =
                std::get_if<ScalarSource::BrushChannel>(&source.value_)) {
            return &value->brush;
        }
        return nullptr;
    }

    [[nodiscard]] static ScalarChannel channel(
        const ScalarSource& source) noexcept {
        if (const auto* value =
                std::get_if<ScalarSource::BrushChannel>(&source.value_)) {
            return value->channel;
        }
        return ScalarChannel::Red;
    }
};

} // namespace ui::detail
