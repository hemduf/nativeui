if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T053 prefix tests require SOURCE_DIR")
endif()

include("${SOURCE_DIR}/cmake/NativeUIConsumerPlatform.cmake")

function(_nativeui_expect_prefix identity expected)
  nativeui_compute_objc_runtime_prefix(_actual "${identity}")
  if(NOT _actual STREQUAL "${expected}")
    message(FATAL_ERROR
      "T053 prefix mismatch for [${identity}]\nexpected: ${expected}\nactual:   ${_actual}")
  endif()
  if(NOT _actual MATCHES "^[A-Za-z][A-Za-z0-9_]*_$")
    message(FATAL_ERROR "T053 generated invalid Objective-C prefix: ${_actual}")
  endif()
endfunction()

# Frozen exact SHA-256 + ASCII fragment vectors.
_nativeui_expect_prefix(
  "com.example.my-app"
  "NUI_com_example_my_app_e70a1f2c202b_")
_nativeui_expect_prefix(
  "com.example.my_app"
  "NUI_com_example_my_app_5dc9af4bd7f8_")
_nativeui_expect_prefix(
  "com.example.my app"
  "NUI_com_example_my_app_f958c8dc29d0_")
_nativeui_expect_prefix(
  "com.exämple.app"
  "NUI_com_ex_mple_app_1461709a4e08_")
_nativeui_expect_prefix(
  "é漢字"
  "NUI_consumer_1d4d0fb5e11a_")
_nativeui_expect_prefix(
  "---"
  "NUI_consumer_cb3f91d54eee_")
_nativeui_expect_prefix(
  "abcdefghijklmnopqrstuvwxyz0123456789"
  "NUI_abcdefghijklmnopqrstuvwx_011fc2994e39_")
_nativeui_expect_prefix(
  "abcdefghijklmnopqrstuvw-x"
  "NUI_abcdefghijklmnopqrstuvw_73fc9878b736_")

# Same readable fragment must remain collision-resistant because the digest is
# computed from the exact original UTF-8 bytes before sanitization.
nativeui_compute_objc_runtime_prefix(_collision_a "com.example.my-app")
nativeui_compute_objc_runtime_prefix(_collision_b "com.example.my_app")
if(_collision_a STREQUAL _collision_b)
  message(FATAL_ERROR "T053 digest failed to separate same-fragment consumer identities")
endif()

# Repeated derivation in the same configure must be byte-for-byte deterministic.
nativeui_compute_objc_runtime_prefix(_repeat_a "com.example.deterministic")
nativeui_compute_objc_runtime_prefix(_repeat_b "com.example.deterministic")
if(NOT _repeat_a STREQUAL _repeat_b)
  message(FATAL_ERROR "T053 prefix derivation is not deterministic")
endif()

message(STATUS "T053 consumer prefix vectors passed")
