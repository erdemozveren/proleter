# Tiny C VM

This is a **small stack-based virtual machine written in C**, along with a simple compiler for a hobby language.

This project exists mainly as a **learning exercise**. The goal is to understand how virtual machines, bytecode, and interpreters work at a low level, while also getting more comfortable writing real C code.

It is not intended to be fast, complete, or production-ready.

## Why this project exists

- To learn **C** by building something concrete
- To understand:
  - bytecode formats
  - stack-based execution
  - instruction dispatch
  - control flow and jumps
- To experiment without overengineering

Some helper parts (like string handling and escape sequences such as `\n`) and example programs were assisted using ChatGPT, mainly to avoid spending too much time on non-core details.

## What it can do

- Stack-based virtual machine
- Basic value types:
  - arrays
  - integers
  - doubles
  - strings
  - `nil`
- Arithmetic and comparison operations
- Local variables
- Control flow (conditionals, jumps)
- Single-pass compilation with backpatching
- Example programs included in the `examples/` directory
- Can run **Rule 110**, meaning the VM is effectively **Turing complete** (examples/rule110.bc have step limiter to see output clearly)

## Project layout (high level)

- **VM**  
  Executes bytecode using an operand stack and instruction pointer

- **Compiler**  (TODO)
  A small compiler for a hobby language that emits bytecode directly

- **Examples**  
  Small programs used for testing and experimentation

## What it is not

- Not a full language implementation
- Not optimized
- Not stable
- Not intended for embedding or real-world use

Expect things to change or break.

## Goals

- Keep the code readable and small
- Learn more about:
  - C memory management
  - VM design
  - compiler structure
- Improve correctness step by step

This project is primarily about learning and experimentation.

## License

MIT
