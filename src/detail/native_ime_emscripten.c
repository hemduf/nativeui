// Browser IME bridge stub.
//
// Pugl's Emscripten backend forwards DOM key/text input directly, so committed
// text reaches NativeUI as PUGL_TEXT events and no platform composition state
// exists to intercept. The bridge therefore keeps only the active flag so the
// public API and ViewCore call sites stay platform-uniform.

#include "native_ime_bridge.h"

#include <stdlib.h>

struct NativeUIImeBridge {
  bool active;
};

NativeUIImeBridge*
nativeuiImeCreate(PuglWorld* world,
                  PuglView* view,
                  void* user_data,
                  NativeUIImeCallback callback)
{
  (void)world;
  (void)view;
  (void)user_data;
  (void)callback;
  return (NativeUIImeBridge*)calloc(1U, sizeof(NativeUIImeBridge));
}

void
nativeuiImeDestroy(NativeUIImeBridge* bridge)
{
  free(bridge);
}

void
nativeuiImeUpdate(NativeUIImeBridge* bridge,
                  bool active,
                  float physical_x,
                  float physical_y,
                  float physical_width,
                  float physical_height,
                  float physical_cursor_offset)
{
  (void)physical_x;
  (void)physical_y;
  (void)physical_width;
  (void)physical_height;
  (void)physical_cursor_offset;
  if (bridge) {
    bridge->active = active;
  }
}

bool
nativeuiImeConsumePuglText(NativeUIImeBridge* bridge,
                           const char* utf8,
                           size_t utf8_size)
{
  (void)bridge;
  (void)utf8;
  (void)utf8_size;
  return false;
}

void
nativeuiImeFlushPendingCancel(NativeUIImeBridge* bridge)
{
  (void)bridge;
}
