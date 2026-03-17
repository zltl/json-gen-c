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
# Find xxd or provide a fallback for Windows
#==============================================================================
find_program(XXD_EXECUTABLE xxd)

# Portable xxd replacement using CMake's file(READ ... HEX) + a small script
if(NOT XXD_EXECUTABLE)
    set(XXD_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/xxd.cmake")
    if(NOT EXISTS "${XXD_SCRIPT}")
        message(FATAL_ERROR "xxd not found and cmake/xxd.cmake fallback is missing")
    endif()
    message(STATUS "xxd not found, using cmake/xxd.cmake fallback")
endif()

#==============================================================================
# Generate embedded code arrays via xxd
#==============================================================================

# extra_codes.inc: embed json_parse.c and json_parse.h
set(GENCODE_EXTRA_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/gencode")
set(GENCODE_EXTRA_INC "${GENCODE_EXTRA_DIR}/extra_codes.inc")
file(MAKE_DIRECTORY "${GENCODE_EXTRA_DIR}")

if(XXD_EXECUTABLE)
    add_custom_command(
        OUTPUT "${GENCODE_EXTRA_INC}"
        COMMAND ${XXD_EXECUTABLE} -i json_parse.c > "${GENCODE_EXTRA_INC}"
        COMMAND ${XXD_EXECUTABLE} -i json_parse.h >> "${GENCODE_EXTRA_INC}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/json_parse.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/json_parse.h"
        COMMENT "Generating extra_codes.inc (json_parse runtime via xxd)"
    )
else()
    add_custom_command(
        OUTPUT "${GENCODE_EXTRA_INC}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/json_parse.c"
            -DVAR_NAME=json_parse_c -DMODE=write
            -DOUTPUT_FILE="${GENCODE_EXTRA_INC}"
            -P "${XXD_SCRIPT}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/json_parse.h"
            -DVAR_NAME=json_parse_h -DMODE=append
            -DOUTPUT_FILE="${GENCODE_EXTRA_INC}"
            -P "${XXD_SCRIPT}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/json_parse.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/json_parse.h"
            "${XXD_SCRIPT}"
        COMMENT "Generating extra_codes.inc (json_parse runtime via cmake)"
    )
endif()

# extra_codes_msgpack.inc: embed msgpack_codec.c and msgpack_codec.h
set(GENCODE_MSGPACK_INC "${GENCODE_EXTRA_DIR}/extra_codes_msgpack.inc")

if(XXD_EXECUTABLE)
    add_custom_command(
        OUTPUT "${GENCODE_MSGPACK_INC}"
        COMMAND ${XXD_EXECUTABLE} -i msgpack_codec.c > "${GENCODE_MSGPACK_INC}"
        COMMAND ${XXD_EXECUTABLE} -i msgpack_codec.h >> "${GENCODE_MSGPACK_INC}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/msgpack_codec.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/msgpack_codec.h"
        COMMENT "Generating extra_codes_msgpack.inc (msgpack runtime via xxd)"
    )
else()
    add_custom_command(
        OUTPUT "${GENCODE_MSGPACK_INC}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/msgpack_codec.c"
            -DVAR_NAME=msgpack_codec_c -DMODE=write
            -DOUTPUT_FILE="${GENCODE_MSGPACK_INC}"
            -P "${XXD_SCRIPT}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/msgpack_codec.h"
            -DVAR_NAME=msgpack_codec_h -DMODE=append
            -DOUTPUT_FILE="${GENCODE_MSGPACK_INC}"
            -P "${XXD_SCRIPT}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/msgpack_codec.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/msgpack_codec.h"
            "${XXD_SCRIPT}"
        COMMENT "Generating extra_codes_msgpack.inc (msgpack runtime via cmake)"
    )
endif()

# extra_codes_cbor.inc: embed cbor_codec.c and cbor_codec.h
set(GENCODE_CBOR_INC "${GENCODE_EXTRA_DIR}/extra_codes_cbor.inc")

if(XXD_EXECUTABLE)
    add_custom_command(
        OUTPUT "${GENCODE_CBOR_INC}"
        COMMAND ${XXD_EXECUTABLE} -i cbor_codec.c > "${GENCODE_CBOR_INC}"
        COMMAND ${XXD_EXECUTABLE} -i cbor_codec.h >> "${GENCODE_CBOR_INC}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/cbor_codec.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/cbor_codec.h"
        COMMENT "Generating extra_codes_cbor.inc (cbor runtime via xxd)"
    )
else()
    add_custom_command(
        OUTPUT "${GENCODE_CBOR_INC}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/cbor_codec.c"
            -DVAR_NAME=cbor_codec_c -DMODE=write
            -DOUTPUT_FILE="${GENCODE_CBOR_INC}"
            -P "${XXD_SCRIPT}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/cbor_codec.h"
            -DVAR_NAME=cbor_codec_h -DMODE=append
            -DOUTPUT_FILE="${GENCODE_CBOR_INC}"
            -P "${XXD_SCRIPT}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/cbor_codec.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/gencode/codes/cbor_codec.h"
            "${XXD_SCRIPT}"
        COMMENT "Generating extra_codes_cbor.inc (cbor runtime via cmake)"
    )
endif()

# extra_codes_sstr.inc: embed sstr.c and sstr.h
set(MAIN_EXTRA_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/main")
set(MAIN_EXTRA_INC "${MAIN_EXTRA_DIR}/extra_codes_sstr.inc")
file(MAKE_DIRECTORY "${MAIN_EXTRA_DIR}")

if(XXD_EXECUTABLE)
    add_custom_command(
        OUTPUT "${MAIN_EXTRA_INC}"
        COMMAND ${XXD_EXECUTABLE} -i sstr.c > "${MAIN_EXTRA_INC}"
        COMMAND ${XXD_EXECUTABLE} -i sstr.h >> "${MAIN_EXTRA_INC}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/utils"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/sstr.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/sstr.h"
        COMMENT "Generating extra_codes_sstr.inc (sstr runtime via xxd)"
    )
else()
    add_custom_command(
        OUTPUT "${MAIN_EXTRA_INC}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/src/utils/sstr.c"
            -DVAR_NAME=sstr_c -DMODE=write
            -DOUTPUT_FILE="${MAIN_EXTRA_INC}"
            -P "${XXD_SCRIPT}"
        COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/src/utils/sstr.h"
            -DVAR_NAME=sstr_h -DMODE=append
            -DOUTPUT_FILE="${MAIN_EXTRA_INC}"
            -P "${XXD_SCRIPT}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/sstr.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/sstr.h"
            "${XXD_SCRIPT}"
        COMMENT "Generating extra_codes_sstr.inc (sstr runtime via cmake)"
    )
endif()

#==============================================================================
# Utils library
#==============================================================================
set(UTILS_SOURCES
    src/utils/sstr.c
    src/utils/diag.c
    src/utils/error_codes.c
    src/utils/hash.c
    src/utils/hash_map.c
    src/utils/io.c
    src/utils/json_context.c
)
if(WIN32)
    list(APPEND UTILS_SOURCES src/utils/getopt_compat.c)
endif()

add_library(json_gen_c_utils STATIC ${UTILS_SOURCES})
target_include_directories(json_gen_c_utils PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
if(WIN32)
    # Windows: no pthreads needed; compat.h uses CRITICAL_SECTION
else()
    find_package(Threads REQUIRED)
    target_link_libraries(json_gen_c_utils PUBLIC Threads::Threads)
endif()

#==============================================================================
# Struct library
#==============================================================================
add_library(json_gen_c_struct STATIC
    src/struct/struct_parse.c
    src/compat/compat_check.c
)
target_link_libraries(json_gen_c_struct PUBLIC json_gen_c_utils)

#==============================================================================
# Gencode library
#==============================================================================
add_library(json_gen_c_gencode STATIC
    src/gencode/gencode.c
    src/gencode/gencode_msgpack.c
    src/gencode/gencode_cbor.c
    "${GENCODE_EXTRA_INC}"
    "${GENCODE_MSGPACK_INC}"
    "${GENCODE_CBOR_INC}"
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

# Compiler warnings
foreach(tgt json_gen_c_utils json_gen_c_struct json_gen_c_gencode json-gen-c)
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /W4 /WX)
    else()
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Werror -Wno-unused-result)
    endif()
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
