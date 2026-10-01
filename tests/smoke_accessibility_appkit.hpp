#pragma once

namespace nativeui_smoke_accessibility {

/// In-process AppKit accessibility query fixture for a real NativeUI view.
///
/// Creates a real Application-managed window, lets the production bridge attach,
/// then queries the native NSView container exactly like an AppKit accessibility
/// client would: the exposed root child, its role, and its value before and
/// after a committed value update. Returns 0 on success and a non-zero stage
/// failure otherwise. This is the automatable VoiceOver-representative fixture;
/// a real AXUIElement/VoiceOver client probe requires TCC consent and stays a
/// documented manual checklist instead.
[[nodiscard]] int run_appkit_query_fixture();

} // namespace nativeui_smoke_accessibility
