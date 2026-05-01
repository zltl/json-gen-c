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
        [ "To Copy or Move Generated Objects", "index.html#autotoc_md20", null ],
        [ "To Selectively Deserialize Top-Level Fields", "index.html#autotoc_md21", null ],
        [ "To Selectively Deserialize Nested Sub-Fields", "index.html#autotoc_md22", null ]
      ] ]
    ] ],
    [ "Build System", "index.html#autotoc_md23", null ],
    [ "The Format of Structs Definition File", "index.html#autotoc_md24", [
      [ "Map fields", "index.html#autotoc_md25", null ],
      [ "Tagged unions (oneof)", "index.html#autotoc_md26", null ],
      [ "<tt>@deprecated</tt> Annotation", "index.html#autotoc_md27", null ]
    ] ],
    [ "The JSON API", "index.html#autotoc_md28", null ],
    [ "Editor Support", "index.html#autotoc_md29", [
      [ "VS Code Extension", "index.html#autotoc_md30", null ],
      [ "LSP for Other Editors", "index.html#autotoc_md31", null ]
    ] ],
    [ "More Resources", "index.html#autotoc_md32", null ],
    [ "Contributing & Community", "index.html#autotoc_md33", null ],
    [ "License", "index.html#autotoc_md34", null ],
    [ "Contributing to json-gen-c", "md_CONTRIBUTING.html", [
      [ "Ways to Contribute", "md_CONTRIBUTING.html#autotoc_md36", null ],
      [ "Development Environment", "md_CONTRIBUTING.html#autotoc_md37", null ],
      [ "Coding Guidelines", "md_CONTRIBUTING.html#autotoc_md38", null ],
      [ "Pull Request Checklist", "md_CONTRIBUTING.html#autotoc_md39", null ],
      [ "Communication", "md_CONTRIBUTING.html#autotoc_md40", null ]
    ] ],
    [ "Copy and Move API Proposal", "md_doc_2copy-move-proposal.html", [
      [ "Status", "md_doc_2copy-move-proposal.html#autotoc_md42", null ],
      [ "Summary", "md_doc_2copy-move-proposal.html#autotoc_md43", null ],
      [ "Motivation", "md_doc_2copy-move-proposal.html#autotoc_md44", null ],
      [ "Goals", "md_doc_2copy-move-proposal.html#autotoc_md45", null ],
      [ "Non-Goals", "md_doc_2copy-move-proposal.html#autotoc_md46", null ],
      [ "Proposed API", "md_doc_2copy-move-proposal.html#autotoc_md47", null ],
      [ "Generated Usage", "md_doc_2copy-move-proposal.html#autotoc_md48", null ],
      [ "Behavioral Contract", "md_doc_2copy-move-proposal.html#autotoc_md49", [
        [ "Preconditions", "md_doc_2copy-move-proposal.html#autotoc_md50", null ],
        [ "Common Rules", "md_doc_2copy-move-proposal.html#autotoc_md51", null ],
        [ "Copy Semantics", "md_doc_2copy-move-proposal.html#autotoc_md52", null ],
        [ "Move Semantics", "md_doc_2copy-move-proposal.html#autotoc_md53", null ],
        [ "Failure Semantics", "md_doc_2copy-move-proposal.html#autotoc_md54", null ]
      ] ],
      [ "Why Not Use Field Masks for Copy", "md_doc_2copy-move-proposal.html#autotoc_md55", [
        [ "1. The Current API Is Parse-Oriented", "md_doc_2copy-move-proposal.html#autotoc_md56", null ],
        [ "2. Nested Selection Is Too Heavy for a Default Object API", "md_doc_2copy-move-proposal.html#autotoc_md57", null ],
        [ "3. Merge Is a Separate Semantics Problem", "md_doc_2copy-move-proposal.html#autotoc_md58", null ]
      ] ],
      [ "Alternatives Considered", "md_doc_2copy-move-proposal.html#autotoc_md59", [
        [ "Reuse marshal/unmarshal round-trip in generated C APIs", "md_doc_2copy-move-proposal.html#autotoc_md60", null ],
        [ "Add <tt>*_copy_selected()</tt> and <tt>*_move_selected()</tt> first", "md_doc_2copy-move-proposal.html#autotoc_md61", null ],
        [ "Leave move source in init-equivalent state", "md_doc_2copy-move-proposal.html#autotoc_md62", null ]
      ] ],
      [ "Implementation Plan", "md_doc_2copy-move-proposal.html#autotoc_md63", [
        [ "Phase 1: Finalize Contract", "md_doc_2copy-move-proposal.html#autotoc_md64", null ],
        [ "Phase 2: Add Generator Entry Points", "md_doc_2copy-move-proposal.html#autotoc_md65", null ],
        [ "Phase 3: Introduce Field-Level Emit Helpers", "md_doc_2copy-move-proposal.html#autotoc_md66", null ],
        [ "Phase 4: Implement Complex Resource Paths", "md_doc_2copy-move-proposal.html#autotoc_md67", null ],
        [ "Phase 5: Add Focused Tests", "md_doc_2copy-move-proposal.html#autotoc_md68", null ],
        [ "Phase 6: Update the C++ Wrapper", "md_doc_2copy-move-proposal.html#autotoc_md69", null ],
        [ "Phase 7: Document the New API", "md_doc_2copy-move-proposal.html#autotoc_md70", null ]
      ] ],
      [ "Test Plan", "md_doc_2copy-move-proposal.html#autotoc_md71", null ],
      [ "Deferred Work", "md_doc_2copy-move-proposal.html#autotoc_md72", null ]
    ] ],
    [ "Schema Evolution Guide", "md_doc_2schema-evolution.html", [
      [ "Overview", "md_doc_2schema-evolution.html#autotoc_md74", null ],
      [ "Forward Compatibility (Already Built-In)", "md_doc_2schema-evolution.html#autotoc_md75", null ],
      [ "Backward Compatibility", "md_doc_2schema-evolution.html#autotoc_md76", null ],
      [ "Safe Changes", "md_doc_2schema-evolution.html#autotoc_md77", null ],
      [ "Breaking Changes", "md_doc_2schema-evolution.html#autotoc_md78", null ],
      [ "Migration Patterns", "md_doc_2schema-evolution.html#autotoc_md79", [
        [ "Renaming a Field", "md_doc_2schema-evolution.html#autotoc_md80", null ],
        [ "Soft-Removing a Field", "md_doc_2schema-evolution.html#autotoc_md81", null ],
        [ "Adding a Required Field to Existing Data", "md_doc_2schema-evolution.html#autotoc_md82", null ],
        [ "Widening a Numeric Type", "md_doc_2schema-evolution.html#autotoc_md83", null ]
      ] ],
      [ "The <tt>@deprecated</tt> Annotation", "md_doc_2schema-evolution.html#autotoc_md84", [
        [ "Syntax", "md_doc_2schema-evolution.html#autotoc_md85", null ],
        [ "Behavior", "md_doc_2schema-evolution.html#autotoc_md86", null ],
        [ "Generated Code Example", "md_doc_2schema-evolution.html#autotoc_md87", null ]
      ] ],
      [ "Best Practices", "md_doc_2schema-evolution.html#autotoc_md88", null ],
      [ "See Also", "md_doc_2schema-evolution.html#autotoc_md89", null ]
    ] ],
    [ "Getting Started with json-gen-c", "md_docs_2GETTING__STARTED.html", [
      [ "Prerequisites", "md_docs_2GETTING__STARTED.html#autotoc_md91", null ],
      [ "1. Install json-gen-c", "md_docs_2GETTING__STARTED.html#autotoc_md92", null ],
      [ "2. Describe Your Data", "md_docs_2GETTING__STARTED.html#autotoc_md93", null ],
      [ "3. Generate Code", "md_docs_2GETTING__STARTED.html#autotoc_md94", null ],
      [ "4. Use the Generated API", "md_docs_2GETTING__STARTED.html#autotoc_md95", null ],
      [ "5. Regenerate on Schema Changes", "md_docs_2GETTING__STARTED.html#autotoc_md96", null ],
      [ "6. Learn More", "md_docs_2GETTING__STARTED.html#autotoc_md97", null ]
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
"sstr_8c.html#af45cbd228a22432910527e63c4b26b3b"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';