"use strict";

const fs = require("fs");
const path = require("path");

const EmitType = {
  c: Symbol("c"),
  bc: Symbol("bytecode"),
};

const TargetPlatform = {
  linux: Symbol("linux"),
  win32: Symbol("windows"),
};

const VALID_TYPES = new Set([
  "int",
  "double",
  "bool",
  "string",
  "array",
  "object",
  "fn",
  "nil",
  "any",
]);

const STRING_CONVERTIBLE_TYPES = new Set([
  "int",
  "double",
  "bool",
  "string",
  "nil",
  "any",
])

function isAnyType(type) {
  return type === "any";
}

function isNilType(type) {
  return type === "nil";
}

function isNumericType(type) {
  return type === "int" || type === "double" || type === "any";
}

function isAssignableType(to, from) {
  if (to === from) return true;
  if (isAnyType(to) || isAnyType(from)) return true;
  if (to === "double" && from === "int") return true;
  if (to === "bool" && from === "int") return true;
  if (!isNilType(to) && isNilType(from)) return true;
  return false;
}

function numericResultType(left, right) {
  if (isAnyType(left) || isAnyType(right)) return "any";
  if (left === "double" || right === "double") return "double";
  return "int";
}

const OPCODE_ENUM = {
  nop: "OP_NOP",

  pushnil: "OP_PUSH_NIL",
  pushi: "OP_PUSH_INT",
  pushd: "OP_PUSH_DOUBLE",
  pushs: "OP_PUSH_STR",
  pushfn: "OP_PUSH_FN",

  add: "OP_ADD",
  sub: "OP_SUB",
  mul: "OP_MUL",
  div: "OP_DIV",
  and: "OP_AND",
  or: "OP_OR",
  not: "OP_NOT",
  pop: "OP_POP",
  dup: "OP_DUP",
  swap: "OP_SWAP",
  pick: "OP_PICK",
  rot: "OP_ROT",
  len: "OP_LEN",
  concat: "OP_CONCAT",
  typeof: "OP_TYPEOF",

  eq: "OP_EQ",
  neq: "OP_NEQ",
  lt: "OP_LT",
  gt: "OP_GT",
  lte: "OP_LTE",
  gte: "OP_GTE",

  jz: "OP_JZ",
  jnz: "OP_JNZ",
  jl: "OP_JL",
  jle: "OP_JLE",
  jgt: "OP_JGT",
  jgte: "OP_JGTE",
  jmp: "OP_JMP",

  enter: "OP_ENTER",
  load: "OP_LOAD",
  store: "OP_STORE",
  loadg: "OP_LOAD_GLOBAL",
  storeg: "OP_STORE_GLOBAL",

  array_new: "OP_ARRAY_NEW",
  array_push: "OP_ARRAY_PUSH",
  array_del: "OP_ARRAY_DEL",
  array_get: "OP_ARRAY_GET",
  array_set: "OP_ARRAY_SET",
  array_len: "OP_ARRAY_LEN",

  object_get: "OP_OBJECT_GET",
  object_set: "OP_OBJECT_SET",

  call: "OP_CALL",
  ret: "OP_RET",
  ret_void: "OP_RET_VOID",

  load_lib: "OP_LOAD_LIB",

  halt: "OP_HALT",
};

function cProgramTemplate(code) {
  return `
#include "vm.h"
#include <stdlib.h>

int main(int argc, char **argv) {
  VM vm = {0};
  vm.heap = (Heap){.objects = NULL,
                   .object_count = 0,
                   .bytes_allocated = 0,
                   .next_gc = VM_GC_START_THRESHOLD};
  ${code}
  vm_run_program(&vm);
  vm_gc_sweep_all(&vm);
  return 0;
}
`;
}

const cInstValFields = ["operand", "u", "d", "chars", "op_type", "source_line_num"];

function makeCInstString(i) {
  if (!i.type) throw new Error("Instruction must have opcode type");

  let txt = ".type = " + i.type;

  Object.entries(i).forEach(([key, value]) => {
    if (key === "type") return;

    if (!cInstValFields.includes(key)) {
      throw new Error("Unknown instruction field " + key + " for " + i.type);
    }

    txt += ", ." + key + " = " + value;
  });

  return "{" + txt + "}";
}

class Emitter {
  constructor({ sourcePath = null, projectRoot = process.cwd(), target = null, emit = EmitType.c } = {}) {
    this.sourcePath = sourcePath;
    this.projectRoot = projectRoot;
    this.target = target || TargetPlatform[process.platform];
    this.emitType = emit;
    this.reset();
  }

  /* =========================
     PUBLIC
     ========================= */

  errorAt(node, message) {
    if (node && node.loc && node.loc.start) {
      const { line, column } = node.loc.start;
      throw new SyntaxError(`${message} at line ${line}, column ${column}`);
    }

    throw new SyntaxError(message);
  }

  checkVarTypeIsValid(typeName, node = null) {
    if (!VALID_TYPES.has(typeName)) {
      this.errorAt(
        node,
        `Unknown type '${typeName}'. Supported types: ${[...VALID_TYPES].join(", ")}`
      );
    }
  }

  checkAssignable(to, from, node = null) {
    if (!isAssignableType(to, from)) {
      this.errorAt(node, `Cannot assign '${from}' to '${to}'`);
    }
  }

  compile(program) {
    this.reset();

    if (!program || program.type !== "Program" || !Array.isArray(program.body)) {
      throw new Error("Emitter expected Program node");
    }

    this.program = program;

    this.collectProgram(program);

    for (const meta of this.functions.values()) {
      this.prepareFunctionMeta(meta);
    }

    this.emit("pushfn", "__init");
    this.emit("call", 0);
    this.emit("halt");

    for (const meta of this.functions.values()) {
      this.compileFunction(meta);
    }

    this.compileSyntheticInit(program.body);

    if (this.emitType === EmitType.bc) {
      return this.out.join("\n");
    }

    const instString = this.out
      .map((inst) => {
        if (typeof inst.lazyRef === "string") {
          inst.u = this.getLabelPc(inst.lazyRef);
          delete inst.lazyRef;
        }

        return makeCInstString(inst);
      })
      .join(",");

    const code = `
  vm.program = malloc(sizeof(Program));
  Inst program_inst[] = {
    ${instString}
  };
  vm.program->inst = program_inst;
  vm.program->inst_count = sizeof(program_inst) / sizeof(program_inst[0]);
`;

    return cProgramTemplate(code);
  }

  reset() {
    this.out = [];
    this.labelId = 0;
    this.pc = 0;
    this.labelMap = {};
    this.sourceLineNum = 1;
    this.loopStack = [];

    this.program = null;

    this.globals = new Map();
    this.globalTypes = new Map();
    this.functions = new Map();
    this.topLevelFunctions = new Map();

    this.current = null;
  }

  /* =========================
     PROGRAM COLLECTION
     ========================= */

  collectProgram(program) {
    for (const item of program.body) {
      if (item.type === "Function") continue;
      this.collectTopLevelGlobals(item);
    }

    for (const item of program.body) {
      if (item.type === "Function") {
        if (this.topLevelFunctions.has(item.name)) {
          throw new Error(`Duplicate function '${item.name}'`);
        }

        this.topLevelFunctions.set(item.name, item.name);
      }
    }

    for (const item of program.body) {
      if (item.type === "Function") {
        this.collectFunction(item, null);
      }
    }
  }

  collectTopLevelGlobals(node) {
    if (!node) return;

    switch (node.type) {
      case "VarDecl":
        this.declareGlobal(node.name, node);
        if (node.expr) this.collectTopLevelGlobals(node.expr);
        return;

      case "Block":
        for (const item of node.body) {
          if (item.type === "Function") continue;
          this.collectTopLevelGlobals(item);
        }
        return;

      case "If":
        this.collectTopLevelGlobals(node.then);
        this.collectTopLevelGlobals(node.else);
        return;

      case "While":
        this.collectTopLevelGlobals(node.body);
        return;

      case "For":
        this.collectTopLevelGlobals(node.init);
        this.collectTopLevelGlobals(node.body);
        return;

      case "ExprStmt":
        this.collectTopLevelGlobals(node.expr);
        return;

      case "Assign":
        this.collectTopLevelGlobals(node.target);
        this.collectTopLevelGlobals(node.expr);
        return;

      case "Unary":
        this.collectTopLevelGlobals(node.expr);
        return;

      case "Binary":
        this.collectTopLevelGlobals(node.left);
        this.collectTopLevelGlobals(node.right);
        return;

      case "Call":
        this.collectTopLevelGlobals(node.callee);
        for (const a of node.args) this.collectTopLevelGlobals(a);
        return;

      case "Index":
        this.collectTopLevelGlobals(node.object);
        this.collectTopLevelGlobals(node.index);
        return;

      case "Member":
        this.collectTopLevelGlobals(node.object);
        return;

      case "ArrayLiteral":
        for (const el of node.elements) this.collectTopLevelGlobals(el);
        return;

      default:
        return;
    }
  }

  collectFunction(fnNode, parentMeta) {
    const fullName = parentMeta ? `${parentMeta.fullName}$${fnNode.name}` : fnNode.name;

    if (this.functions.has(fullName)) {
      throw new Error(`Duplicate function label '${fullName}'`);
    }

    const meta = {
      ast: fnNode,
      name: fnNode.name,
      fullName,
      parent: parentMeta,
      nestedVisible: new Map(),

      paramTypes: fnNode.params.map((p) => this.typeFromTypeNode(p.varType)),
      returnType: this.typeFromTypeNode(fnNode.returnType),

      localCount: 0,
      localTypes: new Map(),
      assignTempSlot: 0,
    };

    this.collectNestedFunctionsInBlock(fnNode.body, meta);

    this.functions.set(fullName, meta);
  }

  collectNestedFunctionsInBlock(block, parentMeta) {
    if (!block || block.type !== "Block") return;

    for (const item of block.body) {
      if (item.type === "Function") {
        const nestedFull = `${parentMeta.fullName}$${item.name}`;

        if (parentMeta.nestedVisible.has(item.name)) {
          throw new Error(`Duplicate nested function '${item.name}' in '${parentMeta.fullName}'`);
        }

        parentMeta.nestedVisible.set(item.name, nestedFull);
        this.collectFunction(item, parentMeta);
        continue;
      }

      this.collectNestedFunctionsInStmt(item, parentMeta);
    }
  }

  collectNestedFunctionsInStmt(stmt, parentMeta) {
    if (!stmt) return;

    switch (stmt.type) {
      case "Block":
        this.collectNestedFunctionsInBlock(stmt, parentMeta);
        return;

      case "If":
        this.collectNestedFunctionsInBlock(stmt.then, parentMeta);

        if (stmt.else) {
          if (stmt.else.type === "Block") this.collectNestedFunctionsInBlock(stmt.else, parentMeta);
          else if (stmt.else.type === "If") this.collectNestedFunctionsInStmt(stmt.else, parentMeta);
        }
        return;

      case "While":
        this.collectNestedFunctionsInBlock(stmt.body, parentMeta);
        return;

      case "For":
        this.collectNestedFunctionsInBlock(stmt.body, parentMeta);
        return;

      default:
        return;
    }
  }

  declareGlobal(name, node = null) {
    if (this.globals.has(name)) {
      this.errorAt(node, `Duplicate global '${name}'`);
    }

    const slot = this.globals.size;
    this.globals.set(name, slot);
    this.globalTypes.set(name, "any");
    return slot;
  }

  /* =========================
     FUNCTION LOCAL PREPASS
     ========================= */

  prepareFunctionMeta(meta) {
    const scopeStack = [];
    const localTypes = new Map();
    let nextLocalSlot = 0;

    const pushScope = () => scopeStack.push(new Map());

    const popScope = () => {
      if (scopeStack.length === 0) throw new Error("Internal: pop empty prepass scope");
      scopeStack.pop();
    };

    const currentScope = () => {
      const s = scopeStack[scopeStack.length - 1];
      if (!s) throw new Error("Internal: no current prepass scope");
      return s;
    };

    const declareLocal = (name, typeNode = null, node = null) => {
      const scope = currentScope();

      if (scope.has(name)) {
        this.errorAt(node, `Duplicate local '${name}'`);
      }

      const slot = nextLocalSlot++;
      const typeName = this.typeFromTypeNode(typeNode);

      this.checkVarTypeIsValid(typeName, node);

      scope.set(name, slot);
      localTypes.set(slot, typeName);

      return slot;
    };

    const collectExpr = (e) => {
      if (!e) return;

      switch (e.type) {
        case "Unary":
          collectExpr(e.expr);
          return;

        case "Binary":
          collectExpr(e.left);
          collectExpr(e.right);
          return;

        case "Assign":
          collectExpr(e.target);
          collectExpr(e.expr);
          return;

        case "Call":
          collectExpr(e.callee);
          for (const a of e.args) collectExpr(a);
          return;

        case "Index":
          collectExpr(e.object);
          collectExpr(e.index);
          return;

        case "Member":
          collectExpr(e.object);
          return;

        case "ArrayLiteral":
          for (const el of e.elements) collectExpr(el);
          return;

        default:
          return;
      }
    };

    const collectStmt = (s) => {
      if (!s) return;

      switch (s.type) {
        case "VarDecl":
          s._slot = declareLocal(s.name, s.varType, s);
          if (s.expr) collectExpr(s.expr);
          return;

        case "If":
          collectExpr(s.cond);
          collectBlock(s.then, true);

          if (s.else) {
            if (s.else.type === "Block") collectBlock(s.else, true);
            else if (s.else.type === "If") collectStmt(s.else);
          }
          return;

        case "While":
          collectExpr(s.cond);
          collectBlock(s.body, true);
          return;

        case "For":
          pushScope();

          if (s.init) {
            if (s.init.type === "VarDecl") collectStmt(s.init);
            else collectExpr(s.init);
          }

          if (s.cond) collectExpr(s.cond);
          if (s.step) collectExpr(s.step);

          collectBlock(s.body, true);
          popScope();
          return;

        case "Return":
          if (s.expr) collectExpr(s.expr);
          return;

        case "ExprStmt":
          collectExpr(s.expr);
          return;

        case "Assign":
          collectExpr(s.target);
          collectExpr(s.expr);
          return;

        case "Break":
        case "Continue":
        case "AsmStmt":
          return;

        case "Block":
          collectBlock(s, true);
          return;

        default:
          return;
      }
    };

    const collectBlock = (block, pushNewScope = true) => {
      if (!block || block.type !== "Block") {
        throw new Error("Expected Block during local prepass");
      }

      if (pushNewScope) pushScope();

      for (const item of block.body) {
        if (item.type === "Function") continue;
        collectStmt(item);
      }

      if (pushNewScope) popScope();
    };

    pushScope();
    for (const typeName of meta.paramTypes) {
      this.checkVarTypeIsValid(typeName, meta.ast);
    }

    this.checkVarTypeIsValid(meta.returnType, meta.ast);
    for (const p of meta.ast.params) {
      if (p.type !== "Param") {
        this.errorAt(p, "Expected Param node");
      }

      p._slot = declareLocal(p.name, p.varType, p);
    }

    collectBlock(meta.ast.body, false);

    meta.assignTempSlot = nextLocalSlot++;
    localTypes.set(meta.assignTempSlot, "any");

    meta.localCount = nextLocalSlot;
    meta.localTypes = localTypes;
  }

  /* =========================
     EMIT CORE
     ========================= */

  emit(opcode, ...operands) {
    if (!OPCODE_ENUM[opcode]) {
      throw new Error("Unknown op code: " + opcode);
    }

    if (this.emitType === EmitType.bc) {
      const optext = operands.length ? " " + operands.join(" ") : "";
      this.out.push("  " + opcode + optext);
      return;
    }

    this.emitCInst(opcode, operands);
  }

  emitCInst(opcode, operands) {
    const type = OPCODE_ENUM[opcode];
    let inst;

    switch (opcode) {
      case "pushnil":
      case "nop":
      case "dup":
      case "swap":
      case "halt":
      case "ret":
      case "ret_void":
      case "add":
      case "sub":
      case "mul":
      case "div":
      case "and":
      case "or":
      case "not":
      case "pop":
      case "len":
      case "concat":
      case "typeof":
      case "eq":
      case "neq":
      case "lt":
      case "gt":
      case "lte":
      case "gte":
      case "array_new":
      case "array_push":
      case "array_del":
      case "array_get":
      case "array_set":
      case "array_len":
      case "object_get":
      case "object_set":
      case "load_lib":
        inst = { type };
        break;

      case "pushi":
        this.expectOperands(opcode, operands, 1);
        inst = { type, operand: operands[0] };
        break;

      case "pushd":
        this.expectOperands(opcode, operands, 1);
        inst = { type, d: operands[0] };
        break;

      case "pushs":
        this.expectOperands(opcode, operands, 1);
        inst = { type, chars: operands[0] };
        break;

      case "pushfn":
        this.expectOperands(opcode, operands, 1);
        inst = { type, lazyRef: operands[0] };
        break;

      case "jz":
      case "jnz":
      case "jl":
      case "jle":
      case "jgt":
      case "jgte":
      case "jmp":
        this.expectOperands(opcode, operands, 1);
        inst = { type, lazyRef: operands[0] };
        break;

      case "enter":
      case "load":
      case "store":
      case "loadg":
      case "storeg":
      case "pick":
      case "rot":
        this.expectOperands(opcode, operands, 1);
        inst = { type, u: operands[0] };
        break;

      case "call":
        this.expectOperands(opcode, operands, 1);
        inst = { type, u: operands[0] };
        break;

      default:
        throw new Error("Unimplemented op code: " + opcode);
    }

    inst.source_line_num = this.sourceLineNum;
    this.out.push(inst);
    this.pc += 1;
  }

  expectOperands(opcode, operands, n) {
    if (operands.length !== n) {
      throw new Error(`Op code ${opcode} must have ${n} operand(s), got ${operands.length}`);
    }
  }

  label(name) {
    if (this.emitType === EmitType.bc) {
      this.out.push(name + ":");
      return;
    }

    this.labelMap[name] = this.pc;
  }

  newLabel(prefix = "L") {
    return `${prefix}_${this.labelId++}`;
  }

  getLabelPc(name) {
    if (!Object.prototype.hasOwnProperty.call(this.labelMap, name)) {
      throw new Error("Label " + name + " does not exist");
    }

    return this.labelMap[name];
  }

  /* =========================
     CODEGEN CONTEXT / SCOPES
     ========================= */

  makeFunctionCodegenContext(meta) {
    return {
      kind: "function",
      fullName: meta.fullName,
      name: meta.name,
      functionMeta: meta,
      scopeStack: [],
      localTypes: meta.localTypes,
      localCount: meta.localCount,
      assignTempSlot: meta.assignTempSlot,
    };
  }

  makeInitCodegenContext() {
    const localTypes = new Map();
    localTypes.set(0, "any");

    return {
      kind: "init",
      fullName: "__init",
      name: "__init",
      functionMeta: null,
      scopeStack: [],
      localTypes,
      localCount: 1,
      assignTempSlot: 0,
    };
  }

  pushScope() {
    if (!this.current || this.current.kind !== "function") return;
    this.current.scopeStack.push(new Map());
  }

  popScope() {
    if (!this.current || this.current.kind !== "function") return;

    if (this.current.scopeStack.length === 0) {
      throw new Error("popScope on empty scope stack");
    }

    this.current.scopeStack.pop();
  }

  currentScope() {
    if (!this.current || this.current.kind !== "function") {
      throw new Error("No active function scope");
    }

    const scope = this.current.scopeStack[this.current.scopeStack.length - 1];

    if (!scope) throw new Error("No current scope");

    return scope;
  }

  bindLocal(name, slot, node = null) {
    const scope = this.currentScope();

    if (scope.has(name)) {
      this.errorAt(node, `Duplicate local '${name}'`);
    }

    scope.set(name, slot);
  }

  resolveLocal(name) {
    if (!this.current || this.current.kind !== "function") return null;

    for (let i = this.current.scopeStack.length - 1; i >= 0; i--) {
      const scope = this.current.scopeStack[i];

      if (scope.has(name)) return scope.get(name);
    }

    return null;
  }

  resolveVar(name, node = null) {
    const localSlot = this.resolveLocal(name);

    if (localSlot != null) {
      return {
        kind: "local",
        slot: localSlot,
        type: this.current.localTypes.get(localSlot) || "any",
      };
    }

    if (this.globals.has(name)) {
      return {
        kind: "global",
        slot: this.globals.get(name),
        type: this.globalTypes.get(name) || "any",
      };
    }

    this.errorAt(node, `Unknown identifier '${name}'`);
  }

  canResolveVar(name) {
    try {
      this.resolveVar(name);
      return true;
    } catch {
      return false;
    }
  }

  resolveFunction(name) {
    if (this.current && this.current.functionMeta) {
      let meta = this.current.functionMeta;

      while (meta) {
        if (meta.nestedVisible.has(name)) {
          return meta.nestedVisible.get(name);
        }

        meta = meta.parent;
      }
    }

    if (this.topLevelFunctions.has(name)) {
      return this.topLevelFunctions.get(name);
    }

    throw new Error(`Unknown function '${name}'`);
  }

  canResolveFunction(name) {
    try {
      this.resolveFunction(name);
      return true;
    } catch {
      return false;
    }
  }

  emitLoadName(name, node = null) {
    const ref = this.resolveVar(name, node);

    if (ref.kind === "local") this.emit("load", ref.slot);
    else this.emit("loadg", ref.slot);
  }

  emitStoreName(name, node = null) {
    const ref = this.resolveVar(name, node);

    if (ref.kind === "local") this.emit("store", ref.slot);
    else this.emit("storeg", ref.slot);
  }

  typeFromTypeNode(t) {
    if (!t) return "any";
    if (t.type === "Type") return t.name;
    if (t.type === "ArrayType") return "array";
    return "any";
  }
  resolveFunctionMeta(name, node = null) {
    const fullName = this.resolveFunction(name);

    const meta = this.functions.get(fullName);
    if (!meta) {
      this.errorAt(node, `Unknown function '${name}'`);
    }

    return meta;
  }

  callExprType(callNode) {
    // Only direct named function calls can be checked strictly for now.
    // Example: add(1, 2)
    //
    // Dynamic calls like:
    //   var f: fn = add
    //   f(1, 2)
    // stay "any" for now.
    if (callNode.callee.type !== "Identifier") {
      return "any";
    }

    if (!this.canResolveFunction(callNode.callee.name)) {
      return "any";
    }

    const meta = this.resolveFunctionMeta(callNode.callee.name, callNode.callee);

    if (callNode.args.length !== meta.paramTypes.length) {
      this.errorAt(
        callNode,
        `Function '${callNode.callee.name}' expects ${meta.paramTypes.length} argument(s), got ${callNode.args.length}`
      );
    }

    for (let i = 0; i < callNode.args.length; i++) {
      const expectedType = meta.paramTypes[i];
      const actualType = this.exprType(callNode.args[i]);

      this.checkAssignable(expectedType, actualType, callNode.args[i]);
    }

    return meta.returnType;
  }
  exprType(e) {
    if (!e) return "any";

    switch (e.type) {
      case "String":
        return "string";

      case "Int":
        return "int";

      case "Double":
        return "double";

      case "Boolean":
        return "bool";

      case "Nil":
        return "nil";

      case "ArrayLiteral":
        return "array";

      case "ImportExpr":
        return "object";

      case "Identifier": {
        if (this.canResolveVar(e.name)) {
          return this.resolveVar(e.name, e).type;
        }

        if (this.canResolveFunction(e.name)) {
          return "fn";
        }

        this.errorAt(e, `Unknown identifier '${e.name}'`);
      }

      case "Assign":
        return this.assignExprType(e);

      case "Unary":
        return this.unaryExprType(e);

      case "Binary":
        return this.binaryExprType(e);

      case "Call":
        return this.callExprType(e);

      case "Member":
        return "any";

      case "Index": {
        const objectType = this.exprType(e.object);
        const indexType = this.exprType(e.index);

        if (!isAssignableType("array", objectType) && !isAssignableType("string", objectType)) {
          this.errorAt(e.object, `Cannot index value of type '${objectType}'`);
        }

        if (!isAssignableType("int", indexType)) {
          this.errorAt(e.index, `Index must be int, got '${indexType}'`);
        }

        return "any";
      }

      default:
        return "any";
    }
  }

  assignExprType(a) {
    const rhsType = this.exprType(a.expr);

    if (a.target.type === "Identifier") {
      const ref = this.resolveVar(a.target.name, a.target);
      this.checkAssignable(ref.type, rhsType, a);
      return ref.type;
    }

    if (a.target.type === "Index" || a.target.type === "Member") {
      return rhsType;
    }

    this.errorAt(a.target, "Bad assignment target");
  }

  unaryExprType(e) {
    const exprType = this.exprType(e.expr);

    if (e.op === "-") {
      if (!isNumericType(exprType)) {
        this.errorAt(e, `Unary '-' requires number, got '${exprType}'`);
      }

      return exprType;
    }

    if (e.op === "!") {
      if (!isAssignableType("bool", exprType)) {
        this.errorAt(e, `Unary '!' requires bool, got '${exprType}'`);
      }

      return "bool";
    }

    this.errorAt(e, `Unsupported unary operator: ${e.op}`);
  }

  binaryExprType(e) {
    const leftType = this.exprType(e.left);
    const rightType = this.exprType(e.right);

    switch (e.op) {
      case "+": {
        if (leftType === "string" || rightType === "string") {
          if (!STRING_CONVERTIBLE_TYPES.has(leftType) || !STRING_CONVERTIBLE_TYPES.has(rightType)) {
            this.errorAt(e, `Operator '+' cannot mix '${leftType}' and '${rightType}'`);
          }

          return "string";
        }

        if (!isNumericType(leftType) || !isNumericType(rightType)) {
          this.errorAt(e, `Operator '+' requires numbers or strings, got '${leftType}' and '${rightType}'`);
        }

        return numericResultType(leftType, rightType);
      }

      case "-":
      case "*":
      case "/":
      case "%": {
        if (!isNumericType(leftType) || !isNumericType(rightType)) {
          this.errorAt(e, `Operator '${e.op}' requires numbers, got '${leftType}' and '${rightType}'`);
        }

        return numericResultType(leftType, rightType);
      }

      case "==":
      case "!=":
        return "bool";

      case "<":
      case ">":
      case "<=":
      case ">=": {
        if (!isNumericType(leftType) || !isNumericType(rightType)) {
          this.errorAt(e, `Operator '${e.op}' requires numbers, got '${leftType}' and '${rightType}'`);
        }

        return "bool";
      }

      case "&&":
      case "||": {
        if (!isAssignableType("bool", leftType) || !isAssignableType("bool", rightType)) {
          this.errorAt(e, `Operator '${e.op}' requires bool values, got '${leftType}' and '${rightType}'`);
        }

        return "bool";
      }

      default:
        this.errorAt(e, `Unknown binary operator '${e.op}'`);
    }
  }

  /* =========================
     FUNCTION / INIT CODEGEN
     ========================= */

  compileFunction(meta) {
    const ctx = this.makeFunctionCodegenContext(meta);

    if (this.emitType === EmitType.bc) this.out.push("");
    this.label(meta.fullName);

    this.current = ctx;

    this.pushScope();

    for (const p of meta.ast.params) {
      if (p.type !== "Param") {
        this.errorAt(p, "Expected Param node");
      }

      this.bindLocal(p.name, p._slot, p);
    }

    this.emit("enter", ctx.localCount);
    this.compileBlock(meta.ast.body, false);

    this.emit("pushnil");
    this.emit("ret");

    this.current = null;
  }

  compileSyntheticInit(items) {
    const initItems = items.filter((item) => item.type !== "Function");
    const ctx = this.makeInitCodegenContext();

    if (this.emitType === EmitType.bc) this.out.push("");
    this.label("__init");

    this.current = ctx;
    this.emit("enter", ctx.localCount);

    this.compileBlock({ type: "Block", body: initItems }, false);

    if (this.topLevelFunctions.has("main")) {
      this.emit("pushfn", this.topLevelFunctions.get("main"));
      this.emit("call", 0);
      this.emit("ret");
    }

    this.emit("pushnil");
    this.emit("ret");

    this.current = null;
  }

  /* =========================
     STATEMENTS / BLOCKS
     ========================= */

  setSourceLine(node) {
    if (node && node.loc && node.loc.start) {
      const { line } = node.loc.start;
      this.sourceLineNum = line;
    }
  }

  compileBlock(block, pushNewScope = true) {
    if (!block || block.type !== "Block") {
      throw new Error("Expected Block");
    }

    if (pushNewScope && this.current && this.current.kind === "function") {
      this.pushScope();
    }

    for (const item of block.body) {
      if (item.type === "Function") continue;
      this.compileStmt(item);
    }

    if (pushNewScope && this.current && this.current.kind === "function") {
      this.popScope();
    }
  }

  compileStmt(s) {
    this.setSourceLine(s);

    switch (s.type) {
      case "VarDecl":
        return this.compileVarDecl(s);

      case "If":
        return this.compileIf(s);

      case "While":
        return this.compileWhile(s);

      case "For":
        return this.compileFor(s);

      case "Break":
        return this.compileBreak();

      case "Continue":
        return this.compileContinue();

      case "Return": {
        const expectedReturnType =
          this.current && this.current.functionMeta
            ? this.current.functionMeta.returnType
            : "any";

        const actualReturnType = s.expr ? this.exprType(s.expr) : "nil";

        this.checkAssignable(expectedReturnType, actualReturnType, s);

        if (s.expr) this.compileExpr(s.expr);
        else this.emit("pushnil");

        this.emit("ret");
        return;
      }

      case "AsmStmt":
        return this.compileAsmStmt(s);

      case "Assign":
        this.compileAssignExpr(s);
        this.emit("pop");
        return;

      case "ExprStmt":
        this.compileExpr(s.expr);
        this.emit("pop");
        return;

      case "Block":
        this.compileBlock(s, true);
        return;

      default:
        this.errorAt(s, `Unsupported statement: ${s.type}`);
    }
  }

  compileVarDecl(s) {
    let slot = null;
    const isFunctionLocal = !!(this.current && this.current.kind === "function");

    const declaredType = this.typeFromTypeNode(s.varType);
    this.checkVarTypeIsValid(declaredType, s);

    if (isFunctionLocal) {
      if (typeof s._slot !== "number") {
        throw new Error(`Missing precomputed slot for local '${s.name}'`);
      }

      this.bindLocal(s.name, s._slot, s);
      slot = s._slot;
    } else {
      this.globalTypes.set(s.name, declaredType);
    }

    if (s.expr) {
      const actualType = this.exprType(s.expr);
      this.checkAssignable(declaredType, actualType, s);
      this.compileExpr(s.expr);
    } else if (s.varType && s.varType.type === "ArrayType") {
      this.emitAllocTypedArray(s.varType);
    } else if (declaredType === "string") {
      this.emit("pushs", JSON.stringify(""));
    } else {
      this.emit("pushnil");
    }

    if (isFunctionLocal) {
      this.emit("store", slot);
    } else {
      this.emitStoreName(s.name, s);
    }
  }

  emitAllocTypedArray(typeNode) {
    const dims = this.getArrayDims(typeNode);
    this.emitAllocNestedArray(dims, 0);
  }

  getArrayDims(typeNode) {
    if (!typeNode || typeNode.type !== "ArrayType") return [1];

    const dims = (typeNode.dims || []).map((d) => {
      const v = Number(d);
      return Number.isFinite(v) && v > 0 ? v : 1;
    });

    return dims.length ? dims : [1];
  }

  emitAllocNestedArray(dims, depth) {
    const size = dims[depth] ?? 1;

    this.emit("pushi", size);
    this.emit("array_new");

    if (depth === dims.length - 1) return;

    for (let i = 0; i < size; i++) {
      this.emit("dup");
      this.emit("pushi", i);
      this.emitAllocNestedArray(dims, depth + 1);
      this.emit("array_set");
      this.emit("pop");
    }
  }

  compileIf(s) {
    const L_else = this.newLabel("else");
    const L_end = this.newLabel("endif");

    this.compileExpr(s.cond);
    this.emit("jz", L_else);

    this.compileBlock(s.then, true);

    if (s.else) this.emit("jmp", L_end);

    this.label(L_else);

    if (s.else) {
      if (s.else.type === "Block") this.compileBlock(s.else, true);
      else if (s.else.type === "If") this.compileIf(s.else);
      else throw new Error("Bad else node: " + s.else.type);
    }

    this.label(L_end);
  }

  compileWhile(s) {
    const L_cond = this.newLabel("while_cond");
    const L_end = this.newLabel("while_end");

    this.loopStack.push({ breakLabel: L_end, continueLabel: L_cond });

    this.label(L_cond);
    this.compileExpr(s.cond);
    this.emit("jz", L_end);

    this.compileBlock(s.body, true);
    this.emit("jmp", L_cond);

    this.label(L_end);
    this.loopStack.pop();
  }

  compileFor(s) {
    const L_cond = this.newLabel("for_cond");
    const L_step = this.newLabel("for_step");
    const L_end = this.newLabel("for_end");

    if (this.current && this.current.kind === "function") {
      this.pushScope();
    }

    if (s.init) {
      if (s.init.type === "VarDecl") {
        this.compileVarDecl(s.init);
      } else {
        this.compileExpr(s.init);
        this.emit("pop");
      }
    }

    this.loopStack.push({ breakLabel: L_end, continueLabel: L_step });

    this.label(L_cond);

    if (s.cond) {
      this.compileExpr(s.cond);
      this.emit("jz", L_end);
    }

    this.compileBlock(s.body, true);

    this.label(L_step);

    if (s.step) {
      this.compileExpr(s.step);
      this.emit("pop");
    }

    this.emit("jmp", L_cond);

    this.label(L_end);
    this.loopStack.pop();

    if (this.current && this.current.kind === "function") {
      this.popScope();
    }
  }

  compileBreak() {
    const top = this.loopStack[this.loopStack.length - 1];

    if (!top) throw new Error("break used outside loop");

    this.emit("jmp", top.breakLabel);
  }

  compileContinue() {
    const top = this.loopStack[this.loopStack.length - 1];

    if (!top) throw new Error("continue used outside loop");

    this.emit("jmp", top.continueLabel);
  }

  /* =========================
     EXPRESSIONS
     ========================= */

  resolveModuleImportPath(spec, node = null) {
    if (!this.sourcePath) {
      throw new Error("sourcePath is required for imports");
    }

    const currentDir = path.dirname(this.sourcePath);

    const localPath = path.resolve(currentDir, spec);
    const platformExt = this.target === TargetPlatform.linux ? ".so" : ".dll";

    if (fs.existsSync(localPath + platformExt)) {
      return path.normalize(path.relative(this.projectRoot, localPath));
    }

    const modulePath = path.resolve(this.projectRoot, "modules", spec);

    if (fs.existsSync(modulePath + platformExt)) {
      return path.normalize(path.relative(this.projectRoot, modulePath));
    }

    this.errorAt(node, `Import not found: '${spec}'`);
  }

  compileExpr(e) {
    if (!e) throw new Error("Unsupported expression: undefined node");

    this.setSourceLine(e);

    switch (e.type) {
      case "Nil":
        this.emit("pushnil");
        return;

      case "Boolean":
        this.emit("pushi", e.value ? 1 : 0);
        return;

      case "Int":
        this.emit("pushi", e.value);
        return;

      case "Double":
        this.emit("pushd", e.value);
        return;

      case "String":
        this.emit("pushs", JSON.stringify(e.value));
        return;

      case "ImportExpr": {
        const resolved = this.resolveModuleImportPath(e.path, e);
        this.emit("pushs", JSON.stringify(resolved));
        this.emit("load_lib");
        return;
      }

      case "Identifier": {
        if (this.canResolveVar(e.name)) {
          this.emitLoadName(e.name, e);
          return;
        }

        if (this.canResolveFunction(e.name)) {
          this.emit("pushfn", this.resolveFunction(e.name));
          return;
        }

        this.errorAt(e, `Unknown identifier '${e.name}'`);
      }

      case "Unary":
        return this.compileUnary(e);

      case "Binary":
        return this.compileBinary(e);

      case "Assign":
        return this.compileAssignExpr(e);

      case "Call":
        return this.compileCall(e);

      case "Member":
        this.compileExpr(e.object);
        this.emit("pushs", JSON.stringify(e.property));
        this.emit("object_get");
        return;

      case "Index":
        this.exprType(e);
        this.compileExpr(e.object);
        this.compileExpr(e.index);
        this.emit("array_get");
        return;

      case "ArrayLiteral":
        return this.compileArrayLiteral(e);

      default:
        this.errorAt(e, `Unsupported expression: ${e.type}`);
    }
  }

  compileUnary(e) {
    this.unaryExprType(e);

    if (e.op === "-") {
      this.emit("pushi", 0);
      this.compileExpr(e.expr);
      this.emit("sub");
      return;
    }

    if (e.op === "!") {
      this.compileExpr(e.expr);
      this.emit("pushi", 0);
      this.emit("eq");
      return;
    }

    throw new Error("Unsupported unary operator: " + e.op);
  }

  compileBinary(e) {
    const resultType = this.binaryExprType(e);

    if (e.op === "&&") return this.emitLogicalAnd(e.left, e.right);
    if (e.op === "||") return this.emitLogicalOr(e.left, e.right);

    this.compileExpr(e.left);
    this.compileExpr(e.right);

    if (e.op === "+") {
      this.emit(resultType === "string" ? "concat" : "add");
      return;
    }

    this.emit(this.binOp(e.op));
  }

  emitLogicalAnd(left, right) {
    const L_false = this.newLabel("and_false");
    const L_end = this.newLabel("and_end");

    this.compileExpr(left);
    this.emit("jz", L_false);

    this.compileExpr(right);
    this.emit("jz", L_false);

    this.emit("pushi", 1);
    this.emit("jmp", L_end);

    this.label(L_false);
    this.emit("pushi", 0);

    this.label(L_end);
  }

  emitLogicalOr(left, right) {
    const L_check = this.newLabel("or_check");
    const L_false = this.newLabel("or_false");
    const L_end = this.newLabel("or_end");

    this.compileExpr(left);
    this.emit("jz", L_check);

    this.emit("pushi", 1);
    this.emit("jmp", L_end);

    this.label(L_check);
    this.compileExpr(right);
    this.emit("jz", L_false);

    this.emit("pushi", 1);
    this.emit("jmp", L_end);

    this.label(L_false);
    this.emit("pushi", 0);

    this.label(L_end);
  }

  compileCall(e) {
    this.callExprType(e);

    this.compileExpr(e.callee);

    for (const arg of e.args) {
      this.compileExpr(arg);
    }

    this.emit("call", e.args.length);
  }

  compileArrayLiteral(node) {
    this.emit("pushi", 0);
    this.emit("array_new");

    for (const el of node.elements) {
      this.compileExpr(el);
      this.emit("array_push");
    }
  }

  splitIndexChain(target) {
    const indices = [];
    let cur = target;

    while (cur && cur.type === "Index") {
      indices.push(cur.index);
      cur = cur.object;
    }

    indices.reverse();

    return { base: cur, indices };
  }

  compileAssignExpr(a) {
    const t = a.target;

    if (t.type === "Identifier") {
      const ref = this.resolveVar(t.name, t);
      const rhsType = this.exprType(a.expr);

      this.checkAssignable(ref.type, rhsType, a);

      this.compileExpr(a.expr);
      this.emit("dup");
      this.emitStoreName(t.name, t);
      return;
    }

    if (t.type === "Index") {
      return this.compileIndexAssignExpr(t, a.expr);
    }

    if (t.type === "Member") {
      return this.compileMemberAssignExpr(t, a.expr);
    }

    this.errorAt(t, "Bad assignment target");
  }

  compileIndexAssignExpr(target, rhs) {
    const { base, indices } = this.splitIndexChain(target);

    if (!base || !indices.length) {
      throw new Error("Bad index assignment target");
    }

    const baseType = this.exprType(base);

    if (!isAssignableType("array", baseType)) {
      this.errorAt(base, `Cannot assign by index on type '${baseType}'`);
    }

    for (const index of indices) {
      const indexType = this.exprType(index);

      if (!isAssignableType("int", indexType)) {
        this.errorAt(index, `Index must be int, got '${indexType}'`);
      }
    }

    const tmp = this.current.assignTempSlot;

    this.compileExpr(base);

    for (let i = 0; i < indices.length - 1; i++) {
      this.compileExpr(indices[i]);
      this.emit("array_get");
    }

    this.compileExpr(indices[indices.length - 1]);
    this.compileExpr(rhs);

    this.emit("dup");
    this.emit("store", tmp);

    this.emit("array_set");
    this.emit("pop");

    this.emit("load", tmp);
  }

  compileMemberAssignExpr(target, rhs) {
    const tmp = this.current.assignTempSlot;

    this.compileExpr(target.object);
    this.emit("pushs", JSON.stringify(target.property));
    this.compileExpr(rhs);

    this.emit("dup");
    this.emit("store", tmp);

    this.emit("object_set");
    this.emit("pop");

    this.emit("load", tmp);
  }

  binOp(op) {
    const m = {
      "-": "sub",
      "*": "mul",
      "/": "div",
      "%": "div",
      "==": "eq",
      "!=": "neq",
      "<": "lt",
      ">": "gt",
      "<=": "lte",
      ">=": "gte",
    };

    if (!m[op]) throw new Error("Unknown binary op: " + op);

    return m[op];
  }

  /* =========================
     RAW ASM
     ========================= */

  compileAsmStmt(s) {
    const lines = s.code
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter((line) => line && !line.startsWith("#"));

    for (const line of lines) {
      this.emitAsmLine(line);
    }
  }

  emitAsmLine(line) {
    if (this.emitType === EmitType.bc) {
      this.out.push("  " + line);
      return;
    }

    const parts = this.splitAsmOperands(line);
    if (parts.length === 0) return;

    const [opcode, ...rawOperands] = parts;
    const operands = rawOperands.map((x) => this.parseAsmOperand(x));

    this.emit(opcode, ...operands);
  }

  splitAsmOperands(line) {
    const re = /"([^"\\]|\\.)*"|\S+/g;
    return line.match(re) || [];
  }

  parseAsmOperand(token) {
    const t = token.trim();

    if (/^".*"$/.test(t)) return t;
    if (/^-?\d+$/.test(t)) return Number(t);
    if (/^-?\d+\.\d+$/.test(t)) return Number(t);

    return t;
  }
}

module.exports = { Emitter, EmitType, TargetPlatform };
