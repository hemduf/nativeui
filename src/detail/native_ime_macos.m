#include "native_ime_bridge.h"

#import <Cocoa/Cocoa.h>
#import <objc/message.h>
#import <objc/runtime.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NATIVEUI_STRINGIFY_IMPL(value) #value
#define NATIVEUI_STRINGIFY(value) NATIVEUI_STRINGIFY_IMPL(value)

struct NativeUIImeBridge {
  NSView* view;
  Class originalClass;
  id windowDelegate;
  Class originalWindowDelegateClass;
  void* userData;
  NativeUIImeCallback callback;
  bool active;
  bool composing;
  float x;
  float y;
  float width;
  float height;
  float cursorOffset;
};

// Per-object associated state only. The key is immutable and carries no
// instance-dependent state; each Pugl view owns its own retained NSValue box.
static const char kBridgeAssociationKey = 0;

static NativeUIImeBridge*
bridgeForObject(id object)
{
  NSValue* const value = objc_getAssociatedObject(object, &kBridgeAssociationKey);
  return value ? (NativeUIImeBridge*)[value pointerValue] : NULL;
}

static void
setBridgeForObject(id object, NativeUIImeBridge* bridge)
{
  if (bridge) {
    objc_setAssociatedObject(object,
                             &kBridgeAssociationKey,
                             [NSValue valueWithPointer:bridge],
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  } else {
    objc_setAssociatedObject(
      object, &kBridgeAssociationKey, nil, OBJC_ASSOCIATION_ASSIGN);
  }
}

static NSString*
plainString(id string)
{
  if ([string isKindOfClass:[NSAttributedString class]]) {
    return [(NSAttributedString*)string string];
  }
  return (NSString*)string;
}

static size_t
utf8SequenceLength(const unsigned char lead)
{
  if ((lead & 0x80U) == 0U) return 1U;
  if ((lead & 0xE0U) == 0xC0U) return 2U;
  if ((lead & 0xF0U) == 0xE0U) return 3U;
  if ((lead & 0xF8U) == 0xF0U) return 4U;
  return 1U;
}

static uint32_t
utf8Codepoint(const unsigned char* bytes, size_t available, size_t* consumed)
{
  const size_t wanted = utf8SequenceLength(bytes[0]);
  if (wanted == 1U || wanted > available) {
    *consumed = 1U;
    return bytes[0];
  }

  uint32_t codepoint = bytes[0] & ((1U << (7U - wanted)) - 1U);
  for (size_t i = 1; i < wanted; ++i) {
    if ((bytes[i] & 0xC0U) != 0x80U) {
      *consumed = 1U;
      return bytes[0];
    }
    codepoint = (codepoint << 6U) | (bytes[i] & 0x3FU);
  }

  *consumed = wanted;
  return codepoint;
}

static size_t
utf8ByteOffsetForUtf16Units(const char* utf8, size_t size, NSUInteger units)
{
  size_t byteOffset = 0;
  NSUInteger usedUnits = 0;
  while (byteOffset < size && usedUnits < units) {
    size_t consumed = 1U;
    const uint32_t codepoint = utf8Codepoint(
      (const unsigned char*)utf8 + byteOffset, size - byteOffset, &consumed);
    const NSUInteger codeUnits = codepoint > 0xFFFFU ? 2U : 1U;
    if (usedUnits + codeUnits > units) {
      break;
    }
    usedUnits += codeUnits;
    byteOffset += consumed;
  }
  return byteOffset;
}

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

static void
callSuperKeyDown(id self, SEL selector, NSEvent* event)
{
  struct objc_super superInfo = {self, class_getSuperclass(object_getClass(self))};
  ((void (*)(struct objc_super*, SEL, NSEvent*))objc_msgSendSuper)(
    &superInfo, selector, event);
}

static void
callSuperSetMarkedText(id self,
                       SEL selector,
                       id string,
                       NSRange selected,
                       NSRange replacement)
{
  struct objc_super superInfo = {self, class_getSuperclass(object_getClass(self))};
  ((void (*)(struct objc_super*, SEL, id, NSRange, NSRange))objc_msgSendSuper)(
    &superInfo, selector, string, selected, replacement);
}

static void
callSuperInsertText(id self, SEL selector, id string, NSRange replacement)
{
  struct objc_super superInfo = {self, class_getSuperclass(object_getClass(self))};
  ((void (*)(struct objc_super*, SEL, id, NSRange))objc_msgSendSuper)(
    &superInfo, selector, string, replacement);
}

static void
callSuperUnmarkText(id self, SEL selector)
{
  struct objc_super superInfo = {self, class_getSuperclass(object_getClass(self))};
  ((void (*)(struct objc_super*, SEL))objc_msgSendSuper)(&superInfo, selector);
}

static BOOL
nativeuiWindowShouldClose(id self, SEL selector, id sender)
{
  struct objc_super superInfo = {self, class_getSuperclass(object_getClass(self))};
  (void)((BOOL (*)(struct objc_super*, SEL, id))objc_msgSendSuper)(
    &superInfo, selector, sender);

  // Pugl's delegate dispatches PUGL_CLOSE and then returns YES, which lets
  // AppKit close the NSWindow immediately. NativeUI owns the v1 close policy:
  // the PUGL_CLOSE callback may veto and accepted closes are completed only at
  // the dispatcher checkpoint. Prevent AppKit from racing that policy; the
  // accepted path later calls puglUnrealize(), whose direct -[NSWindow close]
  // teardown does not consult windowShouldClose:.
  return NO;
}

static void
nativeuiKeyDown(id self, SEL selector, NSEvent* event)
{
  NativeUIImeBridge* bridge = bridgeForObject(self);
  if (bridge && bridge->active && bridge->composing) {
    // Pugl deliberately dispatches special keys without interpretKeyEvents:.
    // Once marked text is active, however, Return/Escape/arrows and similar
    // keys belong to the input manager. Feeding them directly to Cocoa keeps
    // candidate navigation/commit/cancel in the IME and prevents a second
    // NativeUI editor-command path from running for the same key sequence.
    [(NSView*)self interpretKeyEvents:@[event]];
    return;
  }

  callSuperKeyDown(self, selector, event);
}

static void
nativeuiSetMarkedText(id self,
                      SEL selector,
                      id string,
                      NSRange selected,
                      NSRange replacement)
{
  callSuperSetMarkedText(self, selector, string, selected, replacement);

  NativeUIImeBridge* bridge = bridgeForObject(self);
  if (!bridge || !bridge->active) {
    return;
  }

  NSString* const plain = plainString(string);
  const char* const utf8 = [plain UTF8String];
  const size_t size = utf8 ? strlen(utf8) : 0U;
  if (!bridge->composing) {
    bridge->composing = true;
    emitEvent(bridge, NATIVEUI_IME_START, NULL, 0U, 0U, 0U);
  }

  size_t cursorByte = size;
  size_t selectionBytes = 0U;
  if (selected.location != NSNotFound && utf8) {
    cursorByte = utf8ByteOffsetForUtf16Units(utf8, size, selected.location);
    const size_t selectionEnd = utf8ByteOffsetForUtf16Units(
      utf8, size, selected.location + selected.length);
    selectionBytes = selectionEnd >= cursorByte ? selectionEnd - cursorByte : 0U;
  }

  emitEvent(
    bridge, NATIVEUI_IME_UPDATE, utf8, size, cursorByte, selectionBytes);
}

static void
nativeuiInsertText(id self, SEL selector, id string, NSRange replacement)
{
  NativeUIImeBridge* bridge = bridgeForObject(self);
  if (!bridge || !bridge->active || !bridge->composing) {
    callSuperInsertText(self, selector, string, replacement);
    return;
  }

  NSString* const plain = plainString(string);
  const char* const utf8 = [plain UTF8String];
  const size_t size = utf8 ? strlen(utf8) : 0U;

  bridge->composing = false;
  emitEvent(bridge, NATIVEUI_IME_COMMIT, utf8, size, size, 0U);

  // Keep Pugl's NSTextInputClient bookkeeping in sync without forwarding the
  // committed text as PUGL_TEXT. The composition event above is the single
  // authoritative commit path for marked-text input.
  callSuperUnmarkText(self, sel_registerName("unmarkText"));
}

static void
nativeuiUnmarkText(id self, SEL selector)
{
  NativeUIImeBridge* bridge = bridgeForObject(self);
  if (bridge && bridge->active && bridge->composing) {
    bridge->composing = false;
    emitEvent(bridge, NATIVEUI_IME_CANCEL, NULL, 0U, 0U, 0U);
  }
  callSuperUnmarkText(self, selector);
}

static NSRect
nativeuiFirstRectForCharacterRange(id self,
                                   SEL selector,
                                   NSRange range,
                                   NSRangePointer actual)
{
  (void)selector;
  (void)range;
  if (actual) {
    *actual = NSMakeRange(NSNotFound, 0U);
  }

  NativeUIImeBridge* bridge = bridgeForObject(self);
  NSView* const view = (NSView*)self;
  NSWindow* const window = [view window];
  if (!bridge || !bridge->active || !window) {
    const NSRect bounds = [view bounds];
    return NSMakeRect(bounds.origin.x, bounds.origin.y, 1.0, 1.0);
  }

  const CGFloat backingScale = [window backingScaleFactor];
  const CGFloat scale =
    backingScale > (CGFloat)1.0 ? backingScale : (CGFloat)1.0;
  const CGFloat logicalHeight = bridge->height / scale;
  const CGFloat localHeight =
    logicalHeight > (CGFloat)1.0 ? logicalHeight : (CGFloat)1.0;
  const NSRect local = NSMakeRect(
    (bridge->x + bridge->cursorOffset) / scale,
    bridge->y / scale,
    1.0 / scale,
    localHeight);
  const NSRect windowRect = [view convertRect:local toView:nil];
  return [window convertRectToScreen:windowRect];
}

static bool
addOverride(Class subclass, Class original, const char* selectorName, IMP implementation)
{
  const SEL selector = sel_registerName(selectorName);
  const Method originalMethod = class_getInstanceMethod(original, selector);
  return originalMethod && class_addMethod(
    subclass, selector, implementation, method_getTypeEncoding(originalMethod));
}

static Class
bridgeSubclass(Class original)
{
  // The original Pugl class name already includes NATIVEUI_OBJC_RUNTIME_PREFIX
  // from the static-library build contract. Deriving from it keeps this helper
  // class consumer-specific as required for plugin-host coexistence.
  const char* const originalName = class_getName(original);
  const size_t nameSize = strlen(originalName) + sizeof("_NativeUIImeView");
  char* const name = (char*)calloc(nameSize, 1U);
  if (!name) {
    return Nil;
  }
  snprintf(name, nameSize, "%s_NativeUIImeView", originalName);

  Class subclass = objc_lookUpClass(name);
  if (subclass) {
    free(name);
    return class_getSuperclass(subclass) == original ? subclass : Nil;
  }

  subclass = objc_allocateClassPair(original, name, 0U);
  free(name);
  if (!subclass) {
    return Nil;
  }

  if (!addOverride(subclass, original, "keyDown:", (IMP)nativeuiKeyDown) ||
      !addOverride(subclass,
                   original,
                   "setMarkedText:selectedRange:replacementRange:",
                   (IMP)nativeuiSetMarkedText) ||
      !addOverride(subclass,
                   original,
                   "insertText:replacementRange:",
                   (IMP)nativeuiInsertText) ||
      !addOverride(subclass, original, "unmarkText", (IMP)nativeuiUnmarkText) ||
      !addOverride(subclass,
                   original,
                   "firstRectForCharacterRange:actualRange:",
                   (IMP)nativeuiFirstRectForCharacterRange)) {
    objc_disposeClassPair(subclass);
    return Nil;
  }

  objc_registerClassPair(subclass);
  return subclass;
}

static Class
closeGuardSubclass(Class original)
{
#if defined(PuglWindowDelegate)
  // Only NativeUI's consumer-prefixed Pugl top-level delegate is eligible.
  // Embedded views live inside host-owned NSWindows and must never mutate or
  // retain the host's window delegate.
  const char* const originalName = class_getName(original);
  if (strcmp(originalName, NATIVEUI_STRINGIFY(PuglWindowDelegate)) != 0) {
    return original;
  }

  const size_t nameSize = strlen(originalName) + sizeof("_NativeUICloseGuard");
  char* const name = (char*)calloc(nameSize, 1U);
  if (!name) {
    return Nil;
  }
  snprintf(name, nameSize, "%s_NativeUICloseGuard", originalName);

  Class subclass = objc_lookUpClass(name);
  if (subclass) {
    free(name);
    return class_getSuperclass(subclass) == original ? subclass : Nil;
  }

  subclass = objc_allocateClassPair(original, name, 0U);
  free(name);
  if (!subclass) {
    return Nil;
  }

  if (!addOverride(
        subclass, original, "windowShouldClose:", (IMP)nativeuiWindowShouldClose)) {
    objc_disposeClassPair(subclass);
    return Nil;
  }

  objc_registerClassPair(subclass);
  return subclass;
#else
  return original;
#endif
}

NativeUIImeBridge*
nativeuiImeCreate(PuglWorld* world,
                  PuglView* puglView,
                  void* userData,
                  NativeUIImeCallback callback)
{
  (void)world;
  if (!puglView || !callback || ![NSThread isMainThread]) {
    return NULL;
  }

  NSView* const view = (NSView*)(uintptr_t)puglGetNativeView(puglView);
  if (!view) {
    return NULL;
  }

  Class original = object_getClass(view);
  Class subclass = bridgeSubclass(original);
  if (!subclass) {
    return NULL;
  }

  NSWindow* const window = [view window];
  id const candidateDelegate = window ? [window delegate] : nil;
  Class const candidateDelegateClass =
    candidateDelegate ? object_getClass(candidateDelegate) : Nil;
  Class const closeGuard = candidateDelegateClass
    ? closeGuardSubclass(candidateDelegateClass)
    : Nil;
  if (candidateDelegateClass && !closeGuard) {
    return NULL;
  }

  const bool installCloseGuard =
    candidateDelegate && closeGuard && closeGuard != candidateDelegateClass;

  NativeUIImeBridge* const bridge =
    (NativeUIImeBridge*)calloc(1U, sizeof(NativeUIImeBridge));
  if (!bridge) {
    return NULL;
  }

  bridge->view = view;
  bridge->originalClass = original;
  bridge->windowDelegate = installCloseGuard ? candidateDelegate : nil;
  bridge->originalWindowDelegateClass =
    installCloseGuard ? candidateDelegateClass : Nil;
  bridge->userData = userData;
  bridge->callback = callback;

  object_setClass(view, subclass);
  if (installCloseGuard) {
    object_setClass(candidateDelegate, closeGuard);
  }
  setBridgeForObject(view, bridge);
  return bridge;
}

void
nativeuiImeDestroy(NativeUIImeBridge* bridge)
{
  if (!bridge) {
    return;
  }

  nativeuiImeUpdate(bridge, false, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F);
  if (bridge->view) {
    setBridgeForObject(bridge->view, NULL);
    object_setClass(bridge->view, bridge->originalClass);
  }
  if (bridge->windowDelegate && bridge->originalWindowDelegateClass) {
    object_setClass(
      bridge->windowDelegate, bridge->originalWindowDelegateClass);
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
  if (!bridge) {
    return;
  }

  // TextInput/TextArea cancel their model before disabling the native boundary.
  // Do not dispatch back into the tree here: focus teardown is intentionally
  // one-way and non-reentrant.
  if (!active && bridge->composing) {
    bridge->composing = false;
  }

  bridge->active = active;
  bridge->x = physicalX;
  bridge->y = physicalY;
  bridge->width = physicalWidth;
  bridge->height = physicalHeight;
  bridge->cursorOffset = physicalCursorOffset;

  if (!active && bridge->view) {
    callSuperUnmarkText(bridge->view, sel_registerName("unmarkText"));
  }
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
