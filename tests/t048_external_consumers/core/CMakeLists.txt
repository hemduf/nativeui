cmake_minimum_required(VERSION 3.24)
project(NativeUIT048CoreConsumer LANGUAGES CXX)

find_package(NativeUI CONFIG REQUIRED)

add_executable(t048_core_consumer main.cpp)
target_link_libraries(t048_core_consumer PRIVATE NativeUI::Core)
target_compile_features(t048_core_consumer PRIVATE cxx_std_20)
