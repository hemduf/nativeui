#import <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <nativeui/layout.hpp>
#include <nativeui/widgets.hpp>
#include <nativeui/window.hpp>
#include <objc/runtime.h>
#include <stdexcept>
#include <string>
#include <vector>

@interface NativeUIKeyboardTestHostView : NSView
@property(nonatomic, strong) NSMutableArray<NSEvent *> *keyDownEvents;
@property(nonatomic, strong) NSMutableArray<NSEvent *> *keyUpEvents;
@property(nonatomic, strong) NSMutableArray<NSEvent *> *modifierEvents;
@property(nonatomic, copy) void (^onKeyDown)(NSEvent *);
@end

@implementation NativeUIKeyboardTestHostView
- (instancetype)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _keyDownEvents = [NSMutableArray array];
    _keyUpEvents = [NSMutableArray array];
    _modifierEvents = [NSMutableArray array];
  }
  return self;
}
- (BOOL)acceptsFirstResponder {
  return YES;
}
- (void)keyDown:(NSEvent *)event {
  [self.keyDownEvents addObject:event];
  if (self.onKeyDown)
    self.onKeyDown(event);
}
- (void)keyUp:(NSEvent *)event {
  [self.keyUpEvents addObject:event];
}
- (void)flagsChanged:(NSEvent *)event {
  [self.modifierEvents addObject:event];
}
@end

// Model an input manager that accepts a native key by starting marked text.
// This exercises Cocoa's inputContext -> NSTextInputClient route without
// depending on whichever system input language is currently selected.
@interface NativeUIKeyboardTestCompositionContext : NSTextInputContext
@end
@implementation NativeUIKeyboardTestCompositionContext
- (BOOL)handleEvent:(NSEvent *)event {
  if (event.type != NSEventTypeKeyDown)
    return NO;
  [self.client setMarkedText:@"é"
               selectedRange:NSMakeRange(1, 0)
            replacementRange:NSMakeRange(NSNotFound, 0)];
  return YES;
}
@end

namespace {
void expect(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

struct KeyboardProbe {
  std::vector<ui::InputEvent> downs;
  std::vector<ui::InputEvent> ups;
  ui::Key consumed{ui::Key::None};
  std::function<void()> onConsumed;
};

class KeyboardComponent final : public ui::Component {
public:
  explicit KeyboardComponent(KeyboardProbe &probe) : probe_(probe) {}
  bool focusable() const noexcept override { return true; }
  ui::Size measure(const std::vector<ui::ChildMetrics> &) const override { return {100, 60}; }
  void paint(ui::PaintContext &) const override {}
  ui::EventResult input(const ui::InputEvent &event, ui::InputContext &) override {
    if (event.type == ui::InputType::KeyDown) {
      probe_.downs.push_back(event);
      if (probe_.consumed != ui::Key::None && event.key == probe_.consumed) {
        if (probe_.onConsumed)
          probe_.onConsumed();
        return ui::EventResult::Handled;
      }
    } else if (event.type == ui::InputType::KeyUp) {
      probe_.ups.push_back(event);
      // A control may own a press while deliberately ignoring its release.
      // That release must still stay with the plugin, away from the DAW.
    } else if (event.type == ui::InputType::PointerDown || event.type == ui::InputType::PointerUp) {
      return ui::EventResult::Handled;
    }
    return ui::EventResult::Ignored;
  }

private:
  KeyboardProbe &probe_;
};

struct ProbeSpec {
  KeyboardProbe &probe;
  ui::Spec spec() const {
    return {[thisProbe = &probe] { return std::make_unique<KeyboardComponent>(*thisProbe); }, {}};
  }
};

ui::NativeParentHandle parent(NSView *view) {
  return reinterpret_cast<ui::NativeParentHandle>((__bridge void *)view);
}
NSView *native(ui::EmbeddedView &child) {
  return (__bridge NSView *)reinterpret_cast<void *>(child.native_handle());
}
NSEvent *key(NSWindow *window, NSEventType type, NSString *characters, unsigned short code,
             NSEventModifierFlags modifiers = 0, bool repeat = false) {
  static NSTimeInterval timestamp = 1;
  timestamp += .01;
  return [NSEvent keyEventWithType:type
                          location:NSZeroPoint
                     modifierFlags:modifiers
                         timestamp:timestamp
                      windowNumber:window.windowNumber
                           context:nil
                        characters:characters
       charactersIgnoringModifiers:characters
                         isARepeat:repeat
                           keyCode:code];
}
void sendNativeKey(NSEvent *event) {
  [NSApp postEvent:event atStart:YES];
  NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:.05];
  for (int attempt = 0; attempt < 16; ++attempt) {
    NSEvent *queued = [NSApp nextEventMatchingMask:NSEventMaskFromType(event.type)
                                         untilDate:deadline
                                            inMode:NSDefaultRunLoopMode
                                           dequeue:YES];
    if (queued == nil)
      break;
    const bool matches =
        queued.type == event.type && queued.windowNumber == event.windowNumber &&
        queued.keyCode == event.keyCode &&
        std::abs(queued.timestamp - event.timestamp) <= .000001 &&
        (queued.modifierFlags & ~NSEventModifierFlagFunction) ==
            (event.modifierFlags & ~NSEventModifierFlagFunction) &&
        queued.isARepeat == event.isARepeat &&
        [queued.characters isEqualToString:event.characters] &&
        [queued.charactersIgnoringModifiers isEqualToString:event.charactersIgnoringModifiers];
    // The normal dequeue -> send route establishes NSApp.currentEvent for the
    // NSTextInputClient callbacks that Cocoa may run synchronously. AppKit may
    // copy a posted event, normalize its timestamp, and add the function flag
    // for arrow characters. Compare the remaining contents, not pointer identity.
    [NSApp sendEvent:queued];
    if (matches)
      return;
  }
  expect(false, "AppKit dequeues the posted keyboard event before its short deadline");
}
void keyPair(NSWindow *window, NSString *characters, unsigned short code,
             NSEventModifierFlags modifiers = 0) {
  sendNativeKey(key(window, NSEventTypeKeyDown, characters, code, modifiers));
  sendNativeKey(key(window, NSEventTypeKeyUp, characters, code, modifiers));
}
void click(NSView *view, NSPoint point = NSMakePoint(20, 20)) {
  NSWindow *window = view.window;
  for (const auto type : {NSEventTypeLeftMouseDown, NSEventTypeLeftMouseUp}) {
    NSEvent *event = [NSEvent mouseEventWithType:type
                                        location:[view convertPoint:point toView:nil]
                                   modifierFlags:0
                                       timestamp:1
                                    windowNumber:window.windowNumber
                                         context:nil
                                     eventNumber:1
                                      clickCount:1
                                        pressure:1];
    [window sendEvent:event];
  }
  expect(window.firstResponder == view, "A real Cocoa click focuses the embedded view");
}
void clear(NativeUIKeyboardTestHostView *host) {
  [host.keyDownEvents removeAllObjects];
  [host.keyUpEvents removeAllObjects];
  [host.modifierEvents removeAllObjects];
}
void expectEmpty(NativeUIKeyboardTestHostView *host, const char *message) {
  expect(host.keyDownEvents.count == 0 && host.keyUpEvents.count == 0 &&
             host.modifierEvents.count == 0,
         message);
}

void ignoredAndHandledKeys(NSWindow *window, NativeUIKeyboardTestHostView *host, ui::EmbeddedView &child,
                           KeyboardProbe &probe) {
  NSView *view = native(child);
  click(view);
  clear(host);
  const auto downs = probe.downs.size();
  const auto ups = probe.ups.size();
  keyPair(window, @" ", 49);
  expect(host.keyDownEvents.count == 1 && host.keyUpEvents.count == 1,
         "Unconsumed Space reaches the DAW parent exactly once for press and release");
  expect(probe.downs.size() == downs + 1 && probe.ups.size() == ups + 1,
         "The UI sees unconsumed Space once before host fallback");

  clear(host);
  const auto modifiers = NSEventModifierFlagShift | NSEventModifierFlagControl |
                         NSEventModifierFlagOption | NSEventModifierFlagCommand;
  NSEvent *letterDown = key(window, NSEventTypeKeyDown, @"m", 46, modifiers);
  NSEvent *letterRepeat = key(window, NSEventTypeKeyDown, @"m", 46, modifiers, true);
  NSEvent *letterUp = key(window, NSEventTypeKeyUp, @"m", 46, modifiers);
  [window sendEvent:letterDown];
  [window sendEvent:letterRepeat];
  [window sendEvent:letterUp];
  expect(host.keyDownEvents.count == 2 && host.keyUpEvents.count == 1 &&
             host.keyDownEvents[0] == letterDown && host.keyDownEvents[1] == letterRepeat &&
             host.keyUpEvents[0] == letterUp,
         "Host fallback preserves original letter, modifiers, repeat and release events");
  expect(probe.downs.back().key == ui::Key::M && probe.downs.back().shift &&
             probe.downs.back().ctrl && probe.downs.back().alt && probe.downs.back().gui,
         "NativeUI keeps the letter and all modifier flags during local dispatch");

  clear(host);
  NSEvent *shiftDown = key(window, NSEventTypeFlagsChanged, @"", 56, NSEventModifierFlagShift);
  NSEvent *shiftUp = key(window, NSEventTypeFlagsChanged, @"", 56);
  [window sendEvent:shiftDown];
  [window sendEvent:shiftUp];
  expect(host.modifierEvents.count == 2 && host.modifierEvents[0] == shiftDown &&
             host.modifierEvents[1] == shiftUp,
         "Unconsumed modifier changes reach the host as their original Cocoa events");

  clear(host);
  // Bypass the standalone test application's own Quit menu while exercising
  // its embedded child's host fallback for this host-owned accelerator.
  [window sendEvent:key(window, NSEventTypeKeyDown, @"q", 12, NSEventModifierFlagCommand)];
  [window sendEvent:key(window, NSEventTypeKeyUp, @"q", 12, NSEventModifierFlagCommand)];
  expect(!child.should_close() && host.keyDownEvents.count == 1 && host.keyUpEvents.count == 1,
         "An embedded editor leaves unconsumed Command-Q to its host");

  clear(host);
  probe.consumed = ui::Key::Space;
  const auto handledDowns = probe.downs.size();
  const auto handledUps = probe.ups.size();
  keyPair(window, @" ", 49);
  expect(probe.downs.size() == handledDowns + 1 && probe.ups.size() == handledUps + 1,
         "A handled press and ignored release both reach the local component once");
  expectEmpty(host, "A plugin-owned Space press never leaks its ignored release to the host");
  [window sendEvent:key(window, NSEventTypeKeyDown, @" ", 49)];
  probe.consumed = ui::Key::None;
  [window sendEvent:key(window, NSEventTypeKeyDown, @" ", 49, 0, true)];
  [window sendEvent:key(window, NSEventTypeKeyUp, @" ", 49)];
  expectEmpty(host, "A plugin-owned press keeps its repeats and release local if handling changes");
  probe.consumed = ui::Key::None;
}

void equivalentKeys(NSWindow *window, NativeUIKeyboardTestHostView *host, ui::EmbeddedView &child,
                    KeyboardProbe &probe) {
  NSView *view = native(child);
  click(view);
  for (const bool handled : {false, true}) {
    clear(host);
    probe.consumed = handled ? ui::Key::M : ui::Key::None;
    const auto downs = probe.downs.size();
    NSEvent *event = key(window, NSEventTypeKeyDown, @"m", 46, NSEventModifierFlagCommand);
    expect([view performKeyEquivalent:event] == handled,
           "Cocoa key-equivalent result reflects local UI consumption");
    expectEmpty(host, "A key-equivalent probe cannot deliver the shortcut to the host");
    expect(probe.downs.size() == downs + 1,
           "A key-equivalent probe dispatches to the focused UI exactly once");
    [view keyDown:event];
    expect(probe.downs.size() == downs + 1,
           "keyDown after probing the same NSEvent cannot dispatch the shortcut twice");
    [view keyUp:key(window, NSEventTypeKeyUp, @"m", 46, NSEventModifierFlagCommand)];
    if (handled) {
      expectEmpty(host, "Consumed key equivalents and their releases stay local");
    } else {
      expect(host.keyDownEvents.count == 1 && host.keyDownEvents[0] == event &&
                 host.keyUpEvents.count == 1,
             "Ignored key equivalents reach the host once during ordinary key delivery");
    }
  }
  probe.consumed = ui::Key::None;
}

char inputContextAssociation;
IMP originalInputContext{};
IMP originalInterpretKeyEvents{};
id testInputContext(id object, SEL selector) {
  id context = objc_getAssociatedObject(object, &inputContextAssociation);
  if (context)
    return context;
  return reinterpret_cast<id (*)(id, SEL)>(originalInputContext)(object, selector);
}
void testInterpretKeyEvents(id object, SEL selector, NSArray<NSEvent *> *events) {
  NSTextInputContext *context = objc_getAssociatedObject(object, &inputContextAssociation);
  if (context) {
    for (NSEvent *event in events)
      [context handleEvent:event];
    return;
  }
  reinterpret_cast<void (*)(id, SEL, NSArray<NSEvent *> *)>(originalInterpretKeyEvents)(
      object, selector, events);
}
class ScopedCompositionContext {
public:
  explicit ScopedCompositionContext(NSView *view) : view_(view), class_(object_getClass(view)) {
    const auto selector = @selector(inputContext);
    const auto method = class_getInstanceMethod(class_, selector);
    expect(method != nullptr, "The native Cocoa view exposes its input context");
    originalInputContext = class_getMethodImplementation(class_, selector);
    encoding_ = method_getTypeEncoding(method);
    id object = view;
    id<NSTextInputClient> client = object;
    auto *context = [[NativeUIKeyboardTestCompositionContext alloc] initWithClient:client];
    objc_setAssociatedObject(view_, &inputContextAssociation, context,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    // Override only this test consumer's view class and use an association to
    // change the input context of exactly one object. Other instances retain
    // the original context; no NSView/AppKit method is changed globally.
    class_replaceMethod(class_, selector, reinterpret_cast<IMP>(testInputContext), encoding_);
    const auto interpretSelector = @selector(interpretKeyEvents:);
    const auto interpretMethod = class_getInstanceMethod(class_, interpretSelector);
    expect(interpretMethod != nullptr, "The native view exposes Cocoa key interpretation");
    originalInterpretKeyEvents = class_getMethodImplementation(class_, interpretSelector);
    interpretEncoding_ = method_getTypeEncoding(interpretMethod);
    // NSView may call private NSTextInputContext helpers internally. Keep the
    // injected input manager deterministic through its public handleEvent API.
    class_replaceMethod(class_, interpretSelector, reinterpret_cast<IMP>(testInterpretKeyEvents),
                        interpretEncoding_);
  }
  ~ScopedCompositionContext() {
    class_replaceMethod(class_, @selector(interpretKeyEvents:), originalInterpretKeyEvents,
                        interpretEncoding_);
    class_replaceMethod(class_, @selector(inputContext), originalInputContext, encoding_);
    objc_setAssociatedObject(view_, &inputContextAssociation, nil,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  ScopedCompositionContext(const ScopedCompositionContext &) = delete;
  ScopedCompositionContext &operator=(const ScopedCompositionContext &) = delete;

private:
  NSView *view_;
  Class class_;
  const char *encoding_{};
  const char *interpretEncoding_{};
};

void textEditing(NSWindow *window, NativeUIKeyboardTestHostView *host) {
  KeyboardProbe probe;
  ui::State<std::string> text{std::string{}};
  ui::TextInputStyle style;
  style.base.control_width = 180;
  style.base.control_height = 60;
  style.base.field_top = 0;
  style.base.field_height = 60;
  ui::UI tree{ui::Row{ui::TextInput{"Text", text}.style(style), ProbeSpec{probe}}};
  ui::EmbeddedView child{tree, parent(host), {320, 100}};
  NSView *view = native(child);
  [view setFrameOrigin:NSMakePoint(0, 120)];
  expect(child.poll(), "Text-input embedded view polls successfully");
  click(view);
  clear(host);
  keyPair(window, @" ", 49);
  keyPair(window, @"a", 0);
  expect(text.get() == " a", "Focused TextInput receives Space and letters through Cocoa");
  NSString *left = [NSString stringWithFormat:@"%C", static_cast<unichar>(NSLeftArrowFunctionKey)];
  keyPair(window, left, 123);
  keyPair(window, @"\x7f", 51);
  expect(text.get() == "a", "Arrow and backspace edit the focused TextInput");

  NSEvent *selectAll = key(window, NSEventTypeKeyDown, @"a", 0, NSEventModifierFlagCommand);
  expect([view performKeyEquivalent:selectAll], "TextInput owns the Select All key equivalent");
  [view keyDown:selectAll];
  [view keyUp:key(window, NSEventTypeKeyUp, @"a", 0, NSEventModifierFlagCommand)];
  keyPair(window, @"b", 11);
  expect(text.get() == "b", "Select All and typing replace the local selection");
  NSEvent *undo = key(window, NSEventTypeKeyDown, @"z", 6, NSEventModifierFlagCommand);
  expect([view performKeyEquivalent:undo] && text.get() == "a",
         "The text-editing key equivalent performs exactly one Undo");
  [view keyDown:undo];
  [view keyUp:key(window, NSEventTypeKeyUp, @"z", 6, NSEventModifierFlagCommand)];
  expect(text.get() == "a", "keyDown for the same Undo event cannot undo a second text edit");
  keyPair(window, @"a", 0, NSEventModifierFlagCommand);
  keyPair(window, @"b", 11);
  expect(text.get() == "b", "Text editing remains usable after probing an Undo equivalent");
  expectEmpty(host, "Text entry, editing commands and releases never reach the DAW");

  {
    ScopedCompositionContext inputManager{view};
    keyPair(window, @"e", 14);
    id object = view;
    id<NSTextInputClient> client = object;
    expect([client hasMarkedText], "Cocoa input context starts marked text from a native key");
    expectEmpty(host, "A key accepted to start IME composition and its release stay local");
    [client insertText:@"é" replacementRange:NSMakeRange(NSNotFound, 0)];
    expect(text.get() == "bé", "Marked text commits through the native text-input client");
  }

  click(view, NSMakePoint(220, 20));
  keyPair(window, @" ", 49);
  expect(text.get() == "bé" && host.keyDownEvents.count == 1 && host.keyUpEvents.count == 1,
         "After focus leaves TextInput, Space resumes the host shortcut route");
  child.request_close();
}

void lifecycleAndInstances(NSWindow *window, NativeUIKeyboardTestHostView *first,
                           NativeUIKeyboardTestHostView *second, ui::EmbeddedView &child,
                           KeyboardProbe &probe) {
  KeyboardProbe siblingProbe;
  siblingProbe.consumed = ui::Key::Space;
  ui::UI siblingTree{ProbeSpec{siblingProbe}};
  ui::EmbeddedView sibling{siblingTree, parent(second), {320, 100}};
  expect(sibling.poll(), "Independent embedded view polls successfully");
  clear(first);
  clear(second);
  click(native(sibling));
  const auto unfocusedDowns = probe.downs.size();
  NSEvent *equivalent = key(window, NSEventTypeKeyDown, @"m", 46, NSEventModifierFlagCommand);
  expect(![native(child) performKeyEquivalent:equivalent] && probe.downs.size() == unfocusedDowns,
         "An unfocused sibling cannot claim or dispatch a Cocoa key-equivalent probe");
  expectEmpty(first, "An unfocused equivalent probe does not send keys to its parent");
  keyPair(window, @" ", 49);
  expect(siblingProbe.downs.size() == 1, "The independently focused instance handles its key");
  expectEmpty(first, "Sibling handling never reaches the first instance's host");
  expectEmpty(second, "Sibling handling keeps its key and release away from its own host");
  click(native(child));
  keyPair(window, @" ", 49);
  expect(first.keyDownEvents.count == 1 && first.keyUpEvents.count == 1,
         "The first instance keeps an independent host fallback policy");
  expect(siblingProbe.downs.size() == 1, "Host fallback never redispatches to a sibling UI");

  expect(child.hide(), "Hide preserves the native view");
  expect(window.firstResponder != native(child), "Hide relinquishes native keyboard focus");
  NSResponder *responder = window.firstResponder;
  expect(child.show() && window.firstResponder == responder,
         "Show remains passive and does not steal the host's keyboard focus");
  clear(first);
  click(native(child));
  keyPair(window, @" ", 49);
  expect(first.keyDownEvents.count == 1 && first.keyUpEvents.count == 1,
         "Host shortcuts continue after hiding and showing an instance");

  // EmbeddedView has no reparent API. Moving its NSView exercises Cocoa's
  // responder chain after a host reparents the native child.
  expect(child.hide(), "Hide before native host reparenting");
  [native(child) removeFromSuperview];
  [second addSubview:native(child)];
  [native(child) setFrameOrigin:NSMakePoint(0, 120)];
  expect(child.show(), "Show the native child after host reparenting");
  clear(first);
  clear(second);
  click(native(child));
  const auto downs = probe.downs.size();
  keyPair(window, @" ", 49);
  expect(probe.downs.size() == downs + 1 && second.keyDownEvents.count == 1 &&
             second.keyUpEvents.count == 1,
         "Host fallback follows the current Cocoa parent after reparenting");
  expectEmpty(first, "The old parent cannot receive shortcuts after reparenting");
  sibling.request_close();
}

void retainedFocusAfterFallback(NSWindow *window, NativeUIKeyboardTestHostView *host) {
  KeyboardProbe firstProbe;
  KeyboardProbe secondProbe;
  firstProbe.consumed = secondProbe.consumed = ui::Key::Right;
  ui::UI tree{ui::Row{ProbeSpec{firstProbe}, ProbeSpec{secondProbe}}};
  ui::EmbeddedView child{tree, parent(host), {320, 100}};
  NSView *view = native(child);
  [view setFrameOrigin:NSMakePoint(0, 120)];
  expect(child.poll(), "The local-focus fixture polls successfully");
  click(view, NSMakePoint(140, 20));
  clear(host);
  keyPair(window, @" ", 49);
  expect(host.keyDownEvents.count == 1 && host.keyUpEvents.count == 1,
         "The second focused component leaves Space with its host");
  NSString *right =
      [NSString stringWithFormat:@"%C", static_cast<unichar>(NSRightArrowFunctionKey)];
  keyPair(window, right, 124);
  expect(firstProbe.downs.empty() && secondProbe.downs.size() == 2 &&
             secondProbe.downs.back().key == ui::Key::Right,
         "Host fallback preserves retained focus on the second component for the next local key");
  expect(host.keyDownEvents.count == 1 && host.keyUpEvents.count == 1,
         "The retained focused component consumes its arrow and release after host fallback");
  child.request_close();
}

void failedLocalDispatch(NSWindow *window, NativeUIKeyboardTestHostView *host) {
  for (const bool textFailure : {false, true}) {
    KeyboardProbe failingProbe;
    failingProbe.consumed = ui::Key::Space;
    int effects = 0;
    failingProbe.onConsumed = [&] {
      ++effects;
      throw std::runtime_error("Injected keyboard callback failure after a local effect");
    };
    ui::State<std::string> value{"before"};
    auto failingTree = textFailure
                           ? std::make_unique<ui::UI>(ui::TextInput{"Value", value})
                           : std::make_unique<ui::UI>(ProbeSpec{failingProbe});
    ui::EmbeddedView failing{*failingTree, parent(host), {320, 100}};
    expect(failing.poll(), "The failing keyboard fixture polls before injection");
    click(native(failing));
    auto observer = value.observe([&](const std::string &) {
      ++effects;
      throw std::runtime_error("Injected text observer failure after mutation");
    });
    clear(host);
    keyPair(window, textFailure ? @"a" : @" ", textFailure ? 0 : 49);
    expect(effects == 1 && failing.should_close(),
           "A throwing native keyboard/text callback executes its local effect once and closes");
    if (textFailure)
      expect(value.get() != "before", "The text observer throws after the accepted local mutation");
    expectEmpty(host, "A failed local key or text mutation cannot also execute a host shortcut");

    KeyboardProbe independentProbe;
    independentProbe.consumed = ui::Key::Right;
    ui::UI independentTree{ProbeSpec{independentProbe}};
    ui::EmbeddedView independent{independentTree, parent(host), {320, 100}};
    [native(independent) setFrameOrigin:NSMakePoint(0, 120)];
    expect(independent.poll(), "An independent native view survives the neighboring failure");
    click(native(independent));
    NSString *right =
        [NSString stringWithFormat:@"%C", static_cast<unichar>(NSRightArrowFunctionKey)];
    keyPair(window, right, 124);
    expect(independentProbe.downs.size() == 1 && independentProbe.ups.size() == 1,
           "The next operation in another instance retains normal key ownership after failure");
    expectEmpty(host, "Independent handled input still cannot leak to the host after failure");
    independent.request_close();
    failing.request_close();
  }
}

void localCloseDuringKey(NSWindow *window, NativeUIKeyboardTestHostView *host) {
  for (const bool space : {false, true}) {
    KeyboardProbe probe;
    probe.consumed = space ? ui::Key::Space : ui::Key::A;
    ui::UI tree{ProbeSpec{probe}};
    ui::EmbeddedView child{tree, parent(host), {320, 100}};
    NSView *view = native(child);
    expect(child.poll(), "The local-close keyboard fixture polls successfully");
    bool closed = false;
    probe.onConsumed = [&] {
      closed = true;
      child.request_close();
    };
    click(view);
    clear(host);
    [window sendEvent:key(window, NSEventTypeKeyDown, space ? @" " : @"a", space ? 49 : 0)];
    expect(closed && probe.downs.size() == 1 && child.native_handle() == 0 && view.superview == nil,
           "A local control may close its native view synchronously while consuming a key");
    expectEmpty(host, "A key that closed its local editor cannot leak into host fallback");
  }
}

void hostCallbacks(NSWindow *window, NativeUIKeyboardTestHostView *first, NativeUIKeyboardTestHostView *second,
                   ui::EmbeddedView &child) {
  NSView *view = native(child);
  click(view);
  clear(first);
  clear(second);
  second.onKeyDown = ^(NSEvent *) {
    [view removeFromSuperview];
    [first addSubview:view];
    [view setFrameOrigin:NSZeroPoint];
    // Cocoa relinquishes first responder when a host detaches a child. The
    // host owns the focus decision after it attaches the child again.
    [window makeFirstResponder:view];
  };
  keyPair(window, @" ", 49);
  second.onKeyDown = nil;
  expect(second.keyDownEvents.count == 1 && first.keyUpEvents.count == 1 &&
             second.keyUpEvents.count == 0 && first.keyDownEvents.count == 0,
         "Host reparenting during keyDown preserves single delivery and uses the current parent");
  expect(window.firstResponder == view, "Host reparenting preserves the live editor's responder");

  clear(first);
  ui::EmbeddedView *closingChild = &child;
  first.onKeyDown = ^(NSEvent *) {
    closingChild->request_close();
    [window close];
  };
  [window sendEvent:key(window, NSEventTypeKeyDown, @" ", 49)];
  first.onKeyDown = nil;
  expect(child.native_handle() == 0 && view.superview == nil && !window.visible &&
             window.firstResponder != view && first.keyDownEvents.count == 1,
         "Host closure during fallback safely detaches the editor without restoring a dead view");
}
} // namespace

int main() {
  @autoreleasepool {
    @try {
      try {
        [NSApplication sharedApplication];
        if (std::getenv("NATIVEUI_TEST_EXPECT_LIVE"))
          expect([[NSBundle mainBundle].bundleIdentifier isEqualToString:@"com.ableton.live"],
                 "The Live fixture selects the Ableton responder workaround by its bundle ID");
        const auto session = CGSessionCopyCurrentDictionary();
        if (!session || [NSScreen mainScreen] == nil) {
          if (session)
            CFRelease(session);
          std::fprintf(stderr, "WindowServer access required\n");
          return 77;
        }
        CFRelease(session);
        ui::Application application;
        expect(application.valid(), "Create the native Cocoa application event loop");
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(50, 50, 640, 260)
                                                       styleMask:NSWindowStyleMaskTitled
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        window.releasedWhenClosed = NO;
        NativeUIKeyboardTestHostView *first =
            [[NativeUIKeyboardTestHostView alloc] initWithFrame:NSMakeRect(0, 0, 320, 260)];
        NativeUIKeyboardTestHostView *second =
            [[NativeUIKeyboardTestHostView alloc] initWithFrame:NSMakeRect(320, 0, 320, 260)];
        [window.contentView addSubview:first];
        [window.contentView addSubview:second];
        [NSApp activateIgnoringOtherApps:YES];
        [window makeKeyAndOrderFront:nil];
        for (int attempt = 0; attempt < 100 && !window.keyWindow; ++attempt) {
          application.poll(.004);
          [window makeKeyAndOrderFront:nil];
        }
        expect(window.keyWindow, "The keyboard regression test has an active Cocoa window");
        KeyboardProbe probe;
        ui::UI tree{ProbeSpec{probe}};
        {
          ui::EmbeddedView child{tree, parent(first), {320, 100}};
          expect(child.native_handle() != 0 && child.poll(), "Realize the embedded native view");
          ignoredAndHandledKeys(window, first, child, probe);
          equivalentKeys(window, first, child, probe);
          textEditing(window, first);
          retainedFocusAfterFallback(window, first);
          lifecycleAndInstances(window, first, second, child, probe);
          failedLocalDispatch(window, first);
          localCloseDuringKey(window, first);
          hostCallbacks(window, first, second, child);
          child.request_close();
        }
        expect(first.subviews.count == 0 && second.subviews.count == 0,
               "All embedded keyboard views detach on destruction");
        [window close];
        std::puts("NativeUI Cocoa keyboard: host shortcuts, local handling, key equivalents, "
                  "text editing, independent instances, visibility and reparenting passed");
        return 0;
      } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
      }
    } @catch (NSException *exception) {
      std::fprintf(stderr, "Cocoa test failure: %s: %s\n", exception.name.UTF8String,
                   exception.reason.UTF8String);
      return 1;
    }
  }
}
