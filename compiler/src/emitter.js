"use strict";

const { EmitType, TargetPlatform, OPCODE_ENUM } = require("./types");
const { installTypes } = require("./types");
const { installSymbols } = require("./symbols");
const { installCodegen } = require("./codegen");

const C_INST_VALUE_FIELDS = ["operand", "u", "d", "chars", "op_type", "source_line_num"];

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

function makeCInstString(inst) {
  if (!inst.type) throw new Error("Instruction must have opcode type");

  let txt = ".type = " + inst.type;

  Object.entries(inst).forEach(([key, value]) => {
    if (key === "type") return;

    if (!C_INST_VALUE_FIELDS.includes(key)) {
      throw new Error("Unknown instruction field " + key + " for " + inst.type);
    }

    txt += ", ." + key + " = " + value;
  });

  return "{" + txt + "}";
}

function buildCProgram(emitter) {
  const instString = emitter.out
    .map((inst) => {
      if (typeof inst.lazyRef === "string") {
        inst.u = emitter.getLabelPc(inst.lazyRef);
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

function installEmitCore(Emitter) {
  Emitter.prototype.emit = function emit(opcode, ...operands) {
    if (!OPCODE_ENUM[opcode]) {
      throw new Error("Unknown op code: " + opcode);
    }

    this.validateEmitOperands(opcode, operands);

    if (this.emitType === EmitType.bc) {
      const optext = operands.length ? " " + operands.join(" ") : "";
      this.out.push("  " + opcode + optext);
      return;
    }

    this.emitCInst(opcode, operands);
  };

  Emitter.prototype.emitCInst = function emitCInst(opcode, operands) {
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
      case "object_new":
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
  };

  Emitter.prototype.validateEmitOperands = function validateEmitOperands(opcode, operands) {
    const operandCount = {
      pushi: 1,
      pushd: 1,
      pushs: 1,
      pushfn: 1,
      jz: 1,
      jnz: 1,
      jl: 1,
      jle: 1,
      jgt: 1,
      jgte: 1,
      jmp: 1,
      enter: 1,
      load: 1,
      store: 1,
      loadg: 1,
      storeg: 1,
      pick: 1,
      rot: 1,
      call: 1,
    };

    const expected = operandCount[opcode] || 0;
    this.expectOperands(opcode, operands, expected);

    for (const operand of operands) {
      if (operand === undefined || operand === null || operand === "") {
        throw new Error("Op code " + opcode + " has invalid operand " + String(operand));
      }
    }
  };

  Emitter.prototype.expectOperands = function expectOperands(opcode, operands, n) {
    if (operands.length !== n) {
      throw new Error("Op code " + opcode + " must have " + n + " operand(s), got " + operands.length);
    }
  };

  Emitter.prototype.label = function label(name) {
    if (this.emitType === EmitType.bc) {
      this.out.push(name + ":");
      return;
    }

    this.labelMap[name] = this.pc;
  };

  Emitter.prototype.newLabel = function newLabel(prefix = "L") {
    return `${prefix}_${this.labelId++}`;
  };

  Emitter.prototype.getLabelPc = function getLabelPc(name) {
    if (!Object.prototype.hasOwnProperty.call(this.labelMap, name)) {
      throw new Error("Label " + name + " does not exist");
    }

    return this.labelMap[name];
  };
}

class Emitter {
  constructor({
    sourcePath = null,
    projectRoot = process.cwd(),
    target = null,
    emit = EmitType.c,
    parse = null,
    parser = null,
  } = {}) {
    this.sourcePath = sourcePath;
    this.projectRoot = projectRoot;
    this.target = target || TargetPlatform[process.platform];
    this.emitType = emit;
    this.parse = parse || (parser && parser.parse ? parser.parse.bind(parser) : null);
    this.reset();
  }

  reset() {
    this.out = [];
    this.labelId = 0;
    this.pc = 0;
    this.labelMap = {};
    this.sourceLineNum = 1;
    this.loopStack = [];

    this.program = null;

    this.functions = new Map();
    this.modules = new Map();
    this.moduleOrder = [];
    this.moduleId = 0;
    this.nextGlobalSlot = 0;

    this.rootModule = null;
    this.currentModule = null;
    this.current = null;
  }

  compile(program) {
    this.reset();

    if (!program || program.type !== "Program" || !Array.isArray(program.body)) {
      throw new Error("Emitter expected Program node");
    }

    this.program = program;
    this.rootModule = this.createModule({
      id: "main",
      sourcePath: this.sourcePath,
      ast: program,
      isRoot: true,
    });

    this.collectModule(this.rootModule);

    for (const meta of this.functions.values()) {
      this.prepareFunctionMeta(meta);
    }

    this.emit("pushfn", this.rootModule.initLabel);
    this.emit("call", 0);
    this.emit("halt");

    for (const meta of this.functions.values()) {
      this.compileFunction(meta);
    }

    for (const module of this.moduleOrder) {
      this.compileModuleInit(module);
    }

    if (this.emitType === EmitType.bc) {
      return this.out.join("\n");
    }

    return buildCProgram(this);
  }
}

installEmitCore(Emitter);
installTypes(Emitter);
installSymbols(Emitter);
installCodegen(Emitter);

module.exports = { Emitter, EmitType, TargetPlatform };
