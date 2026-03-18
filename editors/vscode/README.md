# json-gen-c VS Code Extension

Syntax highlighting, real-time diagnostics, and language support for `.json-gen-c` schema definition files.

## Features

- **Syntax highlighting** for all schema language elements:
  - Keywords: `struct`, `enum`, `oneof`
  - Field modifiers: `optional`, `nullable`
  - Builtin types: `int`, `long`, `float`, `double`, `bool`, `sstr_t`, precise-width integers
  - Annotations: `@json "alias"`, `@tag "field"`, `@deprecated`
  - `#include` directives
  - `map<K, V>` generic syntax
  - Array declarations (`[]`, `[N]`)
  - Default values (`= 42`, `= "hello"`, `= true`)
  - Comments (`//` line and `/* */` block)
  - String and numeric literals
  - User-defined type references

- **Language Server (LSP)**:
  - Real-time parse error diagnostics as you type
  - Schema validation (undefined types, duplicate fields, etc.)
  - Code completion for keywords, types, and annotations
  - Hover info for type references

- **Language configuration**:
  - Bracket matching and auto-closing (`{}`, `[]`, `<>`, `""`)
  - Comment toggling (`Ctrl+/` for line, `Shift+Alt+A` for block)
  - Code folding on braces
  - Auto-indentation

## Install

### Prerequisites

The language server requires the `json-gen-c` binary (v0.9.0+) to be installed and available in your PATH, or configured via the `json-gen-c.serverPath` setting.

After installing `json-gen-c`, also install the extension's npm dependencies:

```sh
cd editors/vscode
npm install
```

### Method 1: Symlink (development)

```sh
# From the repository root
cd editors/vscode && npm install && cd ../..
ln -s "$(pwd)/editors/vscode" ~/.vscode/extensions/json-gen-c
```

Restart VS Code. The extension activates automatically for `.json-gen-c` files.

### Method 2: Copy

```sh
cd editors/vscode && npm install
cp -r editors/vscode ~/.vscode/extensions/json-gen-c
```

Restart VS Code.

### Method 3: VS Code CLI

```sh
# Package and install (requires vsce)
cd editors/vscode
npm install
npx @vscode/vsce package
code --install-extension json-gen-c-0.9.0.vsix
```

## Configuration

| Setting | Default | Description |
|---------|---------|-------------|
| `json-gen-c.serverPath` | `"json-gen-c"` | Path to the `json-gen-c` executable (must support `--lsp` flag) |

## Example

```
// User schema
#include "common.json-gen-c"

enum Color { RED, GREEN, BLUE }

struct Person {
    @json "user_name"
    sstr_t name;
    int age;
    optional sstr_t email;
    Color favorite_color;
    int scores[];
    bool active = true;
}

oneof Shape {
    @tag "type"
    Circle circle;
    Rectangle rectangle;
}
```

## License

Same as the parent project (GPL-3.0).
