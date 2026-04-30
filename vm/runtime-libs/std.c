#ifndef VM_std_H
#define VM_std_H

#define _POSIX_C_SOURCE 199309L
#include "vm_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Required
int vm_api_version(void) { return PROLETER_API_VERSION; }

// For Terminal State
bool g_input_initialized = false;
bool g_raw_enabled = false;
bool g_nonblock_enabled = false;

// Terminal State and Input Handler for Linux
#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

struct termios g_orig_termios;
int g_orig_flags = -1;

void input_platform_restore(void) {
  if (!g_input_initialized) {
    return;
  }

  tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
  if (g_orig_flags != -1) {
    fcntl(STDIN_FILENO, F_SETFL, g_orig_flags);
  }
}

void input_platform_init(void) {
  if (g_input_initialized) {
    return;
  }

  if (tcgetattr(STDIN_FILENO, &g_orig_termios) == -1) {
    perror("tcgetattr");
    exit(1);
  }

  g_orig_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (g_orig_flags == -1) {
    perror("fcntl(F_GETFL)");
    exit(1);
  }

  atexit(input_platform_restore);
  g_input_initialized = true;
}

void input_platform_apply(void) {
  input_platform_init();

  struct termios t = g_orig_termios;

  if (g_raw_enabled) {
    t.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    t.c_iflag &= (tcflag_t) ~(IXON | ICRNL);
    t.c_oflag &= (tcflag_t) ~(OPOST);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
  }

  if (tcsetattr(STDIN_FILENO, TCSANOW, &t) == -1) {
    perror("tcsetattr");
    exit(1);
  }

  int flags = g_orig_flags;
  if (g_nonblock_enabled) {
    flags |= O_NONBLOCK;
  }

  if (fcntl(STDIN_FILENO, F_SETFL, flags) == -1) {
    perror("fcntl(F_SETFL)");
    exit(1);
  }
}

int input_platform_read_char(void) {
  unsigned char ch;
  ssize_t n = read(STDIN_FILENO, &ch, 1);

  if (n == 1) {
    return (int)ch;
  }

  if (n == 0) {
    return -1;
  }

  if (errno == EAGAIN || errno == EWOULDBLOCK) {
    return -1;
  }

  return -2;
}
#endif

// Terminal state and Input Helpers for Windows

#ifdef _WIN32
#include <conio.h>
#include <windows.h>

DWORD g_orig_console_mode = 0;
HANDLE g_stdin_handle = NULL;

void input_platform_restore(void) {
  if (!g_input_initialized) {
    return;
  }

  if (g_stdin_handle != NULL && g_orig_console_mode != 0) {
    SetConsoleMode(g_stdin_handle, g_orig_console_mode);
  }
}

void input_platform_init(void) {
  if (g_input_initialized) {
    return;
  }

  g_stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
  if (g_stdin_handle == INVALID_HANDLE_VALUE || g_stdin_handle == NULL) {
    fprintf(stderr, "GetStdHandle failed\n");
    exit(1);
  }

  if (!GetConsoleMode(g_stdin_handle, &g_orig_console_mode)) {
    fprintf(stderr, "GetConsoleMode failed\n");
    exit(1);
  }

  atexit(input_platform_restore);
  g_input_initialized = true;
}

void input_platform_apply(void) {
  input_platform_init();

  DWORD mode = g_orig_console_mode;

  if (g_raw_enabled) {
    mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    mode &= ~(ENABLE_PROCESSED_INPUT);
  }

  if (!SetConsoleMode(g_stdin_handle, mode)) {
    fprintf(stderr, "SetConsoleMode failed\n");
    exit(1);
  }
}

int input_platform_read_char(void) {
  if (g_nonblock_enabled) {
    if (!_kbhit()) {
      return -1;
    }
  }

  int ch = _getch();

  if (ch == 0 || ch == 224) {
    int ch2 = _getch();
    return 256 + ch2; // extended keys
  }

  return ch;
}
#endif

// Terminal input helper functions
void input_set_raw(bool enable) {
  g_raw_enabled = enable;
  input_platform_apply();
}

void input_set_nonblock(bool enable) {
  g_nonblock_enabled = enable;
  input_platform_apply();
}

void input_restore_all(void) {
  g_raw_enabled = false;
  g_nonblock_enabled = false;
  input_platform_restore();
}

// ----------------------------
// I/O
// ----------------------------

Value std_input_raw(VM *vm, size_t argc, Value *argv) {
  (void)vm;

  if (argc != 1 || argv[0].type != VAL_INT) {
    vm_panic("inputRaw expects 1 integer argument");
    return VM_NIL;
  }

  input_set_raw(argv[0].as.i != 0);
  return VM_NIL;
}

Value std_input_nonblock(VM *vm, size_t argc, Value *argv) {
  (void)vm;

  if (argc != 1 || argv[0].type != VAL_INT) {
    vm_panic("inputNonblocking expects 1 integer argument");
    return VM_NIL;
  }

  input_set_nonblock(argv[0].as.i != 0);
  return VM_NIL;
}

Value std_input_restore(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  (void)argv;

  if (argc != 0) {
    vm_panic("inputRestore expects 0 arguments");
    return VM_NIL;
  }

  input_restore_all();
  return VM_NIL;
}

Value std_getchar(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  (void)argv;

  if (argc != 0) {
    vm_panic("getChar expects 0 arguments");
    return VM_NIL;
  }

  int ch = input_platform_read_char();
  if (ch == -2) {
    vm_panic("getChar failed");
    return VM_NIL;
  }

  return vm_new_int((int64_t)ch);
}
Value std_cclear(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  (void)argv;
  if (argc != 0) {
    vm_panic("cclear expects 0 arguments");
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

Value std_cmove(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 2) {
    vm_panic("cmove expects 2 arguments");
    return VM_NIL;
  }
  Value x = argv[0];
  Value y = argv[1];
  if (x.type != VAL_INT || y.type != VAL_INT) {
    vm_panic("cmove expects integers");
    return VM_NIL;
  }

  // ANSI cursor move: row;col
  printf("\033[%ld;%ldH", y.as.i, x.as.i);
  fflush(stdout);
  return VM_NIL;
}

Value std_ccolor(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 2) {
    vm_panic("ccolor expects 2 arguments");
    return VM_NIL;
  }

  Value fg = argv[0];
  Value bg = argv[1];

  if (fg.type != VAL_INT || bg.type != VAL_INT) {
    vm_panic("ccolor expects integers");
    return VM_NIL;
  }

  if (fg.as.i < 0 || fg.as.i > 7 || bg.as.i < 0 || bg.as.i > 7) {
    vm_panic("ccolor expects values in range 0..7");
    return VM_NIL;
  }

  printf("\033[%ld;%ldm", 30 + fg.as.i, 40 + bg.as.i);
  fflush(stdout);
  return VM_NIL;
}

Value std_creset(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  (void)argv;
  if (argc != 0) {
    vm_panic("creset expects 0 arguments");
    return VM_NIL;
  }
  printf("\033[0m");
  fflush(stdout);
  return VM_NIL;
}

Value std_print(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc == 0) {
    // keep behavior: require at least 1 arg
    vm_panic("print expects at least 1 argument");
    return VM_NIL;
  }

  for (size_t i = 0; i < argc; i++) {
    Value v = argv[i];
    switch (v.type) {
    case VAL_INT:
      printf("%ld", v.as.i);
      break;
    case VAL_CALLABLE:
      printf("Function()");
      break;
    case VAL_DOUBLE:
      printf("%f", v.as.d);
      break;
    case VAL_STR:
      printf("%s", vm_string_chars(v.as.str));
      break;
    case VAL_ARRAY:
      printf("Array(%ld)", vm_array_len(v.as.arr));
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

Value std_println(VM *vm, size_t argc, Value *argv) {
  std_print(vm, argc, argv);
  printf("\n");
  return VM_NIL;
}

Value std_sleep(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 1) {
    vm_panic("sleep expects 1 argument");
    return VM_NIL;
  }
  Value v = argv[0];
  if (v.type != VAL_INT) {
    vm_panic("sleep expects int");
    return VM_NIL;
  }
  if (v.as.i <= 0) {
    vm_panic("Sleep time must be greater than 0");
    return VM_NIL;
  }

  struct timespec ts = {0};
  ts.tv_sec = v.as.i / 1000;
  ts.tv_nsec = (v.as.i % 1000) * 1000000L;
  nanosleep(&ts, NULL);
  return VM_NIL;
}

Value std_read_int(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  (void)argv;
  if (argc != 0) {
    vm_panic("readInt expects 0 arguments");
    return vm_new_int(0);
  }

  int64_t x;
  if (scanf("%ld", &x) != 1) {
    return vm_new_int(0); // or error
  }
  return vm_new_int(x);
}

Value std_printf(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc < 1) {
    vm_panic("printf expects at least 1 argument");
    return VM_NIL;
  }

  Value fmtv = argv[0];
  if (fmtv.type != VAL_STR) {
    printf("<printf error: format not string>\n");
    return VM_NIL;
  }

  const char *fmt = vm_string_chars(fmtv.as.str);
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
        case 'f':
          if (v.type != VAL_DOUBLE) {
            out += snprintf(out, (size_t)(end - out), "?");
          } else {
            out += snprintf(out, (size_t)(end - out), "%f", v.as.d);
          }
          break;

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
            out += snprintf(out, (size_t)(end - out), "%s",
                            vm_string_chars(v.as.str));
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

Value std_read_line(VM *vm, size_t argc, Value *argv) {
  (void)argv;
  if (argc != 0) {
    vm_panic("readLine expects 0 arguments");
    return vm_new_string(vm, "");
  }

  char buf[1024];
  if (!fgets(buf, sizeof(buf), stdin)) {
    return vm_new_string(vm, "");
  }

  buf[strcspn(buf, "\n")] = '\0';
  return vm_new_string(vm, buf);
}

Value std_to_string(VM *vm, size_t argc, Value *argv) {
  if (argc != 1 || argv[0].type != VAL_INT) {
    vm_panic("toString expects 1 integer");
    return VM_NIL;
  }

  char buf[64];
  snprintf(buf, sizeof(buf), "%lld", (long long)argv[0].as.i);

  // create VM string (adjust if your API name differs)
  return vm_new_string(vm, buf);
}

// ----------------------------
// Init: create std object and attach builtins
// ----------------------------

extern Value PROLETER_LIB_INIT_FN(VM *vm) {
  srand((unsigned)time(NULL));

  Value stdv = vm_object_new(vm, 64);
  if (stdv.type != VAL_OBJECT) {
    vm_panic("vm_object_new did not return VAL_OBJECT for std");
    return VM_NIL;
  }
  Object *std = stdv.as.obj;

  vm_object_set(vm, std, "cclear", vm_make_native(vm, "cclear", std_cclear));
  vm_object_set(vm, std, "cmove", vm_make_native(vm, "cmove", std_cmove));
  vm_object_set(vm, std, "ccolor", vm_make_native(vm, "ccolor", std_ccolor));
  vm_object_set(vm, std, "creset", vm_make_native(vm, "creset", std_creset));
  vm_object_set(vm, std, "print", vm_make_native(vm, "print", std_print));
  vm_object_set(vm, std, "println", vm_make_native(vm, "println", std_println));
  vm_object_set(vm, std, "printf", vm_make_native(vm, "printf", std_printf));
  vm_object_set(vm, std, "readInt",
                vm_make_native(vm, "readInt", std_read_int));
  vm_object_set(vm, std, "readLine",
                vm_make_native(vm, "readLine", std_read_line));
  vm_object_set(vm, std, "toString",
                vm_make_native(vm, "toString", std_to_string));
  vm_object_set(vm, std, "getChar", vm_make_native(vm, "getChar", std_getchar));
  vm_object_set(vm, std, "inputRaw",
                vm_make_native(vm, "inputRaw", std_input_raw));
  vm_object_set(vm, std, "inputNonblocking",
                vm_make_native(vm, "inputNonblocking", std_input_nonblock));
  vm_object_set(vm, std, "inputRestore",
                vm_make_native(vm, "inputRestore", std_input_restore));

  vm_object_set(vm, std, "sleep", vm_make_native(vm, "sleep", std_sleep));
  return stdv;
}

#endif
