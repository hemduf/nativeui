#include <nativeui/nativeui.hpp>

void nativeui_header_compile_nativeui() {}

void nativeui_header_compile_t058_conditional(ui::State<bool>& visible) {
    auto spec = ui::make_spec(ui::If{visible, ui::Spacer{1.0f, 1.0f}});
    (void)spec;
}
