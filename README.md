# Proleter

A tiny **stack-based hobby language** with a virtual machine written in C and a simple compiler written in JavaScript.

This project is mainly a learning exercise. The goal is to understand how bytecode, interpreters, stack machines, memory management, native modules, and compiler structure work by building them from scratch.

It is **not** meant to be fast, stable, complete, or production-ready.

## Why

Proleter exists to learn by building:

- C programming
- stack-based VM design
- bytecode execution
- instruction dispatch
- function calls and stack frames
- arrays, objects, and heap values
- simple compiler/code generation ideas
- native/runtime module experiments

Some helper code, examples, wording, and small implementation details were assisted with AI to avoid spending too much time on non-core parts.

## What it can do

- Execute custom bytecode in a C VM
- Simple Garbage Collection
- **Emit C code and build native executables** (C backend)
- Run example programs, including Rule 110
- Use an operand stack and call frames
- Work with values such as:
  - integers
  - doubles
  - strings
  - arrays
  - objects
  - callables
  - `nil`
- Run arithmetic, comparison, logical, and jump instructions
- Use local variables and globals
- Call user functions and native functions
- Import small runtime/native modules
- Compile a small language to bytecode
- TODO: Type checking isn’t strict yet — will enforce strict types later.

## CLI

Proleter includes a simple command line interface for running and building programs.

### Usage

```sh
proleter-lang ❯ ./proleter
Commands:
  emit <input> <output>    Emit compiled output
  exe  <input> <output>    Build executable
  run  <input>             Run input file

Options:
  --target <platform>      Target platform (linux | win32)
  --emit <type>            Output type (bc | c)

Examples:
  run  main.pl
  emit main.pl out.bc --emit bc
  exe  main.pl app --target linux
```

## Project layout

```text
proleter-lang/
├─ compiler/   JavaScript compiler
├─ vm/         C virtual machine and runtime code
├─ modules/    native/runtime modules
└─ examples/   small example programs
└─ editor/     editor syntax files
````

## What it is not

* Not a serious programming language
* Not optimized
* Not stable
* Not memory-safe yet
* Not a production VM
* Not intended for real-world embedding

Things may change or break often. This project is for fun and learning.

## Goal

Keep the project small, readable, and useful for learning.

## License

MIT
