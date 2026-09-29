#include <nativeui/invalidation.hpp>

void nativeui_header_invalidation_compile() {
    ui::DirtyRegion dirty;
    (void)dirty.add({0.0f, 0.0f, 10.0f, 10.0f}, {0.0f, 0.0f, 20.0f, 20.0f});
}
