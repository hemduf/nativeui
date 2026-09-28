include_guard(GLOBAL)

# The pinned macOS backend realizes embedded views visibly and steals the
# host's first responder; puglHide only hides an owned NSWindow (nil when
# embedded). Its focus events come only from its own window delegate, so embedded
# wrappers also need responder/key-window transitions. Stage the dependency privately
# so source overrides and shared CPM
# caches stay untouched. Installed packages ship this corrected source tree.
# Fail closed on source drift: update/remove this shim when the Pugl pin changes.
function(nativeui_prepare_pugl_visibility source_root output_root)
  file(READ "${source_root}/src/mac.m" _source)
  set(_focus_ivars_before [=[  bool                       reshaped;
}]=])
  set(_focus_ivars_after [=[  bool                       reshaped;
  // Borrowed window; observer registrations belong to this wrapper only.
  NSWindow*                  nativeuiObservedWindow;
  bool                       nativeuiEmbeddedFocused;
}]=])
  set(_focus_before [=[- (BOOL)acceptsFirstResponder
{
  return YES;
}]=])
  set(_focus_after [=[// The wrapper class is consumer-prefixed by NativeUI's final-consumer build.
// Derive new private selectors from that same identity, without host categories.
#define NUI_PUGL_JOIN_IMPL(a, b) a##b
#define NUI_PUGL_JOIN(a, b) NUI_PUGL_JOIN_IMPL(a, b)
#define nativeuiSetEmbeddedFocus NUI_PUGL_JOIN(PuglWrapperView, _setEmbeddedFocus)
#define nativeuiWindowFocusChanged NUI_PUGL_JOIN(PuglWrapperView, _windowFocusChanged)

- (void)nativeuiSetEmbeddedFocus:(BOOL)focused
{
  if (!puglview || !puglview->parent ||
      puglview->stage < PUGL_VIEW_STAGE_REALIZED ||
      nativeuiEmbeddedFocused == (bool)focused) {
    return;
  }

  // Publish before callbacks. Pugl's NativeUI event thunk contains failures;
  // the same transition must never be retried solely because a callback failed.
  nativeuiEmbeddedFocused = (bool)focused;
  PuglEvent event = {{focused ? PUGL_FOCUS_IN : PUGL_FOCUS_OUT, 0U}};
  event.focus.mode = PUGL_CROSSING_NORMAL;
  puglDispatchEvent(puglview, &event);
}

- (void)nativeuiWindowFocusChanged:(NSNotification*)notification
{
  NSWindow* window = [self window];
  if ([notification object] == window) {
    [self nativeuiSetEmbeddedFocus:
      [window isKeyWindow] && [window firstResponder] == self &&
      ![self isHiddenOrHasHiddenAncestor]];
  }
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow
{
  if (nativeuiObservedWindow) {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center removeObserver:self name:NSWindowDidBecomeKeyNotification
                    object:nativeuiObservedWindow];
    [center removeObserver:self name:NSWindowDidResignKeyNotification
                    object:nativeuiObservedWindow];
    if ([nativeuiObservedWindow firstResponder] == self) {
      [nativeuiObservedWindow makeFirstResponder:nil];
    }
    nativeuiObservedWindow = nil;
    [self nativeuiSetEmbeddedFocus:NO];
  }
  [super viewWillMoveToWindow:newWindow];
}

- (void)viewDidMoveToWindow
{
  [super viewDidMoveToWindow];
  if (puglview && puglview->parent && [self window]) {
    nativeuiObservedWindow = [self window];
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(nativeuiWindowFocusChanged:)
                  name:NSWindowDidBecomeKeyNotification object:nativeuiObservedWindow];
    [center addObserver:self selector:@selector(nativeuiWindowFocusChanged:)
                  name:NSWindowDidResignKeyNotification object:nativeuiObservedWindow];
  }
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (BOOL)acceptsFirstResponder
{
  return YES;
}

- (BOOL)becomeFirstResponder
{
  const BOOL accepted = [super becomeFirstResponder];
  if (accepted) {
    [self nativeuiSetEmbeddedFocus:
      [[self window] isKeyWindow] && ![self isHiddenOrHasHiddenAncestor]];
  }
  return accepted;
}

- (BOOL)resignFirstResponder
{
  const BOOL accepted = [super resignFirstResponder];
  if (accepted) {
    [self nativeuiSetEmbeddedFocus:NO];
  }
  return accepted;
}]=])
  set(_mouse_before [=[- (void)mouseDown:(NSEvent*)event
{
  const NSPoint]=])
  set(_mouse_after [=[- (void)mouseDown:(NSEvent*)event
{
  // Passive show never steals the host's responder. A user click does acquire
  // it, activating the retained UI before dispatching this first press.
  if (puglview->parent && ![self isHiddenOrHasHiddenAncestor]) {
    [[self window] makeFirstResponder:self];
  }
  const NSPoint]=])
  set(_realize_before [=[    [pview addSubview:impl->wrapperView];
    [impl->drawView setHidden:NO];
    [[impl->drawView window] makeFirstResponder:impl->wrapperView];]=])
  set(_realize_after [=[    // NativeUI: realization is hidden and does not take host focus.
    [impl->wrapperView setHidden:YES];
    [pview addSubview:impl->wrapperView];
    [impl->drawView setHidden:NO];]=])
  set(_show_before [=[  NSWindow* const window = [impl->wrapperView window];
  if (![window isVisible]) {]=])
  set(_show_after [=[  // NativeUI: show only the embedded child, never its host window.
  if (view->parent) {
    [impl->wrapperView setHidden:NO];
    [impl->drawView setNeedsDisplay:YES];
    return PUGL_SUCCESS;
  }

  NSWindow* const window = [impl->wrapperView window];
  if (![window isVisible]) {]=])
  set(_hide_before [=[  [view->impl->window setIsVisible:NO];
  return PUGL_SUCCESS;]=])
  set(_hide_after [=[  if (view->parent) {
    // ViewCore::hide owns direct, exception-preserving retained cleanup.
    // Clear native focus first without emitting a duplicate foreign callback.
    view->impl->wrapperView->nativeuiEmbeddedFocused = false;
    NSWindow* window = [view->impl->wrapperView window];
    if ([window firstResponder] == view->impl->wrapperView) {
      [window makeFirstResponder:nil];
    }
    [view->impl->wrapperView setHidden:YES];
  } else {
    [view->impl->window setIsVisible:NO];
  }
  return PUGL_SUCCESS;]=])
  foreach(_operation focus_ivars focus mouse realize show hide)
    set(_before "${_${_operation}_before}")
    set(_after "${_${_operation}_after}")
    string(FIND "${_source}" "${_before}" _position)
    if(_position EQUAL -1)
      message(FATAL_ERROR
        "NativeUI embedded visibility shim cannot match Pugl ${_operation} in ${source_root}/src/mac.m; review the dependency pin")
    endif()
    string(REPLACE "${_before}" "${_after}" _source "${_source}")
  endforeach()
  file(MAKE_DIRECTORY "${output_root}")
  file(COPY "${source_root}/include" "${source_root}/COPYING"
    DESTINATION "${output_root}")
  file(COPY "${source_root}/src" DESTINATION "${output_root}"
    PATTERN "mac.m" EXCLUDE)
  # CONFIGURE only updates its output if contents changed (no rebuild churn).
  file(CONFIGURE OUTPUT "${output_root}/src/mac.m" CONTENT "${_source}" @ONLY)
endfunction()
