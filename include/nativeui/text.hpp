#pragma once

#include <nativeui/geometry.hpp>

#include <string_view>

namespace ui {

enum class TextAlign { Left, Center, Right };
enum class FontWeight { Regular, Bold };

struct TextStyle {
    float size{14.0f};
    Color color{colors::text};
    TextAlign align{TextAlign::Left};
    FontWeight weight{FontWeight::Regular};
};

struct TextMetrics {
    float width{};
    float height{};
    float ascent{};
    float descent{};
    float leading{};
};

class TextService {
public:
    [[nodiscard]] static TextMetrics measure(std::string_view text, const TextStyle& style);
    [[nodiscard]] static TextMetrics measure(std::string_view text, float size) {
        TextStyle style{};
        style.size = size;
        return measure(text, style);
    }
};

} // namespace ui
