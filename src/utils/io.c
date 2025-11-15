#include "utils/io.h"

#include <stdio.h>
#include <stdlib.h>

int read_file(const char* filename, sstr_t content) {
    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    sstr_clear(content);
    sstr_append_zero(content, fsize);
    fread(sstr_cstr(content), 1, fsize, f);
    fclose(f);
    return 0;
}

int write_file(const char* filename, sstr_t content) {
    FILE* f = fopen(filename, "wb");
    if (f == NULL) {
        return -1;
    }
    fwrite(sstr_cstr(content), 1, sstr_length(content), f);
    fclose(f);
    return 0;
}
