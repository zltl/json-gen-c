#include "utils/io.h"

#include <stdio.h>
#include <stdlib.h>

int read_file(const char* filename, sstr_t content) {
    FILE* f = fopen(filename, "rb");
    if (f == NULL) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long fsize = ftell(f);
    if (fsize < 0) {
        fclose(f);
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }

    sstr_clear(content);
    sstr_append_zero(content, fsize);
    size_t read_size = fread(sstr_cstr(content), 1, fsize, f);
    fclose(f);

    if (read_size != (size_t)fsize) {
        return -1;
    }
    return 0;
}

int write_file(const char* filename, sstr_t content) {
    FILE* f = fopen(filename, "wb");
    if (f == NULL) {
        return -1;
    }
    size_t written = fwrite(sstr_cstr(content), 1, sstr_length(content), f);
    fclose(f);
    
    if (written != sstr_length(content)) {
        return -1;
    }
    return 0;
}
