"use strict";

function installModuleHelpers(Emitter) {
  Emitter.prototype.createModule = function createModule({ id, sourcePath, ast, isRoot = false }) {
    const module = {
      id,
      sourcePath,
      ast,
      isRoot,
      status: "new",
      globals: new Map(),
      globalTypes: new Map(),
      topLevelFunctions: new Map(),
      initLabel: isRoot ? "__init" : `${id}$__init`,
      cacheSlot: this.allocGlobalSlot(),
      loadedSlot: this.allocGlobalSlot(),
      exportShape: new Map(),
      globalShapes: new Map(),
      initLocalCount: 1,
      initLocalTypes: new Map([[0, "any"]]),
      initAssignTempSlot: 0,
    };

    if (sourcePath) {
      this.modules.set(this.normalizeModulePath(sourcePath), module);
    }

    this.moduleOrder.push(module);
    return module;
  };

  Emitter.prototype.allocGlobalSlot = function allocGlobalSlot() {
    return this.nextGlobalSlot++;
  };

  Emitter.prototype.collectModule = function collectModule(module) {
    if (module.status === "collected" || module.status === "collecting") return;

    module.status = "collecting";
    const prevModule = this.currentModule;
    this.currentModule = module;

    this.collectProgram(module.ast);

    this.currentModule = prevModule;
    module.status = "collected";
  };
}

function installContextHelpers(Emitter) {
  Emitter.prototype.makeFunctionCodegenContext = function makeFunctionCodegenContext(meta) {
    return {
      kind: "function",
      module: meta.module,
      fullName: meta.fullName,
      name: meta.name,
      functionMeta: meta,
      scopeStack: [],
      localTypes: meta.localTypes,
      localShapes: new Map(),
      localCount: meta.localCount,
      assignTempSlot: meta.assignTempSlot,
    };
  };

  Emitter.prototype.makeInitCodegenContext = function makeInitCodegenContext(module) {
    return {
      kind: "init",
      module,
      fullName: module.initLabel,
      name: module.initLabel,
      functionMeta: null,
      scopeStack: [],
      localTypes: module.initLocalTypes || new Map([[0, "any"]]),
      localShapes: new Map(),
      localCount: module.initLocalCount || 1,
      assignTempSlot: typeof module.initAssignTempSlot === "number" ? module.initAssignTempSlot : 0,
      didExport: false,
    };
  };

  Emitter.prototype.hasLocalScope = function hasLocalScope() {
    return !!(this.current && (this.current.kind === "function" || this.current.kind === "init"));
  };

  Emitter.prototype.pushScope = function pushScope() {
    if (!this.hasLocalScope()) return;
    this.current.scopeStack.push(new Map());
  };

  Emitter.prototype.popScope = function popScope() {
    if (!this.hasLocalScope()) return;

    if (this.current.scopeStack.length === 0) {
      throw new Error("popScope on empty scope stack");
    }

    this.current.scopeStack.pop();
  };

  Emitter.prototype.currentScope = function currentScope() {
    if (!this.hasLocalScope()) {
      throw new Error("No active local scope");
    }

    const scope = this.current.scopeStack[this.current.scopeStack.length - 1];
    if (!scope) throw new Error("No current scope");
    return scope;
  };

  Emitter.prototype.bindLocal = function bindLocal(name, slot, node = null) {
    const scope = this.currentScope();

    if (scope.has(name)) {
      this.errorAt(node, `Duplicate local '${name}'`);
    }

    scope.set(name, slot);
  };

  Emitter.prototype.resolveLocal = function resolveLocal(name) {
    if (!this.hasLocalScope()) return null;

    for (let i = this.current.scopeStack.length - 1; i >= 0; i--) {
      const scope = this.current.scopeStack[i];
      if (scope.has(name)) return scope.get(name);
    }

    return null;
  };

  Emitter.prototype.getActiveModule = function getActiveModule() {
    if (this.current && this.current.module) return this.current.module;
    if (this.currentModule) return this.currentModule;
    return this.rootModule;
  };

  Emitter.prototype.resolveVar = function resolveVar(name, node = null) {
    const localSlot = this.resolveLocal(name);

    if (localSlot != null) {
      return {
        kind: "local",
        slot: localSlot,
        type: this.current.localTypes.get(localSlot) || "any",
      };
    }

    const module = this.getActiveModule();

    if (module && module.globals.has(name)) {
      return {
        kind: "global",
        slot: module.globals.get(name),
        type: module.globalTypes.get(name) || "any",
      };
    }

    this.errorAt(node, `Unknown identifier '${name}'`);
  };

  Emitter.prototype.resolveVarShape = function resolveVarShape(name) {
    const localSlot = this.resolveLocal(name);

    if (localSlot != null) {
      return this.current && this.current.localShapes
        ? this.current.localShapes.get(localSlot) || null
        : null;
    }

    const module = this.getActiveModule();
    if (module && module.globalShapes && module.globalShapes.has(name)) {
      return module.globalShapes.get(name) || null;
    }

    return null;
  };

  Emitter.prototype.canResolveVar = function canResolveVar(name) {
    try {
      this.resolveVar(name);
      return true;
    } catch {
      return false;
    }
  };

  Emitter.prototype.resolveFunction = function resolveFunction(name) {
    if (this.current && this.current.functionMeta) {
      let meta = this.current.functionMeta;

      while (meta) {
        if (meta.nestedVisible.has(name)) {
          return meta.nestedVisible.get(name);
        }

        meta = meta.parent;
      }
    }

    const module = this.getActiveModule();

    if (module && module.topLevelFunctions.has(name)) {
      return module.topLevelFunctions.get(name);
    }

    throw new Error(`Unknown function '${name}'`);
  };

  Emitter.prototype.canResolveFunction = function canResolveFunction(name) {
    try {
      this.resolveFunction(name);
      return true;
    } catch {
      return false;
    }
  };

  Emitter.prototype.resolveFunctionMeta = function resolveFunctionMeta(name, node = null) {
    const fullName = this.resolveFunction(name);

    const meta = this.functions.get(fullName);
    if (!meta) {
      this.errorAt(node, `Unknown function '${name}'`);
    }

    return meta;
  };

  Emitter.prototype.emitLoadName = function emitLoadName(name, node = null) {
    const ref = this.resolveVar(name, node);

    if (ref.kind === "local") this.emit("load", ref.slot);
    else this.emit("loadg", ref.slot);
  };

  Emitter.prototype.emitStoreName = function emitStoreName(name, node = null) {
    const ref = this.resolveVar(name, node);

    if (ref.kind === "local") this.emit("store", ref.slot);
    else this.emit("storeg", ref.slot);
  };

  Emitter.prototype.typeFromTypeNode = function typeFromTypeNode(typeNode) {
    if (!typeNode) return "any";
    if (typeNode.type === "Type") return typeNode.name;
    if (typeNode.type === "ArrayType") return "array";
    return "any";
  };
}

function installCollector(Emitter) {
  Emitter.prototype.collectProgram = function collectProgram(program) {
    const module = this.getActiveModule();

    for (const item of program.body) {
      if (item.type === "VarDecl") {
        this.declareGlobal(item.name, item);
      }
    }

    for (const item of program.body) {
      if (item.type === "Function") {
        if (module.topLevelFunctions.has(item.name)) {
          this.errorAt(item, `Duplicate function '${item.name}'`);
        }

        module.topLevelFunctions.set(item.name, `${module.id}$${item.name}`);
      }
    }

    for (const item of program.body) {
      if (item.type === "Function") {
        this.collectFunction(item, null);
      }
    }

    this.collectModuleExportShape(module, program);

    for (const item of program.body) {
      this.collectImportsInNode(item);
    }

    this.prepareModuleInitMeta(module);
  };

  Emitter.prototype.collectModuleExportShape = function collectModuleExportShape(module, program) {
    module.exportShape = new Map();

    if (module.isRoot) return;

    for (const item of program.body) {
      if (!item || item.type !== "ExportStmt") continue;

      if (!item.expr || item.expr.type !== "ObjectLiteral") {
        this.errorAt(item, "export expects an object literal");
      }

      for (const prop of item.expr.properties || []) {
        if (!prop.key || prop.key.type !== "String") {
          this.errorAt(prop.key || prop, "export object keys must be string literals");
        }

        const exportName = prop.key.value;
        let exported = { kind: "value", type: "any" };

        if (prop.value && prop.value.type === "Identifier") {
          const localName = prop.value.name;

          if (module.topLevelFunctions.has(localName)) {
            const fullName = module.topLevelFunctions.get(localName);
            const meta = this.functions.get(fullName);

            if (meta) {
              exported = {
                kind: "function",
                name: localName,
                fullName,
                paramTypes: meta.paramTypes.slice(),
                returnType: meta.returnType,
              };
            }
          } else if (module.globals.has(localName)) {
            exported = {
              kind: "value",
              type: module.globalTypes.get(localName) || "any",
            };
          }
        }

        module.exportShape.set(exportName, exported);
      }
    }
  };

  Emitter.prototype.collectImportsInNode = function collectImportsInNode(node) {
    if (!node) return;

    switch (node.type) {
      case "ImportExpr":
        this.resolveModuleImport(node.path, node);
        return;

      case "Function":
        this.collectImportsInNode(node.body);
        return;

      case "Block":
        for (const item of node.body) this.collectImportsInNode(item);
        return;

      case "VarDecl":
        this.collectImportsInNode(node.expr);
        return;

      case "ExportStmt":
        this.collectImportsInNode(node.expr);
        return;

      case "If":
        this.collectImportsInNode(node.cond);
        this.collectImportsInNode(node.then);
        this.collectImportsInNode(node.else);
        return;

      case "While":
        this.collectImportsInNode(node.cond);
        this.collectImportsInNode(node.body);
        return;

      case "For":
        this.collectImportsInNode(node.init);
        this.collectImportsInNode(node.cond);
        this.collectImportsInNode(node.step);
        this.collectImportsInNode(node.body);
        return;

      case "Return":
      case "ExprStmt":
        this.collectImportsInNode(node.expr);
        return;

      case "Assign":
        this.collectImportsInNode(node.target);
        this.collectImportsInNode(node.expr);
        return;

      case "Unary":
        this.collectImportsInNode(node.expr);
        return;

      case "Binary":
        this.collectImportsInNode(node.left);
        this.collectImportsInNode(node.right);
        return;

      case "Call":
        this.collectImportsInNode(node.callee);
        for (const arg of node.args) this.collectImportsInNode(arg);
        return;

      case "Index":
        this.collectImportsInNode(node.object);
        this.collectImportsInNode(node.index);
        return;

      case "Member":
        this.collectImportsInNode(node.object);
        return;

      case "ArrayLiteral":
        for (const el of node.elements) this.collectImportsInNode(el);
        return;

      case "ObjectLiteral":
        for (const prop of node.properties) {
          this.collectImportsInNode(prop.key);
          this.collectImportsInNode(prop.value);
        }
        return;

      default:
        return;
    }
  };

  Emitter.prototype.collectFunction = function collectFunction(fnNode, parentMeta) {
    const module = parentMeta ? parentMeta.module : this.getActiveModule();
    const fullName = parentMeta ? `${parentMeta.fullName}$${fnNode.name}` : `${module.id}$${fnNode.name}`;

    if (this.functions.has(fullName)) {
      this.errorAt(fnNode, `Duplicate function label '${fullName}'`);
    }

    const meta = {
      module,
      ast: fnNode,
      name: fnNode.name,
      fullName,
      parent: parentMeta,
      nestedVisible: new Map(),

      paramTypes: fnNode.params.map((param) => this.typeFromTypeNode(param.varType)),
      returnType: this.typeFromTypeNode(fnNode.returnType),

      localCount: 0,
      localTypes: new Map(),
      assignTempSlot: 0,
    };

    this.collectNestedFunctionsInBlock(fnNode.body, meta);

    this.functions.set(fullName, meta);
  };

  Emitter.prototype.collectNestedFunctionsInBlock = function collectNestedFunctionsInBlock(block, parentMeta) {
    if (!block || block.type !== "Block") return;

    for (const item of block.body) {
      if (item.type === "Function") {
        const nestedFull = `${parentMeta.fullName}$${item.name}`;

        if (parentMeta.nestedVisible.has(item.name)) {
          this.errorAt(item, `Duplicate nested function '${item.name}' in '${parentMeta.fullName}'`);
        }

        parentMeta.nestedVisible.set(item.name, nestedFull);
        this.collectFunction(item, parentMeta);
        continue;
      }

      this.collectNestedFunctionsInStmt(item, parentMeta);
    }
  };

  Emitter.prototype.collectNestedFunctionsInStmt = function collectNestedFunctionsInStmt(stmt, parentMeta) {
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
  };

  Emitter.prototype.declareGlobal = function declareGlobal(name, node = null) {
    const module = this.getActiveModule();

    if (!module) {
      throw new Error("Internal: no active module for global declaration");
    }

    if (module.globals.has(name)) {
      this.errorAt(node, `Duplicate global '${name}'`);
    }

    const slot = this.allocGlobalSlot();
    if (node) node._global = true;
    module.globals.set(name, slot);
    module.globalTypes.set(name, "any");
    return slot;
  };
}

function installLocalPrepass(Emitter) {
  Emitter.prototype.prepareModuleInitMeta = function prepareModuleInitMeta(module) {
    const scopeStack = [];
    const localTypes = new Map();
    let nextLocalSlot = 0;

    const pushScope = () => scopeStack.push(new Map());

    const popScope = () => {
      if (scopeStack.length === 0) throw new Error("Internal: pop empty init prepass scope");
      scopeStack.pop();
    };

    const currentScope = () => {
      const scope = scopeStack[scopeStack.length - 1];
      if (!scope) throw new Error("Internal: no current init prepass scope");
      return scope;
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

    const collectExpr = (expr) => {
      if (!expr) return;

      switch (expr.type) {
        case "Unary":
          collectExpr(expr.expr);
          return;

        case "Binary":
          collectExpr(expr.left);
          collectExpr(expr.right);
          return;

        case "Assign":
          collectExpr(expr.target);
          collectExpr(expr.expr);
          return;

        case "Call":
          collectExpr(expr.callee);
          for (const arg of expr.args || []) collectExpr(arg);
          return;

        case "Index":
          collectExpr(expr.object);
          collectExpr(expr.index);
          return;

        case "Member":
          collectExpr(expr.object);
          return;

        case "ArrayLiteral":
          for (const el of expr.elements || []) collectExpr(el);
          return;

        case "ObjectLiteral":
          for (const prop of expr.properties || []) {
            collectExpr(prop.key);
            collectExpr(prop.value);
          }
          return;

        default:
          return;
      }
    };

    const collectStmt = (stmt, isTopLevel = false) => {
      if (!stmt) return;

      switch (stmt.type) {
        case "VarDecl":
          if (isTopLevel || stmt._global) {
            if (stmt.expr) collectExpr(stmt.expr);
            return;
          }

          stmt._slot = declareLocal(stmt.name, stmt.varType, stmt);
          if (stmt.expr) collectExpr(stmt.expr);
          return;

        case "ExportStmt":
          collectExpr(stmt.expr);
          return;

        case "If":
          collectExpr(stmt.cond);
          collectBlock(stmt.then, true);

          if (stmt.else) {
            if (stmt.else.type === "Block") collectBlock(stmt.else, true);
            else if (stmt.else.type === "If") collectStmt(stmt.else, false);
          }
          return;

        case "While":
          collectExpr(stmt.cond);
          collectBlock(stmt.body, true);
          return;

        case "For":
          pushScope();

          if (stmt.init) {
            if (stmt.init.type === "VarDecl") collectStmt(stmt.init, false);
            else collectExpr(stmt.init);
          }

          if (stmt.cond) collectExpr(stmt.cond);
          if (stmt.step) collectExpr(stmt.step);

          collectBlock(stmt.body, true);
          popScope();
          return;

        case "Return":
          if (stmt.expr) collectExpr(stmt.expr);
          return;

        case "ExprStmt":
          collectExpr(stmt.expr);
          return;

        case "Assign":
          collectExpr(stmt.target);
          collectExpr(stmt.expr);
          return;

        case "Break":
        case "Continue":
        case "AsmStmt":
          return;

        case "Block":
          collectBlock(stmt, true);
          return;

        default:
          return;
      }
    };

    const collectBlock = (block, pushNewScope = true) => {
      if (!block || block.type !== "Block") {
        throw new Error("Expected Block during init local prepass");
      }

      if (pushNewScope) pushScope();

      for (const item of block.body || []) {
        if (item.type === "Function") continue;
        collectStmt(item, false);
      }

      if (pushNewScope) popScope();
    };

    pushScope();

    for (const item of module.ast.body || []) {
      if (item.type === "Function") continue;
      collectStmt(item, true);
    }

    const tempSlot = nextLocalSlot++;
    localTypes.set(tempSlot, "any");

    module.initAssignTempSlot = tempSlot;
    module.initLocalCount = nextLocalSlot;
    module.initLocalTypes = localTypes;
  };

  Emitter.prototype.prepareFunctionMeta = function prepareFunctionMeta(meta) {
    const scopeStack = [];
    const localTypes = new Map();
    let nextLocalSlot = 0;

    const pushScope = () => scopeStack.push(new Map());

    const popScope = () => {
      if (scopeStack.length === 0) throw new Error("Internal: pop empty prepass scope");
      scopeStack.pop();
    };

    const currentScope = () => {
      const scope = scopeStack[scopeStack.length - 1];
      if (!scope) throw new Error("Internal: no current prepass scope");
      return scope;
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

    const collectExpr = (expr) => {
      if (!expr) return;

      switch (expr.type) {
        case "Unary":
          collectExpr(expr.expr);
          return;

        case "Binary":
          collectExpr(expr.left);
          collectExpr(expr.right);
          return;

        case "Assign":
          collectExpr(expr.target);
          collectExpr(expr.expr);
          return;

        case "Call":
          collectExpr(expr.callee);
          for (const arg of expr.args) collectExpr(arg);
          return;

        case "Index":
          collectExpr(expr.object);
          collectExpr(expr.index);
          return;

        case "Member":
          collectExpr(expr.object);
          return;

        case "ArrayLiteral":
          for (const el of expr.elements) collectExpr(el);
          return;

        case "ObjectLiteral":
          for (const prop of expr.properties) {
            collectExpr(prop.key);
            collectExpr(prop.value);
          }
          return;

        default:
          return;
      }
    };

    const collectStmt = (stmt) => {
      if (!stmt) return;

      switch (stmt.type) {
        case "VarDecl":
          stmt._slot = declareLocal(stmt.name, stmt.varType, stmt);
          if (stmt.expr) collectExpr(stmt.expr);
          return;

        case "ExportStmt":
          collectExpr(stmt.expr);
          return;

        case "If":
          collectExpr(stmt.cond);
          collectBlock(stmt.then, true);

          if (stmt.else) {
            if (stmt.else.type === "Block") collectBlock(stmt.else, true);
            else if (stmt.else.type === "If") collectStmt(stmt.else);
          }
          return;

        case "While":
          collectExpr(stmt.cond);
          collectBlock(stmt.body, true);
          return;

        case "For":
          pushScope();

          if (stmt.init) {
            if (stmt.init.type === "VarDecl") collectStmt(stmt.init);
            else collectExpr(stmt.init);
          }

          if (stmt.cond) collectExpr(stmt.cond);
          if (stmt.step) collectExpr(stmt.step);

          collectBlock(stmt.body, true);
          popScope();
          return;

        case "Return":
          if (stmt.expr) collectExpr(stmt.expr);
          return;

        case "ExprStmt":
          collectExpr(stmt.expr);
          return;

        case "Assign":
          collectExpr(stmt.target);
          collectExpr(stmt.expr);
          return;

        case "Break":
        case "Continue":
        case "AsmStmt":
          return;

        case "Block":
          collectBlock(stmt, true);
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

    for (const param of meta.ast.params) {
      if (param.type !== "Param") {
        this.errorAt(param, "Expected Param node");
      }

      param._slot = declareLocal(param.name, param.varType, param);
    }

    collectBlock(meta.ast.body, false);

    meta.assignTempSlot = nextLocalSlot++;
    localTypes.set(meta.assignTempSlot, "any");

    meta.localCount = nextLocalSlot;
    meta.localTypes = localTypes;
  };
}

function installSymbols(Emitter) {
  installModuleHelpers(Emitter);
  installContextHelpers(Emitter);
  installCollector(Emitter);
  installLocalPrepass(Emitter);
}

module.exports = { installSymbols };
