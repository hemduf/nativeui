include_guard(GLOBAL)

# The pinned macOS backend realizes embedded views visibly and steals the
# host's first responder; puglHide only hides an owned NSWindow (nil when
# embedded). Stage the dependency privately so source overrides and shared CPM
# caches stay untouched. Installed packages ship this corrected source tree.
# Fail closed on source drift: update/remove this shim when the Pugl pin changes.
function(nativeui_prepare_pugl_visibility source_root output_root)
  file(READ "${source_root}/src/mac.m" _source)
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
    [view->impl->wrapperView setHidden:YES];
  } else {
    [view->impl->window setIsVisible:NO];
  }
  return PUGL_SUCCESS;]=])
  foreach(_operation realize show hide)
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
