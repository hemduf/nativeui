#include <nativeui/input.hpp>

static_assert(!ui::detail::text_input_boundary_needs_update(
    true,
    ui::Rect{25.0f, 16.0f, 240.0f, 48.0f},
    74.5f,
    true,
    ui::Rect{12.5f, 8.0f, 120.0f, 24.0f},
    37.25f,
    2.0f));

static_assert(ui::detail::text_input_boundary_needs_update(
    true,
    ui::Rect{25.0f, 16.0f, 240.0f, 48.0f},
    74.5f,
    true,
    ui::Rect{12.5f, 8.0f, 120.0f, 24.0f},
    40.0f,
    2.0f));

static_assert(ui::detail::text_input_boundary_needs_update(
    true,
    ui::Rect{25.0f, 16.0f, 240.0f, 48.0f},
    74.5f,
    true,
    ui::Rect{13.0f, 8.0f, 120.0f, 24.0f},
    37.25f,
    2.0f));

static_assert(ui::detail::text_input_boundary_needs_update(
    true,
    ui::Rect{25.0f, 16.0f, 240.0f, 48.0f},
    74.5f,
    false,
    ui::Rect{},
    0.0f,
    2.0f));

void nativeui_header_compile_input() {}
