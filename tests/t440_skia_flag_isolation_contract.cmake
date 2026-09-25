cmake_minimum_required(VERSION 3.24)

# T440 research invariant: the upstream Skia thread-local strike-cache build
# flag and its experimental runtime selector may only be referenced from
# tests/ (the research probe and its contracts) and docs/ (the research
# report). NativeUI public headers, implementation sources and the
# installed/consumer-facing CMake surface must stay independent of it.
#
# The scan walks the repository except for tests/, docs/ and local build
# directories. Third-party Skia headers are not part of the repository; the
# installed package keeps them in the pinned Skia archive directory, not in
# NativeUI's public header surface.

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(_needles
  "skia_enable_threadlocal_strikecache"
  "SK_ENABLE_THREADLOCAL_STRIKECACHE"
  "gSkUseThreadLocalStrikeCaches_IAcknowledgeThisIsIncrediblyExperimental"
)

set(_exempt_directories "tests" "docs")
set(_skip_directory_prefixes "build" "_deps" "out" "cmake-build")

set(_scan_files "")
file(GLOB _source_entries RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*")
foreach(_entry IN LISTS _source_entries)
  if(_entry STREQUAL ".git" OR _entry IN_LIST _exempt_directories)
    continue()
  endif()
  if(IS_DIRECTORY "${SOURCE_DIR}/${_entry}")
    set(_skip_directory FALSE)
    foreach(_prefix IN LISTS _skip_directory_prefixes)
      if(_entry MATCHES "^${_prefix}")
        set(_skip_directory TRUE)
      endif()
    endforeach()
    if(_skip_directory)
      continue()
    endif()
    file(GLOB_RECURSE _nested RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/${_entry}/*")
    list(APPEND _scan_files ${_nested})
  else()
    list(APPEND _scan_files "${_entry}")
  endif()
endforeach()

set(_violations "")
foreach(_file IN LISTS _scan_files)
  if(IS_DIRECTORY "${SOURCE_DIR}/${_file}")
    continue()
  endif()
  file(READ "${SOURCE_DIR}/${_file}" _contents)
  foreach(_needle IN LISTS _needles)
    string(FIND "${_contents}" "${_needle}" _index)
    if(NOT _index EQUAL -1)
      list(APPEND _violations "${_file}: ${_needle}")
    endif()
  endforeach()
endforeach()

if(_violations)
  list(REMOVE_DUPLICATES _violations)
  string(REPLACE ";" "\n  - " _report "${_violations}")
  message(FATAL_ERROR
    "T440 Skia strike-cache flag isolation contract failed. The upstream Skia "
    "strike-cache build flag/runtime selector may only be referenced under "
    "tests/ and docs/, but was found in:\n  - ${_report}")
endif()

message(STATUS "T440 Skia strike-cache flag isolation contract passed")
