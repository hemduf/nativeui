if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(_shader_header "${SOURCE_DIR}/include/nativeui/shader.hpp")
set(_shader_backend "${SOURCE_DIR}/src/skia_shader.cpp")
set(_access_header "${SOURCE_DIR}/src/detail/shader_instance_access.hpp")

foreach(_path IN ITEMS "${_shader_header}" "${_shader_backend}" "${_access_header}")
  if(NOT EXISTS "${_path}")
    message(FATAL_ERROR "T080: missing required file ${_path}")
  endif()
endforeach()

file(READ "${_shader_header}" _public)
file(READ "${_shader_backend}" _backend)
file(READ "${_access_header}" _access)

foreach(_required IN ITEMS
    "enum class ShaderUniformType"
    "struct ShaderUniformInfo"
    "enum class ShaderSetResult"
    "class ShaderInstance"
    "std::span<const ShaderUniformInfo>"
    "std::int32_t"
    "set_color"
    "ShaderInstance(ShaderInstance&& other) noexcept")
  string(FIND "${_public}" "${_required}" _required_pos)
  if(_required_pos EQUAL -1)
    message(FATAL_ERROR "T080: missing public contract token '${_required}'")
  endif()
endforeach()

foreach(_forbidden IN ITEMS
    "SkRuntimeEffect"
    "SkShader"
    "SkData"
    "SkCanvas"
    "SkString"
    "SkRuntimeEffectBuilder")
  string(FIND "${_public}" "${_forbidden}" _forbidden_pos)
  if(NOT _forbidden_pos EQUAL -1)
    message(FATAL_ERROR "T080: public shader.hpp leaks backend token '${_forbidden}'")
  endif()
endforeach()

foreach(_backend_required IN ITEMS
    "backend.effect->uniforms()"
    "backend.effect->children()"
    "uniform.isArray()"
    "ShaderCompileError::UnsupportedInterface"
    "std::byte{0}"
    "std::memcpy"
    "packed_array_byte_size"
    "packed_array_byte_size(value)"
    "packed_array_byte_size(backend_values)"
    "packed_array_byte_size(components)"
    "reflected.offset != previous_end"
    "previous_end != uniform_size"
    "std::string_view{data.uniforms[index].name}")
  string(FIND "${_backend}" "${_backend_required}" _backend_required_pos)
  if(_backend_required_pos EQUAL -1)
    message(FATAL_ERROR "T080: missing implementation contract '${_backend_required}'")
  endif()
endforeach()

# Vector setters must derive the copied byte extent from element count/type,
# never from the std::array wrapper object's representation.
string(REGEX MATCHALL "packed_array_byte_size\\(value\\)" _float_vector_byte_sizes "${_backend}")
list(LENGTH _float_vector_byte_sizes _float_vector_byte_size_count)
if(NOT _float_vector_byte_size_count EQUAL 3)
  message(FATAL_ERROR
    "T080: expected packed element extents for exactly float2/float3/float4 setters")
endif()

string(REGEX MATCHALL "packed_array_byte_size\\(backend_values\\)" _int_vector_byte_sizes "${_backend}")
list(LENGTH _int_vector_byte_sizes _int_vector_byte_size_count)
if(NOT _int_vector_byte_size_count EQUAL 3)
  message(FATAL_ERROR
    "T080: expected packed element extents for exactly int2/int3/int4 setters")
endif()

string(REGEX MATCHALL "packed_array_byte_size\\(components\\)" _color_byte_sizes "${_backend}")
list(LENGTH _color_byte_sizes _color_byte_size_count)
if(NOT _color_byte_size_count EQUAL 1)
  message(FATAL_ERROR
    "T080: expected one packed element extent for the Color setter")
endif()

string(FIND "${_backend}" "sizeof(backend_values)" _backend_array_object_size)
if(NOT _backend_array_object_size EQUAL -1)
  message(FATAL_ERROR "T080: int vector packing must not use std::array object size")
endif()

string(FIND "${_backend}" "sizeof(components)" _color_array_object_size)
if(NOT _color_array_object_size EQUAL -1)
  message(FATAL_ERROR "T080: Color packing must not use std::array object size")
endif()

string(FIND "${_access}" "binding_bytes" _binding_access)
if(_binding_access EQUAL -1)
  message(FATAL_ERROR "T080: missing private immutable binding snapshot access seam")
endif()

file(READ "${SOURCE_DIR}/tests/CMakeLists.txt" _tests_cmake)
string(FIND "${_tests_cmake}" "nativeui_t080_shader_contract" _ctest_registration)
if(_ctest_registration EQUAL -1)
  message(FATAL_ERROR "T080: source contract is not registered with CTest")
endif()

file(READ "${SOURCE_DIR}/examples/features/t080_shader_uniforms.cpp" _example)
foreach(_example_required IN ITEMS
    "self_test_requested"
    "ShaderInstance"
    "uniforms()"
    "T081")
  string(FIND "${_example}" "${_example_required}" _example_required_pos)
  if(_example_required_pos EQUAL -1)
    message(FATAL_ERROR "T080: feature example misses '${_example_required}'")
  endif()
endforeach()
