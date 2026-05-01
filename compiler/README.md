# Compiler notes

Compiler is split into 4 parts.

## `emitter.js`

Start here when you want to see the main compile flow.

Contains the `Emitter` class, compiler state, `compile()`, emit helpers, labels, and final output generation.

## `symbols.js`

Name/scope stuff lives here.

Globals, locals, functions, nested functions, module namespaces, identifier resolving, and local slot prepass are handled here.

If something says duplicate variable, unknown identifier, wrong slot, or module scope leak, check this file first.

## `types.js`

Type rules live here.

Valid types, assign checks, expression type detection, function call argument checks, and builtin method typing are handled here.

If valid code fails because of types, or bad code passes, check this file.

## `codegen.js`

AST to bytecode lives here.

Statements, expressions, loops, imports, exports, arrays, objects, member access, calls, and raw asm are emitted here.

If bytecode output looks wrong, check this file first.

## Quick direction

```txt
compile flow / output format  -> emitter.js
names, scopes, slots          -> symbols.js
type errors                   -> types.js
wrong bytecode                -> codegen.js
```

For module import bugs, usually check `symbols.js` and `codegen.js` together.
