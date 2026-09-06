# Small CPM.cmake bootstrap. The actual dependency manager is downloaded into
# the build directory, while project dependencies are declared with CPMAddPackage.
set(CPM_DOWNLOAD_VERSION 0.43.1)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cmake")
  file(DOWNLOAD
    "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
    "${CPM_DOWNLOAD_LOCATION}"
    TLS_VERIFY ON
    STATUS CPM_DOWNLOAD_STATUS
  )
  list(GET CPM_DOWNLOAD_STATUS 0 CPM_DOWNLOAD_CODE)
  list(GET CPM_DOWNLOAD_STATUS 1 CPM_DOWNLOAD_MESSAGE)
  if(NOT CPM_DOWNLOAD_CODE EQUAL 0)
    message(FATAL_ERROR "Failed to download CPM.cmake: ${CPM_DOWNLOAD_MESSAGE}")
  endif()
endif()

include("${CPM_DOWNLOAD_LOCATION}")
