if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T069 API freeze contract requires SOURCE_DIR")
endif()

file(GLOB _public_headers "${SOURCE_DIR}/include/nativeui/*.hpp")
if(NOT _public_headers)
  message(FATAL_ERROR "T069 RED: no normal public headers discovered")
endif()

foreach(_header IN LISTS _public_headers)
  file(READ "${_header}" _text)
  foreach(_forbidden IN ITEMS
      "#include <pugl/"
      "#include \"pugl/"
      "#include <windows.h>"
      "#include <AppKit/"
      "#include <X11/"
      "#include \"include/core/Sk"
      "#include \"include/gpu/Sk")
    string(FIND "${_text}" "${_forbidden}" _hit)
    if(NOT _hit EQUAL -1)
      message(FATAL_ERROR
        "T069 RED: normal public header ${_header} exposes forbidden backend include ${_forbidden}")
    endif()
  endforeach()
endforeach()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
string(FIND "${_root_cmake}" "NativeUI::NativeUI" _complete_target)
if(NOT _complete_target EQUAL -1)
  message(FATAL_ERROR "T069 RED: source tree still publishes NativeUI::NativeUI")
endif()
string(FIND "${_root_cmake}" "target_link_libraries(nativeui_core PRIVATE SkiaBuilder::svg)" _private_core)
if(_private_core EQUAL -1)
  message(FATAL_ERROR "T069 RED: Core backend dependency is not private/link-only")
endif()

file(READ "${SOURCE_DIR}/cmake/NativeUIConsumerPlatform.cmake" _platform_helper)
string(FIND "${_platform_helper}" "NativeUI::NativeUI" _helper_complete_target)
if(NOT _helper_complete_target EQUAL -1)
  message(FATAL_ERROR "T069 RED: public package helper still references NativeUI::NativeUI")
endif()
string(FIND "${_platform_helper}" "PRIVATE SkiaBuilder::skia" _private_package_renderer)
if(_private_package_renderer EQUAL -1)
  message(FATAL_ERROR "T069 RED: installed private platform target lacks its private renderer dependency")
endif()

file(READ "${SOURCE_DIR}/README.md" _readme)
foreach(_forbidden IN ITEMS "NativeUI POC" "proof of concept" "window.run()" "NativeUI::NativeUI" "#include <nativeui/detail/")
  string(FIND "${_readme}" "${_forbidden}" _readme_hit)
  if(NOT _readme_hit EQUAL -1)
    message(FATAL_ERROR "T069 RED: README contains obsolete/unsupported current API text: ${_forbidden}")
  endif()
endforeach()

set(_pugl_pin "723474fa43a5d1b08be2446966a4db9007b749c6")
file(READ "${SOURCE_DIR}/cmake/Dependencies.cmake" _dependencies)
string(FIND "${_dependencies}" "${_pugl_pin}" _pin_source)
string(FIND "${_readme}" "${_pugl_pin}" _pin_docs)
if(_pin_source EQUAL -1 OR _pin_docs EQUAL -1)
  message(FATAL_ERROR "T069 RED: documented Pugl pin does not match Dependencies.cmake")
endif()

if(NOT EXISTS "${SOURCE_DIR}/docs/V1_API.md")
  message(FATAL_ERROR "T069 RED: durable v1 API inventory is missing")
endif()
file(READ "${SOURCE_DIR}/docs/V1_API.md" _inventory)
string(FIND "${_inventory}" "| Header | Public type/function/helper | Purpose | Ownership" _inventory_header)
if(_inventory_header EQUAL -1)
  message(FATAL_ERROR "T069 RED: v1 API inventory is missing required columns")
endif()

file(GLOB_RECURSE _public_examples "${SOURCE_DIR}/examples/*.cpp" "${SOURCE_DIR}/examples/*.hpp")
foreach(_example IN LISTS _public_examples)
  file(READ "${_example}" _example_text)
  foreach(_forbidden IN ITEMS "#include <nativeui/detail/" "#include <pugl/" "#include <windows.h>" "#include <AppKit/" "#include <X11/" "include/core/Sk")
    string(FIND "${_example_text}" "${_forbidden}" _example_hit)
    if(NOT _example_hit EQUAL -1)
      message(FATAL_ERROR "T069 RED: public example ${_example} exposes unsupported backend/detail include ${_forbidden}")
    endif()
  endforeach()
endforeach()

message(STATUS "T069 public API/CMake/docs freeze contract passed")
