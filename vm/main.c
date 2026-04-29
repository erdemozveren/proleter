#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
  const char *progname = argv[0];
  const char *p = strrchr(progname, '/');
  if (p)
    progname = p + 1;
  if (argc < 2) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <file>\n", progname);
    return 1;
  }

  const char *source_path = argv[1];
  if (access(source_path, F_OK) != 0) {
    printf("File %s not exists\n", source_path);
    exit(1);
  }

  VM vm = {0};
  vm.heap = (Heap){.objects = NULL,
                   .object_count = 0,
                   .bytes_allocated = 0,
                   .next_gc = VM_GC_START_THRESHOLD};
  vm_load_program(&vm, source_path);
  vm_run_program(&vm);
#if PROLETER_DEBUG
  printf("\n");
  vm_print_stack_top(&vm);
#endif
  vm_gc_sweep_all(&vm);
  vm_free_program(vm.program);
  return 0;
}
