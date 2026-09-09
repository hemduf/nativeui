include_guard(GLOBAL)
include(CMakeParseArguments)

function(_nativeui_application_fail message_text)
  message(FATAL_ERROR "NativeUI nativeui_add_application: ${message_text}")
endfunction()

function(_nativeui_validate_application_product_name product_name)
  if("${product_name}" STREQUAL "")
    _nativeui_application_fail("PRODUCT_NAME is required and must be non-empty")
  endif()

  string(HEX "${product_name}" _nativeui_name_hex)
  string(LENGTH "${_nativeui_name_hex}" _nativeui_hex_length)
  math(EXPR _nativeui_byte_count "${_nativeui_hex_length} / 2")
  set(_nativeui_byte_index 0)
  while(_nativeui_byte_index LESS _nativeui_byte_count)
    math(EXPR _nativeui_hex_index "${_nativeui_byte_index} * 2")
    string(SUBSTRING "${_nativeui_name_hex}" ${_nativeui_hex_index} 2 _nativeui_byte_hex)
    math(EXPR _nativeui_byte "0x${_nativeui_byte_hex}")

    if(_nativeui_byte LESS 32)
      _nativeui_application_fail(
        "PRODUCT_NAME contains an ASCII control byte; portable names must not contain U+0000 through U+001F")
    endif()

    set(_nativeui_sequence_length 1)
    set(_nativeui_second_min 128)
    set(_nativeui_second_max 191)
    if(_nativeui_byte LESS 128)
      set(_nativeui_sequence_length 1)
    elseif(_nativeui_byte GREATER_EQUAL 194 AND _nativeui_byte LESS_EQUAL 223)
      set(_nativeui_sequence_length 2)
    elseif(_nativeui_byte EQUAL 224)
      set(_nativeui_sequence_length 3)
      set(_nativeui_second_min 160)
    elseif(_nativeui_byte GREATER_EQUAL 225 AND _nativeui_byte LESS_EQUAL 236)
      set(_nativeui_sequence_length 3)
    elseif(_nativeui_byte EQUAL 237)
      set(_nativeui_sequence_length 3)
      set(_nativeui_second_max 159)
    elseif(_nativeui_byte GREATER_EQUAL 238 AND _nativeui_byte LESS_EQUAL 239)
      set(_nativeui_sequence_length 3)
    elseif(_nativeui_byte EQUAL 240)
      set(_nativeui_sequence_length 4)
      set(_nativeui_second_min 144)
    elseif(_nativeui_byte GREATER_EQUAL 241 AND _nativeui_byte LESS_EQUAL 243)
      set(_nativeui_sequence_length 4)
    elseif(_nativeui_byte EQUAL 244)
      set(_nativeui_sequence_length 4)
      set(_nativeui_second_max 143)
    else()
      _nativeui_application_fail("PRODUCT_NAME is not valid UTF-8")
    endif()

    if(_nativeui_sequence_length GREATER 1)
      math(EXPR _nativeui_remaining "${_nativeui_byte_count} - ${_nativeui_byte_index}")
      if(_nativeui_remaining LESS _nativeui_sequence_length)
        _nativeui_application_fail("PRODUCT_NAME is not valid UTF-8")
      endif()

      math(EXPR _nativeui_second_hex_index "(${_nativeui_byte_index} + 1) * 2")
      string(SUBSTRING "${_nativeui_name_hex}" ${_nativeui_second_hex_index} 2 _nativeui_second_hex)
      math(EXPR _nativeui_second "0x${_nativeui_second_hex}")
      if(_nativeui_second LESS _nativeui_second_min OR _nativeui_second GREATER _nativeui_second_max)
        _nativeui_application_fail("PRODUCT_NAME is not valid UTF-8")
      endif()

      set(_nativeui_continuation_index 2)
      while(_nativeui_continuation_index LESS _nativeui_sequence_length)
        math(EXPR _nativeui_continuation_hex_index
          "(${_nativeui_byte_index} + ${_nativeui_continuation_index}) * 2")
        string(SUBSTRING "${_nativeui_name_hex}" ${_nativeui_continuation_hex_index} 2
          _nativeui_continuation_hex)
        math(EXPR _nativeui_continuation "0x${_nativeui_continuation_hex}")
        if(_nativeui_continuation LESS 128 OR _nativeui_continuation GREATER 191)
          _nativeui_application_fail("PRODUCT_NAME is not valid UTF-8")
        endif()
        math(EXPR _nativeui_continuation_index "${_nativeui_continuation_index} + 1")
      endwhile()
    endif()

    math(EXPR _nativeui_byte_index "${_nativeui_byte_index} + ${_nativeui_sequence_length}")
  endwhile()

  if("${product_name}" STREQUAL "." OR "${product_name}" STREQUAL "..")
    _nativeui_application_fail("PRODUCT_NAME must not be '.' or '..'")
  endif()

  foreach(_nativeui_forbidden IN ITEMS "/" "\\" "<" ">" ":" "\"" "|" "?" "*")
    string(FIND "${product_name}" "${_nativeui_forbidden}" _nativeui_forbidden_at)
    if(NOT _nativeui_forbidden_at EQUAL -1)
      _nativeui_application_fail(
        "PRODUCT_NAME '${product_name}' contains forbidden filename character '${_nativeui_forbidden}'")
    endif()
  endforeach()

  string(LENGTH "${product_name}" _nativeui_name_length)
  math(EXPR _nativeui_last_index "${_nativeui_name_length} - 1")
  string(SUBSTRING "${product_name}" ${_nativeui_last_index} 1 _nativeui_last_character)
  if(_nativeui_last_character STREQUAL " " OR _nativeui_last_character STREQUAL ".")
    _nativeui_application_fail("PRODUCT_NAME must not end in an ASCII space or '.'")
  endif()

  string(TOUPPER "${product_name}" _nativeui_upper_name)
  if(_nativeui_upper_name MATCHES "^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])($|\\.)")
    _nativeui_application_fail(
      "PRODUCT_NAME '${product_name}' uses a reserved Windows DOS device basename")
  endif()
endfunction()

function(_nativeui_validate_application_version version)
  if("${version}" STREQUAL "" OR NOT "${version}" MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    _nativeui_application_fail(
      "VERSION '${version}' must use the exact MAJOR.MINOR.PATCH decimal grammar")
  endif()
endfunction()

function(_nativeui_resolve_application_icon out_path out_basename icon_path extension label)
  if(IS_ABSOLUTE "${icon_path}")
    set(_nativeui_icon_path "${icon_path}")
  else()
    get_filename_component(_nativeui_icon_path "${icon_path}" ABSOLUTE
      BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()

  if(NOT EXISTS "${_nativeui_icon_path}")
    _nativeui_application_fail("${label} '${icon_path}' does not exist")
  endif()

  get_filename_component(_nativeui_icon_extension "${_nativeui_icon_path}" LAST_EXT)
  string(TOLOWER "${_nativeui_icon_extension}" _nativeui_icon_extension_lower)
  if(NOT _nativeui_icon_extension_lower STREQUAL "${extension}")
    _nativeui_application_fail(
      "${label} '${icon_path}' must use the ${extension} extension")
  endif()

  get_filename_component(_nativeui_icon_basename "${_nativeui_icon_path}" NAME)
  set(${out_path} "${_nativeui_icon_path}" PARENT_SCOPE)
  set(${out_basename} "${_nativeui_icon_basename}" PARENT_SCOPE)
endfunction()

function(_nativeui_xml_escape out_value value)
  set(_nativeui_xml "${value}")
  string(REPLACE "&" "&amp;" _nativeui_xml "${_nativeui_xml}")
  string(REPLACE "<" "&lt;" _nativeui_xml "${_nativeui_xml}")
  string(REPLACE ">" "&gt;" _nativeui_xml "${_nativeui_xml}")
  set(${out_value} "${_nativeui_xml}" PARENT_SCOPE)
endfunction()

function(_nativeui_application_is_keyword out_value token)
  if(token STREQUAL "PRODUCT_NAME" OR
     token STREQUAL "BUNDLE_ID" OR
     token STREQUAL "VERSION" OR
     token STREQUAL "MACOS_ICON" OR
     token STREQUAL "WINDOWS_ICON" OR
     token STREQUAL "SOURCES")
    set(${out_value} TRUE PARENT_SCOPE)
  else()
    set(${out_value} FALSE PARENT_SCOPE)
  endif()
endfunction()

function(nativeui_add_application)
  if(ARGC LESS 1 OR "${ARGV0}" STREQUAL "")
    _nativeui_application_fail("target name is required")
  endif()
  set(target "${ARGV0}")
  if(TARGET "${target}")
    _nativeui_application_fail("target '${target}' already exists")
  endif()

  # T054 deliberately accepts semicolons and public-keyword spellings inside
  # one-value arguments such as PRODUCT_NAME. A function exposes the original
  # ARGVn boundaries, unlike a macro's flattened ${ARGV} list, so consume every
  # one-value field from the next original argument before interpreting syntax.
  foreach(_nativeui_application_field IN ITEMS
      PRODUCT_NAME BUNDLE_ID VERSION MACOS_ICON WINDOWS_ICON SOURCES)
    unset(_nativeui_application_${_nativeui_application_field})
    unset(_nativeui_application_seen_${_nativeui_application_field})
  endforeach()

  set(_nativeui_application_index 1)
  while(_nativeui_application_index LESS ARGC)
    set(_nativeui_application_arg_name "ARGV${_nativeui_application_index}")
    set(_nativeui_application_keyword "${${_nativeui_application_arg_name}}")
    _nativeui_application_is_keyword(
      _nativeui_application_is_known_keyword "${_nativeui_application_keyword}")
    if(NOT _nativeui_application_is_known_keyword)
      _nativeui_application_fail("unknown argument '${_nativeui_application_keyword}'")
    endif()
    if(DEFINED _nativeui_application_seen_${_nativeui_application_keyword})
      _nativeui_application_fail(
        "keyword '${_nativeui_application_keyword}' may only be specified once")
    endif()
    set(_nativeui_application_seen_${_nativeui_application_keyword} TRUE)
    math(EXPR _nativeui_application_index "${_nativeui_application_index} + 1")

    if(_nativeui_application_keyword STREQUAL "SOURCES")
      set(_nativeui_application_source_count 0)
      while(_nativeui_application_index LESS ARGC)
        set(_nativeui_application_arg_name "ARGV${_nativeui_application_index}")
        set(_nativeui_application_token "${${_nativeui_application_arg_name}}")
        _nativeui_application_is_keyword(
          _nativeui_application_next_is_keyword "${_nativeui_application_token}")
        if(_nativeui_application_next_is_keyword)
          break()
        endif()
        if(_nativeui_application_token MATCHES "^[A-Z][A-Z0-9_]+$")
          _nativeui_application_fail("unknown argument '${_nativeui_application_token}'")
        endif()
        list(APPEND _nativeui_application_SOURCES "${_nativeui_application_token}")
        math(EXPR _nativeui_application_source_count
          "${_nativeui_application_source_count} + 1")
        math(EXPR _nativeui_application_index "${_nativeui_application_index} + 1")
      endwhile()
      if(_nativeui_application_source_count EQUAL 0)
        _nativeui_application_fail("SOURCES requires at least one caller-owned source")
      endif()
    else()
      if(_nativeui_application_index GREATER_EQUAL ARGC)
        _nativeui_application_fail(
          "keyword '${_nativeui_application_keyword}' is missing its value")
      endif()
      set(_nativeui_application_arg_name "ARGV${_nativeui_application_index}")
      set(_nativeui_application_${_nativeui_application_keyword}
        "${${_nativeui_application_arg_name}}")
      math(EXPR _nativeui_application_index "${_nativeui_application_index} + 1")
    endif()
  endwhile()

  if(NOT _nativeui_application_SOURCES)
    _nativeui_application_fail("SOURCES requires at least one caller-owned source")
  endif()

  _nativeui_validate_application_product_name("${_nativeui_application_PRODUCT_NAME}")
  if("${_nativeui_application_BUNDLE_ID}" STREQUAL "")
    _nativeui_application_fail("BUNDLE_ID is required")
  endif()
  if(COMMAND _nativeui_validate_consumer_id)
    _nativeui_validate_consumer_id("${_nativeui_application_BUNDLE_ID}")
  endif()
  _nativeui_validate_application_version("${_nativeui_application_VERSION}")

  string(SHA256 _nativeui_application_target_digest "${target}")
  string(SUBSTRING "${_nativeui_application_target_digest}" 0 16
    _nativeui_application_target_key)
  set(_nativeui_application_metadata_dir
    "${CMAKE_CURRENT_BINARY_DIR}/nativeui_application/${_nativeui_application_target_key}")
  file(MAKE_DIRECTORY "${_nativeui_application_metadata_dir}")

  if(APPLE)
    add_executable("${target}" MACOSX_BUNDLE
      ${_nativeui_application_SOURCES})

    _nativeui_xml_escape(_nativeui_application_product_xml
      "${_nativeui_application_PRODUCT_NAME}")
    _nativeui_xml_escape(_nativeui_application_bundle_xml
      "${_nativeui_application_BUNDLE_ID}")
    _nativeui_xml_escape(_nativeui_application_version_xml
      "${_nativeui_application_VERSION}")

    set(_nativeui_application_icon_plist "")
    if(NOT "${_nativeui_application_MACOS_ICON}" STREQUAL "")
      _nativeui_resolve_application_icon(
        _nativeui_application_icon_path
        _nativeui_application_icon_basename
        "${_nativeui_application_MACOS_ICON}" ".icns" "MACOS_ICON")
      target_sources("${target}" PRIVATE
        "${_nativeui_application_icon_path}")
      set_source_files_properties("${_nativeui_application_icon_path}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
      _nativeui_xml_escape(_nativeui_application_icon_xml
        "${_nativeui_application_icon_basename}")
      set(_nativeui_application_icon_plist
        "  <key>CFBundleIconFile</key>\n  <string>${_nativeui_application_icon_xml}</string>\n")
    endif()

    set(_nativeui_application_plist
      "${_nativeui_application_metadata_dir}/Info.plist")
    file(WRITE "${_nativeui_application_plist}"
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
"<plist version=\"1.0\">\n"
"<dict>\n"
"  <key>CFBundleIdentifier</key>\n"
"  <string>${_nativeui_application_bundle_xml}</string>\n"
"  <key>CFBundleName</key>\n"
"  <string>${_nativeui_application_product_xml}</string>\n"
"  <key>CFBundleDisplayName</key>\n"
"  <string>${_nativeui_application_product_xml}</string>\n"
"  <key>CFBundleExecutable</key>\n"
"  <string>${_nativeui_application_product_xml}</string>\n"
"  <key>CFBundleShortVersionString</key>\n"
"  <string>${_nativeui_application_version_xml}</string>\n"
"  <key>CFBundleVersion</key>\n"
"  <string>${_nativeui_application_version_xml}</string>\n"
"  <key>CFBundlePackageType</key>\n"
"  <string>APPL</string>\n"
"  <key>NSHighResolutionCapable</key>\n"
"  <true/>\n"
"${_nativeui_application_icon_plist}"
"</dict>\n"
"</plist>\n")
    set_target_properties("${target}" PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_INFO_PLIST "${_nativeui_application_plist}"
    )
  elseif(WIN32)
    add_executable("${target}" WIN32
      ${_nativeui_application_SOURCES})

    if(NOT "${_nativeui_application_WINDOWS_ICON}" STREQUAL "")
      _nativeui_resolve_application_icon(
        _nativeui_application_icon_path
        _nativeui_application_icon_basename
        "${_nativeui_application_WINDOWS_ICON}" ".ico" "WINDOWS_ICON")
      set(_nativeui_application_icon_copy
        "${_nativeui_application_metadata_dir}/${_nativeui_application_icon_basename}")
      configure_file(
        "${_nativeui_application_icon_path}"
        "${_nativeui_application_icon_copy}"
        COPYONLY
      )
      set(_nativeui_application_rc
        "${_nativeui_application_metadata_dir}/application.rc")
      file(WRITE "${_nativeui_application_rc}"
        "IDI_NATIVEUI_APPLICATION_ICON ICON \"${_nativeui_application_icon_basename}\"\n")
      target_sources("${target}" PRIVATE
        "${_nativeui_application_rc}")
    endif()
  else()
    add_executable("${target}"
      ${_nativeui_application_SOURCES})
  endif()

  set_target_properties("${target}" PROPERTIES
    OUTPUT_NAME "${_nativeui_application_PRODUCT_NAME}")
  target_link_libraries("${target}" PRIVATE NativeUI::Core)

  # T047's public attachment helper must execute at CMake file scope because it
  # may enable C and Objective-C on CMake 3.24-4.4. Generate a tiny per-target
  # subdirectory and execute it synchronously. The target and consumer identity
  # are inherited as variables, so generated code contains no caller-controlled
  # text and T047/T053 remain the sole platform/naming implementation. Keep this
  # private configure machinery separate from application packaging metadata.
  set(_NATIVEUI_APPLICATION_ATTACH_TARGET "${target}")
  set(_NATIVEUI_APPLICATION_ATTACH_CONSUMER_ID
    "${_nativeui_application_BUNDLE_ID}")
  set(_nativeui_application_attach_root
    "${CMAKE_CURRENT_BINARY_DIR}/nativeui_platform_attach/${_nativeui_application_target_key}")
  set(_nativeui_application_attach_source_dir
    "${_nativeui_application_attach_root}/source")
  set(_nativeui_application_attach_binary_dir
    "${_nativeui_application_attach_root}/build")
  file(MAKE_DIRECTORY "${_nativeui_application_attach_source_dir}")
  file(WRITE "${_nativeui_application_attach_source_dir}/CMakeLists.txt" [=[
if(NOT DEFINED _NATIVEUI_APPLICATION_ATTACH_TARGET OR
   NOT DEFINED _NATIVEUI_APPLICATION_ATTACH_CONSUMER_ID)
  message(FATAL_ERROR "NativeUI T054 internal platform attachment context is missing")
endif()
nativeui_attach_platform(
  TARGET "${_NATIVEUI_APPLICATION_ATTACH_TARGET}"
  CONSUMER_ID "${_NATIVEUI_APPLICATION_ATTACH_CONSUMER_ID}"
)
]=])
  add_subdirectory(
    "${_nativeui_application_attach_source_dir}"
    "${_nativeui_application_attach_binary_dir}"
  )
endfunction()
