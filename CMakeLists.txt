cmake_minimum_required(VERSION 3.16)
project(json-gen-c
    VERSION 0.9.0
    DESCRIPTION "Generate C code for JSON serialization and deserialization"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(JSON_GEN_C_DEBUG "Enable debug output" OFF)
option(JSON_GEN_C_SANITIZE "Enable AddressSanitizer" OFF)

#==============================================================================
# Find xxd (required for embedding runtime files)
#==============================================================================
find_program(XXD_EXECUTABLE xxd REQUIRED)

#==============================================================================
# Generate embedded code arrays via xxd
#==============================================================================

# extra_codes.inc: embed json_parse.c and json_parse.h
set(GENCODE_EXTRA_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/gencode")
set(GENCODE_EXTRA_INC "${GENCODE_EXTRA_DIR}/extra_codes.inc")
file(MAKE_DIRECTORY "${GENCODE_EXTRA_DIR}")

add_custom_command(
    OUTPUT "${GENCODE_EXTRA_INC}"
    COMMAND ${XXD_EXECUTABLE} -i json_parse.c > "${GENCODE_EXTRA_INC}"
    COMMAND ${XXD_EXECUTABLE} -i json_parse.h >> "${GENCODE_EXTRA_INC}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/json_parse.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/json_parse.h"
    COMMENT "Generating extra_codes.inc (json_parse runtime)"
)

# extra_codes_sstr.inc: embed sstr.c and sstr.h
set(MAIN_EXTRA_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/main")
set(MAIN_EXTRA_INC "${MAIN_EXTRA_DIR}/extra_codes_sstr.inc")
file(MAKE_DIRECTORY "${MAIN_EXTRA_DIR}")

add_custom_command(
    OUTPUT "${MAIN_EXTRA_INC}"
    COMMAND ${XXD_EXECUTABLE} -i sstr.c > "${MAIN_EXTRA_INC}"
    COMMAND ${XXD_EXECUTABLE} -i sstr.h >> "${MAIN_EXTRA_INC}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/utils"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/sstr.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/sstr.h"
    COMMENT "Generating extra_codes_sstr.inc (sstr runtime)"
)

#==============================================================================
# Utils library
#==============================================================================
add_library(json_gen_c_utils STATIC
    src/utils/sstr.c
    src/utils/diag.c
    src/utils/error_codes.c
    src/utils/hash.c
    src/utils/hash_map.c
    src/utils/io.c
    src/utils/json_context.c
)
target_include_directories(json_gen_c_utils PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
find_package(Threads REQUIRED)
target_link_libraries(json_gen_c_utils PUBLIC Threads::Threads)

#==============================================================================
# Struct library
#==============================================================================
add_library(json_gen_c_struct STATIC
    src/struct/struct_parse.c
)
target_link_libraries(json_gen_c_struct PUBLIC json_gen_c_utils)

#==============================================================================
# Gencode library
#==============================================================================
add_library(json_gen_c_gencode STATIC
    src/gencode/gencode.c
    "${GENCODE_EXTRA_INC}"
)
target_include_directories(json_gen_c_gencode PRIVATE
    "${GENCODE_EXTRA_DIR}"
)
target_link_libraries(json_gen_c_gencode PUBLIC json_gen_c_utils)

#==============================================================================
# Main executable
#==============================================================================
add_executable(json-gen-c
    src/main/main.c
    "${MAIN_EXTRA_INC}"
)
target_include_directories(json-gen-c PRIVATE
    "${MAIN_EXTRA_DIR}"
)
target_link_libraries(json-gen-c PRIVATE
    json_gen_c_gencode
    json_gen_c_struct
    json_gen_c_utils
)
target_compile_definitions(json-gen-c PRIVATE
    JSON_GEN_C_VERSION="${PROJECT_VERSION}"
)

# Optional flags
if(JSON_GEN_C_DEBUG)
    target_compile_definitions(json-gen-c PRIVATE JSON_DEBUG)
endif()
if(JSON_GEN_C_SANITIZE)
    target_compile_options(json-gen-c PRIVATE -fsanitize=address)
    target_link_options(json-gen-c PRIVATE -fsanitize=address)
endif()

# Compiler warnings (match existing Makefile)
foreach(tgt json_gen_c_utils json_gen_c_struct json_gen_c_gencode json-gen-c)
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -Werror -Wno-unused-result)
endforeach()

#==============================================================================
# Installation
#==============================================================================
include(GNUInstallDirs)

install(TARGETS json-gen-c
    EXPORT json-gen-c-targets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(FILES doc/json-gen-c.1
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
)

# CMake package config for find_package(JsonGenC)
install(EXPORT json-gen-c-targets
    FILE JsonGenCTargets.cmake
    NAMESPACE JsonGenC::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/JsonGenC"
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/JsonGenCConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/JsonGenCConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/JsonGenC"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/JsonGenCConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/JsonGenCConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/JsonGenCConfigVersion.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/JsonGenCMacro.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/JsonGenC"
)

# pkg-config
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/json-gen-c.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/json-gen-c.pc"
    @ONLY
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/json-gen-c.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)
