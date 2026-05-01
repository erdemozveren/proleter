"use strict";

const fs = require("fs");
const path = require("path");
const { EmitType, TargetPlatform, isAssignableType } = require("./types");

function installStatementCompiler(Emitter) {
  Emitter.prototype.compileFunction = function compileFunction(meta) {
    const ctx = this.makeFunctionCodegenContext(meta);

    if (this.emitType === EmitType.bc) this.out.push("");
    this.label(meta.fullName);

    const prevModule = this.currentModule;
    this.currentModule = meta.module;
    this.current = ctx;
    this.pushScope();

    for (const param of meta.ast.params) {
      if (param.type !== "Param") this.errorAt(param, "Expected Param node");
      this.bindLocal(param.name, param._slot, param);
    }

    this.emit("enter", ctx.localCount);
    this.compileBlock(meta.ast.body, false);

    this.emit("pushnil");
    this.emit("ret");

    this.current = null;
    this.currentModule = prevModule;
  };

  Emitter.prototype.compileModuleInit = function compileModuleInit(module) {
    const initItems = module.ast.body.filter((item) => item.type !== "Function");
    const ctx = this.makeInitCodegenContext(module);

    if (this.emitType === EmitType.bc) this.out.push("");
    this.label(module.initLabel);

    const prevModule = this.currentModule;
    this.currentModule = module;
    this.current = ctx;
    this.pushScope();
    this.emit("enter", ctx.localCount);

    if (module.isRoot) {
      this.emitInitModuleCacheFlags();
    }

    this.compileBlock({ type: "Block", body: initItems }, false);

    if (module.isRoot) {
      this.emit("pushnil");
      this.emit("ret");
    } else if (!ctx.didExport) {
      this.emit("pushi", 0);
      this.emit("object_new");
      this.emit("dup");
      this.emit("storeg", module.cacheSlot);
      this.emit("pushi", 1);
      this.emit("storeg", module.loadedSlot);
      this.emit("ret");
    }

    this.popScope();
    this.current = null;
    this.currentModule = prevModule;
  };

  Emitter.prototype.compileBlock = function compileBlock(block, pushNewScope = true) {
    if (!block || block.type !== "Block") {
      throw new Error("Expected Block");
    }

    if (pushNewScope && this.hasLocalScope()) {
      this.pushScope();
    }

    for (const item of block.body) {
      if (item.type === "Function") continue;
      this.compileStmt(item);
    }

    if (pushNewScope && this.hasLocalScope()) {
      this.popScope();
    }
  };

  Emitter.prototype.compileStmt = function compileStmt(stmt) {
    this.setSourceLine(stmt);

    switch (stmt.type) {
      case "VarDecl":
        return this.compileVarDecl(stmt);

      case "If":
        return this.compileIf(stmt);

      case "While":
        return this.compileWhile(stmt);

      case "For":
        return this.compileFor(stmt);

      case "Break":
        return this.compileBreak();

      case "Continue":
        return this.compileContinue();

      case "Return":
        return this.compileReturn(stmt);

      case "ExportStmt":
        return this.compileExportStmt(stmt);

      case "AsmStmt":
        return this.compileAsmStmt(stmt);

      case "Assign":
        this.compileAssignExpr(stmt);
        this.emit("pop");
        return;

      case "ExprStmt":
        this.compileExpr(stmt.expr);
        this.emit("pop");
        return;

      case "Block":
        this.compileBlock(stmt, true);
        return;

      default:
        this.errorAt(stmt, `Unsupported statement: ${stmt.type}`);
    }
  };

  Emitter.prototype.compileReturn = function compileReturn(stmt) {
    const expectedReturnType = this.current && this.current.functionMeta
      ? this.current.functionMeta.returnType
      : "any";

    const actualReturnType = stmt.expr ? this.exprType(stmt.expr) : "nil";
    this.checkAssignable(expectedReturnType, actualReturnType, stmt);

    if (stmt.expr) this.compileExpr(stmt.expr);
    else this.emit("pushnil");

    this.emit("ret");
  };

  Emitter.prototype.compileExportStmt = function compileExportStmt(stmt) {
    if (!this.current || this.current.kind !== "init") {
      this.errorAt(stmt, "export can only be used at module top level");
    }

    const module = this.current.module;

    if (module.isRoot) {
      this.errorAt(stmt, "export can only be used inside imported .plt modules");
    }

    this.current.didExport = true;
    this.compileExpr(stmt.expr);
    this.emit("dup");
    this.emit("storeg", module.cacheSlot);
    this.emit("pushi", 1);
    this.emit("storeg", module.loadedSlot);
    this.emit("ret");
  };

  Emitter.prototype.compileVarDecl = function compileVarDecl(stmt) {
    let slot = null;
    const isFunctionLocal = !!(this.hasLocalScope() && !stmt._global);

    const declaredType = this.typeFromTypeNode(stmt.varType);
    this.checkVarTypeIsValid(declaredType, stmt);

    if (isFunctionLocal) {
      if (typeof stmt._slot !== "number") {
        throw new Error(`Missing precomputed slot for local '${stmt.name}'`);
      }

      this.bindLocal(stmt.name, stmt._slot, stmt);
      slot = stmt._slot;
    } else {
      const module = this.getActiveModule();
      module.globalTypes.set(stmt.name, declaredType);
    }

    if (stmt.expr) {
      const actualType = this.exprType(stmt.expr);
      this.checkAssignable(declaredType, actualType, stmt);
      this.compileExpr(stmt.expr);
    } else if (stmt.varType && stmt.varType.type === "ArrayType") {
      this.emitAllocTypedArray(stmt.varType);
    } else if (declaredType === "string") {
      this.emit("pushs", JSON.stringify(""));
    } else {
      this.emit("pushnil");
    }

    this.recordVarShape(stmt.name, slot, stmt.expr, isFunctionLocal);

    if (isFunctionLocal) this.emit("store", slot);
    else this.emitStoreName(stmt.name, stmt);
  };

  Emitter.prototype.emitAllocTypedArray = function emitAllocTypedArray(typeNode) {
    const dims = this.getArrayDims(typeNode);
    this.emitAllocNestedArray(dims, 0);
  };

  Emitter.prototype.getArrayDims = function getArrayDims(typeNode) {
    if (!typeNode || typeNode.type !== "ArrayType") return [1];

    const dims = (typeNode.dims || []).map((dim) => {
      const value = Number(dim);
      return Number.isFinite(value) && value > 0 ? value : 1;
    });

    return dims.length ? dims : [1];
  };

  Emitter.prototype.emitAllocNestedArray = function emitAllocNestedArray(dims, depth) {
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
  };

  Emitter.prototype.compileIf = function compileIf(stmt) {
    const L_else = this.newLabel("else");
    const L_end = this.newLabel("endif");

    this.compileExpr(stmt.cond);
    this.emit("jz", L_else);

    this.compileBlock(stmt.then, true);

    if (stmt.else) this.emit("jmp", L_end);

    this.label(L_else);

    if (stmt.else) {
      if (stmt.else.type === "Block") this.compileBlock(stmt.else, true);
      else if (stmt.else.type === "If") this.compileIf(stmt.else);
      else throw new Error("Bad else node: " + stmt.else.type);
    }

    this.label(L_end);
  };

  Emitter.prototype.compileWhile = function compileWhile(stmt) {
    const L_cond = this.newLabel("while_cond");
    const L_end = this.newLabel("while_end");

    this.loopStack.push({ breakLabel: L_end, continueLabel: L_cond });

    this.label(L_cond);
    this.compileExpr(stmt.cond);
    this.emit("jz", L_end);

    this.compileBlock(stmt.body, true);
    this.emit("jmp", L_cond);

    this.label(L_end);
    this.loopStack.pop();
  };

  Emitter.prototype.compileFor = function compileFor(stmt) {
    const L_cond = this.newLabel("for_cond");
    const L_step = this.newLabel("for_step");
    const L_end = this.newLabel("for_end");

    if (this.hasLocalScope()) {
      this.pushScope();
    }

    if (stmt.init) {
      if (stmt.init.type === "VarDecl") {
        this.compileVarDecl(stmt.init);
      } else {
        this.compileExpr(stmt.init);
        this.emit("pop");
      }
    }

    this.loopStack.push({ breakLabel: L_end, continueLabel: L_step });

    this.label(L_cond);

    if (stmt.cond) {
      this.compileExpr(stmt.cond);
      this.emit("jz", L_end);
    }

    this.compileBlock(stmt.body, true);

    this.label(L_step);

    if (stmt.step) {
      this.compileExpr(stmt.step);
      this.emit("pop");
    }

    this.emit("jmp", L_cond);

    this.label(L_end);
    this.loopStack.pop();

    if (this.hasLocalScope()) {
      this.popScope();
    }
  };

  Emitter.prototype.compileBreak = function compileBreak() {
    const top = this.loopStack[this.loopStack.length - 1];
    if (!top) throw new Error("break used outside loop");
    this.emit("jmp", top.breakLabel);
  };

  Emitter.prototype.compileContinue = function compileContinue() {
    const top = this.loopStack[this.loopStack.length - 1];
    if (!top) throw new Error("continue used outside loop");
    this.emit("jmp", top.continueLabel);
  };
}


function installExpressionCompiler(Emitter) {
  Emitter.prototype.compileExpr = function compileExpr(expr) {
    if (!expr) throw new Error("Unsupported expression: undefined node");

    this.setSourceLine(expr);

    switch (expr.type) {
      case "Nil":
        this.emit("pushnil");
        return;

      case "Boolean":
        this.emit("pushi", expr.value ? 1 : 0);
        return;

      case "Int":
        this.emit("pushi", expr.value);
        return;

      case "Double":
        this.emit("pushd", expr.value);
        return;

      case "String":
        this.emit("pushs", JSON.stringify(expr.value));
        return;

      case "ImportExpr":
        return this.compileImportExpr(expr);

      case "Identifier": {
        if (this.canResolveVar(expr.name)) {
          this.emitLoadName(expr.name, expr);
          return;
        }

        if (this.canResolveFunction(expr.name)) {
          this.emit("pushfn", this.resolveFunction(expr.name));
          return;
        }

        this.errorAt(expr, `Unknown identifier '${expr.name}'`);
      }

      case "Unary":
        return this.compileUnary(expr);

      case "Binary":
        return this.compileBinary(expr);

      case "Assign":
        return this.compileAssignExpr(expr);

      case "Call":
        return this.compileCall(expr);

      case "Member":
        return this.compileMember(expr);

      case "Index":
        this.exprType(expr);
        this.compileExpr(expr.object);
        this.compileExpr(expr.index);
        this.emit("array_get");
        return;

      case "ArrayLiteral":
        return this.compileArrayLiteral(expr);

      case "ObjectLiteral":
        return this.compileObjectLiteral(expr);

      default:
        this.errorAt(expr, `Unsupported expression: ${expr.type}`);
    }
  };

  Emitter.prototype.compileUnary = function compileUnary(expr) {
    this.unaryExprType(expr);

    if (expr.op === "-") {
      this.emit("pushi", 0);
      this.compileExpr(expr.expr);
      this.emit("sub");
      return;
    }

    if (expr.op === "!") {
      this.compileExpr(expr.expr);
      this.emit("pushi", 0);
      this.emit("eq");
      return;
    }

    throw new Error("Unsupported unary operator: " + expr.op);
  };

  Emitter.prototype.compileBinary = function compileBinary(expr) {
    const resultType = this.binaryExprType(expr);

    if (expr.op === "&&") return this.emitLogicalAnd(expr.left, expr.right);
    if (expr.op === "||") return this.emitLogicalOr(expr.left, expr.right);

    this.compileExpr(expr.left);
    this.compileExpr(expr.right);

    if (expr.op === "+") {
      this.emit(resultType === "string" ? "concat" : "add");
      return;
    }

    this.emit(this.binOp(expr.op));
  };

  Emitter.prototype.emitLogicalAnd = function emitLogicalAnd(left, right) {
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
  };

  Emitter.prototype.emitLogicalOr = function emitLogicalOr(left, right) {
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
  };

  Emitter.prototype.compileObjectMethodCall = function compileObjectMethodCall(expr, member) {
    switch (member.property) {
      case "keys":
        if (expr.args.length !== 0) this.errorAt(expr, "object.keys() expects 0 arguments");

        this.compileExpr(member.object);
        this.emit("pushs", JSON.stringify("keys"));
        this.emit("object_get");
        this.compileExpr(member.object);
        this.emit("call", 1);
        return;

      case "values":
        if (expr.args.length !== 0) this.errorAt(expr, "object.values() expects 0 arguments");

        this.compileExpr(member.object);
        this.emit("pushs", JSON.stringify("values"));
        this.emit("object_get");
        this.compileExpr(member.object);
        this.emit("call", 1);
        return;

      case "has":
        if (expr.args.length !== 1) this.errorAt(expr, "object.has(key) expects 1 argument");

        this.compileExpr(member.object);
        this.emit("pushs", JSON.stringify("has"));
        this.emit("object_get");
        this.compileExpr(member.object);
        this.compileExpr(expr.args[0]);
        this.emit("call", 2);
        return;

      default:
        this.compileExpr(member);

        for (const arg of expr.args) {
          this.compileExpr(arg);
        }

        this.emit("call", expr.args.length);
        return;
    }
  };

  Emitter.prototype.compileArrayMethodCall = function compileArrayMethodCall(expr, member) {
    switch (member.property) {
      case "push":
        if (expr.args.length !== 1) this.errorAt(expr, "array.push(value) expects 1 argument");

        this.compileExpr(member.object);
        this.compileExpr(expr.args[0]);
        this.emit("array_push");
        return;

      case "get":
        if (expr.args.length !== 1) this.errorAt(expr, "array.get(index) expects 1 argument");

        this.compileExpr(member.object);
        this.compileExpr(expr.args[0]);
        this.emit("array_get");
        return;

      case "set":
        if (expr.args.length !== 2) this.errorAt(expr, "array.set(index, value) expects 2 arguments");

        this.compileExpr(member.object);
        this.compileExpr(expr.args[0]);
        this.compileExpr(expr.args[1]);
        this.emit("array_set");
        return;

      default:
        this.errorAt(member, `Unknown array method '${member.property}'`);
    }
  };

  Emitter.prototype.isBuiltinMethod = function isBuiltinMethod(objectType, name) {
    if (objectType === "object") return ["keys", "values", "has"].includes(name);
    if (objectType === "array") return ["push", "pop", "get", "set"].includes(name);
    return false;
  };

  Emitter.prototype.compileMethodCall = function compileMethodCall(expr) {
    const member = expr.callee;
    const objectType = this.exprType(member.object);

    if (objectType === "array") return this.compileArrayMethodCall(expr, member);
    if (objectType === "object") return this.compileObjectMethodCall(expr, member);

    this.compileExpr(member);

    for (const arg of expr.args) {
      this.compileExpr(arg);
    }

    this.emit("call", expr.args.length);
  };

  Emitter.prototype.compileMember = function compileMember(expr) {
    const objectType = this.exprType(expr.object);

    if (objectType === "array" && expr.property === "len") {
      this.compileExpr(expr.object);
      this.emit("array_len");
      return;
    }

    this.compileExpr(expr.object);
    this.emit("pushs", JSON.stringify(expr.property));
    this.emit("object_get");
  };

  Emitter.prototype.compileCall = function compileCall(expr) {
    this.callExprType(expr);

    if (expr.callee.type === "Member") return this.compileMethodCall(expr);

    this.compileExpr(expr.callee);

    for (const arg of expr.args) {
      this.compileExpr(arg);
    }

    this.emit("call", expr.args.length);
  };

  Emitter.prototype.compileArrayLiteral = function compileArrayLiteral(node) {
    this.emit("pushi", node.elements.length);
    this.emit("array_new");

    for (const el of node.elements) {
      this.compileExpr(el);
      this.emit("array_push");
    }
  };

  Emitter.prototype.compileObjectLiteral = function compileObjectLiteral(node) {
    this.emit("pushi", node.properties.length);
    this.emit("object_new");

    for (const prop of node.properties) {
      const keyType = this.exprType(prop.key);
      if (!isAssignableType("string", keyType)) {
        this.errorAt(prop.key, `Object key must be string, got '${keyType}'`);
      }

      this.emit("pushs", JSON.stringify(prop.key.value));
      this.compileExpr(prop.value);
      this.emit("object_set");
    }
  };

  Emitter.prototype.splitIndexChain = function splitIndexChain(target) {
    const indices = [];
    let cur = target;

    while (cur && cur.type === "Index") {
      indices.push(cur.index);
      cur = cur.object;
    }

    indices.reverse();

    return { base: cur, indices };
  };

  Emitter.prototype.compileAssignExpr = function compileAssignExpr(assignNode) {
    const target = assignNode.target;

    if (target.type === "Identifier") {
      const ref = this.resolveVar(target.name, target);
      const rhsType = this.exprType(assignNode.expr);

      this.checkAssignable(ref.type, rhsType, assignNode);

      this.compileExpr(assignNode.expr);
      this.emit("dup");
      this.emitStoreName(target.name, target);
      this.recordResolvedVarShape(target.name, assignNode.expr);
      return;
    }

    if (target.type === "Index") return this.compileIndexAssignExpr(target, assignNode.expr);
    if (target.type === "Member") return this.compileMemberAssignExpr(target, assignNode.expr);

    this.errorAt(target, "Bad assignment target");
  };

  Emitter.prototype.compileIndexAssignExpr = function compileIndexAssignExpr(target, rhs) {
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
  };

  Emitter.prototype.compileMemberAssignExpr = function compileMemberAssignExpr(target, rhs) {
    const tmp = this.current.assignTempSlot;

    this.compileExpr(target.object);
    this.emit("pushs", JSON.stringify(target.property));
    this.compileExpr(rhs);

    this.emit("dup");
    this.emit("store", tmp);

    this.emit("object_set");
    this.emit("pop");

    this.emit("load", tmp);
  };

  Emitter.prototype.binOp = function binOp(op) {
    const opMap = {
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

    if (!opMap[op]) throw new Error("Unknown binary op: " + op);
    return opMap[op];
  };
}


function installStaticShapeHelpers(Emitter) {
  Emitter.prototype.recordVarShape = function recordVarShape(name, slot, expr, isFunctionLocal) {
    const shape = this.getStaticObjectShape(expr);

    if (isFunctionLocal) {
      if (this.current && this.current.localShapes && typeof slot === "number") {
        if (shape) this.current.localShapes.set(slot, shape);
        else this.current.localShapes.delete(slot);
      }
      return;
    }

    const module = this.getActiveModule();
    if (module && module.globalShapes) {
      if (shape) module.globalShapes.set(name, shape);
      else module.globalShapes.delete(name);
    }
  };

  Emitter.prototype.recordResolvedVarShape = function recordResolvedVarShape(name, expr) {
    const ref = this.resolveVar(name);
    const shape = this.getStaticObjectShape(expr);

    if (ref.kind === "local") {
      if (this.current && this.current.localShapes) {
        if (shape) this.current.localShapes.set(ref.slot, shape);
        else this.current.localShapes.delete(ref.slot);
      }
      return;
    }

    const module = this.getActiveModule();
    if (module && module.globalShapes) {
      if (shape) module.globalShapes.set(name, shape);
      else module.globalShapes.delete(name);
    }
  };

  Emitter.prototype.getStaticObjectShape = function getStaticObjectShape(expr) {
    if (!expr) return null;

    if (expr.type === "ImportExpr") {
      const resolved = this.resolveModuleImport(expr.path, expr);
      if (resolved.kind === "source") return resolved.module.exportShape || null;
      return null;
    }

    if (expr.type === "Identifier") {
      return this.resolveVarShape(expr.name);
    }

    if (expr.type === "Assign") {
      return this.getStaticObjectShape(expr.expr);
    }

    if (expr.type === "ObjectLiteral") {
      const shape = new Map();

      for (const prop of expr.properties || []) {
        if (!prop.key || prop.key.type !== "String") continue;

        let valueShape = { kind: "value", type: this.exprType(prop.value) || "any" };

        if (prop.value && prop.value.type === "Identifier" && this.canResolveFunction(prop.value.name)) {
          const meta = this.resolveFunctionMeta(prop.value.name, prop.value);
          valueShape = {
            kind: "function",
            name: prop.value.name,
            fullName: meta.fullName,
            paramTypes: meta.paramTypes.slice(),
            returnType: meta.returnType,
          };
        }

        shape.set(prop.key.value, valueShape);
      }

      return shape;
    }

    return null;
  };
}

function installImportResolver(Emitter) {
  Emitter.prototype.normalizeModulePath = function normalizeModulePath(filePath) {
    return path.normalize(path.resolve(filePath));
  };

  Emitter.prototype.parseModuleSource = function parseModuleSource(source, filename) {
    if (!this.parse) {
      throw new Error("A parse function is required to import .plt source modules");
    }

    return this.parse(source, { sourcePath: filename, grammarSource: filename });
  };

  Emitter.prototype.makeSourceModuleId = function makeSourceModuleId(filename) {
    const base = path.basename(filename, path.extname(filename)).replace(/[^A-Za-z0-9_]/g, "_");
    return `mod_${++this.moduleId}_${base}`;
  };

  Emitter.prototype.tryResolveSourceModulePath = function tryResolveSourceModulePath(spec, fromDir) {
    const candidates = [];

    const pushCandidate = (base) => {
      candidates.push(base);
      if (!path.extname(base)) candidates.push(base + ".plt");
      if (!path.extname(base)) candidates.push(path.join(base, "index.plt"));
    };

    pushCandidate(path.resolve(fromDir, spec));
    pushCandidate(path.resolve(this.projectRoot, "modules", spec));

    for (const candidate of candidates) {
      if (fs.existsSync(candidate) && fs.statSync(candidate).isFile()) {
        return this.normalizeModulePath(candidate);
      }
    }

    return null;
  };

  Emitter.prototype.tryResolveNativeModulePath = function tryResolveNativeModulePath(spec, fromDir) {
    const platformExt = this.target === TargetPlatform.linux ? ".so" : ".dll";
    const candidates = [
      path.resolve(fromDir, spec),
      path.resolve(this.projectRoot, "modules", spec),
    ];

    for (const candidate of candidates) {
      if (fs.existsSync(candidate + platformExt)) {
        return path.normalize(path.relative(this.projectRoot, candidate));
      }
    }

    return null;
  };

  Emitter.prototype.isRelativeImportSpec = function isRelativeImportSpec(spec) {
    return spec.startsWith("./") || spec.startsWith("../") || spec.startsWith("/");
  };

  Emitter.prototype.shouldPreferSourceImport = function shouldPreferSourceImport(spec) {
    return this.isRelativeImportSpec(spec) || path.extname(spec) === ".plt";
  };

  Emitter.prototype.resolveSourceModuleImport = function resolveSourceModuleImport(spec, fromDir, activeModule, node = null) {
    const sourcePath = this.tryResolveSourceModulePath(spec, fromDir);
    if (!sourcePath) return null;

    // Source module wrappers can have the same basename as a native module:
    // modules/fs.plt may contain @import("fs") to load modules/fs.so.
    // Without this guard, the import resolves to itself and emits recursive
    // pushfn/call bytecode back into the current module init.
    if (activeModule && activeModule.sourcePath) {
      const activePath = this.normalizeModulePath(activeModule.sourcePath);
      if (sourcePath === activePath) return null;
    }

    if (this.modules.has(sourcePath)) {
      const existing = this.modules.get(sourcePath);
      if (existing && existing.isRoot) {
        this.errorAt(node, `Import '${spec}' resolves to the current root source file`);
      }
      return { kind: "source", module: existing };
    }

    const source = fs.readFileSync(sourcePath, "utf8");
    const ast = this.parseModuleSource(source, sourcePath);
    const module = this.createModule({
      id: this.makeSourceModuleId(sourcePath),
      sourcePath,
      ast,
      isRoot: false,
    });

    this.collectModule(module);
    return { kind: "source", module };
  };

  Emitter.prototype.resolveNativeModuleImport = function resolveNativeModuleImport(spec, fromDir) {
    const nativePath = this.tryResolveNativeModulePath(spec, fromDir);
    if (!nativePath) return null;
    return { kind: "native", path: nativePath };
  };

  Emitter.prototype.resolveModuleImport = function resolveModuleImport(spec, node = null) {
    const activeModule = this.getActiveModule();

    if (!activeModule || !activeModule.sourcePath) {
      throw new Error("sourcePath is required for imports");
    }

    const fromDir = path.dirname(activeModule.sourcePath);

    if (this.shouldPreferSourceImport(spec)) {
      const source = this.resolveSourceModuleImport(spec, fromDir, activeModule, node);
      if (source) return source;

      const native = this.resolveNativeModuleImport(spec, fromDir);
      if (native) return native;

      this.errorAt(node, `Import not found: '${spec}'`);
    }

    const native = this.resolveNativeModuleImport(spec, fromDir);
    if (native) return native;

    const source = this.resolveSourceModuleImport(spec, fromDir, activeModule, node);
    if (source) return source;

    this.errorAt(node, `Import not found: '${spec}'`);
  };

  Emitter.prototype.emitInitModuleCacheFlags = function emitInitModuleCacheFlags() {
    for (const module of this.moduleOrder) {
      if (module.isRoot) continue;

      if (typeof module.loadedSlot !== "number") {
        throw new Error("Internal: source module has no loaded global slot");
      }

      this.emit("pushi", 0);
      this.emit("storeg", module.loadedSlot);
    }
  };

  Emitter.prototype.compileSourceImportExpr = function compileSourceImportExpr(module) {
    if (!module || typeof module.cacheSlot !== "number") {
      throw new Error("Internal: source module has no cache global slot");
    }

    if (typeof module.loadedSlot !== "number") {
      throw new Error("Internal: source module has no loaded global slot");
    }

    const L_load = this.newLabel("module_load");
    const L_end = this.newLabel("module_end");

    this.emit("loadg", module.loadedSlot);
    this.emit("jz", L_load);
    this.emit("loadg", module.cacheSlot);
    this.emit("jmp", L_end);

    this.label(L_load);
    this.emit("pushfn", module.initLabel);
    this.emit("call", 0);

    this.label(L_end);
  };

  Emitter.prototype.compileImportExpr = function compileImportExpr(expr) {
    const resolved = this.resolveModuleImport(expr.path, expr);

    if (resolved.kind === "source") {
      this.compileSourceImportExpr(resolved.module);
      return;
    }

    this.emit("pushs", JSON.stringify(resolved.path));
    this.emit("load_lib");
  };

  Emitter.prototype.resolveModuleImportPath = function resolveModuleImportPath(spec, node = null) {
    const resolved = this.resolveModuleImport(spec, node);
    if (resolved.kind !== "native") {
      this.errorAt(node, `Expected native module import, got source module '${spec}'`);
    }
    return resolved.path;
  };
}

function installAsmCompiler(Emitter) {
  Emitter.prototype.compileAsmStmt = function compileAsmStmt(stmt) {
    const lines = stmt.code
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter((line) => line && !line.startsWith("#"));

    for (const line of lines) {
      this.emitAsmLine(line);
    }
  };

  Emitter.prototype.emitAsmLine = function emitAsmLine(line) {
    if (this.emitType === EmitType.bc) {
      this.out.push("  " + line);
      return;
    }

    const parts = this.splitAsmOperands(line);
    if (parts.length === 0) return;

    const [opcode, ...rawOperands] = parts;
    const operands = rawOperands.map((operand) => this.parseAsmOperand(operand));

    this.emit(opcode, ...operands);
  };

  Emitter.prototype.splitAsmOperands = function splitAsmOperands(line) {
    const re = /"([^"\\]|\\.)*"|\S+/g;
    return line.match(re) || [];
  };

  Emitter.prototype.parseAsmOperand = function parseAsmOperand(token) {
    const t = token.trim();

    if (/^".*"$/.test(t)) return t;
    if (/^-?\d+$/.test(t)) return Number(t);
    if (/^-?\d+\.\d+$/.test(t)) return Number(t);

    return t;
  };
}

function installCodegen(Emitter) {
  installStatementCompiler(Emitter);
  installExpressionCompiler(Emitter);
  installStaticShapeHelpers(Emitter);
  installImportResolver(Emitter);
  installAsmCompiler(Emitter);
}

module.exports = { installCodegen };
