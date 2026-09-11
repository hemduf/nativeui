cmake_minimum_required(VERSION 3.24)
project(NativeUIT064Contract LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

function(t064_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

add_executable(t064_public_contract public_contract.cpp)
target_include_directories(t064_public_contract PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../include"
)
t064_enable_warnings(t064_public_contract)
add_test(NAME t064.public_contract COMMAND t064_public_contract)

add_executable(t064_request_contract
  request_contract.cpp
  "${CMAKE_CURRENT_LIST_DIR}/../../src/desktop_services.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../../src/dispatcher.cpp"
)
target_include_directories(t064_request_contract PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../include"
)
t064_enable_warnings(t064_request_contract)
add_test(NAME t064.request_contract COMMAND t064_request_contract)

add_executable(t064_owner_isolation_contract
  owner_isolation_contract.cpp
  "${CMAKE_CURRENT_LIST_DIR}/../../src/desktop_services.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../../src/dispatcher.cpp"
)
target_include_directories(t064_owner_isolation_contract PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../include"
)
t064_enable_warnings(t064_owner_isolation_contract)
add_test(NAME t064.owner_isolation_contract COMMAND t064_owner_isolation_contract)

add_executable(t064_inline_backend_contract
  inline_backend_contract.cpp
  "${CMAKE_CURRENT_LIST_DIR}/../../src/desktop_services.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../../src/dispatcher.cpp"
)
target_include_directories(t064_inline_backend_contract PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../include"
)
t064_enable_warnings(t064_inline_backend_contract)
add_test(NAME t064.inline_backend_contract COMMAND t064_inline_backend_contract)
