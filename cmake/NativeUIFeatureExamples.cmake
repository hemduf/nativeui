include_guard(GLOBAL)

function(nativeui_discover_feature_examples out_var source_dir)
  if(NOT IS_DIRECTORY "${source_dir}/examples/features")
    message(FATAL_ERROR
      "NativeUI feature example discovery: missing ${source_dir}/examples/features")
  endif()

  set(_feature_dir "${source_dir}/examples/features")

  # CONFIGURE_DEPENDS is valid during project configuration but intentionally
  # unavailable in `cmake -P` script mode. The script-mode path keeps this
  # helper directly testable while normal builds reconfigure when examples are
  # added or removed.
  if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB _feature_sources
      RELATIVE "${_feature_dir}"
      "${_feature_dir}/t[0-9][0-9][0-9]*.cpp"
    )
  else()
    file(GLOB _feature_sources CONFIGURE_DEPENDS
      RELATIVE "${_feature_dir}"
      "${_feature_dir}/t[0-9][0-9][0-9]*.cpp"
    )
  endif()

  list(SORT _feature_sources)

  set(_feature_examples)
  foreach(_source IN LISTS _feature_sources)
    if(NOT _source MATCHES "^t[0-9][0-9][0-9]_[A-Za-z0-9_]+[.]cpp$")
      message(FATAL_ERROR
        "NativeUI feature example discovery: invalid feature example filename '${_source}'. "
        "Expected tNNN_<feature>.cpp")
    endif()

    get_filename_component(_name "${_source}" NAME_WE)
    list(APPEND _feature_examples "${_name}")
  endforeach()

  set(${out_var} "${_feature_examples}" PARENT_SCOPE)
endfunction()
