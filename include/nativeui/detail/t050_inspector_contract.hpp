#pragma once

// Compile-time guard used by NativeUI-owned header tests. Keeping this tiny
// header separate ensures enabling inspector diagnostics in one translation
// unit does not alter process-global or library runtime state.
#if defined(NATIVEUI_ENABLE_INSPECTOR)
inline constexpr bool nativeui_inspector_compiled = true;
#else
inline constexpr bool nativeui_inspector_compiled = false;
#endif
