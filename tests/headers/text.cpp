#include <nativeui/text.hpp>

void nativeui_header_text_compile() {
    ui::TextStyle style{};
    const auto metrics = ui::TextService::measure("NativeUI", style);
    (void)metrics;
}
