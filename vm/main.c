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
  vm.heap =
      (Heap){.current = NULL, .used = 0, .capacity = (1024 * 1024 * 1024)};
  vm_load_program(&vm, source_path);
  vm_run_program(&vm);
  vm_heap_free(&vm);
  vm_free_program(vm.program);
  return 0;
}
