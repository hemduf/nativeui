#pragma once

namespace nativeui_smoke_accessibility_windows {

/// In-process UI Automation query fixture for a real NativeUI view.
///
/// Creates a real Application-managed window, lets the production Win32 bridge
/// attach, then queries the fragment exactly like an out-of-process UI
/// Automation client: the exposed fragment root, its control type, and its
/// RangeValue before and after a committed value update. Returns 0 on success
/// and a non-zero stage failure otherwise. This is the automatable
/// screen-reader-representative fixture; a real Narrator/Inspect probe stays a
/// documented manual checklist instead. Windows CI only.
[[nodiscard]] int run_uia_query_fixture();

} // namespace nativeui_smoke_accessibility_windows
