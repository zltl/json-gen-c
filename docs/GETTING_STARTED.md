# Getting Started with json-gen-c

This guide walks you through installing json-gen-c, generating code, and integrating it into your project.

## Prerequisites

- A C11-compatible compiler (GCC or Clang).
- GNU Make.
- Optional: `pkg-config`, `clang-format`, and `cppcheck` for a smoother workflow.

## 1. Install json-gen-c

```bash
git clone https://github.com/zltl/json-gen-c.git
cd json-gen-c
make
sudo make install   # optional, installs json-gen-c into your PATH
```

## 2. Describe Your Data

Create a `.json-gen-c` file describing the structs to serialize:

```c
// file: demo.json-gen-c
struct User {
    int id;
    sstr_t name;
    double balance;
    Address address;
};

struct Address {
    sstr_t city;
    sstr_t country;
};
```

## 3. Generate Code

```bash
json-gen-c -in demo.json-gen-c -out gen
```

The command emits:

- `gen/json.gen.h` / `gen/json.gen.c` – struct definitions and JSON helpers.
- `gen/sstr.h` / `gen/sstr.c` – lightweight string utilities required by the generated code.

## 4. Use the Generated API

```c
#include "json.gen.h"

int main(void) {
    struct User user;
    User_init(&user);

    user.id = 7;
    user.name = sstr("Ada");

    sstr_t payload = sstr_new();
    json_marshal_User(&user, payload);
    printf("%s\n", sstr_cstr(payload));

    sstr_free(payload);
    User_clear(&user);
    return 0;
}
```

Link your program with the generated `json.gen.c` and `sstr.c` files.

## 5. Regenerate on Schema Changes

Any time you edit the `.json-gen-c` schema, rerun `json-gen-c` to refresh the generated sources. Consider automating this step with a Makefile target so your build always has up-to-date code.

## 6. Learn More

- Browse the `example/` directory for reference schemas and usage.
- Run `json-gen-c --help` to see command-line options.
- Consult the [online documentation](https://zltl.github.io/json-gen-c/) for API details.

Happy hacking! 🎉
