/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "json-gen-c", "index.html", [
    [ "Highlights", "index.html#autotoc_md0", null ],
    [ "Benchmark", "index.html#autotoc_md1", null ],
    [ "Contents", "index.html#autotoc_md2", null ],
    [ "Overview", "index.html#autotoc_md3", null ],
    [ "Build and Install", "index.html#autotoc_md4", [
      [ "Alternative Build Systems", "index.html#autotoc_md5", null ],
      [ "Package Managers", "index.html#autotoc_md6", null ]
    ] ],
    [ "Quick Start", "index.html#autotoc_md7", [
      [ "Define Structs", "index.html#autotoc_md8", null ],
      [ "Compiling Your Struct Definition File", "index.html#autotoc_md9", [
        [ "MessagePack Format", "index.html#autotoc_md10", null ],
        [ "CBOR Format", "index.html#autotoc_md11", null ]
      ] ],
      [ "C++ Wrapper (Optional)", "index.html#autotoc_md12", null ],
      [ "Rust Module (Optional)", "index.html#autotoc_md13", null ],
      [ "Go Source (Optional)", "index.html#autotoc_md14", null ],
      [ "Use Your Generated Codes", "index.html#autotoc_md15", [
        [ "To Serialize Structs to JSON", "index.html#autotoc_md16", null ],
        [ "To Serialize Array of Structs to JSON", "index.html#autotoc_md17", null ],
        [ "To Deserialize JSON to Structs", "index.html#autotoc_md18", null ],
        [ "To Deserialize JSON to Array of Structs", "index.html#autotoc_md19", null ],
        [ "To Selectively Deserialize Top-Level Fields", "index.html#autotoc_md20", null ],
        [ "To Selectively Deserialize Nested Sub-Fields", "index.html#autotoc_md21", null ]
      ] ]
    ] ],
    [ "Build System", "index.html#autotoc_md22", null ],
    [ "The Format of Structs Definition File", "index.html#autotoc_md23", [
      [ "Map fields", "index.html#autotoc_md24", null ],
      [ "Tagged unions (oneof)", "index.html#autotoc_md25", null ],
      [ "<tt>@deprecated</tt> Annotation", "index.html#autotoc_md26", null ]
    ] ],
    [ "The JSON API", "index.html#autotoc_md27", null ],
    [ "Editor Support", "index.html#autotoc_md28", [
      [ "VS Code Extension", "index.html#autotoc_md29", null ],
      [ "LSP for Other Editors", "index.html#autotoc_md30", null ]
    ] ],
    [ "More Resources", "index.html#autotoc_md31", null ],
    [ "Contributing & Community", "index.html#autotoc_md32", null ],
    [ "License", "index.html#autotoc_md33", null ],
    [ "Contributing to json-gen-c", "md_CONTRIBUTING.html", [
      [ "Ways to Contribute", "md_CONTRIBUTING.html#autotoc_md35", null ],
      [ "Development Environment", "md_CONTRIBUTING.html#autotoc_md36", null ],
      [ "Coding Guidelines", "md_CONTRIBUTING.html#autotoc_md37", null ],
      [ "Pull Request Checklist", "md_CONTRIBUTING.html#autotoc_md38", null ],
      [ "Communication", "md_CONTRIBUTING.html#autotoc_md39", null ]
    ] ],
    [ "Schema Evolution Guide", "md_doc_2schema-evolution.html", [
      [ "Overview", "md_doc_2schema-evolution.html#autotoc_md41", null ],
      [ "Forward Compatibility (Already Built-In)", "md_doc_2schema-evolution.html#autotoc_md42", null ],
      [ "Backward Compatibility", "md_doc_2schema-evolution.html#autotoc_md43", null ],
      [ "Safe Changes", "md_doc_2schema-evolution.html#autotoc_md44", null ],
      [ "Breaking Changes", "md_doc_2schema-evolution.html#autotoc_md45", null ],
      [ "Migration Patterns", "md_doc_2schema-evolution.html#autotoc_md46", [
        [ "Renaming a Field", "md_doc_2schema-evolution.html#autotoc_md47", null ],
        [ "Soft-Removing a Field", "md_doc_2schema-evolution.html#autotoc_md48", null ],
        [ "Adding a Required Field to Existing Data", "md_doc_2schema-evolution.html#autotoc_md49", null ],
        [ "Widening a Numeric Type", "md_doc_2schema-evolution.html#autotoc_md50", null ]
      ] ],
      [ "The <tt>@deprecated</tt> Annotation", "md_doc_2schema-evolution.html#autotoc_md51", [
        [ "Syntax", "md_doc_2schema-evolution.html#autotoc_md52", null ],
        [ "Behavior", "md_doc_2schema-evolution.html#autotoc_md53", null ],
        [ "Generated Code Example", "md_doc_2schema-evolution.html#autotoc_md54", null ]
      ] ],
      [ "Best Practices", "md_doc_2schema-evolution.html#autotoc_md55", null ],
      [ "See Also", "md_doc_2schema-evolution.html#autotoc_md56", null ]
    ] ],
    [ "Getting Started with json-gen-c", "md_docs_2GETTING__STARTED.html", [
      [ "Prerequisites", "md_docs_2GETTING__STARTED.html#autotoc_md58", null ],
      [ "1. Install json-gen-c", "md_docs_2GETTING__STARTED.html#autotoc_md59", null ],
      [ "2. Describe Your Data", "md_docs_2GETTING__STARTED.html#autotoc_md60", null ],
      [ "3. Generate Code", "md_docs_2GETTING__STARTED.html#autotoc_md61", null ],
      [ "4. Use the Generated API", "md_docs_2GETTING__STARTED.html#autotoc_md62", null ],
      [ "5. Regenerate on Schema Changes", "md_docs_2GETTING__STARTED.html#autotoc_md63", null ],
      [ "6. Learn More", "md_docs_2GETTING__STARTED.html#autotoc_md64", null ]
    ] ],
    [ "Project Improvement Plan", "md_docs_2IMPROVEMENTS.html", [
      [ "Current Assessment", "md_docs_2IMPROVEMENTS.html#autotoc_md66", null ],
      [ "Completed Foundational Work", "md_docs_2IMPROVEMENTS.html#autotoc_md67", [
        [ "Implement Hash Map Resizing", "md_docs_2IMPROVEMENTS.html#autotoc_md68", null ],
        [ "Optimize <tt>hash_2s</tt>", "md_docs_2IMPROVEMENTS.html#autotoc_md69", null ],
        [ "Unify Hash Functions", "md_docs_2IMPROVEMENTS.html#autotoc_md70", null ],
        [ "Use <tt>stdbool.h</tt>", "md_docs_2IMPROVEMENTS.html#autotoc_md71", null ],
        [ "Improve Argument Parsing", "md_docs_2IMPROVEMENTS.html#autotoc_md72", null ],
        [ "Improve Error Handling Consistency", "md_docs_2IMPROVEMENTS.html#autotoc_md73", null ],
        [ "Fix Parser Off-by-One Bug", "md_docs_2IMPROVEMENTS.html#autotoc_md74", null ],
        [ "Refactor Top-Level <tt>#include</tt> Parsing", "md_docs_2IMPROVEMENTS.html#autotoc_md75", null ],
        [ "Harden File I/O", "md_docs_2IMPROVEMENTS.html#autotoc_md76", null ],
        [ "Add Enum Type Support", "md_docs_2IMPROVEMENTS.html#autotoc_md77", null ],
        [ "Add Fixed-Size Array Support", "md_docs_2IMPROVEMENTS.html#autotoc_md78", null ],
        [ "Add Map/Dictionary Support", "md_docs_2IMPROVEMENTS.html#autotoc_md79", null ]
      ] ],
      [ "Roadmap", "md_docs_2IMPROVEMENTS.html#autotoc_md80", [
        [ "Phase 1: Bug Fixes and Technical Debt Cleanup", "md_docs_2IMPROVEMENTS.html#autotoc_md81", null ],
        [ "Phase 2: Type System Expansion", "md_docs_2IMPROVEMENTS.html#autotoc_md82", null ],
        [ "Phase 3: Developer Experience", "md_docs_2IMPROVEMENTS.html#autotoc_md83", null ],
        [ "Phase 4: Build Ecosystem and Cross-Platform Support", "md_docs_2IMPROVEMENTS.html#autotoc_md84", null ],
        [ "Phase 5: Performance and Reliability", "md_docs_2IMPROVEMENTS.html#autotoc_md85", null ],
        [ "Phase 6: Long-Term Vision", "md_docs_2IMPROVEMENTS.html#autotoc_md86", null ]
      ] ],
      [ "Key Files", "md_docs_2IMPROVEMENTS.html#autotoc_md87", null ],
      [ "Project Direction Notes", "md_docs_2IMPROVEMENTS.html#autotoc_md88", null ],
      [ "Open Design Questions", "md_docs_2IMPROVEMENTS.html#autotoc_md89", null ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", null ],
        [ "Variables", "functions_vars.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"sstr_8h.html#a2d6e86f4ff2db447c525e82b88d3173e"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';