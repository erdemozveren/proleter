"use strict";

const EmitType = Object.freeze({
  c: Symbol("c"),
  bc: Symbol("bytecode"),
});

const TargetPlatform = Object.freeze({
  linux: Symbol("linux"),
  win32: Symbol("windows"),
});

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
]);

const OPCODE_ENUM = Object.freeze({
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

  object_new: "OP_OBJECT_NEW",
  object_get: "OP_OBJECT_GET",
  object_set: "OP_OBJECT_SET",

  call: "OP_CALL",
  ret: "OP_RET",
  ret_void: "OP_RET_VOID",

  load_lib: "OP_LOAD_LIB",

  halt: "OP_HALT",
});

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


function installErrorHelpers(Emitter) {
  Emitter.prototype.errorAt = function errorAt(node, message) {
    if (node && node.loc && node.loc.start) {
      const { line, column } = node.loc.start;
      throw new SyntaxError(`${message} at line ${line}, column ${column}`);
    }

    throw new SyntaxError(message);
  };

  Emitter.prototype.checkVarTypeIsValid = function checkVarTypeIsValid(typeName, node = null) {
    if (!VALID_TYPES.has(typeName)) {
      this.errorAt(
        node,
        `Unknown type '${typeName}'. Supported types: ${[...VALID_TYPES].join(", ")}`
      );
    }
  };

  Emitter.prototype.checkAssignable = function checkAssignable(to, from, node = null) {
    if (!isAssignableType(to, from)) {
      this.errorAt(node, `Cannot assign '${from}' to '${to}'`);
    }
  };

  Emitter.prototype.setSourceLine = function setSourceLine(node) {
    if (node && node.loc && node.loc.start) {
      const { line } = node.loc.start;
      this.sourceLineNum = line;
    }
  };
}


function installTypeChecker(Emitter) {
  Emitter.prototype.memberCallExprType = function memberCallExprType(callNode) {
    const member = callNode.callee;
    const objectType = this.exprType(member.object);
    const method = member.property;
    const shape = this.getStaticObjectShape(member.object);

    if (shape && shape.has(method)) {
      const prop = shape.get(method);

      if (prop && prop.kind === "function") {
        if (callNode.args.length !== prop.paramTypes.length) {
          this.errorAt(
            callNode,
            `Function '${method}' expects ${prop.paramTypes.length} argument(s), got ${callNode.args.length}`
          );
        }

        for (let i = 0; i < callNode.args.length; i++) {
          const expectedType = prop.paramTypes[i];
          const actualType = this.exprType(callNode.args[i]);
          this.checkAssignable(expectedType, actualType, callNode.args[i]);
        }

        return prop.returnType || "any";
      }

      return prop.type || "any";
    }

    if (objectType === "object") {
      switch (method) {
        case "keys":
          if (callNode.args.length !== 0) this.errorAt(callNode, "object.keys() expects 0 arguments");
          return "array";

        case "values":
          if (callNode.args.length !== 0) this.errorAt(callNode, "object.values() expects 0 arguments");
          return "array";

        case "has": {
          if (callNode.args.length !== 1) this.errorAt(callNode, "object.has(key) expects 1 argument");
          const keyType = this.exprType(callNode.args[0]);
          this.checkAssignable("string", keyType, callNode.args[0]);
          return "bool";
        }

        default:
          return "any";
      }
    }

    if (objectType === "array") {
      switch (method) {
        case "push":
          if (callNode.args.length !== 1) this.errorAt(callNode, "array.push(value) expects 1 argument");
          return "nil";

        case "get": {
          if (callNode.args.length !== 1) this.errorAt(callNode, "array.get(index) expects 1 argument");
          const indexType = this.exprType(callNode.args[0]);
          this.checkAssignable("int", indexType, callNode.args[0]);
          return "any";
        }

        case "set": {
          if (callNode.args.length !== 2) this.errorAt(callNode, "array.set(index, value) expects 2 arguments");
          const indexType = this.exprType(callNode.args[0]);
          this.checkAssignable("int", indexType, callNode.args[0]);
          return "nil";
        }

        default:
          return "any";
      }
    }

    return "any";
  };

  Emitter.prototype.callExprType = function callExprType(callNode) {
    if (callNode.callee.type === "Member") {
      return this.memberCallExprType(callNode);
    }

    if (callNode.callee.type !== "Identifier") return "any";
    if (!this.canResolveFunction(callNode.callee.name)) return "any";

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
  };

  Emitter.prototype.exprType = function exprType(expr) {
    if (!expr) return "any";

    switch (expr.type) {
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
        if (this.canResolveVar(expr.name)) return this.resolveVar(expr.name, expr).type;
        if (this.canResolveFunction(expr.name)) return "fn";
        this.errorAt(expr, `Unknown identifier '${expr.name}'`);
      }

      case "Assign":
        return this.assignExprType(expr);

      case "Unary":
        return this.unaryExprType(expr);

      case "Binary":
        return this.binaryExprType(expr);

      case "ObjectLiteral":
        return "object";

      case "Call":
        return this.callExprType(expr);

      case "Member":
        return "any";

      case "Index": {
        const objectType = this.exprType(expr.object);
        const indexType = this.exprType(expr.index);

        if (!isAssignableType("array", objectType) && !isAssignableType("string", objectType)) {
          this.errorAt(expr.object, `Cannot index value of type '${objectType}'`);
        }

        if (!isAssignableType("int", indexType)) {
          this.errorAt(expr.index, `Index must be int, got '${indexType}'`);
        }

        return "any";
      }

      default:
        return "any";
    }
  };

  Emitter.prototype.assignExprType = function assignExprType(assignNode) {
    const rhsType = this.exprType(assignNode.expr);

    if (assignNode.target.type === "Identifier") {
      const ref = this.resolveVar(assignNode.target.name, assignNode.target);
      this.checkAssignable(ref.type, rhsType, assignNode);
      return ref.type;
    }

    if (assignNode.target.type === "Index" || assignNode.target.type === "Member") {
      return rhsType;
    }

    this.errorAt(assignNode.target, "Bad assignment target");
  };

  Emitter.prototype.unaryExprType = function unaryExprType(expr) {
    const innerType = this.exprType(expr.expr);

    if (expr.op === "-") {
      if (!isNumericType(innerType)) this.errorAt(expr, `Unary '-' requires number, got '${innerType}'`);
      return innerType;
    }

    if (expr.op === "!") {
      if (!isAssignableType("bool", innerType)) this.errorAt(expr, `Unary '!' requires bool, got '${innerType}'`);
      return "bool";
    }

    this.errorAt(expr, `Unsupported unary operator: ${expr.op}`);
  };

  Emitter.prototype.binaryExprType = function binaryExprType(expr) {
    const leftType = this.exprType(expr.left);
    const rightType = this.exprType(expr.right);

    switch (expr.op) {
      case "+": {
        if (leftType === "string" || rightType === "string") {
          if (!STRING_CONVERTIBLE_TYPES.has(leftType) || !STRING_CONVERTIBLE_TYPES.has(rightType)) {
            this.errorAt(expr, `Operator '+' cannot mix '${leftType}' and '${rightType}'`);
          }

          return "string";
        }

        if (!isNumericType(leftType) || !isNumericType(rightType)) {
          this.errorAt(expr, `Operator '+' requires numbers or strings, got '${leftType}' and '${rightType}'`);
        }

        return numericResultType(leftType, rightType);
      }

      case "-":
      case "*":
      case "/":
      case "%": {
        if (!isNumericType(leftType) || !isNumericType(rightType)) {
          this.errorAt(expr, `Operator '${expr.op}' requires numbers, got '${leftType}' and '${rightType}'`);
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
          this.errorAt(expr, `Operator '${expr.op}' requires numbers, got '${leftType}' and '${rightType}'`);
        }

        return "bool";
      }

      case "&&":
      case "||": {
        if (!isAssignableType("bool", leftType) || !isAssignableType("bool", rightType)) {
          this.errorAt(expr, `Operator '${expr.op}' requires bool values, got '${leftType}' and '${rightType}'`);
        }

        return "bool";
      }

      default:
        this.errorAt(expr, `Unknown binary operator '${expr.op}'`);
    }
  };
}

function installTypes(Emitter) {
  installErrorHelpers(Emitter);
  installTypeChecker(Emitter);
}

module.exports = {
  EmitType,
  TargetPlatform,
  VALID_TYPES,
  STRING_CONVERTIBLE_TYPES,
  OPCODE_ENUM,
  isAnyType,
  isNilType,
  isNumericType,
  isAssignableType,
  numericResultType,
  installTypes,
};
