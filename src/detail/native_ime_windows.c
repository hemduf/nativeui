#include "native_ime_bridge.h"

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>
#include <imm.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct NativeUIImeBridge {
  HWND hwnd;
  WNDPROC previousProc;
  void* userData;
  NativeUIImeCallback callback;
  bool active;
  bool composing;
  bool committed;
  float x;
  float y;
  float width;
  float height;
  float cursorOffset;
};

// The property is scoped to the NativeUI-owned child HWND. It stores no
// process-global instance state and does not touch Pugl's GWLP_USERDATA slot.
static const wchar_t kBridgeProperty[] = L"NativeUI.ImeBridge";

static void
emitEvent(NativeUIImeBridge* bridge,
          NativeUIImeEventType type,
          const char* utf8,
          size_t size,
          size_t cursorByte,
          size_t selectionBytes)
{
  if (bridge && bridge->callback) {
    bridge->callback(
      bridge->userData, type, utf8, size, cursorByte, selectionBytes);
  }
}

static char*
utf8FromWide(const wchar_t* wide, int wideLength, size_t* utf8Size)
{
  *utf8Size = 0U;
  if (!wide || wideLength <= 0) {
    return (char*)calloc(1U, 1U);
  }

  const int bytes = WideCharToMultiByte(
    CP_UTF8, WC_ERR_INVALID_CHARS, wide, wideLength, NULL, 0, NULL, NULL);
  if (bytes <= 0) return NULL;

  char* const utf8 = (char*)calloc((size_t)bytes + 1U, 1U);
  if (!utf8) return NULL;
  if (WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, wide, wideLength, utf8, bytes, NULL, NULL) !=
      bytes) {
    free(utf8);
    return NULL;
  }
  *utf8Size = (size_t)bytes;
  return utf8;
}

static size_t
utf8BytesForWidePrefix(const wchar_t* wide, int wideLength)
{
  if (!wide || wideLength <= 0) return 0U;
  const int bytes = WideCharToMultiByte(
    CP_UTF8, WC_ERR_INVALID_CHARS, wide, wideLength, NULL, 0, NULL, NULL);
  return bytes > 0 ? (size_t)bytes : 0U;
}

static wchar_t*
getCompositionWide(HIMC context, DWORD index, int* wideLength)
{
  *wideLength = 0;
  const LONG byteCount = ImmGetCompositionStringW(context, index, NULL, 0U);
  if (byteCount < 0) return NULL;

  const size_t wideCount = (size_t)byteCount / sizeof(wchar_t);
  wchar_t* const result = (wchar_t*)calloc(wideCount + 1U, sizeof(wchar_t));
  if (!result) return NULL;

  if (byteCount > 0 &&
      ImmGetCompositionStringW(
        context, index, result, (DWORD)((wideCount + 1U) * sizeof(wchar_t))) < 0) {
    free(result);
    return NULL;
  }
  *wideLength = (int)wideCount;
  return result;
}

static void
emitCompositionUpdate(NativeUIImeBridge* bridge, HIMC context)
{
  int wideLength = 0;
  wchar_t* const wide = getCompositionWide(context, GCS_COMPSTR, &wideLength);
  if (!wide) return;

  size_t utf8Size = 0U;
  char* const utf8 = utf8FromWide(wide, wideLength, &utf8Size);
  if (!utf8) {
    free(wide);
    return;
  }

  LONG cursor = ImmGetCompositionStringW(context, GCS_CURSORPOS, NULL, 0U);
  if (cursor < 0) cursor = wideLength;
  if (cursor > wideLength) cursor = wideLength;
  const size_t cursorByte = utf8BytesForWidePrefix(wide, (int)cursor);

  size_t selectionBytes = 0U;
  const LONG attrCount = ImmGetCompositionStringW(context, GCS_COMPATTR, NULL, 0U);
  if (attrCount > 0 && cursor >= 0 && cursor < attrCount) {
    BYTE* const attrs = (BYTE*)calloc((size_t)attrCount, sizeof(BYTE));
    if (attrs) {
      const LONG copied = ImmGetCompositionStringW(
        context, GCS_COMPATTR, attrs, (DWORD)attrCount);
      if (copied == attrCount) {
        LONG end = cursor;
        while (end < attrCount &&
               (attrs[end] == ATTR_TARGET_CONVERTED ||
                attrs[end] == ATTR_TARGET_NOTCONVERTED)) {
          ++end;
        }
        if (end > cursor && end <= wideLength) {
          selectionBytes = utf8BytesForWidePrefix(wide + cursor, (int)(end - cursor));
        }
      }
      free(attrs);
    }
  }

  emitEvent(
    bridge, NATIVEUI_IME_UPDATE, utf8, utf8Size, cursorByte, selectionBytes);
  free(utf8);
  free(wide);
}

static void
emitCompositionCommit(NativeUIImeBridge* bridge, HIMC context)
{
  int wideLength = 0;
  wchar_t* const wide = getCompositionWide(context, GCS_RESULTSTR, &wideLength);
  if (!wide) return;

  size_t utf8Size = 0U;
  char* const utf8 = utf8FromWide(wide, wideLength, &utf8Size);
  if (utf8) {
    if (!bridge->composing) {
      bridge->composing = true;
      emitEvent(bridge, NATIVEUI_IME_START, NULL, 0U, 0U, 0U);
    }
    emitEvent(
      bridge, NATIVEUI_IME_COMMIT, utf8, utf8Size, utf8Size, 0U);
    bridge->composing = false;
    bridge->committed = true;
    free(utf8);
  }
  free(wide);
}

static void
updateCandidatePosition(NativeUIImeBridge* bridge)
{
  if (!bridge || !bridge->active || !bridge->hwnd) return;

  HIMC const context = ImmGetContext(bridge->hwnd);
  if (!context) return;

  const LONG caretX = (LONG)lroundf(bridge->x + bridge->cursorOffset);
  const LONG top = (LONG)lroundf(bridge->y);
  const LONG bottom = (LONG)lroundf(bridge->y + bridge->height);

  CANDIDATEFORM candidate;
  memset(&candidate, 0, sizeof(candidate));
  candidate.dwIndex = 0U;
  candidate.dwStyle = CFS_EXCLUDE;
  candidate.ptCurrentPos.x = caretX;
  candidate.ptCurrentPos.y = bottom;
  candidate.rcArea.left = (LONG)lroundf(bridge->x);
  candidate.rcArea.top = top;
  candidate.rcArea.right = (LONG)lroundf(bridge->x + bridge->width);
  candidate.rcArea.bottom = bottom;
  (void)ImmSetCandidateWindow(context, &candidate);

  COMPOSITIONFORM composition;
  memset(&composition, 0, sizeof(composition));
  composition.dwStyle = CFS_POINT;
  composition.ptCurrentPos.x = caretX;
  composition.ptCurrentPos.y = bottom;
  (void)ImmSetCompositionWindow(context, &composition);

  ImmReleaseContext(bridge->hwnd, context);
}

static LRESULT CALLBACK
nativeuiImeWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  NativeUIImeBridge* const bridge =
    (NativeUIImeBridge*)GetPropW(hwnd, kBridgeProperty);
  if (!bridge) return DefWindowProcW(hwnd, message, wParam, lParam);
  if (!bridge->active) {
    return CallWindowProcW(bridge->previousProc, hwnd, message, wParam, lParam);
  }

  switch (message) {
  case WM_IME_STARTCOMPOSITION:
    bridge->composing = true;
    bridge->committed = false;
    emitEvent(bridge, NATIVEUI_IME_START, NULL, 0U, 0U, 0U);
    updateCandidatePosition(bridge);
    return 0;

  case WM_IME_COMPOSITION: {
    HIMC const context = ImmGetContext(hwnd);
    if (context) {
      if (lParam & GCS_COMPSTR) {
        if (!bridge->composing) {
          bridge->composing = true;
          bridge->committed = false;
          emitEvent(bridge, NATIVEUI_IME_START, NULL, 0U, 0U, 0U);
        }
        emitCompositionUpdate(bridge, context);
      }
      if (lParam & GCS_RESULTSTR) emitCompositionCommit(bridge, context);
      ImmReleaseContext(hwnd, context);
    }
    return 0;
  }

  case WM_IME_ENDCOMPOSITION:
    if (bridge->composing && !bridge->committed) {
      emitEvent(bridge, NATIVEUI_IME_CANCEL, NULL, 0U, 0U, 0U);
    }
    bridge->composing = false;
    bridge->committed = false;
    return 0;

  case WM_IME_CHAR:
    // DefWindowProc converts this into WM_CHAR. GCS_RESULTSTR above is the
    // authoritative commit, so swallowing WM_IME_CHAR prevents double insert.
    return 0;

  default:
    return CallWindowProcW(bridge->previousProc, hwnd, message, wParam, lParam);
  }
}

NativeUIImeBridge*
nativeuiImeCreate(PuglWorld* world,
                  PuglView* view,
                  void* userData,
                  NativeUIImeCallback callback)
{
  (void)world;
  if (!view || !callback) return NULL;

  HWND const hwnd = (HWND)(uintptr_t)puglGetNativeView(view);
  if (!hwnd) return NULL;

  NativeUIImeBridge* const bridge =
    (NativeUIImeBridge*)calloc(1U, sizeof(NativeUIImeBridge));
  if (!bridge) return NULL;

  bridge->hwnd = hwnd;
  bridge->userData = userData;
  bridge->callback = callback;

  if (!SetPropW(hwnd, kBridgeProperty, bridge)) {
    free(bridge);
    return NULL;
  }

  SetLastError(ERROR_SUCCESS);
  const LONG_PTR previous = SetWindowLongPtrW(
    hwnd, GWLP_WNDPROC, (LONG_PTR)nativeuiImeWindowProc);
  if (!previous && GetLastError() != ERROR_SUCCESS) {
    RemovePropW(hwnd, kBridgeProperty);
    free(bridge);
    return NULL;
  }

  bridge->previousProc = (WNDPROC)previous;
  return bridge;
}

void
nativeuiImeDestroy(NativeUIImeBridge* bridge)
{
  if (!bridge) return;

  nativeuiImeUpdate(bridge, false, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F);
  if (bridge->hwnd && bridge->previousProc) {
    (void)SetWindowLongPtrW(
      bridge->hwnd, GWLP_WNDPROC, (LONG_PTR)bridge->previousProc);
    RemovePropW(bridge->hwnd, kBridgeProperty);
  }
  free(bridge);
}

void
nativeuiImeUpdate(NativeUIImeBridge* bridge,
                  bool active,
                  float physicalX,
                  float physicalY,
                  float physicalWidth,
                  float physicalHeight,
                  float physicalCursorOffset)
{
  if (!bridge) return;

  if (!active && bridge->active) {
    // The focused editor cancels its model before disabling native text input.
    // Avoid dispatching back into Tree::deactivate_focus() from this boundary.
    bridge->composing = false;
    bridge->committed = false;
    bridge->active = false;

    HIMC const context = ImmGetContext(bridge->hwnd);
    if (context) {
      (void)ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0U);
      ImmReleaseContext(bridge->hwnd, context);
    }
    return;
  }

  bridge->active = active;
  bridge->x = physicalX;
  bridge->y = physicalY;
  bridge->width = physicalWidth;
  bridge->height = physicalHeight;
  bridge->cursorOffset = physicalCursorOffset;
  if (active) updateCandidatePosition(bridge);
}

bool
nativeuiImeConsumePuglText(NativeUIImeBridge* bridge,
                           const char* utf8,
                           size_t utf8Size)
{
  (void)bridge;
  (void)utf8;
  (void)utf8Size;
  return false;
}

void
nativeuiImeFlushPendingCancel(NativeUIImeBridge* bridge)
{
  (void)bridge;
}
