#include "vm.h"
#include "vm_api.h"
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

const char *vm_opcode_name(OpCode op) {
  switch (op) {
  case OP_NOP:
    return "nop";
  case OP_PUSH_NIL:
    return "pushnil";
  case OP_PUSH_INT:
    return "pushi";
  case OP_PUSH_DOUBLE:
    return "pushd";
  case OP_PUSH_STR:
    return "pushs";
  case OP_PUSH_FN:
    return "pushfn";

  case OP_ADD:
    return "add";
  case OP_SUB:
    return "sub";
  case OP_MUL:
    return "mul";
  case OP_DIV:
    return "div";
  case OP_AND:
    return "and";
  case OP_OR:
    return "or";
  case OP_NOT:
    return "not";
  case OP_POP:
    return "pop";
  case OP_DUP:
    return "dup";
  case OP_SWAP:
    return "swap";
  case OP_PICK:
    return "pick";
  case OP_ROT:
    return "rot";
  case OP_LEN:
    return "len";
  case OP_CONCAT:
    return "concat";
  case OP_TYPEOF:
    return "typeof";

  case OP_EQ:
    return "eq";
  case OP_NEQ:
    return "neq";
  case OP_LT:
    return "lt";
  case OP_GT:
    return "gt";
  case OP_LTE:
    return "lte";
  case OP_GTE:
    return "gte";

  case OP_JZ:
    return "jz";
  case OP_JNZ:
    return "jnz";
  case OP_JL:
    return "jl";
  case OP_JLE:
    return "jle";
  case OP_JGT:
    return "jgt";
  case OP_JGTE:
    return "jgte";
  case OP_JMP:
    return "jmp";

  case OP_ENTER:
    return "enter";
  case OP_LOAD:
    return "load";
  case OP_STORE:
    return "store";
  case OP_LOAD_GLOBAL:
    return "loadg";
  case OP_STORE_GLOBAL:
    return "storeg";

  case OP_ARRAY_NEW:
    return "array_new";
  case OP_ARRAY_PUSH:
    return "array_push";
  case OP_ARRAY_DEL:
    return "array_del";
  case OP_ARRAY_GET:
    return "array_get";
  case OP_ARRAY_SET:
    return "array_set";
  case OP_ARRAY_LEN:
    return "array_len";

  case OP_OBJECT_NEW:
    return "object_new";
  case OP_OBJECT_DEL:
    return "object_del";
  case OP_OBJECT_GET:
    return "object_get";
  case OP_OBJECT_SET:
    return "object_set";
  case OP_OBJECT_LEN:
    return "object_len";

  case OP_CALL:
    return "call";
  case OP_RET:
    return "ret";
  case OP_RET_VOID:
    return "ret_void";

  case OP_LOAD_LIB:
    return "load_lib";

  case OP_HALT:
    return "halt";

  default:
    return "unknown_op";
  }
}

const Value *vm_assertType(VM *vm, const Value *v, ...) {
  if (!v) {
    vm_inst_errorf(NULL, "Value is NULL");
  }

  va_list ap;
  va_start(ap, v);

  bool ok = false;
  ValueType t;

  /* First pass: check if type matches */
  while ((t = va_arg(ap, ValueType)) != VAL_TYPE_END) {
    if (v->type == t) {
      ok = true;
      break;
    }
  }

  va_end(ap);

  if (ok) {
    return v;
  }

  /* Second pass: build error message */
  fprintf(stderr, "Runtime error: expected one of: ");

  va_start(ap, v);
  bool first = true;
  while ((t = va_arg(ap, ValueType)) != VAL_TYPE_END) {
    if (!first)
      fprintf(stderr, ", ");
    fprintf(stderr, "%s", vm_value_type_name(t));
    first = false;
  }
  va_end(ap);

  fprintf(stderr, ", got %s", vm_value_type_name(v->type));
  if (vm) {
    fprintf(stderr, ", at line %ld\n",
            vm->program->inst[vm->ip].source_line_num);
  } else {
    fprintf(stderr, "\n");
  }

  exit(1);
}

void vm_push(VM *vm, Value v) {
  assert(vm->sp < VM_STACK_MAX);
  vm->stack[vm->sp++] = v;
}

Value vm_pop(VM *vm) {
  assert(vm->sp > 0);
  return vm->stack[--vm->sp];
}

int vm_inst_execute(VM *vm, const Inst inst) {
  CallFrame *f = vm->frame_top > 0 ? &vm->frames[vm->frame_top - 1] : NULL;
  switch (inst.type) {
  case OP_NOP:
    vm->ip += 1;
    break;
  case OP_TYPEOF: {
    vm_assert_min_stack(vm, 2);
    vm_push(vm, vm_new_string(vm, vm_value_type_name(vm_pop(vm).type)));
    vm->ip += 1;
    break;
  }
  case OP_PUSH_NIL:
    vm_push(vm, VM_NIL);
    vm->ip += 1;
    break;
  case OP_PUSH_DOUBLE:
    vm_push(vm, vm_new_double(inst.d));
    vm->ip += 1;
    break;
  case OP_PUSH_INT:
    vm_push(vm, vm_new_int(inst.operand));
    vm->ip += 1;
    break;
  case OP_PUSH_STR: {
    vm_push(vm, vm_new_string(vm, inst.chars));
    vm->ip += 1;
    break;
  }
  case OP_PUSH_FN: {
    Callable *fn = (Callable *)vm_gc_alloc(vm, sizeof(Callable), OBJ_CALLABLE);
    fn->type = CALLABLE_USER;
    fn->as.entry_ip = inst.u;
    vm_push(vm, (Value){.type = VAL_CALLABLE, .as.fn = fn});
    vm->ip += 1;
    break;
  }
  case OP_LOAD_LIB: {
    vm_assert_min_stack(vm, 1);
    Value libname = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &libname, VAL_STR);
    // Automatically pushes library value
    if (!vm_load_and_push_lib(libname.as.str->chars, vm)) {
      vm_inst_errorf(&inst, "Load dll error");
      return 1;
    }
    vm->ip += 1;
    break;
  }
  case OP_ADD: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    vm_push(vm, vm_new_int(a.as.i + b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_SUB: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    vm_push(vm, vm_new_int(a.as.i - b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_MUL: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    vm_push(vm, vm_new_int(a.as.i * b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_DIV: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT, VAL_DOUBLE);
    VM_ASSERT_TYPE(vm, &a, VAL_INT, VAL_DOUBLE);
    if ((b.type == VAL_DOUBLE && b.as.d == 0) ||
        (b.type == VAL_INT && b.as.i == 0)) {
      printf("Error Divide by 0");
      exit(1);
    }
    if (a.type == VAL_DOUBLE || b.type == VAL_DOUBLE) {
      if (a.type == VAL_DOUBLE && b.type == VAL_DOUBLE) {
        vm_push(vm, vm_new_double(a.as.d / b.as.d));
      } else if (a.type == VAL_DOUBLE) {
        vm_push(vm, vm_new_double(a.as.d / (double)b.as.i));
      } else {
        vm_push(vm, vm_new_double((double)a.as.i / b.as.d));
      }
    } else {
      vm_push(vm, vm_new_int((int64_t)a.as.i / b.as.i));
    }
    vm->ip += 1;
    break;
  }
  case OP_LT: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    vm_push(vm, vm_new_int(a.as.i < b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_LTE: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    vm_push(vm, vm_new_int(a.as.i <= b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_GT: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    vm_push(vm, vm_new_int(a.as.i > b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_GTE: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    vm_push(vm, vm_new_int(a.as.i >= b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_LEN: {
    vm_assert_min_stack(vm, 1);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &a, VAL_STR);
    vm_push(vm, vm_new_int((int64_t)a.as.str->len));
    vm->ip += 1;
    break;
  }
  case OP_CONCAT: {
    vm_assert_min_stack(vm, 1);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_STR, VAL_INT, VAL_DOUBLE);
    VM_ASSERT_TYPE(vm, &a, VAL_STR, VAL_INT, VAL_DOUBLE);
    char *concat = NULL;
    vm_concat_val_as_string(&a, &b, &concat);
    vm_push(vm, vm_new_string(vm, concat));
    free(concat);
    vm->ip += 1;
    break;
  }
  case OP_EQ: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    vm_push(vm, vm_new_int(vm_val_is_eq(a, b)));
    vm->ip += 1;
    break;
  }
  case OP_NEQ: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    vm_push(vm, vm_new_int(!vm_val_is_eq(a, b)));
    vm->ip += 1;
    break;
  }
  case OP_NOT: {
    vm_assert_min_stack(vm, 1);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    vm_push(vm, vm_new_int(!a.as.i));
    vm->ip += 1;
    break;
  }
  case OP_AND: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    vm_push(vm, vm_new_int(a.as.i && b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_OR: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    vm_push(vm, vm_new_int(a.as.i || b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_POP: {
    vm_assert_min_stack(vm, 1);
    vm_pop(vm);
    vm->ip += 1;
    break;
  }
  case OP_DUP: {
    vm_assert_min_stack(vm, 1);
    Value v = vm->stack[vm->sp - 1];
    vm->stack[vm->sp++] = v;
    vm->ip += 1;
    break;
  }
  case OP_SWAP: {
    vm_assert_min_stack(vm, 2);
    Value a = vm->stack[vm->sp - 2];
    vm->stack[vm->sp - 2] = vm->stack[vm->sp - 1];
    vm->stack[vm->sp - 1] = a;
    vm->ip += 1;
    break;
  }

  case OP_JMP: {
    vm->ip = inst.u;
    break;
  }
  case OP_JNZ: {
    vm_assert_min_stack(vm, 1);
    Value v = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &v, VAL_INT);
    if (v.as.i != 0) {
      vm->ip = inst.u;
    } else {
      vm->ip += 1;
    }
    break;
  }
  case OP_JZ: {
    vm_assert_min_stack(vm, 1);
    Value v = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &v, VAL_INT);
    if (v.as.i == 0) {
      vm->ip = inst.u;
    } else {
      vm->ip += 1;
    }
    break;
  }
  case OP_JL: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    if (a.as.i < b.as.i) {
      vm->ip = inst.u;
    } else {
      vm->ip += 1;
    }
    break;
  }
  case OP_JLE: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    if (a.as.i <= b.as.i) {
      vm->ip = inst.u;
    } else {
      vm->ip += 1;
    }
    break;
  }
  case OP_JGT: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    if (a.as.i > b.as.i) {
      vm->ip = inst.u;
    } else {
      vm->ip += 1;
    }

    break;
  }
  case OP_JGTE: {
    vm_assert_min_stack(vm, 2);
    Value b = vm_pop(vm);
    Value a = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &b, VAL_INT);
    VM_ASSERT_TYPE(vm, &a, VAL_INT);
    if (a.as.i >= b.as.i) {
      vm->ip = inst.u;
    } else {
      vm->ip += 1;
    }
    break;
  }
  case OP_PICK: {
    if (vm->sp <= inst.u) {
      printf("PICK out of range\n");
      exit(1);
    }
    vm_push(vm, vm->stack[vm->sp - 1 - inst.u]);
    vm->ip++;
    break;
  }
  case OP_ROT: {
    if (inst.u < 2 || vm->sp < inst.u) {
      vm_inst_errorf(&inst, "ROT requires %d values in stack\n", inst.u);
    }

    Value first = vm->stack[vm->sp - inst.u];
    for (size_t i = vm->sp - inst.u; i < vm->sp - 1; i++) {
      vm->stack[i] = vm->stack[i + 1];
    }
    vm->stack[vm->sp - 1] = first;

    vm->ip++;
    break;
  }
  case OP_LOAD_GLOBAL: {
    if (inst.u >= VM_GLOBALS_MAX) {
      vm_inst_errorf(&inst,
                     "OP_LOAD_GLOBAL: global index out of range 0<= %ld < %d",
                     inst.u, VM_GLOBALS_MAX);
      exit(1);
    }
    Value copy = vm->globals[inst.u];
    vm_push(vm, copy);
    vm->ip += 1;
    break;
  }
  case OP_STORE_GLOBAL: {
    vm_assert_min_stack(vm, 1);
    if (inst.u >= VM_GLOBALS_MAX) {
      vm_inst_errorf(&inst,
                     "OP_LOAD_GLOBAL: global index out of range 0<= %ld < %d",
                     inst.u, VM_GLOBALS_MAX);
    }

    vm->globals[inst.u] = vm_pop(vm);
    vm->ip += 1;
    break;
  }
  case OP_LOAD: {
    size_t limit = f->argc + f->locals;
    if (inst.u >= limit) {
      vm_inst_errorf(&inst,
                     "OP_LOAD operand should be x>=0 (Overflow/Underflow)");
      exit(1);
    }
    Value copy = vm->stack[vm->fp + inst.u];
    vm_push(vm, copy);
    vm->ip += 1;
    break;
  }
  case OP_STORE: {
    vm_assert_min_stack(vm, 1);
    size_t slot_limit = f->argc + f->locals;
    if (inst.u >= slot_limit) {
      vm_inst_errorf(&inst, "STORE out of frame");
    }

    vm->stack[vm->fp + inst.u] = vm_pop(vm);
    vm->ip += 1;
    break;
  }
  case OP_OBJECT_NEW: {
    vm_push(vm, vm_object_new(vm, 0));
    vm->ip += 1;
    break;
  }
  case OP_OBJECT_SET: {
    vm_assert_min_stack(vm, 3);
    Value v = vm_pop(vm);
    Value key = vm_pop(vm);
    Value obj = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &obj, VAL_OBJECT);
    VM_ASSERT_TYPE(vm, &key, VAL_STR);
    vm_object_set(vm, obj.as.obj, key.as.str->chars, v);
    vm_push(vm, obj);
    vm->ip += 1;
    break;
  }
  case OP_OBJECT_DEL: {
    vm_assert_min_stack(vm, 2);
    Value key = vm_pop(vm);
    Value obj = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &obj, VAL_OBJECT);
    VM_ASSERT_TYPE(vm, &key, VAL_STR);
    vm_object_del(obj.as.obj, key.as.str->chars);
    vm_push(vm, obj);
    vm->ip += 1;
    break;
  }
  case OP_OBJECT_GET: {
    vm_assert_min_stack(vm, 2);
    Value key = vm_pop(vm);
    Value obj = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &obj, VAL_OBJECT);
    VM_ASSERT_TYPE(vm, &key, VAL_STR);
    Value out = {0};
    if (vm_object_get(obj.as.obj, key.as.str->chars, &out)) {
      vm_push(vm, out);
    } else {
      vm_push(vm, VM_NIL);
    }
    vm->ip += 1;
    break;
  }
  case OP_OBJECT_LEN: {
    vm_assert_min_stack(vm, 1);
    Value obj = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &obj, VAL_OBJECT);
    vm_push(vm, vm_new_int((int64_t)obj.as.obj->len));
    vm->ip += 1;
    break;
  }

  case OP_ARRAY_NEW: {
    vm_assert_min_stack(vm, 1);
    Value v = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &v, VAL_INT);
    vm_push(vm, vm_array_new(vm, v.as.u));
    vm->ip += 1;
    break;
  }
  case OP_ARRAY_PUSH: {
    vm_assert_min_stack(vm, 2);
    Value v = vm_pop(vm);
    Value arr = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &arr, VAL_ARRAY);
    vm_array_push(vm, arr.as.arr, v);
    vm_push(vm, arr);
    vm->ip += 1;
    break;
  }
  case OP_ARRAY_DEL: {
    vm_assert_min_stack(vm, 2);
    Value v = vm_pop(vm);
    Value arr = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &arr, VAL_ARRAY);
    VM_ASSERT_TYPE(vm, &v, VAL_INT);
    if (v.as.i < 0 || v.as.i >= (int64_t)arr.as.arr->len) {
      vm_inst_errorf(&inst, "Array index out of bounds, idx:%i len:%d", v.as.i,
                     arr.as.arr->len);
    }
    vm_array_del(arr.as.arr, v.as.u);
    vm_push(vm, arr);
    vm->ip += 1;

    break;
  }
  case OP_ARRAY_GET: {
    vm_assert_min_stack(vm, 2);
    Value v = vm_pop(vm);
    Value arr = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &arr, VAL_ARRAY);
    VM_ASSERT_TYPE(vm, &v, VAL_INT);
    if (v.as.i < 0 || v.as.i > (int64_t)arr.as.arr->len - 1) {
      vm_inst_errorf(&inst, "array_get index out of bounds, idx:%i len:%d",
                     v.as.i, arr.as.arr->len);
    }
    vm_push(vm, arr.as.arr->items[v.as.u]);
    vm->ip += 1;

    break;
  }
  case OP_ARRAY_SET: {
    vm_assert_min_stack(vm, 3);
    Value v = vm_pop(vm);
    Value index = vm_pop(vm);
    Value arr = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &arr, VAL_ARRAY);
    VM_ASSERT_TYPE(vm, &index, VAL_INT);
    if (index.as.i < 0) {
      vm_inst_errorf(&inst, "array_set index out of bounds, idx:%i len:%d",
                     index.as.i, arr.as.arr->len);
    }
    vm_array_set(vm, arr.as.arr, index.as.u, v);
    // if (index.as.u > arr.as.arr->len) {
    //   arr.as.arr->len = index.as.u + 1;
    // }
    vm_push(vm, arr);
    vm->ip += 1;

    break;
  }
  case OP_ARRAY_LEN: {
    vm_assert_min_stack(vm, 1);
    Value arr = vm_pop(vm);
    VM_ASSERT_TYPE(vm, &arr, VAL_ARRAY);
    vm_push(vm, vm_new_int((int64_t)arr.as.arr->len));
    vm->ip += 1;
    break;
  }

  case OP_ENTER: {
    f->locals = inst.u;
    for (size_t i = 0; i < inst.u; i++)
      vm_push(vm, VM_NIL);
    vm->ip += 1;
    break;
  }
  case OP_CALL: {
    assert(vm->frame_top < VM_CALL_MAX);
    size_t argc = inst.u;
    if (argc > 0) {
      vm_assert_min_stack(vm, argc + 1);
    }
    // callable + argc
    size_t base = vm->sp - (argc + 1);
    size_t argv_sp = base + 1;
    Value callee = vm->stack[base];
    if (callee.type != VAL_CALLABLE) {
      vm_inst_errorf(&inst, "Calling a non function");
    }
    if (callee.as.fn->type == CALLABLE_NATIVE) {
      Value res = callee.as.fn->as.native(vm, argc, &vm->stack[argv_sp]);
      vm->sp = base;
      vm_push(vm, res);
      vm->ip += 1;
      break;
    }

    // ---- create function frame ----
    vm->frames[vm->frame_top++] = (CallFrame){
        .return_ip = vm->ip + 1,
        .old_fp = vm->fp,
        .argc = argc,
        .locals = 0,
    };
    // Remove function value, keep args
    if (argc > 0) {
      memmove(&vm->stack[base], &vm->stack[argv_sp], argc * sizeof(Value));
    }
    vm->sp -= 1;
    // Make arguments become locals
    vm->fp = base;
    vm->ip = callee.as.fn->as.entry_ip;
    break;
  }
  case OP_RET: {
    Value ret;
    // TODO: make exitCode for halt
    if (vm->frame_top == 1) {
      // Ret from main
      // if (vm->sp < 1) {
      //   // no exit code given
      //   vm->halt = true;
      // }
      // ret = vm_pop(vm);
      // if (ret.type == VAL_INT) {
      //   if (ret.as.i == 0) {
      //     vm->halt = true;
      //
      //   } else {
      vm->halt = true;

      // exit((int)ret.as.i);
      // }
    }
    if (vm->frame_top == 0) {
      vm_inst_errorf(&inst, "RET must be used in function");
    }
    vm_assert_min_stack(vm, 1);
    ret = vm_pop(vm);
    vm->sp = vm->fp;
    CallFrame frame = vm->frames[--vm->frame_top];
    vm->fp = frame.old_fp;
    vm->ip = frame.return_ip;
    vm_push(vm, ret);
    break;
  }

  case OP_RET_VOID: {
    if (vm->frame_top == 0) {
      exit(0);
    }
    vm->sp = vm->fp;
    CallFrame frame = vm->frames[--vm->frame_top];
    vm->fp = frame.old_fp;
    vm->ip = frame.return_ip;
    break;
  }
  case OP_HALT:
    vm->halt = true;
    break;
  default: {
    printf("Unkown instruction");
    exit(1);
  }
  }
  return 0;
}

void vm_print_inst(const Inst *in) {
  printf("Inst { ");

  printf("type=%s", vm_opcode_name(in->type));

  printf(", line=%ld", in->source_line_num);
  if (in->type == OP_PUSH_STR) {
    printf(", chars=\"%s\"", in->chars);
  } else {
    printf(", operand=%lld", (long long)in->operand);
  }
  if (in->type == OP_CALL) {
    printf(", argc=%lld", (long long)in->u);
  }
  printf(" }\n");
}

void vm_run_program(VM *vm) {
  while (!vm->halt && (vm->ip < vm->program->inst_count &&
                       vm->program->inst[vm->ip].type != OP_HALT)) {
    // printf("execute ip %d\n", vm->ip);
    // vm_print_inst(&vm->program->inst[vm->ip]);
    Inst *previnst = &vm->program->inst[vm->ip];
    // vm_gc_pause(vm);
    int next_ip = vm_inst_execute(vm, vm->program->inst[vm->ip]);
    if (next_ip != 0) {
      if (previnst) {
        printf("Error on instruction at\n");
        vm_print_inst(previnst);
      }
      break;
    }
    vm_gc_collect_if_needed(vm);
  }
}

size_t vm_align_up(size_t n, size_t align) {
  return (n + align - 1) & ~(align - 1);
}

void vm_gc_mark_value(Value v) {
  switch (v.type) {
  case VAL_STR:
    vm_gc_mark_obj((HeapObj *)v.as.str);
    break;

  case VAL_ARRAY:
    vm_gc_mark_obj((HeapObj *)v.as.arr);
    break;

  case VAL_OBJECT:
    vm_gc_mark_obj((HeapObj *)v.as.obj);
    break;

  case VAL_CALLABLE:
    vm_gc_mark_obj((HeapObj *)v.as.fn);
    break;
  case VAL_INT:
  case VAL_NIL:
  case VAL_DOUBLE:
  case VAL_TYPE_END:
  default:
    break;
  }
}

void vm_gc_mark_obj(HeapObj *obj) {
  if (!obj || obj->marked)
    return;

  obj->marked = true;

  switch ((ObjKind)obj->kind) {
  case OBJ_STRING:
  case OBJ_CALLABLE:
    break;

  case OBJ_ARRAY: {
    Array *a = (Array *)obj;
    for (size_t i = 0; i < a->len; i++) {
      vm_gc_mark_value(a->items[i]);
    }
    break;
  }

  case OBJ_OBJECT: {
    Object *o = (Object *)obj;
    for (size_t i = 0; i < o->len; i++) {
      vm_gc_mark_value(o->entries[i].value);
    }

    if (o->proto) {
      vm_gc_mark_obj((HeapObj *)o->proto);
    }
    break;
  }
  }
}
void vm_gc_mark_roots(VM *vm) {
  for (size_t i = 0; i < vm->sp; i++) {
    vm_gc_mark_value(vm->stack[i]);
  }
  for (size_t i = 0; i < VM_GLOBALS_MAX; i++) {
    vm_gc_mark_value(vm->globals[i]);
  }
}

void vm_gc_sweep(VM *vm) {
  HeapObj **obj = &vm->heap.objects;

  while (*obj) {
    if (!(*obj)->marked) {
      HeapObj *unreached = *obj;
      *obj = unreached->next;
      vm_gc_free_obj(vm, unreached);
    } else {
      (*obj)->marked = false; // reset for next cycle
      obj = &(*obj)->next;
    }
  }
}
void vm_gc_sweep_all(VM *vm) {
  HeapObj *obj = vm->heap.objects;

  while (obj) {
    HeapObj *next = obj->next;
    vm_gc_free_obj(vm, obj);
    obj = next;
  }

  vm->heap.objects = NULL;
}

void vm_gc_collect(VM *vm) {
  // 1. mark
  vm_gc_mark_roots(vm);
  // 2. sweep
  vm_gc_sweep(vm);
  // 3. reset threshold
  vm->heap.next_gc = vm->heap.bytes_allocated * VM_GC_GROW_FACTOR;
}

void *vm_gc_alloc(VM *vm, size_t size, ObjKind kind) {
  if (!kind) {
    vm_panic("Memory allocation needs object kind");
  }
  if (vm->heap.bytes_allocated + size > vm->heap.next_gc) {
    vm->gc_requested = true;
  }
  HeapObj *obj = malloc(size);
  if (!obj) {
    vm_panic("Cannot allocate memory: %s", strerror(errno));
  }
  obj->kind = kind;
  obj->marked = false;
  obj->size = size;

  obj->next = vm->heap.objects;
  vm->heap.objects = obj;

  vm->heap.object_count++;
  vm->heap.bytes_allocated += size;
  return obj;
}

void vm_gc_free_obj(VM *vm, HeapObj *obj) {
  if (!obj)
    return;
  vm->heap.bytes_allocated -= obj->size;
  vm->heap.object_count--;
  switch (obj->kind) {
  case OBJ_OBJECT: {
    Object *o = (Object *)obj;
    for (size_t i = 0; i < o->len; i++) {
      free(o->entries[i].key);
    }
    if (o->entries) {
      free(o->entries);
    }
    free(o);
    break;
  }
  case OBJ_ARRAY: {
    Array *a = (Array *)obj;
    vm->heap.bytes_allocated -= a->cap * sizeof(Value);
    if (a->items) {
      free(a->items);
    }
    free(obj);
    break;
  }
  case OBJ_STRING:
  case OBJ_CALLABLE:
    free(obj);
    break;
  default:
    vm_panic("Unknown heap object kind");
    break;
  }
}

void vm_gc_collect_if_needed(VM *vm) {
  if (!vm->gc_requested)
    return;
  if (vm->gc_pause_count > 0)
    return;

  vm->gc_requested = false;
  vm_gc_collect(vm);
}

void vm_gc_pause(VM *vm) { vm->gc_pause_count++; }
void vm_gc_resume(VM *vm) {
  if (vm->gc_pause_count == 0)
    return;

  vm->gc_pause_count--;

  if (vm->gc_pause_count == 0 && vm->gc_requested) {
    vm->gc_requested = false;
    vm_gc_collect(vm);
  }
}

void vm_free_program(Program *p) {
  for (size_t i = 0; i < p->inst_count; i++) {
    Inst inst = p->inst[i];
    if (inst.type == OP_PUSH_STR) {
      free(inst.chars);
    }
  }
  if (p->inst != NULL) {
    free(p->inst);
  }
  free(p);
}

char *vm_strdup(const char *src) {
  size_t len = strlen(src) + 1;    // String plus '\0'
  char *dst = (char *)malloc(len); // Allocate space
  if (dst == NULL)
    return NULL;         // No memory
  memcpy(dst, src, len); // Copy the block
  return dst;            // Return the new string
}

void vm_inst_errorf(const Inst *inst, const char *fmt, ...) {
  fprintf(stderr, "\n=== RUNTIME ERROR ===\n");

  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  if (inst) {
    printf("instruction at line %ld", inst->source_line_num);
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "========================\n");
  exit(1);
}

void vm_panic(const char *fmt, ...) {
  fprintf(stderr, "\n=== ERROR ===\n");

  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  fprintf(stderr, "========================\n");
  exit(1);
}

char *vm_value_type_name(ValueType type) {
  if (!type)
    return "undefined";
  switch (type) {
  case VAL_NIL:
    return "nil";
  case VAL_INT:
    return "int";
  case VAL_DOUBLE:
    return "double";
  case VAL_STR:
    return "string";
  case VAL_ARRAY:
    return "array";
  case VAL_OBJECT:
    return "object";
  case VAL_CALLABLE:
    return "function";
  case VAL_TYPE_END:
    vm_inst_errorf(NULL, "type_end should never fire in vm_value_type_name");
    break;
  default:
    vm_inst_errorf(NULL, "undeclared vm_value_type_name");
    break;
  }
  return "undefined";
}

void vm_assert_min_stack(VM *vm, size_t min) {
  if (vm->sp >= min)
    return;
  vm_inst_errorf(NULL,
                 "Stack Underflow -- instruction need %ld values got %ld, "
                 "at line %ld (%s)",
                 min, vm->sp, vm->program->inst[vm->ip].source_line_num,
                 vm_opcode_name(vm->program->inst[vm->ip].type));
}

void vm_halt(VM *vm) { vm->halt = true; }

inline int vm_val_is_numeric(Value v) {
  return v.type == VAL_INT || v.type == VAL_DOUBLE;
}
inline double vm_val_to_double(Value v) {
  return v.type == VAL_INT ? (double)v.as.i : v.as.d;
}
const char *vm_string_chars(String *s) { return s ? s->chars : ""; }
size_t vm_string_len(String *s) { return s ? s->len : 0; }

inline int vm_val_is_eq(Value a, Value b) {
  /* numeric cross-type equality */
  if (vm_val_is_numeric(a) && vm_val_is_numeric(b)) {
    return vm_val_to_double(a) == vm_val_to_double(b);
  }

  /* non-numeric types must match exactly */
  if (a.type != b.type) {
    return 0;
  }

  switch (a.type) {
  case VAL_CALLABLE:
    return a.as.fn == b.as.fn;

  case VAL_NIL:
    return 1;
  case VAL_ARRAY:
    return a.as.arr == b.as.arr;
  case VAL_OBJECT:
    return a.as.obj == b.as.obj;

  case VAL_STR:
    return strcmp(a.as.str->chars, b.as.str->chars) == 0;

  // Handled already,just to supress varning
  case VAL_INT:
  case VAL_DOUBLE:
  case VAL_TYPE_END:
    vm_inst_errorf(NULL, "This should never fire");
    return 0;
  default:
    return 0;
  }
}
// Useful when debugging
void vm_print_stack_top(const VM *vm) {
  printf("=== STACK (top 20) sp[%ld] ===\n", vm->sp);

  size_t count = vm->sp < 20 ? vm->sp : 20;
  for (size_t i = 0; i < count; i++) {
    size_t idx = vm->sp - 1 - i;
    Value v = vm->stack[idx];
    // +1 from idx
    if (idx == vm->fp - 1) {
      printf("---------fp[%ld]---------\n", vm->fp);
    }
    printf("[%2ld] ", idx);

    switch (v.type) {
    case VAL_CALLABLE:
      if (v.as.fn->type == CALLABLE_NATIVE) {
        printf("func %s()\n", v.as.fn->name);
      } else {
        printf("func ip:%ld()\n", v.as.fn->as.entry_ip);
      }
      break;
    case VAL_NIL:
      printf("NIL\n");
      break;
    case VAL_INT:
      printf("INT    %ld\n", v.as.i);
      break;
    case VAL_DOUBLE:
      printf("DOUBLE    %f\n", v.as.d);
      break;
    case VAL_ARRAY:
      printf("ARRAY(%ld)\n", v.as.arr->len);
      break;
    case VAL_OBJECT:
      printf("OBJECT(%ld)\n", v.as.obj->len);
      break;

    case VAL_STR:
      printf("STRING \"%s\"\n", v.as.str->chars);
      break;
    case VAL_TYPE_END:
    default:
      printf("UNKNOWN\n");
      break;
    }
  }

  if (vm->sp == 0)
    printf("(stack empty)\n");

  printf("======================\n");
}

Value vm_new_int(int64_t val) { return (Value){.type = VAL_INT, .as.i = val}; }

Value vm_new_double(double val) {
  return (Value){.type = VAL_DOUBLE, .as.d = val};
}

Value vm_object_new(VM *vm, size_t initial_cap) {
  Object *o = vm_gc_alloc(vm, sizeof(Object), OBJ_OBJECT);
  size_t cap = initial_cap < 1 ? 8 : initial_cap;
  size_t cap_bytes = cap * sizeof(ObjEntry);
  o->len = 0;
  o->proto = NULL;
  o->cap = cap;
  o->entries = malloc(cap_bytes);
  vm->heap.bytes_allocated += cap_bytes;
  return (Value){.type = VAL_OBJECT, .as.obj = o};
}

void vm_object_grow(VM *vm, Object *o, size_t needed_size) {
  if (needed_size <= o->cap)
    return;
  size_t old_bytes = o->cap * sizeof(ObjEntry);
  size_t new_cap = o->cap < 1 ? 1 : o->cap;
  while (new_cap < needed_size) {
    new_cap *= 2;
  }
  size_t new_bytes = new_cap * sizeof(ObjEntry);
  ObjEntry *entries = realloc(o->entries, new_cap * sizeof(ObjEntry));
  if (!entries) {
    vm_panic("object: failed to grow");
  }
  memset(entries + o->cap, 0, (new_cap - o->cap) * sizeof(ObjEntry));

  o->entries = entries;
  o->cap = new_cap;
  vm->heap.bytes_allocated += new_bytes - old_bytes;
}

void vm_object_set(VM *vm, Object *o, const char *key, Value v) {
  // overwrite if exists
  for (size_t i = 0; i < o->len; i++) {
    if (strcmp(o->entries[i].key, key) == 0) {
      o->entries[i].value = v;
      return;
    }
  }

  // grow if needed
  if (o->len + 1 > o->cap) {
    vm_object_grow(vm, o, o->len + 1);
  }
  o->entries[o->len].key = vm_strdup(key);
  o->entries[o->len].value = v;
  o->len++;
}

bool vm_object_get(Object *o, const char *key, Value *out) {
  for (size_t i = 0; i < o->len; i++) {
    if (strcmp(o->entries[i].key, key) == 0) {
      *out = o->entries[i].value;
      return true;
    }
  }

  if (o->proto) {
    return vm_object_get(o->proto, key, out);
  }

  return false;
}

void vm_object_del(Object *o, const char *key) {
  for (size_t i = 0; i < o->len; i++) {
    if (strcmp(o->entries[i].key, key) == 0) {
      free(o->entries[i].key);
      for (size_t j = i; j < o->len - 1; j++) {
        o->entries[j] = o->entries[j + 1];
      }
      o->len--;
      return;
    }
  }

  // not found → no-op
}

size_t vm_array_len(Array *a) {
  if (!a)
    return 0;
  return a->len;
}

Value vm_array_new(VM *vm, size_t initial_cap) {
  Array *a = vm_gc_alloc(vm, sizeof(Array), OBJ_ARRAY);
  size_t cap = initial_cap < 1 ? 1 : initial_cap;
  size_t cap_bytes = cap * sizeof(Value);
  a->len = 0;
  a->cap = cap;
  a->items = calloc(cap, sizeof(Value));
  vm->heap.bytes_allocated += cap_bytes;
  return (Value){.type = VAL_ARRAY, .as.arr = a};
}

void vm_array_grow_capacity(VM *vm, Array *a, size_t needed_size) {
  size_t new_cap = needed_size < 1 ? 8 : a->cap;
  if (new_cap < a->cap)
    return;

  size_t old_bytes = a->cap * sizeof(Value);
  while (new_cap < needed_size) {
    new_cap *= 2;
  }
  size_t new_bytes = new_cap * sizeof(Value);
  Value *items = realloc(a->items, new_bytes);
  if (!items) {
    vm_panic("Cannot alloc memory: array grow failed");
  }

  memset(items + a->cap, 0, (new_cap - a->cap) * sizeof(Value));

  a->items = items;
  a->cap = new_cap;
  vm->heap.bytes_allocated += new_bytes - old_bytes;
}

void vm_array_set(VM *vm, Array *a, size_t index, Value v) {
  if (index >= a->cap) {
    size_t new_cap = a->cap ? a->cap * 2 : 4;
    vm_array_grow_capacity(vm, a, new_cap);
  }
  if (index >= a->len) {
    // if its adds a new elements
    a->len++;
  }
  a->items[index] = v;
}

void vm_array_push(VM *vm, Array *a, Value v) {
  if (a->len >= a->cap) {
    size_t new_cap = a->cap ? a->cap * 2 : 4;
    vm_array_grow_capacity(vm, a, new_cap);
  }
  a->items[a->len++] = v;
}

void vm_array_del(Array *a, size_t index) {
  if (index + 1 > a->len && a->len > 0) {
    vm_inst_errorf(NULL,
                   "Array try to delete item out of bound, idx:%ld len:%ld",
                   index, a->len);
  }
  if (index != a->len - 1) {
    for (size_t i = index; i < a->len - 1; i++) {
      Value next = a->items[i + 1];
      a->items[i] = next;
    }
  }
  a->len -= 1;
}

String *vm_malloc_string(VM *vm, const char *s) {
  const char *p = s;
  size_t len = 0;

  /* -------- pass 1: compute decoded length -------- */
  while (*p) {
    if (*p == '\\' && p[1]) {
      p++; // skip escape marker
    }
    len++;
    p++;
  }
  size_t total_size = sizeof(String) + len + 1;

  String *str = (String *)vm_gc_alloc(vm, total_size, OBJ_STRING);
  str->len = len;
  /* -------- pass 2: decode escapes -------- */
  char *out = str->chars;
  p = s;

  while (*p) {
    if (*p == '\\') {
      p++;
      switch (*p) {
      case 'n':
        *out++ = '\n';
        break;
      case 't':
        *out++ = '\t';
        break;
      case '\\':
        *out++ = '\\';
        break;
      case '"':
        *out++ = '"';
        break;
      default:
        /* unknown escape → keep literal */
        *out++ = *p;
        break;
      }
    } else {
      *out++ = *p;
    }
    p++;
  }

  *out = '\0';

  return str;
}

Value vm_new_string(VM *vm, char *str) {
  return (Value){.type = VAL_STR, .as.str = vm_malloc_string(vm, str)};
}

void vm_sb_init(StrBuf *b) {
  b->buf = NULL;
  b->len = 0;
  b->cap = 0;
}

void vm_sb_reserve(StrBuf *b, size_t need) {
  if (b->len + need <= b->cap)
    return;

  size_t new_cap = b->cap ? b->cap * 2 : 16;
  while (new_cap < b->len + need)
    new_cap *= 2;

  char *p = realloc(b->buf, new_cap);
  if (!p) {
    printf("vm_sb_reserve realloc error");
    exit(1);
  }
  b->buf = p;
  b->cap = new_cap;
}

// void vm_sb_push_char(StrBuf *b, char c) {
//   vm_sb_reserve(b, 1);
//   b->buf[b->len++] = c;
// }

// void vm_sb_append(StrBuf *b, const char *s) {
//   size_t n = strlen(s);
//   vm_sb_reserve(b, n);
//   memcpy(b->buf + b->len, s, n);
//   b->len += n;
// }
size_t vm_value_char_len(Value *v) {
  if (!v) {
    vm_inst_errorf(NULL, "vm_value_char_len needs a value");
  }
  int len = 0;
  switch (v->type) {
  case VAL_INT:
    len = snprintf(NULL, 0, "%d", (int)v->as.i);
    break;
  case VAL_DOUBLE:
    len = snprintf(NULL, 0, "%f", v->as.d);
    break;
  case VAL_STR:
    len = (int)v->as.str->len;
    break;
  case VAL_CALLABLE:
  case VAL_ARRAY:
  case VAL_OBJECT:
  // fallthrough
  case VAL_NIL:
  case VAL_TYPE_END:
  default:
    vm_inst_errorf(NULL, "%s type as string length can not be used",
                   vm_value_type_name(v->type));
    break;
  }
  if (len < 0) {
    vm_inst_errorf(NULL, "unkown error at %s type string length conversion",
                   vm_value_type_name(v->type));
  }
  return (size_t)len;
}

size_t vm_append_value_as_str(char *out, size_t cap, size_t off, Value *v) {
  switch (v->type) {
  case VAL_INT:
    return off + (size_t)snprintf(out + off, cap - off, "%d", (int)v->as.i);
  case VAL_DOUBLE:
    return off + (size_t)snprintf(out + off, cap - off, "%f", v->as.d);
  case VAL_STR:
    return off + (size_t)snprintf(out + off, cap - off, "%.*s",
                                  (int)v->as.str->len, v->as.str->chars);
  case VAL_CALLABLE:
  case VAL_ARRAY:
  case VAL_OBJECT:
  // fallthrough
  case VAL_NIL:
  case VAL_TYPE_END:
  default:
    return off;
  }
}

void vm_concat_val_as_string(Value *a, Value *b, char **out) {
  size_t alen = vm_value_char_len(a);
  size_t blen = vm_value_char_len(b);
  size_t size = sizeof(char) * (alen + blen + 1);
  size_t off = 0;
  *out = malloc(size);
  off = vm_append_value_as_str(*out, size, off, a);
  off = vm_append_value_as_str(*out, size, off, b);
}

size_t vm_gc_allocated(VM *vm) { return vm->heap.bytes_allocated; }
size_t vm_gc_next_bytes(VM *vm) { return vm->heap.next_gc; }

bool vm_has_shared_ext(const char *path) {
  const char *dot = strrchr(path, '.');
  if (!dot)
    return false;

#ifdef _WIN32
  return strcmp(dot, ".dll") == 0;
#else
  return strcmp(dot, ".so") == 0;
#endif
}

Value vm_make_native(VM *vm, const char *name, NativeFn fn) {
  // NOTE: Callable must be pointer inside Value (Value.as.fn is Callable*)
  Callable *c = (Callable *)vm_gc_alloc(vm, sizeof(Callable), OBJ_CALLABLE);
  c->name = name;
  c->type = CALLABLE_NATIVE;
  c->as.native = fn; // NativeFn is already a function pointer typedef
  return (Value){.type = VAL_CALLABLE, .as.fn = c};
}
/*
 * Loads a shared library and calls PROLETER_LIB_INIT_FN(vm).
 * uses memcpy isntead of casting because of ISO C standarts
 * Returns 0 on success, -1 on failure.
 */
bool vm_load_and_push_lib(const char *path, VM *vm) {
#ifdef _WIN32
  const char *ext = ".dll";
#else
  const char *ext = ".so";
#endif

  char *fullpath = NULL;

  if (vm_has_shared_ext(path)) {
    fullpath = vm_strdup(path);
  } else {
    size_t len = strlen(path) + strlen(ext) + 1;
    fullpath = malloc(len);
    if (!fullpath)
      return false;

    strcpy(fullpath, path);
    strcat(fullpath, ext);
  }
  void *handle;
  void *sym;
  init_lib_fn init_lib_handle;
  vm_api_version_fn get_ver_handle;

  dlerror();
  handle = dlopen(fullpath, RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    fprintf(stderr, "dlopen failed: %s\n", dlerror());
    return false;
  }

  sym = dlsym(handle, "vm_api_version");
  memmove(&get_ver_handle, &sym, sizeof(get_ver_handle));
  if (!sym) {
    // reject: no version function
    vm_panic("Import %s does not have a valid api version", path);
  }

  int lib_ver = get_ver_handle();

  if (lib_ver != PROLETER_API_VERSION) {
    vm_panic("Import %s api version mismatch, required version is %d got %d",
             path, PROLETER_API_VERSION, lib_ver);
  }

  sym = dlsym(handle, PROLETER_LIB_INIT_NAME);
  if (!sym) {
    fprintf(stderr, "dlsym failed: %s\n", dlerror());
    dlclose(handle);
    vm_panic("Import %s does not have register function", path);
    return false;
  }

  memmove(&init_lib_handle, &sym, sizeof(init_lib_handle));

  Value res = init_lib_handle(vm);
  vm_push(vm, res);

  free(fullpath);
  return true;
}
