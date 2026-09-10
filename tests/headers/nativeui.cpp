#include <nativeui/nativeui.hpp>
#include <nativeui/semantics.hpp>

void nativeui_header_compile_nativeui() {
    ui::SemanticInfo info;
    info.role = ui::SemanticRole::Custom;
    ui::SemanticNodeSnapshot snapshot;
    snapshot.info = std::move(info);
}
