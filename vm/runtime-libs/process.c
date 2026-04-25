#ifndef VM_std_H
#define VM_std_H

#include "vm_api.h"
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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
    vm_errorf("failed to get current working directory: %s", strerror(errno));
    return VM_NIL;
  }
  Value v = vm_new_string(vm, cwd);
  free(cwd);
  return v;
}

Value native_env(VM *vm, size_t argc, Value *argv) {
  if (argc < 1 || argv[0].type != VAL_STR) {
    vm_errorf("process.env: env name must be string");
    return VM_NIL;
  }
  if (argc >= 2 && argv[1].type != VAL_STR) {
    vm_errorf("process.env: default value must be string");
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
    vm_errorf("exit expects 0 arguments");
    return VM_NIL;
  }
  return vm_new_int(get_pid());
}

Value prc_exit(VM *vm, size_t argc, Value *argv) {
  (void)argv;
  if (argc != 0) {
    vm_errorf("exit expects 0 arguments");
    return VM_NIL;
  }
  vm_halt(vm);
  return VM_NIL;
}

// Useful when debugging
size_t get_rss_bytes(void) {
  FILE *f = fopen("/proc/self/status", "r");
  if (!f)
    return 0;

  char line[256];
  size_t rss = 0;

  while (fgets(line, sizeof(line), f)) {
    if (strncmp(line, "VmRSS:", 6) == 0) {
      sscanf(line + 6, "%zu", &rss);
      break;
    }
  }

  fclose(f);
  return rss * 1024; // kB → bytes
}

Value prc_memory_usage(VM *vm, size_t argc, Value *argv) {
  (void)argc;
  (void)argv;
  Value ret = vm_object_new(vm, 3);
  Object *obj = ret.as.obj;
  size_t used = vm_used_memory(vm);
  size_t cap = vm_memory_capacity(vm);
  size_t free = cap - used;
  vm_object_set(vm, obj, "free", vm_new_double((double)free));
  vm_object_set(vm, obj, "used", vm_new_double((double)used));
  vm_object_set(vm, obj, "rss", vm_new_double((double)get_rss_bytes()));
  vm_object_set(vm, obj, "total", vm_new_double((double)cap));
  return ret;
}

// ----------------------------
// Init: create std object and attach builtins
// ----------------------------

extern Value PROLETER_LIB_INIT_FN(VM *vm) {

  Value stdv = vm_object_new(vm, 64);
  if (stdv.type != VAL_OBJECT) {
    vm_errorf("vm_object_new did not return VAL_OBJECT for std");
    return VM_NIL;
  }
  Object *std = stdv.as.obj;

  vm_object_set(vm, std, "memory_usage",
                vm_make_native(vm, "memory_usage", prc_memory_usage));
  vm_object_set(vm, std, "pid", vm_make_native(vm, "pid", prc_pid));
  vm_object_set(vm, std, "cwd", vm_make_native(vm, "cwd", native_cwd));
  vm_object_set(vm, std, "env", vm_make_native(vm, "env", native_env));
  vm_object_set(vm, std, "exit", vm_make_native(vm, "exit", prc_exit));

  return stdv;
}

#endif
