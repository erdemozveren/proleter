"use strict";

const fs = require("fs");
const path = require("path");
const parser = require("./parser");
const { Emitter, TargetPlatform, EmitType } = require("./emitter");

module.exports = {
  TargetPlatform, EmitType,
  compile(inputSourcePath, projectRoot, emit = EmitType.bc, target = TargetPlatform.linux) {
    let source;
    try {
      source = fs.readFileSync(inputSourcePath, "utf8");
    } catch (e) {
      throw new Error(`Cannot read input file: ${inputSourcePath}`);
    }

    let ast;
    const emitter = new Emitter({ sourcePath: inputSourcePath, projectRoot, target, emit });
    try {
      ast = parser.parse(source);
      return emitter.compile(ast);
    } catch (e) {
      // Peggy parse error formatting
      if (e.location) {
        const { line, column } = e.location.start;
        throw new Error(`${path.basename(inputSourcePath)}:${line}:${column}: ${e.message}`);
      }
      throw e;
    }
  }
};
