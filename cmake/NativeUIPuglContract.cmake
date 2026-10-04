include_guard(GLOBAL)

# Upgrade defaults cached by earlier NativeUI versions. An explicitly selected
# different revision remains authoritative, as does NATIVEUI_PUGL_SOURCE.
function(_nativeui_configure_pugl_pin required_commit)
  set(previous_defaults
    195f79b22644010c81a5e0c3231c591856787ec6
    94982803985eefcbaeb0a1c8d0136ec862d7cb59
    2b1852a1898020855fb115f5c9ffdb7ddec51ebb
    165c50f08c6e65505198aa95e2cdf9d4028af7b6
    c1d7ddd13f74613c83cbbeee9028ca017b50ff0e)
  if(NATIVEUI_PUGL_COMMIT IN_LIST previous_defaults
      AND NOT NATIVEUI_PUGL_COMMIT STREQUAL required_commit)
    message(STATUS "NativeUI: upgrading cached Pugl default to ${required_commit}")
    set(NATIVEUI_PUGL_COMMIT "${required_commit}" CACHE STRING "Pinned hemduf/pugl commit" FORCE)
  else()
    set(NATIVEUI_PUGL_COMMIT "${required_commit}" CACHE STRING "Pinned hemduf/pugl commit")
  endif()
endfunction()

function(_nativeui_validate_pugl_cocoa source_root)
  if(NOT EXISTS "${source_root}/src/mac.m")
    message(FATAL_ERROR "NativeUI requires the Cocoa backend in ${source_root}/src/mac.m")
  endif()
  file(READ "${source_root}/src/mac.m" cocoa_source)
  foreach(method IN ITEMS "- (BOOL)puglPreserveEmbeddedFocus" "- (void)puglSetEmbeddedFocus:")
    string(FIND "${cocoa_source}" "${method}" method_index)
    if(method_index EQUAL -1)
      message(FATAL_ERROR
        "NativeUI requires the Pugl Cocoa keyboard/focus hooks (${method}). "
        "Select NATIVEUI_PUGL_COMMIT=${NATIVEUI_PUGL_REQUIRED_COMMIT}, or update "
        "NATIVEUI_PUGL_SOURCE to a compatible checkout. Incompatible source: ${source_root}")
    endif()
  endforeach()
endfunction()
