cmake_minimum_required(VERSION 3.24)
project(NativeUIWasmPackageConsumer LANGUAGES CXX)

find_package(NativeUI CONFIG REQUIRED)

nativeui_add_application(nativeui_wasm_package_consumer
  PRODUCT_NAME "NativeUIWasmPackageConsumer"
  BUNDLE_ID org.nativeui.test.wasm-package
  VERSION 0.1.0
  SOURCES main.cpp
)
