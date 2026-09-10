#if defined(_WIN32)
#include <windows.h>
#endif

#include <nativeui/theme.hpp>

void nativeui_header_compile_theme() {
#if defined(_WIN32)
    const ui::Theme theme{};
    (void)theme.spacing.sm;
    (void)theme.radii.sm;
#endif
}
