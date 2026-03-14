# JsonGenCMacro.cmake
# Provides json_gen_c_generate() for downstream consumers.
#
# Usage:
#   find_package(JsonGenC REQUIRED)
#   json_gen_c_generate(
#       TARGET my_app
#       SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/schema.json-gen-c"
#       OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated"
#   )
#
# This adds a custom command that runs json-gen-c on the schema file,
# producing json.gen.h, json.gen.c, sstr.h, and sstr.c in OUTPUT_DIR.
# The generated sources are added to TARGET automatically.

function(json_gen_c_generate)
    cmake_parse_arguments(ARG "" "TARGET;SCHEMA;OUTPUT_DIR" "" ${ARGN})

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "json_gen_c_generate: TARGET is required")
    endif()
    if(NOT ARG_SCHEMA)
        message(FATAL_ERROR "json_gen_c_generate: SCHEMA is required")
    endif()
    if(NOT ARG_OUTPUT_DIR)
        set(ARG_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    endif()

    set(_gen_h "${ARG_OUTPUT_DIR}/json.gen.h")
    set(_gen_c "${ARG_OUTPUT_DIR}/json.gen.c")
    set(_sstr_h "${ARG_OUTPUT_DIR}/sstr.h")
    set(_sstr_c "${ARG_OUTPUT_DIR}/sstr.c")

    add_custom_command(
        OUTPUT "${_gen_h}" "${_gen_c}" "${_sstr_h}" "${_sstr_c}"
        COMMAND JsonGenC::json-gen-c -in "${ARG_SCHEMA}" -out "${ARG_OUTPUT_DIR}"
        DEPENDS "${ARG_SCHEMA}" JsonGenC::json-gen-c
        COMMENT "Generating JSON code from ${ARG_SCHEMA}"
        VERBATIM
    )

    target_sources(${ARG_TARGET} PRIVATE
        "${_gen_c}" "${_sstr_c}"
    )
    target_include_directories(${ARG_TARGET} PRIVATE
        "${ARG_OUTPUT_DIR}"
    )
endfunction()
