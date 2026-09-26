#include "native_accessibility_bridge.h"

/// Non-Apple platform stub for the macOS-produced accessibility bridge.
///
/// Windows/Linux/WebAssembly keep the same C ABI contract so the portable
/// platform layer can call it unconditionally. Every entry point fails closed
/// and no platform accessibility object is created.

NativeUIAccessibilityBridge*
nativeuiAccessibilityCreate(void* native_view, const void* binding)
{
  (void)native_view;
  (void)binding;
  return NULL;
}

void
nativeuiAccessibilityDestroy(NativeUIAccessibilityBridge* bridge)
{
  (void)bridge;
}

bool
nativeuiAccessibilityDeliver(NativeUIAccessibilityBridge* bridge, const void* batch)
{
  (void)bridge;
  (void)batch;
  return false;
}

void*
nativeuiAccessibilityTargetClass(NativeUIAccessibilityBridge* bridge)
{
  (void)bridge;
  return NULL;
}

void
nativeuiAccessibilitySetNotificationRecorderForTest(
  NativeUIAccessibilityBridge* bridge,
  void* user_data,
  NativeUIAccessibilityNotificationRecorder recorder)
{
  (void)bridge;
  (void)user_data;
  (void)recorder;
}
