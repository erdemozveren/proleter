#ifndef VM_BUILTIN_H
#define VM_BUILTIN_H

#define _POSIX_C_SOURCE 200809L
#include "vm_structs.h"
#include "vm_utils.h"
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

// I/O

static void builtin_getchr(VM *vm, int argc) {
  if (argc != 0) {
    printf("getchr expects no arguments\n");
    exit(1);
  }

  int ch = read_single_key();

  push(vm, vm_new_int((int64_t)ch));
}

static void builtin_cclear(VM *vm, int argc) {
  (void)vm;
  (void)argc;

#ifdef _WIN32
  system("cls");
#else
  printf("\033[2J\033[H");
#endif

  fflush(stdout);
  push(vm, VM_NIL);
}

static void builtin_cmove(VM *vm, int argc) {
  if (argc != 2) {
    printf("cmove expects 2 arguments got %d\n", argc);
    exit(1);
  }

  size_t base = vm->sp - 2;
  Value x = vm->stack[base];
  Value y = vm->stack[base + 1];

  vm->sp -= 2;

  if (x.type != VAL_INT || y.type != VAL_INT) {
    printf("cmove expects integers\n");
    exit(1);
  }

  // ANSI cursor move: row;col
  printf("\033[%ld;%ldH", y.as.i, x.as.i);
  fflush(stdout);
  push(vm, VM_NIL);
}

static void builtin_ccolor(VM *vm, int argc) {
  if (argc != 2) {
    printf("ccolor expects 2 arguments\n");
    exit(1);
  }

  size_t base = vm->sp - 2;
  Value fg = vm->stack[base];
  Value bg = vm->stack[base + 1];

  vm->sp -= 2;

  if (fg.type != VAL_INT || bg.type != VAL_INT) {
    printf("ccolor expects integers\n");
    exit(1);
  }

  if (fg.as.i < 0 || fg.as.i > 7 || bg.as.i < 0 || bg.as.i > 7) {
    printf("ccolor expects values in range 0..7\n");
    exit(1);
  }

  printf("\033[%ld;%ldm", 30 + fg.as.i, 40 + bg.as.i);
  fflush(stdout);
  push(vm, VM_NIL);
}

static void builtin_creset(VM *vm, int argc) {
  (void)vm;
  (void)argc;
  if (argc != 0) {
    printf("console.reset expects 0 arguments\n");
    exit(1);
  }

  printf("\033[0m");
  fflush(stdout);
  push(vm, VM_NIL);
}

static void builtin_print(VM *vm, int argc) {
  assert(argc > 0);
  size_t argsize = (size_t)argc;
  assert_min_stack(vm, argsize);

  size_t start = vm->sp - argsize;

  // print in correct order
  for (size_t i = 0; i < argsize; i++) {
    Value v = vm->stack[start + i];
    switch (v.type) {
    case VAL_INT:
    case VAL_REF:
      printf("%ld", v.as.i);
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
    case VAL_NIL:
      printf("<nil>");
      break;
    case VAL_TYPE_END:
    default:
      printf("<undefined>");
      break;
    }
  }

  // now remove arguments from stack
  vm->sp -= argsize;
  push(vm, VM_NIL);
}
static void builtin_println(VM *vm, int argc) {
  builtin_print(vm, argc);
  // builtin_print already push nil
  printf("\n");
}

static void builtin_sleepms(VM *vm, int argc) {
  assert_min_stack(vm, 1);
  if (argc != 1) {
    vm_runtime_errorf(NULL, "sleep accepts 1 argument");
  }
  Value v = pop(vm);
  ASSERT_TYPE(vm, &v, VAL_INT);
  if (v.as.i <= 0) {
    vm_runtime_errorf(NULL, "Sleep time must be greater than 0");
  }
  struct timespec ts;
  ts.tv_sec = v.as.i / 1000;
  ts.tv_nsec = (v.as.i % 1000) * 1000000L;
  nanosleep(&ts, NULL);
  push(vm, VM_NIL);
}

static void builtin_read_int(VM *vm, int argc) {
  (void)argc;

  int64_t x;
  if (scanf("%ld", &x) != 1) {
    push(vm, vm_new_int(0)); // or error
    return;
  }
  push(vm, vm_new_int(x));
}

static void builtin_printf(VM *vm, int argc) {
  if (argc <= 0) {
    vm_runtime_errorf(&vm->program->inst[vm->ip],
                      "printf excpects at least 1 argument");
  }
  size_t argsize = (size_t)argc;
  assert_min_stack(vm, argsize);

  size_t start = vm->sp - argsize;

  Value fmtv = vm->stack[start];
  if (fmtv.type != VAL_STR) {
    printf("<printf error: format not string>");
    vm->sp -= argsize;
    return;
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
        if (argi >= argsize)
          break;

        Value v = vm->stack[start + argi++];

        switch (*fmt) {
        case 'd':
          out += snprintf(out, (size_t)(end - out), "%ld", v.as.i);
          break;
        case 's':
          out += snprintf(out, (size_t)(end - out), "%s", v.as.str->chars);
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

  vm->sp -= argsize;
  push(vm, VM_NIL);
}

static void builtin_read_line(VM *vm, int argc) {
  (void)argc;

  char buf[1024];
  if (!fgets(buf, sizeof(buf), stdin)) {
    push(vm, vm_new_string(vm, ""));
    return;
  }

  buf[strcspn(buf, "\n")] = '\0';
  push(vm, vm_new_string(vm, buf));
}

// End if I/O

// Math

static void builtin_abs(VM *vm, int argc) {
  assert(argc == 1);
  assert_min_stack(vm, 1);

  size_t base = vm->sp - 1;
  Value v = vm->stack[base];

  vm->sp -= 1;

  if (v.type != VAL_INT) {
    printf("abs expects int\n");
    exit(1);
  }

  int64_t x = v.as.i;
  if (x < 0)
    x = -x;

  push(vm, vm_new_int(x));
}
static void builtin_min(VM *vm, int argc) {
  assert(argc == 2);
  assert_min_stack(vm, 2);

  size_t base = vm->sp - 2;
  Value a = vm->stack[base];
  Value b = vm->stack[base + 1];

  vm->sp -= 2;

  if (a.type != VAL_INT || b.type != VAL_INT) {
    printf("min expects ints\n");
    exit(1);
  }

  push(vm, vm_new_int(a.as.i < b.as.i ? a.as.i : b.as.i));
}

static void builtin_max(VM *vm, int argc) {
  assert(argc == 2);
  assert_min_stack(vm, 2);

  size_t base = vm->sp - 2;
  Value a = vm->stack[base];
  Value b = vm->stack[base + 1];

  vm->sp -= 2;

  if (a.type != VAL_INT || b.type != VAL_INT) {
    printf("max expects ints\n");
    exit(1);
  }

  push(vm, vm_new_int(a.as.i > b.as.i ? a.as.i : b.as.i));
}

static void builtin_clamp(VM *vm, int argc) {
  assert(argc == 3);
  assert_min_stack(vm, 3);

  size_t base = vm->sp - 3;
  Value x = vm->stack[base];
  Value lo = vm->stack[base + 1];
  Value hi = vm->stack[base + 2];

  vm->sp -= 3;

  if (x.type != VAL_INT || lo.type != VAL_INT || hi.type != VAL_INT) {
    printf("clamp expects ints\n");
    exit(1);
  }

  int64_t v = x.as.i;
  if (v < lo.as.i)
    v = lo.as.i;
  if (v > hi.as.i)
    v = hi.as.i;

  push(vm, vm_new_int(v));
}

static void builtin_rand(VM *vm, int argc) {
  assert(argc == 0);

  int64_t r = (int64_t)rand();
  push(vm, vm_new_int(r));
}

static void builtin_rand_range(VM *vm, int argc) {
  assert(argc == 2);
  assert_min_stack(vm, 2);

  size_t base = vm->sp - 2;
  Value lo = vm->stack[base];
  Value hi = vm->stack[base + 1];

  vm->sp -= 2;

  if (lo.type != VAL_INT || hi.type != VAL_INT) {
    printf("rand_range expects ints\n");
    exit(1);
  }

  if (lo.as.i > hi.as.i) {
    printf("rand_range: lo > hi\n");
    exit(1);
  }

  int64_t range = hi.as.i - lo.as.i + 1;
  int64_t r = lo.as.i + (rand() % range);

  push(vm, vm_new_int(r));
}

// End of Math
// Debug
static void builtin_dump_stack(VM *vm, int argc) {
  (void)argc;
  vm_print_stack_top(vm);
}

static void builtin_exit(VM *vm, int argc) {
  (void)argc;
  vm->halt = true;
}

#endif
