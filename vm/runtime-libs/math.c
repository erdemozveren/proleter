#ifndef VM_std_H
#define VM_std_H

#include <stdalign.h>

#include "vm_api.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Required
int vm_api_version(void) { return PROLETER_API_VERSION; }

// ----------------------------
// Math builtins
// ----------------------------

Value std_abs(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 1 || argv[0].type != VAL_INT) {
    vm_panic("abs expects 1 int");
    return VM_NIL;
  }
  int64_t x = argv[0].as.i;
  if (x < 0)
    x = -x;
  return vm_new_int(x);
}

Value std_min(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 2 || argv[0].type != VAL_INT || argv[1].type != VAL_INT) {
    vm_panic("min expects 2 ints");
    return VM_NIL;
  }
  return vm_new_int(argv[0].as.i < argv[1].as.i ? argv[0].as.i : argv[1].as.i);
}

Value std_max(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 2 || argv[0].type != VAL_INT || argv[1].type != VAL_INT) {
    vm_panic("max expects 2 ints");
    return VM_NIL;
  }
  return vm_new_int(argv[0].as.i > argv[1].as.i ? argv[0].as.i : argv[1].as.i);
}

Value std_clamp(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 3 || argv[0].type != VAL_INT || argv[1].type != VAL_INT ||
      argv[2].type != VAL_INT) {
    vm_panic("clamp expects 3 ints");
    return VM_NIL;
  }

  int64_t v = argv[0].as.i;
  int64_t lo = argv[1].as.i;
  int64_t hi = argv[2].as.i;

  if (v < lo)
    v = lo;
  if (v > hi)
    v = hi;

  return vm_new_int(v);
}

Value std_rand(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  (void)argv;
  if (argc != 0) {
    vm_panic("rand expects 0 arguments");
    return VM_NIL;
  }
  int64_t r = (int64_t)rand();
  return vm_new_int(r);
}

Value std_rand_range(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 2 || argv[0].type != VAL_INT || argv[1].type != VAL_INT) {
    vm_panic("randRange expects 2 ints");
    return VM_NIL;
  }

  int64_t lo = argv[0].as.i;
  int64_t hi = argv[1].as.i;

  if (lo > hi) {
    vm_panic("randRange: lo > hi");
    return VM_NIL;
  }

  int64_t range = hi - lo + 1;
  int64_t r = lo + (rand() % range);

  return vm_new_int(r);
}

// ----------------------------
// Init: create std object and attach builtins
// ----------------------------

extern Value PROLETER_LIB_INIT_FN(VM *vm) {

  Value val = vm_object_new(vm, 64);
  Object *o = val.as.obj;
  vm_object_set(vm, o, "min", vm_make_native(vm, "min", std_min));
  vm_object_set(vm, o, "max", vm_make_native(vm, "max", std_max));
  vm_object_set(vm, o, "abs", vm_make_native(vm, "abs", std_abs));
  vm_object_set(vm, o, "rand", vm_make_native(vm, "rand", std_rand));
  vm_object_set(vm, o, "randRange",
                vm_make_native(vm, "randRange", std_rand_range));
  vm_object_set(vm, o, "clamp", vm_make_native(vm, "clamp", std_clamp));

  return val;
}

#endif
