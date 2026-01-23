#ifndef VM_BUILTIN_H
#define VM_BUILTIN_H

#include <stdalign.h>
#define _POSIX_C_SOURCE 200809L

#include "../vm.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Cross platform helper
#ifndef _WIN32
#include <termios.h>
#include <unistd.h>

static int read_single_key(void) {
  struct termios oldt, newt;
  int ch;

  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;

  newt.c_lflag &= ~(tcflag_t)(ICANON | ECHO); // raw mode
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return ch;
}
#endif

#ifdef _WIN32
#include <Windows.h>
#include <conio.h>
static int read_single_key(void) { return _getch(); }
#endif

// ----------------------------
// Helpers
// ----------------------------

static Value vm_make_native(VM *vm, const char *name, NativeFn fn) {
  // NOTE: Callable must be pointer inside Value (Value.as.fn is Callable*)
  Callable *c =
      (Callable *)vm_heap_alloc(&vm->heap, sizeof(Callable), alignof(Callable));
  if (!c) {
    fprintf(stderr, "Out of memory allocating Callable\n");
    exit(1);
  }
  c->name = name;
  c->type = CALLABLE_NATIVE;
  c->as.native = fn; // NativeFn is already a function pointer typedef
  return (Value){.type = VAL_CALLABLE, .as.fn = c};
}

// ----------------------------
// I/O builtins (NativeFn now returns Value)
// ----------------------------

static Value builtin_getchr(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  (void)argv;
  if (argc != 0) {
    vm_runtime_errorf(NULL, "getchr expects 0 arguments");
    return VM_NIL;
  }
  int ch = read_single_key();
  return vm_new_int((int64_t)ch);
}

static Value builtin_cclear(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  (void)argv;
  if (argc != 0) {
    vm_runtime_errorf(NULL, "cclear expects 0 arguments");
    return VM_NIL;
  }
#ifdef _WIN32
  system("cls");
#else
  printf("\033[2J\033[H");
#endif
  fflush(stdout);
  return VM_NIL;
}

static Value builtin_cmove(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 2) {
    vm_runtime_errorf(NULL, "cmove expects 2 arguments");
    return VM_NIL;
  }
  Value x = argv[0];
  Value y = argv[1];
  if (x.type != VAL_INT || y.type != VAL_INT) {
    vm_runtime_errorf(NULL, "cmove expects integers");
    return VM_NIL;
  }

  // ANSI cursor move: row;col
  printf("\033[%ld;%ldH", y.as.i, x.as.i);
  fflush(stdout);
  return VM_NIL;
}

static Value builtin_ccolor(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 2) {
    vm_runtime_errorf(NULL, "ccolor expects 2 arguments");
    return VM_NIL;
  }

  Value fg = argv[0];
  Value bg = argv[1];

  if (fg.type != VAL_INT || bg.type != VAL_INT) {
    vm_runtime_errorf(NULL, "ccolor expects integers");
    return VM_NIL;
  }

  if (fg.as.i < 0 || fg.as.i > 7 || bg.as.i < 0 || bg.as.i > 7) {
    vm_runtime_errorf(NULL, "ccolor expects values in range 0..7");
    return VM_NIL;
  }

  printf("\033[%ld;%ldm", 30 + fg.as.i, 40 + bg.as.i);
  fflush(stdout);
  return VM_NIL;
}

static Value builtin_creset(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  (void)argv;
  if (argc != 0) {
    vm_runtime_errorf(NULL, "creset expects 0 arguments");
    return VM_NIL;
  }
  printf("\033[0m");
  fflush(stdout);
  return VM_NIL;
}

static Value builtin_print(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc == 0) {
    // keep behavior: require at least 1 arg
    vm_runtime_errorf(NULL, "print expects at least 1 argument");
    return VM_NIL;
  }

  for (size_t i = 0; i < argc; i++) {
    Value v = argv[i];
    switch (v.type) {
    case VAL_INT:
      printf("%ld", v.as.i);
      break;
    case VAL_CALLABLE:
      printf("Function %s()", v.as.fn ? v.as.fn->name : "<fn?>");
      break;
    case VAL_DOUBLE:
      printf("%f", v.as.d);
      break;
    case VAL_STR:
      printf("%s", v.as.str->chars);
      break;
    case VAL_ARRAY:
      printf("Array(%ld)", v.as.arr->len);
      break;
    case VAL_OBJECT:
      printf("Object");
      break;
    case VAL_NIL:
      printf("<nil>");
      break;
    case VAL_TYPE_END:
    default:
      printf("<undefined>");
      break;
    }
  }

  return VM_NIL;
}

static Value builtin_println(VM *vm, size_t argc, Value *argv) {
  builtin_print(vm, argc, argv);
  printf("\n");
  return VM_NIL;
}

static Value builtin_sleepms(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 1) {
    vm_runtime_errorf(NULL, "msleep expects 1 argument");
    return VM_NIL;
  }
  Value v = argv[0];
  if (v.type != VAL_INT) {
    vm_runtime_errorf(NULL, "msleep expects int");
    return VM_NIL;
  }
  if (v.as.i <= 0) {
    vm_runtime_errorf(NULL, "Sleep time must be greater than 0");
    return VM_NIL;
  }

  struct timespec ts;
  ts.tv_sec = v.as.i / 1000;
  ts.tv_nsec = (v.as.i % 1000) * 1000000L;
  nanosleep(&ts, NULL);
  return VM_NIL;
}

static Value builtin_read_int(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  (void)argv;
  if (argc != 0) {
    vm_runtime_errorf(NULL, "read_int expects 0 arguments");
    return vm_new_int(0);
  }

  int64_t x;
  if (scanf("%ld", &x) != 1) {
    return vm_new_int(0); // or error
  }
  return vm_new_int(x);
}

static Value builtin_printf(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc < 1) {
    vm_runtime_errorf(NULL, "printf expects at least 1 argument");
    return VM_NIL;
  }

  Value fmtv = argv[0];
  if (fmtv.type != VAL_STR) {
    printf("<printf error: format not string>");
    return VM_NIL;
  }

  const char *fmt = fmtv.as.str->chars;
  size_t argi = 1;

  char buffer[4096];
  char *out = buffer;
  char *end = buffer + sizeof(buffer) - 1;

  while (*fmt && out < end) {
    if (*fmt == '%' && fmt[1]) {
      fmt++;

      if (*fmt == '%') {
        *out++ = '%';
      } else {
        if (argi >= argc)
          break;

        Value v = argv[argi++];

        switch (*fmt) {
        case 'd':
          if (v.type != VAL_INT) {
            out += snprintf(out, (size_t)(end - out), "?");
          } else {
            out += snprintf(out, (size_t)(end - out), "%ld", v.as.i);
          }
          break;
        case 's':
          if (v.type != VAL_STR) {
            out += snprintf(out, (size_t)(end - out), "?");
          } else {
            out += snprintf(out, (size_t)(end - out), "%s", v.as.str->chars);
          }
          break;
        default:
          *out++ = '?';
          break;
        }
      }
    } else {
      *out++ = *fmt;
    }
    fmt++;
  }

  *out = '\0';
  printf("%s", buffer);
  return VM_NIL;
}

static Value builtin_read_line(VM *vm, size_t argc, Value *argv) {
  (void)argv;
  if (argc != 0) {
    vm_runtime_errorf(NULL, "read_line expects 0 arguments");
    return vm_new_string(vm, "");
  }

  char buf[1024];
  if (!fgets(buf, sizeof(buf), stdin)) {
    return vm_new_string(vm, "");
  }

  buf[strcspn(buf, "\n")] = '\0';
  return vm_new_string(vm, buf);
}

// ----------------------------
// Math builtins
// ----------------------------

static Value builtin_abs(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 1 || argv[0].type != VAL_INT) {
    vm_runtime_errorf(NULL, "abs expects 1 int");
    return VM_NIL;
  }
  int64_t x = argv[0].as.i;
  if (x < 0)
    x = -x;
  return vm_new_int(x);
}

static Value builtin_min(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 2 || argv[0].type != VAL_INT || argv[1].type != VAL_INT) {
    vm_runtime_errorf(NULL, "min expects 2 ints");
    return VM_NIL;
  }
  return vm_new_int(argv[0].as.i < argv[1].as.i ? argv[0].as.i : argv[1].as.i);
}

static Value builtin_max(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 2 || argv[0].type != VAL_INT || argv[1].type != VAL_INT) {
    vm_runtime_errorf(NULL, "max expects 2 ints");
    return VM_NIL;
  }
  return vm_new_int(argv[0].as.i > argv[1].as.i ? argv[0].as.i : argv[1].as.i);
}

static Value builtin_clamp(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 3 || argv[0].type != VAL_INT || argv[1].type != VAL_INT ||
      argv[2].type != VAL_INT) {
    vm_runtime_errorf(NULL, "clamp expects 3 ints");
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

static Value builtin_rand(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  (void)argv;
  if (argc != 0) {
    vm_runtime_errorf(NULL, "rand expects 0 arguments");
    return VM_NIL;
  }
  int64_t r = (int64_t)rand();
  return vm_new_int(r);
}

static Value builtin_rand_range(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 2 || argv[0].type != VAL_INT || argv[1].type != VAL_INT) {
    vm_runtime_errorf(NULL, "rand_range expects 2 ints");
    return VM_NIL;
  }

  int64_t lo = argv[0].as.i;
  int64_t hi = argv[1].as.i;

  if (lo > hi) {
    vm_runtime_errorf(NULL, "rand_range: lo > hi");
    return VM_NIL;
  }

  int64_t range = hi - lo + 1;
  int64_t r = lo + (rand() % range);

  return vm_new_int(r);
}

// ----------------------------
// Debug builtins
// ----------------------------

static Value builtin_dump_stack(VM *vm, size_t argc, Value *argv) {
  (void)argv;
  if (argc != 0) {
    vm_runtime_errorf(NULL, "dump_stack expects 0 arguments");
    return VM_NIL;
  }
  vm_print_stack_top(vm);
  return VM_NIL;
}

static Value builtin_exit(VM *vm, size_t argc, Value *argv) {
  (void)argv;
  if (argc != 0) {
    vm_runtime_errorf(NULL, "exit expects 0 arguments");
    return VM_NIL;
  }
  vm->halt = true;
  return VM_NIL;
}

// ----------------------------
// Init: create std object and attach builtins
// ----------------------------

extern Value init_lib(VM *vm) {
  srand((unsigned)time(NULL));

  // Create std object
  Value stdv = vm_object_new(vm, 64);
  if (stdv.type != VAL_OBJECT) {
    vm_runtime_errorf(NULL, "vm_object_new did not return VAL_OBJECT for std");
    return VM_NIL;
  }
  Object *std = stdv.as.obj;

  // Debug
  vm_object_set(vm, std, "dump_stack",
                vm_make_native(vm, "dump_stack", builtin_dump_stack));
  vm_object_set(vm, std, "exit", vm_make_native(vm, "exit", builtin_exit));

  // I/O
  vm_object_set(vm, std, "cclear",
                vm_make_native(vm, "cclear", builtin_cclear));
  vm_object_set(vm, std, "cmove", vm_make_native(vm, "cmove", builtin_cmove));
  vm_object_set(vm, std, "ccolor",
                vm_make_native(vm, "ccolor", builtin_ccolor));
  vm_object_set(vm, std, "creset",
                vm_make_native(vm, "creset", builtin_creset));
  vm_object_set(vm, std, "print", vm_make_native(vm, "print", builtin_print));
  vm_object_set(vm, std, "println",
                vm_make_native(vm, "println", builtin_println));
  vm_object_set(vm, std, "printf",
                vm_make_native(vm, "printf", builtin_printf));
  vm_object_set(vm, std, "read_int",
                vm_make_native(vm, "read_int", builtin_read_int));
  vm_object_set(vm, std, "read_line",
                vm_make_native(vm, "read_line", builtin_read_line));
  vm_object_set(vm, std, "getchr",
                vm_make_native(vm, "getchr", builtin_getchr));
  vm_object_set(vm, std, "msleep",
                vm_make_native(vm, "msleep", builtin_sleepms));

  // Math
  vm_object_set(vm, std, "min", vm_make_native(vm, "min", builtin_min));
  vm_object_set(vm, std, "max", vm_make_native(vm, "max", builtin_max));
  vm_object_set(vm, std, "abs", vm_make_native(vm, "abs", builtin_abs));
  vm_object_set(vm, std, "rand", vm_make_native(vm, "rand", builtin_rand));
  vm_object_set(vm, std, "rand_range",
                vm_make_native(vm, "rand_range", builtin_rand_range));
  vm_object_set(vm, std, "clamp", vm_make_native(vm, "clamp", builtin_clamp));

  return stdv;
}

#endif
