#ifndef NATIVEUI_DETAIL_NATIVE_IME_BRIDGE_H
#define NATIVEUI_DETAIL_NATIVE_IME_BRIDGE_H

#include <pugl/pugl.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum NativeUIImeEventType {
  NATIVEUI_IME_START,
  NATIVEUI_IME_UPDATE,
  NATIVEUI_IME_COMMIT,
  NATIVEUI_IME_CANCEL
} NativeUIImeEventType;

typedef void (*NativeUIImeCallback)(void*                user_data,
                                    NativeUIImeEventType  type,
                                    const char*           utf8,
                                    size_t                utf8_size,
                                    size_t                cursor_byte,
                                    size_t                selection_bytes);

typedef struct NativeUIImeBridge NativeUIImeBridge;

NativeUIImeBridge*
nativeuiImeCreate(PuglWorld* world,
                  PuglView* view,
                  void* user_data,
                  NativeUIImeCallback callback);

void
nativeuiImeDestroy(NativeUIImeBridge* bridge);

void
nativeuiImeUpdate(NativeUIImeBridge* bridge,
                  bool active,
                  float physical_x,
                  float physical_y,
                  float physical_width,
                  float physical_height,
                  float physical_cursor_offset);

/**
 * Consume a committed Pugl text event when a platform composition bridge must
 * turn it into a single IME commit instead. Returns true only when the event
 * was consumed. Platforms that deliver commits directly return false.
 */
bool
nativeuiImeConsumePuglText(NativeUIImeBridge* bridge,
                           const char* utf8,
                           size_t utf8_size);

/**
 * Resolve a platform composition that ended without a committed Pugl text
 * event. X11 needs this deferred boundary because XIM reports preedit-done
 * before Xutf8LookupString returns the result. Other platforms are no-ops.
 */
void
nativeuiImeFlushPendingCancel(NativeUIImeBridge* bridge);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NATIVEUI_DETAIL_NATIVE_IME_BRIDGE_H
