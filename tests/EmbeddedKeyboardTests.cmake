add_executable(nativeui_embedded_keyboard_tests
  ${PROJECT_SOURCE_DIR}/tests/embedded_keyboard_macos_tests.mm)
set_target_properties(nativeui_embedded_keyboard_tests PROPERTIES OBJCXX_STANDARD 20)
target_compile_options(nativeui_embedded_keyboard_tests PRIVATE -fobjc-arc)
target_link_libraries(nativeui_embedded_keyboard_tests PRIVATE "-framework CoreGraphics")
_nativeui_attach_consumer_platform(
  TARGET nativeui_embedded_keyboard_tests
  CONSUMER_ID org.nativeui.test.embedded-keyboard)
nativeui_enable_project_warnings(nativeui_embedded_keyboard_tests)
nativeui_enable_sanitizers(nativeui_embedded_keyboard_tests)

# Run the same Cocoa host contract with the Live-specific responder handoff.
# This fixture is a test process, not evidence of an actual DAW session.
set(_keyboard_live_app "${CMAKE_CURRENT_BINARY_DIR}/nativeui-keyboard-live.app")
set(_keyboard_live_executable "nativeui-keyboard-live")
file(GENERATE OUTPUT "${_keyboard_live_app}/Contents/Info.plist" CONTENT
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\"><dict>
<key>CFBundleIdentifier</key><string>com.ableton.live</string>
<key>CFBundleExecutable</key><string>${_keyboard_live_executable}</string>
<key>CFBundleName</key><string>NativeUI Keyboard Live Fixture</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleVersion</key><string>1</string>
</dict></plist>
")
add_custom_command(TARGET nativeui_embedded_keyboard_tests POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_keyboard_live_app}/Contents/MacOS"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:nativeui_embedded_keyboard_tests>"
    "${_keyboard_live_app}/Contents/MacOS/${_keyboard_live_executable}"
  VERBATIM)
add_test(NAME nativeui_embedded_keyboard_tests COMMAND nativeui_embedded_keyboard_tests)
add_test(NAME nativeui_embedded_keyboard_live_tests
  COMMAND "${_keyboard_live_app}/Contents/MacOS/${_keyboard_live_executable}")
set_tests_properties(nativeui_embedded_keyboard_tests nativeui_embedded_keyboard_live_tests
  PROPERTIES LABELS "integration;platform;macos;keyboard" TIMEOUT 60
  SKIP_RETURN_CODE 77 RUN_SERIAL TRUE)
set_tests_properties(nativeui_embedded_keyboard_live_tests
  PROPERTIES ENVIRONMENT "NATIVEUI_TEST_EXPECT_LIVE=1")
