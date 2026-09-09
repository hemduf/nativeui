#include "native_ime_bridge.h"

#include "internal.h"
#include "x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

struct NativeUIImeBridge {
  PuglWorld* world;
  PuglView* view;
  XIC xic;
  void* userData;
  NativeUIImeCallback callback;
  wchar_t* preedit;
  XIMFeedback* feedback;
  size_t preeditLength;
  size_t caret;
  bool callbacksInstalled;
  bool active;
  bool composing;
  bool awaitingCommit;
  float x;
  float y;
  float width;
  float height;
  float cursorOffset;
};

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

static size_t
appendUtf8(uint32_t codepoint, char* output)
{
  if (codepoint <= 0x7FU) {
    output[0] = (char)codepoint;
    return 1U;
  }
  if (codepoint <= 0x7FFU) {
    output[0] = (char)(0xC0U | (codepoint >> 6U));
    output[1] = (char)(0x80U | (codepoint & 0x3FU));
    return 2U;
  }
  if (codepoint <= 0xFFFFU) {
    output[0] = (char)(0xE0U | (codepoint >> 12U));
    output[1] = (char)(0x80U | ((codepoint >> 6U) & 0x3FU));
    output[2] = (char)(0x80U | (codepoint & 0x3FU));
    return 3U;
  }
  if (codepoint <= 0x10FFFFU) {
    output[0] = (char)(0xF0U | (codepoint >> 18U));
    output[1] = (char)(0x80U | ((codepoint >> 12U) & 0x3FU));
    output[2] = (char)(0x80U | ((codepoint >> 6U) & 0x3FU));
    output[3] = (char)(0x80U | (codepoint & 0x3FU));
    return 4U;
  }
  output[0] = (char)0xEF;
  output[1] = (char)0xBF;
  output[2] = (char)0xBD;
  return 3U;
}

static char*
utf8FromWide(const wchar_t* wide, size_t length, size_t* utf8Size)
{
  *utf8Size = 0U;
  char* const result = (char*)calloc(length * 4U + 1U, 1U);
  if (!result) {
    return NULL;
  }

  size_t out = 0U;
  for (size_t i = 0U; i < length; ++i) {
    uint32_t codepoint = (uint32_t)wide[i];
#if WCHAR_MAX <= 0xFFFF
    if (codepoint >= 0xD800U && codepoint <= 0xDBFFU && i + 1U < length) {
      const uint32_t low = (uint32_t)wide[i + 1U];
      if (low >= 0xDC00U && low <= 0xDFFFU) {
        codepoint = 0x10000U + ((codepoint - 0xD800U) << 10U) + (low - 0xDC00U);
        ++i;
      }
    }
#endif
    out += appendUtf8(codepoint, result + out);
  }
  result[out] = '\0';
  *utf8Size = out;
  return result;
}

static wchar_t*
wideFromXimText(const XIMText* text, size_t* wideLength)
{
  *wideLength = 0U;
  if (!text || text->length == 0U) {
    return (wchar_t*)calloc(1U, sizeof(wchar_t));
  }

  if (text->encoding_is_wchar) {
    wchar_t* const result =
      (wchar_t*)calloc((size_t)text->length + 1U, sizeof(wchar_t));
    if (!result) return NULL;
    memcpy(result, text->string.wide_char, (size_t)text->length * sizeof(wchar_t));
    *wideLength = text->length;
    return result;
  }

  // XIMText.length is measured in characters, not bytes. The multibyte string
  // is NUL-terminated, so measure its byte extent instead of truncating a UTF-8
  // preedit at the first multibyte character boundary.
  const size_t byteLength = text->string.multi_byte ? strlen(text->string.multi_byte) : 0U;
  char* const bytes = (char*)calloc(byteLength + 1U, 1U);
  if (!bytes) return NULL;
  if (byteLength) memcpy(bytes, text->string.multi_byte, byteLength);

  size_t required = mbstowcs(NULL, bytes, 0U);
  if (required == (size_t)-1) {
    // Do not mutate the process locale from an embeddable library. Preserve
    // the complete byte sequence as a deterministic fallback instead of
    // reading only XIMText.length bytes (which is a character count).
    required = byteLength;
    wchar_t* const fallback =
      (wchar_t*)calloc(required + 1U, sizeof(wchar_t));
    if (fallback) {
      for (size_t i = 0U; i < required; ++i) {
        fallback[i] = (unsigned char)bytes[i];
      }
      *wideLength = required;
    }
    free(bytes);
    return fallback;
  }

  wchar_t* const result = (wchar_t*)calloc(required + 1U, sizeof(wchar_t));
  if (!result) {
    free(bytes);
    return NULL;
  }
  const size_t converted = mbstowcs(result, bytes, required + 1U);
  free(bytes);
  if (converted == (size_t)-1) {
    free(result);
    return NULL;
  }
  *wideLength = converted;
  return result;
}

static void
resetPreedit(NativeUIImeBridge* bridge)
{
  free(bridge->preedit);
  free(bridge->feedback);
  bridge->preedit = NULL;
  bridge->feedback = NULL;
  bridge->preeditLength = 0U;
  bridge->caret = 0U;
}

static bool
replacePreedit(NativeUIImeBridge* bridge, const XIMPreeditDrawCallbackStruct* draw)
{
  size_t insertedLength = 0U;
  wchar_t* inserted = wideFromXimText(draw ? draw->text : NULL, &insertedLength);
  if (!inserted) return false;

  const size_t first = draw && draw->chg_first > 0
                         ? (size_t)draw->chg_first
                         : 0U;
  const size_t begin = first < bridge->preeditLength ? first : bridge->preeditLength;
  const size_t requestedRemove = draw && draw->chg_length > 0
                                   ? (size_t)draw->chg_length
                                   : 0U;
  const size_t remove = requestedRemove < bridge->preeditLength - begin
                          ? requestedRemove
                          : bridge->preeditLength - begin;
  const size_t tailBegin = begin + remove;
  const size_t tailLength = bridge->preeditLength - tailBegin;
  const size_t newLength = begin + insertedLength + tailLength;

  wchar_t* const newPreedit =
    (wchar_t*)calloc(newLength + 1U, sizeof(wchar_t));
  XIMFeedback* const newFeedback =
    newLength ? (XIMFeedback*)calloc(newLength, sizeof(XIMFeedback)) : NULL;
  if (!newPreedit || (newLength && !newFeedback)) {
    free(inserted);
    free(newPreedit);
    free(newFeedback);
    return false;
  }

  if (begin) {
    memcpy(newPreedit, bridge->preedit, begin * sizeof(wchar_t));
    if (bridge->feedback) {
      memcpy(newFeedback, bridge->feedback, begin * sizeof(XIMFeedback));
    }
  }
  if (insertedLength) {
    memcpy(newPreedit + begin, inserted, insertedLength * sizeof(wchar_t));
    if (draw && draw->text && draw->text->feedback) {
      const size_t feedbackCount = draw->text->length < insertedLength
                                     ? draw->text->length
                                     : insertedLength;
      memcpy(newFeedback + begin,
             draw->text->feedback,
             feedbackCount * sizeof(XIMFeedback));
    }
  }
  if (tailLength) {
    memcpy(newPreedit + begin + insertedLength,
           bridge->preedit + tailBegin,
           tailLength * sizeof(wchar_t));
    if (bridge->feedback) {
      memcpy(newFeedback + begin + insertedLength,
             bridge->feedback + tailBegin,
             tailLength * sizeof(XIMFeedback));
    }
  }

  free(inserted);
  free(bridge->preedit);
  free(bridge->feedback);
  bridge->preedit = newPreedit;
  bridge->feedback = newFeedback;
  bridge->preeditLength = newLength;
  bridge->caret = draw && draw->caret >= 0 ? (size_t)draw->caret : newLength;
  if (bridge->caret > newLength) bridge->caret = newLength;
  return true;
}

static bool
isSelectedFeedback(XIMFeedback feedback)
{
  return (feedback &
          (XIMReverse | XIMHighlight | XIMPrimary | XIMSecondary | XIMTertiary)) != 0;
}

static void
emitPreeditUpdate(NativeUIImeBridge* bridge)
{
  if (!bridge || !bridge->active) return;

  size_t utf8Size = 0U;
  char* const utf8 = utf8FromWide(bridge->preedit, bridge->preeditLength, &utf8Size);
  if (!utf8) return;

  size_t prefixSize = 0U;
  char* const prefix = utf8FromWide(bridge->preedit, bridge->caret, &prefixSize);
  free(prefix);

  size_t selectionChars = 0U;
  if (bridge->feedback && bridge->caret < bridge->preeditLength &&
      isSelectedFeedback(bridge->feedback[bridge->caret])) {
    size_t end = bridge->caret;
    while (end < bridge->preeditLength && isSelectedFeedback(bridge->feedback[end])) {
      ++end;
    }
    selectionChars = end - bridge->caret;
  }

  size_t selectionBytes = 0U;
  if (selectionChars) {
    char* const selection = utf8FromWide(
      bridge->preedit + bridge->caret, selectionChars, &selectionBytes);
    free(selection);
  }

  emitEvent(
    bridge, NATIVEUI_IME_UPDATE, utf8, utf8Size, prefixSize, selectionBytes);
  free(utf8);
}

static int
preeditStart(XIC xic, XPointer clientData, XPointer callData)
{
  (void)xic;
  (void)callData;
  NativeUIImeBridge* const bridge = (NativeUIImeBridge*)clientData;
  if (!bridge || !bridge->active) return -1;

  resetPreedit(bridge);
  bridge->composing = true;
  bridge->awaitingCommit = false;
  emitEvent(bridge, NATIVEUI_IME_START, NULL, 0U, 0U, 0U);
  return -1;
}

static void
preeditDone(XIC xic, XPointer clientData, XPointer callData)
{
  (void)xic;
  (void)callData;
  NativeUIImeBridge* const bridge = (NativeUIImeBridge*)clientData;
  if (bridge && bridge->active && bridge->composing) {
    // XIM reports preedit-done before Xutf8LookupString exposes the committed
    // result. Defer cancellation until the Pugl event cycle has had a chance
    // to deliver that text through nativeuiImeConsumePuglText().
    bridge->awaitingCommit = true;
  }
}

static void
preeditDraw(XIC xic, XPointer clientData, XPointer callData)
{
  (void)xic;
  NativeUIImeBridge* const bridge = (NativeUIImeBridge*)clientData;
  const XIMPreeditDrawCallbackStruct* const draw =
    (const XIMPreeditDrawCallbackStruct*)callData;
  if (!bridge || !bridge->active || !draw) return;

  if (!bridge->composing) {
    bridge->composing = true;
    bridge->awaitingCommit = false;
    emitEvent(bridge, NATIVEUI_IME_START, NULL, 0U, 0U, 0U);
  }
  if (replacePreedit(bridge, draw)) {
    emitPreeditUpdate(bridge);
  }
}

static void
preeditCaret(XIC xic, XPointer clientData, XPointer callData)
{
  (void)xic;
  NativeUIImeBridge* const bridge = (NativeUIImeBridge*)clientData;
  const XIMPreeditCaretCallbackStruct* const caret =
    (const XIMPreeditCaretCallbackStruct*)callData;
  if (!bridge || !bridge->active || !bridge->composing || !caret) return;

  if (caret->direction == XIMAbsolutePosition && caret->position >= 0) {
    bridge->caret = (size_t)caret->position;
    if (bridge->caret > bridge->preeditLength) {
      bridge->caret = bridge->preeditLength;
    }
    emitPreeditUpdate(bridge);
  }
}

static bool
supportsCallbackStyle(XIM xim)
{
  XIMStyles* styles = NULL;
  if (!xim || XGetIMValues(xim, XNQueryInputStyle, &styles, NULL) != NULL || !styles) {
    return false;
  }

  const XIMStyle wanted = XIMPreeditCallbacks | XIMStatusNothing;
  bool found = false;
  for (unsigned short i = 0U; i < styles->count_styles; ++i) {
    if (styles->supported_styles[i] == wanted) {
      found = true;
      break;
    }
  }
  XFree(styles);
  return found;
}

static XIC
createCallbackIc(NativeUIImeBridge* bridge)
{
  PuglWorldInternals* const worldImpl = bridge->world->impl;
  PuglInternals* const viewImpl = bridge->view->impl;

  XIMCallback start = {(XPointer)bridge, (XIMProc)preeditStart};
  XIMCallback done = {(XPointer)bridge, (XIMProc)preeditDone};
  XIMCallback draw = {(XPointer)bridge, (XIMProc)preeditDraw};
  XIMCallback caret = {(XPointer)bridge, (XIMProc)preeditCaret};
  XVaNestedList preeditAttributes = XVaCreateNestedList(
    0,
    XNPreeditStartCallback,
    &start,
    XNPreeditDoneCallback,
    &done,
    XNPreeditDrawCallback,
    &draw,
    XNPreeditCaretCallback,
    &caret,
    NULL);
  if (!preeditAttributes) return NULL;

  XIC const xic = XCreateIC(worldImpl->xim,
                            XNInputStyle,
                            XIMPreeditCallbacks | XIMStatusNothing,
                            XNClientWindow,
                            viewImpl->win,
                            XNFocusWindow,
                            viewImpl->win,
                            XNPreeditAttributes,
                            preeditAttributes,
                            NULL);
  XFree(preeditAttributes);
  return xic;
}

static void
updateSpotLocation(NativeUIImeBridge* bridge)
{
  if (!bridge || !bridge->active || !bridge->callbacksInstalled || !bridge->xic) {
    return;
  }

  XPoint spot;
  spot.x = (short)lroundf(bridge->x + bridge->cursorOffset);
  spot.y = (short)lroundf(bridge->y + bridge->height);
  XRectangle area;
  area.x = (short)lroundf(bridge->x);
  area.y = (short)lroundf(bridge->y);
  area.width = (unsigned short)fmaxf(1.0F, bridge->width);
  area.height = (unsigned short)fmaxf(1.0F, bridge->height);

  XVaNestedList preeditAttributes =
    XVaCreateNestedList(0, XNSpotLocation, &spot, XNArea, &area, NULL);
  if (preeditAttributes) {
    (void)XSetICValues(bridge->xic, XNPreeditAttributes, preeditAttributes, NULL);
    XFree(preeditAttributes);
  }
}

NativeUIImeBridge*
nativeuiImeCreate(PuglWorld* world,
                  PuglView* view,
                  void* userData,
                  NativeUIImeCallback callback)
{
  if (!world || !view || !callback) return NULL;

  NativeUIImeBridge* const bridge =
    (NativeUIImeBridge*)calloc(1U, sizeof(NativeUIImeBridge));
  if (!bridge) return NULL;

  bridge->world = world;
  bridge->view = view;
  bridge->userData = userData;
  bridge->callback = callback;

  if (world->impl->xim && supportsCallbackStyle(world->impl->xim)) {
    if (view->impl->xic) {
      XDestroyIC(view->impl->xic);
      view->impl->xic = NULL;
    }
    bridge->xic = createCallbackIc(bridge);
    if (bridge->xic) {
      view->impl->xic = bridge->xic;
      bridge->callbacksInstalled = true;
    } else {
      view->impl->xic = XCreateIC(world->impl->xim,
                                  XNInputStyle,
                                  XIMPreeditNothing | XIMStatusNothing,
                                  XNClientWindow,
                                  view->impl->win,
                                  XNFocusWindow,
                                  view->impl->win,
                                  (XIM)0);
    }
  } else {
    bridge->xic = view->impl->xic;
  }

  return bridge;
}

void
nativeuiImeDestroy(NativeUIImeBridge* bridge)
{
  if (!bridge) return;

  nativeuiImeUpdate(bridge, false, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F);
  if (bridge->callbacksInstalled && bridge->view &&
      bridge->view->impl->xic == bridge->xic) {
    XDestroyIC(bridge->xic);
    bridge->view->impl->xic = NULL;
  }
  resetPreedit(bridge);
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
    // The retained editor cancels its model before disabling the platform
    // text-input service. Never dispatch a second cancel back into the tree
    // from this teardown path: focus/deactivation must stay non-reentrant.
    bridge->composing = false;
    bridge->awaitingCommit = false;
    resetPreedit(bridge);
    if (bridge->xic) {
      char* const resetText = XmbResetIC(bridge->xic);
      if (resetText) XFree(resetText);
    }
  }

  bridge->active = active;
  bridge->x = physicalX;
  bridge->y = physicalY;
  bridge->width = physicalWidth;
  bridge->height = physicalHeight;
  bridge->cursorOffset = physicalCursorOffset;
  if (active) updateSpotLocation(bridge);
}

bool
nativeuiImeConsumePuglText(NativeUIImeBridge* bridge,
                           const char* utf8,
                           size_t utf8Size)
{
  if (!bridge || !bridge->active ||
      (!bridge->composing && !bridge->awaitingCommit)) {
    return false;
  }

  emitEvent(bridge, NATIVEUI_IME_COMMIT, utf8, utf8Size, utf8Size, 0U);
  bridge->composing = false;
  bridge->awaitingCommit = false;
  resetPreedit(bridge);
  return true;
}

void
nativeuiImeFlushPendingCancel(NativeUIImeBridge* bridge)
{
  if (!bridge || !bridge->active || !bridge->awaitingCommit) return;
  emitEvent(bridge, NATIVEUI_IME_CANCEL, NULL, 0U, 0U, 0U);
  bridge->composing = false;
  bridge->awaitingCommit = false;
  resetPreedit(bridge);
}
