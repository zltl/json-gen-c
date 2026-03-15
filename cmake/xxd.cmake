# cmake/xxd.cmake — Portable xxd -i replacement for systems without xxd.
#
# Usage:
#   cmake -DINPUT_FILE=<path> -DVAR_NAME=<name> -DMODE=write|append
#         -DOUTPUT_FILE=<path> -P cmake/xxd.cmake
#
# Produces output identical to `xxd -i <file>`:
#   unsigned char <var_name>[] = { 0x.., 0x.., ... };
#   unsigned int <var_name>_len = <length>;

cmake_minimum_required(VERSION 3.16)

if(NOT INPUT_FILE OR NOT VAR_NAME OR NOT OUTPUT_FILE OR NOT MODE)
    message(FATAL_ERROR "Usage: cmake -DINPUT_FILE=... -DVAR_NAME=... -DMODE=write|append -DOUTPUT_FILE=... -P xxd.cmake")
endif()

file(READ "${INPUT_FILE}" FILE_CONTENT HEX)
string(LENGTH "${FILE_CONTENT}" HEX_LENGTH)
math(EXPR FILE_LENGTH "${HEX_LENGTH} / 2")

# Build comma-separated hex byte list
set(HEX_ARRAY "")
set(COL 0)
math(EXPR LAST_BYTE "${FILE_LENGTH} - 1")
foreach(IDX RANGE 0 ${LAST_BYTE})
    math(EXPR OFFSET "${IDX} * 2")
    string(SUBSTRING "${FILE_CONTENT}" ${OFFSET} 2 BYTE)
    if(IDX EQUAL 0)
        string(APPEND HEX_ARRAY "  0x${BYTE}")
    else()
        string(APPEND HEX_ARRAY ", 0x${BYTE}")
    endif()
    math(EXPR COL "${COL} + 1")
    if(COL EQUAL 12)
        string(APPEND HEX_ARRAY "\n")
        set(COL 0)
    endif()
endforeach()

set(OUTPUT_TEXT "unsigned char ${VAR_NAME}[] = {\n${HEX_ARRAY}\n};\nunsigned int ${VAR_NAME}_len = ${FILE_LENGTH};\n")

if(MODE STREQUAL "write")
    file(WRITE "${OUTPUT_FILE}" "${OUTPUT_TEXT}")
elseif(MODE STREQUAL "append")
    file(APPEND "${OUTPUT_FILE}" "${OUTPUT_TEXT}")
else()
    message(FATAL_ERROR "MODE must be 'write' or 'append', got '${MODE}'")
endif()
