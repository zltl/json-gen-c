project('json-gen-c', 'c',
    version: '0.9.0',
    license: 'GPL-3.0-or-later',
    default_options: [
        'c_std=c11',
        'warning_level=2',
        'werror=true',
    ],
)

cc = meson.get_compiler('c')
thread_dep = dependency('threads')
xxd = find_program('xxd', required: true)

add_project_arguments('-Wno-unused-result', language: 'c')

#==============================================================================
# Generate embedded code arrays via xxd
#==============================================================================

gencode_inc_dir = meson.current_build_dir()

extra_codes_inc = custom_target('extra_codes_inc',
    output: 'extra_codes.inc',
    input: [
        'src/gencode/codes/json_parse.c',
        'src/gencode/codes/json_parse.h',
    ],
    command: [
        'sh', '-c',
        'cd @SOURCE_ROOT@/src/gencode/codes && @0@ -i json_parse.c > @1@/extra_codes.inc && @0@ -i json_parse.h >> @1@/extra_codes.inc'.format(xxd.full_path(), gencode_inc_dir),
    ],
)

extra_codes_sstr_inc = custom_target('extra_codes_sstr_inc',
    output: 'extra_codes_sstr.inc',
    input: [
        'src/utils/sstr.c',
        'src/utils/sstr.h',
    ],
    command: [
        'sh', '-c',
        'cd @SOURCE_ROOT@/src/utils && @0@ -i sstr.c > @1@/extra_codes_sstr.inc && @0@ -i sstr.h >> @1@/extra_codes_sstr.inc'.format(xxd.full_path(), gencode_inc_dir),
    ],
)

# Include directory for generated .inc files
gen_inc = include_directories('.')  # build dir is added implicitly

#==============================================================================
# Libraries
#==============================================================================

utils_lib = static_library('utils',
    'src/utils/sstr.c',
    'src/utils/diag.c',
    'src/utils/error_codes.c',
    'src/utils/hash.c',
    'src/utils/hash_map.c',
    'src/utils/io.c',
    'src/utils/json_context.c',
    include_directories: 'src',
    dependencies: thread_dep,
)

struct_lib = static_library('struct',
    'src/struct/struct_parse.c',
    include_directories: 'src',
    link_with: utils_lib,
)

gencode_lib = static_library('gencode',
    'src/gencode/gencode.c',
    extra_codes_inc,
    include_directories: 'src',
    c_args: ['-I' + gencode_inc_dir],
    link_with: utils_lib,
)

#==============================================================================
# Main executable
#==============================================================================

json_gen_c = executable('json-gen-c',
    'src/main/main.c',
    extra_codes_sstr_inc,
    include_directories: 'src',
    link_with: [gencode_lib, struct_lib, utils_lib],
    c_args: [
        '-DJSON_GEN_C_VERSION="' + meson.project_version() + '"',
        '-I' + gencode_inc_dir,
    ],
    install: true,
)

install_man('doc/json-gen-c.1')
