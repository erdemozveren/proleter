#ifndef VM_std_H
#define VM_std_H

#include "vm_api.h"
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Required
int vm_api_version(void) { return PROLETER_API_VERSION; }

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define GETCWD _getcwd
uint32_t get_pid(void) { return (uint32_t)GetCurrentProcessId(); }
#else
#include <unistd.h>
uint32_t get_pid(void) { return (uint32_t)getpid(); }
#include <unistd.h>
#define GETCWD getcwd
#endif

Value native_cwd(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  (void)argc;
  (void)argv;
  char *cwd = GETCWD(NULL, 0);
  if (!cwd) {
    vm_panic("failed to get current working directory: %s", strerror(errno));
    return VM_NIL;
  }
  Value v = vm_new_string(vm, cwd);
  free(cwd);
  return v;
}

Value native_env(VM *vm, size_t argc, Value *argv) {
  if (argc < 1 || argv[0].type != VAL_STR) {
    vm_panic("process.env: env name must be string");
    return VM_NIL;
  }
  if (argc >= 2 && argv[1].type != VAL_STR) {
    vm_panic("process.env: default value must be string");
    return VM_NIL;
  }
  const char *key = vm_string_chars(argv[0].as.str);
  const char *val = getenv(key);

  if (!val && argc == 1)
    return VM_NIL;
  // Default fallback to second arg
  if (!val && argc == 2 && argv[1].type == VAL_STR) {
    return argv[1];
  }

  return vm_new_string(vm, (char *)val);
}

Value prc_pid(VM *vm, size_t argc, Value *argv) {
  (void)argv;
  (void)vm;
  if (argc != 0) {
    vm_panic("exit expects 0 arguments");
    return VM_NIL;
  }
  return vm_new_int(get_pid());
}

Value prc_exit(VM *vm, size_t argc, Value *argv) {
  (void)argv;
  if (argc != 0) {
    vm_panic("exit expects 0 arguments");
    return VM_NIL;
  }
  vm_halt(vm);
  return VM_NIL;
}

// ----------------------------
// Init
// ----------------------------

extern Value PROLETER_LIB_INIT_FN(VM *vm) {

  Value val = vm_object_new(vm, 64);
  Object *o = val.as.obj;

  vm_object_set(vm, o, "pid", vm_make_native(vm, "pid", prc_pid));
  vm_object_set(vm, o, "cwd", vm_make_native(vm, "cwd", native_cwd));
  vm_object_set(vm, o, "env", vm_make_native(vm, "env", native_env));
  vm_object_set(vm, o, "exit", vm_make_native(vm, "exit", prc_exit));

  return val;
}

#endif
