#ifndef VM_IMP_H
#define VM_IMP_H
#include "vm_builtins.h"
#include "vm_structs.h"
#include "vm_utils.h"
#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static int vm_inst_execute(VM *vm, const Inst inst) {
  CallFrame *f = vm->frame_top > 0 ? &vm->frames[vm->frame_top - 1] : NULL;
  switch (inst.type) {
  case OP_NOP:
    vm->ip += 1;
    break;
  case OP_TYPEOF: {
    assert_min_stack(vm, 2);
    push(vm, vm_new_string(vm, typeof_value(pop(vm).type)));
    vm->ip += 1;
    break;
  }
  case OP_PUSH_NIL:
    push(vm, VM_NIL);
    vm->ip += 1;
    break;
  case OP_PUSH_DOUBLE:
    push(vm, vm_new_double(inst.d));
    vm->ip += 1;
    break;
  case OP_PUSH_INT:
    push(vm, vm_new_int(inst.operand));
    vm->ip += 1;
    break;
  case OP_PUSH_STR: {
    push(vm, (Value){.type = VAL_STR, .as.str = inst.str});
    vm->ip += 1;
    break;
  }
  case OP_ADD: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &a, VAL_INT);
    push(vm, vm_new_int(a.as.i + b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_SUB: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    ASSERT_TYPE(vm, &a, VAL_INT);
    push(vm, vm_new_int(a.as.i - b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_MUL: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    ASSERT_TYPE(vm, &a, VAL_INT);
    push(vm, vm_new_int(a.as.i * b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_DIV: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    ASSERT_TYPE(vm, &a, VAL_INT);
    if (b.as.i == 0) {
      printf("Divide by 0 error");
      exit(1);
    }
    push(vm, vm_new_int((int64_t)a.as.i / b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_LT: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    ASSERT_TYPE(vm, &a, VAL_INT);
    push(vm, vm_new_int(a.as.i < b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_LTE: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    ASSERT_TYPE(vm, &a, VAL_INT);
    push(vm, vm_new_int(a.as.i <= b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_GT: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    ASSERT_TYPE(vm, &a, VAL_INT);
    push(vm, vm_new_int(a.as.i > b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_GTE: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    ASSERT_TYPE(vm, &a, VAL_INT);
    push(vm, vm_new_int(a.as.i >= b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_LEN: {
    assert_min_stack(vm, 1);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &a, VAL_STR);
    push(vm, vm_new_int((int64_t)a.as.str->len));
    vm->ip += 1;
    break;
  }
  case OP_CONCAT: {
    assert_min_stack(vm, 1);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_STR, VAL_INT, VAL_DOUBLE);
    ASSERT_TYPE(vm, &a, VAL_STR, VAL_INT, VAL_DOUBLE);
    char *concat = NULL;
    concat_val_as_string(&a, &b, &concat);
    push(vm, vm_new_string(vm, concat));
    free(concat);
    vm->ip += 1;
    break;
  }
  case OP_EQ: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    push(vm, vm_new_int(val_eq(a, b)));
    vm->ip += 1;
    break;
  }
  case OP_NEQ: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    push(vm, vm_new_int(!val_eq(a, b)));
    vm->ip += 1;
    break;
  }
  case OP_NOT: {
    assert_min_stack(vm, 1);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &a, VAL_INT);
    push(vm, vm_new_int(!a.as.i));
    vm->ip += 1;
    break;
  }
  case OP_AND: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    ASSERT_TYPE(vm, &a, VAL_INT);
    push(vm, vm_new_int(a.as.i && b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_OR: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    ASSERT_TYPE(vm, &a, VAL_INT);
    push(vm, vm_new_int(a.as.i || b.as.i));
    vm->ip += 1;
    break;
  }
  case OP_POP: {
    assert_min_stack(vm, 1);
    pop(vm);
    vm->ip += 1;
    break;
  }
  case OP_DUP: {
    assert_min_stack(vm, 1);
    Value v = vm->stack[vm->sp - 1];
    vm->stack[vm->sp++] = v;
    vm->ip += 1;
    break;
  }
  case OP_SWAP: {
    assert_min_stack(vm, 2);
    Value a = vm->stack[vm->sp - 2];
    vm->stack[vm->sp - 2] = vm->stack[vm->sp - 1];
    vm->stack[vm->sp - 1] = a;
    vm->ip += 1;
    break;
  }

  case OP_JMP: {
    vm->ip = inst.ref;
    break;
  }
  case OP_JNZ: {
    assert_min_stack(vm, 1);
    Value v = pop(vm);
    ASSERT_TYPE(vm, &v, VAL_INT);
    if (v.as.i != 0) {
      vm->ip = inst.ref;
    } else {
      vm->ip += 1;
    }
    break;
  }
  case OP_JZ: {
    assert_min_stack(vm, 1);
    Value v = pop(vm);
    ASSERT_TYPE(vm, &v, VAL_INT);
    if (v.as.i == 0) {
      vm->ip = inst.ref;
    } else {
      vm->ip += 1;
    }
    break;
  }
  case OP_JL: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    ASSERT_TYPE(vm, &a, VAL_INT);
    if (a.as.i < b.as.i) {
      vm->ip = inst.ref;
    } else {
      vm->ip += 1;
    }
    break;
  }
  case OP_JLE: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    ASSERT_TYPE(vm, &a, VAL_INT);
    if (a.as.i <= b.as.i) {
      vm->ip = inst.ref;
    } else {
      vm->ip += 1;
    }
    break;
  }
  case OP_JGT: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    ASSERT_TYPE(vm, &a, VAL_INT);
    if (a.as.i > b.as.i) {
      vm->ip = inst.ref;
    } else {
      vm->ip += 1;
    }

    break;
  }
  case OP_JGTE: {
    assert_min_stack(vm, 2);
    Value b = pop(vm);
    Value a = pop(vm);
    ASSERT_TYPE(vm, &b, VAL_INT);
    ASSERT_TYPE(vm, &a, VAL_INT);
    if (a.as.i >= b.as.i) {
      vm->ip = inst.ref;
    } else {
      vm->ip += 1;
    }
    break;
  }
  case OP_PICK: {
    if (vm->sp <= inst.ref) {
      printf("PICK out of range\n");
      exit(1);
    }
    push(vm, vm->stack[vm->sp - 1 - inst.ref]);
    vm->ip++;
    break;
  }
  case OP_ROT: {
    if (inst.ref < 2 || vm->sp < inst.ref) {
      vm_runtime_errorf(&inst, "ROT requires %d values in stack\n", inst.ref);
    }

    Value first = vm->stack[vm->sp - inst.ref];
    for (size_t i = vm->sp - inst.ref; i < vm->sp - 1; i++) {
      vm->stack[i] = vm->stack[i + 1];
    }
    vm->stack[vm->sp - 1] = first;

    vm->ip++;
    break;
  }
  case OP_LOAD_GLOBAL: {
    if (inst.ref >= GLOBALS_MAX) {
      vm_runtime_errorf(
          &inst, "OP_LOAD_GLOBAL: global index out of range 0<= %ld < %d",
          inst.ref, GLOBALS_MAX);
      exit(1);
    }
    Value copy = vm->globals[inst.ref];
    push(vm, copy);
    vm->ip += 1;
    break;
  }
  case OP_STORE_GLOBAL: {
    assert_min_stack(vm, 1);
    if (inst.ref >= GLOBALS_MAX) {
      vm_runtime_errorf(
          &inst, "OP_LOAD_GLOBAL: global index out of range 0<= %ld < %d",
          inst.ref, GLOBALS_MAX);
    }

    vm->globals[inst.ref] = pop(vm);
    vm->ip += 1;
    break;
  }
  case OP_LOAD: {
    size_t limit = f->argc + f->locals;
    if (inst.ref >= limit) {
      vm_runtime_errorf(&inst,
                        "OP_LOAD operand should be x>=0 (Overflow/Underflow)");
      exit(1);
    }
    Value copy = vm->stack[vm->fp + inst.ref];
    push(vm, copy);
    vm->ip += 1;
    break;
  }
  case OP_STORE: {
    assert_min_stack(vm, 1);
    size_t slot_limit = f->argc + f->locals;
    if (inst.ref >= slot_limit) {
      vm_runtime_errorf(&inst, "STORE out of frame");
    }

    vm->stack[vm->fp + inst.ref] = pop(vm);
    vm->ip += 1;
    break;
  }
  case OP_OBJECT_NEW: {
    push(vm, vm_object_new(vm, 0));
    vm->ip += 1;
    break;
  }
  case OP_OBJECT_SET: {
    assert_min_stack(vm, 3);
    Value v = pop(vm);
    Value key = pop(vm);
    Value obj = pop(vm);
    ASSERT_TYPE(vm, &obj, VAL_OBJECT);
    ASSERT_TYPE(vm, &key, VAL_STR);
    vm_object_set(vm, obj.as.obj, key.as.str->chars, v);
    push(vm, obj);
    vm->ip += 1;
    break;
  }
  case OP_OBJECT_DEL: {
    assert_min_stack(vm, 2);
    Value key = pop(vm);
    Value obj = pop(vm);
    ASSERT_TYPE(vm, &obj, VAL_OBJECT);
    ASSERT_TYPE(vm, &key, VAL_STR);
    vm_object_del(obj.as.obj, key.as.str->chars);
    push(vm, obj);
    vm->ip += 1;
    break;
  }
  case OP_OBJECT_GET: {
    assert_min_stack(vm, 2);
    Value key = pop(vm);
    Value obj = pop(vm);
    ASSERT_TYPE(vm, &obj, VAL_OBJECT);
    ASSERT_TYPE(vm, &key, VAL_STR);
    Value out = {0};
    if (vm_object_get(obj.as.obj, key.as.str->chars, &out)) {
      push(vm, out);
    } else {
      push(vm, VM_NIL);
    }
    vm->ip += 1;
    break;
  }
  case OP_OBJECT_LEN: {
    assert_min_stack(vm, 1);
    Value obj = pop(vm);
    ASSERT_TYPE(vm, &obj, VAL_OBJECT);
    push(vm, vm_new_int((int64_t)obj.as.obj->len));
    vm->ip += 1;
    break;
  }

  case OP_ARRAY_NEW: {
    assert_min_stack(vm, 1);
    Value v = pop(vm);
    ASSERT_TYPE(vm, &v, VAL_INT);
    push(vm, vm_array_new(vm, v.as.u));
    vm->ip += 1;
    break;
  }
  case OP_ARRAY_PUSH: {
    assert_min_stack(vm, 2);
    Value v = pop(vm);
    Value arr = pop(vm);
    ASSERT_TYPE(vm, &arr, VAL_ARRAY);
    vm_array_push(vm, arr.as.arr, v);
    push(vm, arr);
    vm->ip += 1;
    break;
  }
  case OP_ARRAY_DEL: {
    assert_min_stack(vm, 2);
    Value v = pop(vm);
    Value arr = pop(vm);
    ASSERT_TYPE(vm, &arr, VAL_ARRAY);
    ASSERT_TYPE(vm, &v, VAL_INT);
    if (v.as.i < 0 || v.as.i >= (int64_t)arr.as.arr->len) {
      vm_runtime_errorf(&inst, "Array index out of bounds, idx:%i len:%d",
                        v.as.i, arr.as.arr->len);
    }
    vm_array_del(arr.as.arr, v.as.u);
    push(vm, arr);
    vm->ip += 1;

    break;
  }
  case OP_ARRAY_GET: {
    assert_min_stack(vm, 2);
    Value v = pop(vm);
    Value arr = pop(vm);
    ASSERT_TYPE(vm, &arr, VAL_ARRAY);
    ASSERT_TYPE(vm, &v, VAL_INT);
    if (v.as.i < 0 || v.as.i > (int64_t)arr.as.arr->len - 1) {
      vm_runtime_errorf(&inst, "array_get index out of bounds, idx:%i len:%d",
                        v.as.i, arr.as.arr->len);
    }
    push(vm, arr.as.arr->items[v.as.u]);
    vm->ip += 1;

    break;
  }
  case OP_ARRAY_SET: {
    assert_min_stack(vm, 3);
    Value v = pop(vm);
    Value index = pop(vm);
    Value arr = pop(vm);
    ASSERT_TYPE(vm, &arr, VAL_ARRAY);
    ASSERT_TYPE(vm, &index, VAL_INT);
    if (index.as.i < 0) {
      vm_runtime_errorf(&inst, "array_set index out of bounds, idx:%i len:%d",
                        index.as.i, arr.as.arr->len);
    }
    if (index.as.i >= (int64_t)arr.as.arr->len) {
      vm_array_grow(vm, arr.as.arr, index.as.u * 2);
    }
    arr.as.arr->items[index.as.u] = v;
    push(vm, arr);
    vm->ip += 1;

    break;
  }
  case OP_ARRAY_LEN: {
    assert_min_stack(vm, 1);
    Value arr = pop(vm);
    ASSERT_TYPE(vm, &arr, VAL_ARRAY);
    push(vm, vm_new_int((int64_t)arr.as.arr->len));
    vm->ip += 1;
    break;
  }

  case OP_ENTER: {
    f->locals = inst.ref;
    for (size_t i = 0; i < inst.ref; i++)
      push(vm, VM_NIL);
    vm->ip += 1;
    break;
  }
  case OP_CALL: {
    assert_min_stack(vm, (size_t)inst.argc);
    if (inst.op_type == VAL_REF) {
      if (inst.ref >= vm->function_count) {
        printf("Unkown builtin function call with id %ld\n", inst.ref);
        exit(1);
      }
      BuiltinFunc *fn = &vm->functions[inst.ref];
      if (inst.argc > -1) {
        assert_min_stack(vm, (size_t)inst.argc);
        fn->fn(vm, (int)inst.argc);
      } else {
        fn->fn(vm, 0);
      }
      vm->ip++;
      break;
    }
    assert(vm->frame_top < CALL_MAX);
    // ---- create function frame ----
    vm->frames[vm->frame_top++] = (CallFrame){
        .return_ip = vm->ip + 1,
        .old_fp = vm->fp,
        .argc = inst.ref,
        .locals = 0,
    };

    // Make arguments become locals
    if (inst.argc < -1) {
      vm_runtime_errorf(
          &inst,
          "Function arity must be equal or greater then -1 , given " PRIi64,
          inst.argc);
    }
    // for locals
    vm->fp = vm->sp - (size_t)inst.argc;
    // Jump to function
    vm->ip = inst.ref;
    break;
  }
  case OP_RET: {
    Value ret;
    if (vm->frame_top == 1) {
      // Ret from main
      if (vm->sp < 1) {
        // no exit code given
        exit(0);
      }
      ret = pop(vm);
      if (ret.type == VAL_INT) {
        exit((int)ret.as.i);
      } else {
        exit(0);
      }
    }
    if (vm->frame_top == 0) {
      vm_runtime_errorf(&inst, "RET must be used in function");
    }
    assert_min_stack(vm, 1);
    ret = pop(vm);
    vm->sp = vm->fp;
    CallFrame frame = vm->frames[--vm->frame_top];
    vm->fp = frame.old_fp;
    vm->ip = frame.return_ip;
    push(vm, ret);
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

static void register_builtin_fn(VM *vm, char *name, BuiltinFnCb cb) {
  vm->builtin_map[vm->builtin_count++] =
      (BuiltinMap){.name = name, .id = vm->function_count};
  vm->functions[vm->function_count++] = (BuiltinFunc){
      .name = name,
      .fn = cb,
  };
}
static void print_inst(const Inst *in) {
  printf("Inst { ");

  printf("type=%s", opcode_name(in->type));

  printf(", line=%ld", in->soure_line_num);
  if (in->type == OP_PUSH_STR) {
    printf(", chars=\"%s\"", in->str->chars);
  } else {
    printf(", operand=%lld", (long long)in->operand);
  }

  printf(", argc=%lld", (long long)in->argc);
  printf(" }\n");
}

static void register_std_lib(VM *vm) {
  srand((unsigned)time(NULL));

  // Debug
  register_builtin_fn(vm, "dump_stack", builtin_dump_stack);
  register_builtin_fn(vm, "exit", builtin_exit);
  // I/O
  register_builtin_fn(vm, "cclear", builtin_cclear);
  register_builtin_fn(vm, "cmove", builtin_cmove);
  register_builtin_fn(vm, "ccolor", builtin_ccolor);
  register_builtin_fn(vm, "creset", builtin_creset);
  register_builtin_fn(vm, "print", builtin_print);
  register_builtin_fn(vm, "println", builtin_println);
  register_builtin_fn(vm, "printf", builtin_printf);
  register_builtin_fn(vm, "read_int", builtin_read_int);
  register_builtin_fn(vm, "read_line", builtin_read_line);
  register_builtin_fn(vm, "getchr", builtin_getchr);
  register_builtin_fn(vm, "msleep", builtin_sleepms);
  // Math
  register_builtin_fn(vm, "min", builtin_min);
  register_builtin_fn(vm, "max", builtin_max);
  register_builtin_fn(vm, "abs", builtin_abs);
  register_builtin_fn(vm, "rand", builtin_rand);
  register_builtin_fn(vm, "rand_range", builtin_rand_range);
  register_builtin_fn(vm, "clamp", builtin_clamp);
}

static void vm_run(VM *vm) {
  while (!vm->halt && (vm->ip < vm->program->inst_count &&
                       vm->program->inst[vm->ip].type != OP_HALT)) {
    // printf("execute ip %d\n", vm->ip);
    // print_inst(&vm->program->inst[vm->ip]);
    Inst *previnst = &vm->program->inst[vm->ip];
    int next_ip = vm_inst_execute(vm, vm->program->inst[vm->ip]);
    if (next_ip != 0) {
      if (previnst) {
        printf("Error on instruction at\n");
        print_inst(previnst);
      }
      break;
    }
  }
}
#endif /* ifndef VM_IMP_H */
