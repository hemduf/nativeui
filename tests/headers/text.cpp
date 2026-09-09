#include <nativeui/text.hpp>

void nativeui_header_text_compile() {
    ui::TextStyle style{};
    const auto metrics = ui::TextService::measure("NativeUI", style);
    const auto prefix = ui::text::utf8_prefix("NativeUI", 6);
    (void)metrics;
    (void)prefix;
}
