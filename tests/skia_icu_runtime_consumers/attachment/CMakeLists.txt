if(RUNTIME_DATA_MODE STREQUAL "mock")
  function(attach_mock_runtime target)
    # This function changes no compiler/platform configuration. It only selects
    # the helper's Windows branch for an already-created native target.
    set(WIN32 TRUE)
    include("${RUNTIME_DATA_MODULE}")
    nativeui_attach_runtime_data(TARGET "${target}")
    nativeui_attach_runtime_data(TARGET "${target}")
  endfunction()
  foreach(_target IN ITEMS runtime_one runtime_two runtime_shared
      runtime_library runtime_module)
    attach_mock_runtime("${_target}")
  endforeach()
else()
  # The independent host intentionally has no Core linkage or data attachment.
  nativeui_attach_runtime_data(TARGET runtime_core)
  nativeui_attach_runtime_data(TARGET runtime_module)
  nativeui_attach_runtime_data(TARGET runtime_core)
  nativeui_attach_runtime_data(TARGET runtime_module)
endif()
