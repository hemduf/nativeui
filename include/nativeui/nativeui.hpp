#pragma once

/// \file
/// Convenience umbrella for the normal NativeUI public consumer surface.
///
/// Include this header when compile-time granularity is not important. It
/// aggregates public geometry/state/input/rendering/component/widget/UI/window
/// APIs. Headers under `nativeui/detail/` remain implementation-only even when
/// included transitively by public headers. Optional debug-only facilities such
/// as the inspector may still require their dedicated header/configuration.

#include <nativeui/geometry.hpp>
#include <nativeui/constraints.hpp>
#include <nativeui/invalidation.hpp>
#include <nativeui/input.hpp>
#include <nativeui/gesture.hpp>
#include <nativeui/edit.hpp>
#include <nativeui/state.hpp>
#include <nativeui/dispatcher.hpp>
#include <nativeui/animation.hpp>
#include <nativeui/desktop_services.hpp>
#include <nativeui/semantics.hpp>
#include <nativeui/text_edit.hpp>
#include <nativeui/text.hpp>
#include <nativeui/theme.hpp>
#include <nativeui/style_scope.hpp>
#include <nativeui/style.hpp>
#include <nativeui/slider_style.hpp>
#include <nativeui/progress_style.hpp>
#include <nativeui/toggle_style.hpp>
#include <nativeui/text_input_style.hpp>
#include <nativeui/text_area_style.hpp>
#include <nativeui/scrollbar_style.hpp>
#include <nativeui/combo_popup_style.hpp>
#include <nativeui/list_tabs_style.hpp>
#include <nativeui/path.hpp>
#include <nativeui/resource.hpp>
#include <nativeui/embedded_resource.hpp>
#include <nativeui/resource_manager.hpp>
#include <nativeui/image.hpp>
#include <nativeui/shader.hpp>
#include <nativeui/noise.hpp>
#include <nativeui/scalar_source.hpp>
#include <nativeui/svg.hpp>
#include <nativeui/paint.hpp>
#include <nativeui/component.hpp>
#include <nativeui/component_state.hpp>
#include <nativeui/dynamic.hpp>
#include <nativeui/overlay.hpp>
#include <nativeui/tooltip.hpp>
#include <nativeui/command.hpp>
#include <nativeui/focus.hpp>
#include <nativeui/layout.hpp>
#include <nativeui/virtual_list.hpp>
#include <nativeui/widgets.hpp>
#include <nativeui/ui.hpp>
#include <nativeui/dialog.hpp>
#include <nativeui/headless.hpp>
#include <nativeui/window.hpp>
