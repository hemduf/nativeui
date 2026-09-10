#include <nativeui/semantics.hpp>

static_assert(ui::kInvalidSemanticId == ui::SemanticId{});

void nativeui_semantics_header_compile_probe() {
    ui::SemanticInfo info;
    info.role = ui::SemanticRole::Custom;
    ui::SemanticNodeSnapshot snapshot;
    snapshot.info = std::move(info);
}
