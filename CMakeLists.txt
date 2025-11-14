cmake_minimum_required(VERSION 3.21)
project(ouro LANGUAGES CXX)

include(CheckCXXCompilerFlag)

# Only check for x86/x64 SIMD instructions on compatible architectures
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
  check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
  check_cxx_compiler_flag("-mavx" COMPILER_SUPPORTS_AVX)
  check_cxx_compiler_flag("-mfma" COMPILER_SUPPORTS_FMA)
  check_cxx_compiler_flag("-msse4.2" COMPILER_SUPPORTS_SSE42)
  check_cxx_compiler_flag("-msse4.1" COMPILER_SUPPORTS_SSE41)
  check_cxx_compiler_flag("-msse3" COMPILER_SUPPORTS_SSE3)
  check_cxx_compiler_flag("-msse2" COMPILER_SUPPORTS_SSE2)
else()
  # On ARM and other architectures, disable x86 SIMD
  set(COMPILER_SUPPORTS_AVX2 FALSE)
  set(COMPILER_SUPPORTS_AVX FALSE)
  set(COMPILER_SUPPORTS_FMA FALSE)
  set(COMPILER_SUPPORTS_SSE42 FALSE)
  set(COMPILER_SUPPORTS_SSE41 FALSE)
  set(COMPILER_SUPPORTS_SSE3 FALSE)
  set(COMPILER_SUPPORTS_SSE2 FALSE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")

file(GLOB_RECURSE PROJECT_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
file(GLOB_RECURSE PROJECT_HDR CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/src/*.hpp")
add_executable(${PROJECT_NAME} ${PROJECT_SRC} ${PROJECT_HDR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGLM_FORCE_INLINE=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGLM_FORCE_DEFAULT_ALIGNED_GENTYPES=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGLM_ENABLE_EXPERIMENTAL=1")
if(COMPILER_SUPPORTS_SSE2)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGLM_FORCE_SSE2=1")
endif()
if(COMPILER_SUPPORTS_SSE3)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGLM_FORCE_SSE3=1")
endif()
if(COMPILER_SUPPORTS_SSE41)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGLM_FORCE_SSE41=1")
endif()
if(COMPILER_SUPPORTS_SSE42)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGLM_FORCE_SSE42=1")
endif()
if(COMPILER_SUPPORTS_AVX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGLM_FORCE_AVX=1")
endif()
if(COMPILER_SUPPORTS_AVX2)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGLM_FORCE_AVX2=1")
endif()
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/third_party/glm")

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/third_party/raylib")
target_compile_options(raylib PRIVATE -w)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/third_party/Unity")

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/third_party/tinycthread")

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src" "${CMAKE_CURRENT_LIST_DIR}/third_party/fmod/api/core/inc")
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${CMAKE_CURRENT_LIST_DIR}/third_party/raylib/src")
if(APPLE)
  set(FMOD_LIB_PATH "lib_macos")
else()
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(FMOD_LIB_PATH "lib_linux/x86_64")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(FMOD_LIB_PATH "lib_linux/arm64")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    set(FMOD_LIB_PATH "lib_linux/arm")
  else()
    message(FATAL_ERROR "Unsupported Linux architecture: ${CMAKE_SYSTEM_PROCESSOR}")
  endif()
endif()
find_library(FMOD_CORE_LIBRARY NAMES fmodL PATHS "${CMAKE_CURRENT_LIST_DIR}/third_party/fmod/api/core/${FMOD_LIB_PATH}")

target_link_libraries(${PROJECT_NAME} PRIVATE
  raylib
  glm
  unity
  tinycthread
  ${FMOD_CORE_LIBRARY})

# Common compiler options
set(COMMON_COMPILE_OPTIONS
  -Wall
  -Wcast-align
  -Wcast-qual
  -Wconversion
  -Werror
  -Wextra
  -Wno-c99-designator
  -Wno-missing-field-initializers
  -Wno-unsequenced
  -Wno-unused-function
  -Wno-unused-parameter
  -Wno-unused-variable
  -Wnull-dereference
  -Wpointer-arith
  -Wredundant-decls
  -Wshadow
  -Wsign-conversion
  -Wstrict-aliasing
  -Wstrict-overflow=5
  -Wwrite-strings
  -fno-builtin
  -fno-exceptions
  -fno-rtti
  $<$<PLATFORM_ID:Darwin>:-Wno-deprecated-declarations>
)

# SIMD flags
set(SIMD_FLAGS
  $<$<BOOL:${COMPILER_SUPPORTS_SSE2}>:-msse2>
  $<$<BOOL:${COMPILER_SUPPORTS_SSE3}>:-msse3>
  $<$<BOOL:${COMPILER_SUPPORTS_SSE41}>:-msse4.1>
  $<$<BOOL:${COMPILER_SUPPORTS_SSE42}>:-msse4.2>
  $<$<BOOL:${COMPILER_SUPPORTS_AVX}>:-mavx>
  $<$<BOOL:${COMPILER_SUPPORTS_AVX2}>:-mavx2>
  $<$<BOOL:${COMPILER_SUPPORTS_FMA}>:-mfma>
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  # target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined,leak,pointer-compare,pointer-subtract,pointer-overflow)
  # target_compile_definitions(${PROJECT_NAME} PRIVATE OURO_TRACE)
  # target_compile_definitions(${PROJECT_NAME} PRIVATE OURO_DEBUG)
  target_compile_definitions(${PROJECT_NAME} PRIVATE OURO_PROFILE OURO_DEVEL)
  target_compile_options(${PROJECT_NAME} PRIVATE
    ${COMMON_COMPILE_OPTIONS} ${SIMD_FLAGS}
    -ggdb
    -O0
    -fno-omit-frame-pointer
    -fstack-protector-strong
    # -ftime-trace -c
    # -fsanitize=address,undefined,leak,pointer-compare,pointer-subtract,pointer-overflow
  )
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_link_options(${PROJECT_NAME} PRIVATE -s)
  target_compile_definitions(${PROJECT_NAME} PRIVATE OURO_PROFILE OURO_RELEASE)
  target_compile_options(${PROJECT_NAME} PRIVATE
    ${COMMON_COMPILE_OPTIONS} ${SIMD_FLAGS}
    -O3
    -ffast-math
    -ftree-vectorize
    -fomit-frame-pointer
  )
endif()
