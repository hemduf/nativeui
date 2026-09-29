cmake_minimum_required(VERSION 3.24)
project(NativeUIT048StandaloneConsumer LANGUAGES CXX)

find_package(NativeUI CONFIG REQUIRED)

add_executable(t048_standalone_consumer main.cpp)
target_link_libraries(t048_standalone_consumer PRIVATE NativeUI::Core)
target_compile_features(t048_standalone_consumer PRIVATE cxx_std_20)
nativeui_attach_platform(
  TARGET t048_standalone_consumer
  CONSUMER_ID org.nativeui.t048.standalone
)
