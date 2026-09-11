if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T043 registration contract requires SOURCE_DIR")
endif()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _nativeui_root_cmake)

foreach(_required IN ITEMS
    "t043_resize_scale"
    "nativeui_add_core_test(nativeui_t043_view_geometry_tests tests/t043_view_geometry_tests.cpp)")
  string(FIND "${_nativeui_root_cmake}" "${_required}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR
      "T043 completion artifact is not wired into the root build: ${_required}")
  endif()
endforeach()

message(STATUS "T043 completion artifacts are registered in the root build")
