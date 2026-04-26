#ifndef VM_std_H
#define VM_std_H

#include "vm_api.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Required
int vm_api_version(void) { return PROLETER_API_VERSION; }

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

Value prc_memory_allocated(VM *vm, size_t argc, Value *argv) {
  (void)argc;
  (void)argv;
  return vm_new_int((int)vm_gc_allocated(vm));
}

Value prc_memory_next_gc(VM *vm, size_t argc, Value *argv) {
  (void)argc;
  (void)argv;
  return vm_new_int((int)vm_gc_next_bytes(vm));
}

Value prc_memory_rss(VM *vm, size_t argc, Value *argv) {
  (void)argc;
  (void)argv;
  (void)vm;
  return vm_new_int((int)get_rss_bytes());
}

Value trigger_gc_collect(VM *vm, size_t argc, Value *argv) {
  (void)argc;
  (void)argv;
  size_t removed = vm_gc_allocated(vm);
  vm_gc_collect(vm);
  removed -= vm_gc_allocated(vm);
  return vm_new_int((int)removed);
}

// ----------------------------
// Init
// ----------------------------

extern Value PROLETER_LIB_INIT_FN(VM *vm) {

  Value val = vm_object_new(vm, 64);
  Object *o = val.as.obj;

  vm_object_set(vm, o, "allocated",
                vm_make_native(vm, "allocated", prc_memory_allocated));
  vm_object_set(vm, o, "next_gc",
                vm_make_native(vm, "next_gc", prc_memory_next_gc));
  vm_object_set(vm, o, "collect",
                vm_make_native(vm, "collect", trigger_gc_collect));
  vm_object_set(vm, o, "rss", vm_make_native(vm, "rss", prc_memory_rss));
  return val;
}

#endif
