#ifndef NATIVEUI_DETAIL_NATIVE_ACCESSIBILITY_BRIDGE_H
#define NATIVEUI_DETAIL_NATIVE_ACCESSIBILITY_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Opaque per-native-view accessibility bridge.
typedef struct NativeUIAccessibilityBridge NativeUIAccessibilityBridge;

/**
 * Attach the platform accessibility bridge to one already-realized native view.
 *
 * `native_view` is the platform-native view handle (an NSView on Apple).
 * `binding` is a borrowed `const ui::detail::NativeAccessibilityAttachBinding*`
 * that stays valid only for the duration of this call. The bridge copies the
 * two weak semantic endpoints and never retains the binding or the view bridge.
 *
 * Every failure path returns NULL: missing/malformed inputs, a detached
 * publication source, a non-main-thread caller, an existing bridge for the same
 * native view, a native view class without the consumer-scoped runtime prefix,
 * a conflicting pre-existing runtime class, or allocation failure during class
 * creation/state/child-resolver construction. Attach failure disables
 * accessibility for that view only; it never fails the surrounding native view.
 */
NativeUIAccessibilityBridge*
nativeuiAccessibilityCreate(void* native_view, const void* binding);

/// Detach and release one bridge. The native view class is restored before any
/// bridge state is released. Idempotent for NULL.
void
nativeuiAccessibilityDestroy(NativeUIAccessibilityBridge* bridge);

/**
 * Deliver exactly one committed native publication batch to the platform
 * accessibility system. `batch` is a borrowed
 * `const ui::detail::SemanticNativePublicationBatch*` valid only for this call.
 *
 * Returns true only when the batch is this bridge's exact current native
 * publication and notification mapping completed. A NULL bridge/batch, a
 * non-main-thread caller, an expired publication source, or a superseded or
 * foreign batch returns false and posts nothing. No exception crosses this
 * boundary.
 */
bool
nativeuiAccessibilityDeliver(NativeUIAccessibilityBridge* bridge,
                             const void* batch);

/**
 * Runtime class currently installed on the native view, or NULL. Exposed for
 * the consumer-prefixed Objective-C runtime audit; never a generic class name.
 */
void*
nativeuiAccessibilityTargetClass(NativeUIAccessibilityBridge* bridge);

/**
 * Deterministic recorder for the closed notification mapping. This is an
 * internal test seam: it replaces the platform post for this bridge only and is
 * never used by production code. `notification` is the AppKit notification name
 * and `element` is the posted object. A throwing recorder is contained.
 */
typedef void (*NativeUIAccessibilityNotificationRecorder)(void* user_data,
                                                          const char* notification,
                                                          const void* element);

void
nativeuiAccessibilitySetNotificationRecorderForTest(
    NativeUIAccessibilityBridge* bridge,
    void* user_data,
    NativeUIAccessibilityNotificationRecorder recorder);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NATIVEUI_DETAIL_NATIVE_ACCESSIBILITY_BRIDGE_H
