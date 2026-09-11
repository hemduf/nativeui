cmake_minimum_required(VERSION 3.24)
project(NativeUIT064Contract LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_executable(t064_public_contract public_contract.cpp)
target_include_directories(t064_public_contract PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../include"
)

if(MSVC)
  target_compile_options(t064_public_contract PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(t064_public_contract PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME t064.public_contract COMMAND t064_public_contract)
