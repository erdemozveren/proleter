#include "vm.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void vm_compile_errorf(const char *path, size_t line, const char *fmt, ...) {
  fprintf(stderr, "\n=== VM COMPILE ERROR ===\n");
  fprintf(stderr, "%s:%ld: ", path, line);
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n========================\n");
  exit(1);
}

void vm_rtrim(char *s) {
  int n = (int)strlen(s);
  while (n > 0 && isspace((unsigned char)s[n - 1]))
    s[--n] = 0;
}

char *vm_ltrim(char *s) {
  while (*s && isspace((unsigned char)*s))
    s++;
  return s;
}

int vm_is_empty_or_comment(const char *s) {
  while (*s && isspace((unsigned char)*s))
    s++;
  return (*s == 0 || *s == '#' || (*s == '/' && s[1] == '/'));
}

/* ================= labels  ================= */

Label labels[VM_LABELS_MAX];
size_t label_count = 0;

bool vm_is_eol_or_eof(char c) { return c == '\0' || c == '\n'; }
bool vm_parse_string_literal(const char *p, char **out, const char *path,
                             size_t line) {
  if (*p != '"')
    return false;

  p++; // skip opening quote

  const char *start = p;
  size_t len = 0;

  while (*p) {
    if (*p == '\\') {
      p++; // skip backslash

      if (!*p) {
        vm_compile_errorf(path, line,
                          "unterminated escape sequence in string literal");
        return false;
      }

      p++; // skip escaped char
      len += 2;
      continue;
    }

    if (*p == '"') {
      p++; // skip closing quote

      if (!vm_is_eol_or_eof(*p)) {
        vm_compile_errorf(
            path, line,
            "syntax error in string declaration, expecting end-of-input");
        return false;
      }

      if (out) {
        char *buf = malloc(len + 1);
        if (!buf) {
          vm_compile_errorf(path, line, "out of memory");
          return false;
        }

        memcpy(buf, start, len);
        buf[len] = '\0';
        *out = buf;
      }

      return true;
    }

    p++;
    len++;
  }

  vm_compile_errorf(path, line, "unterminated string literal");
  return false;
}

void vm_add_label(const char *path, size_t line, const char *name, size_t ip) {
  for (size_t i = 0; i < label_count; i++)
    if (strcmp(labels[i].name, name) == 0)
      vm_compile_errorf(path, line, "duplicate label \"%s\"", name);

  labels[label_count].name = vm_strdup(name);
  labels[label_count].ip = ip;
  label_count++;
}

bool vm_vm_is_label_exist(const char *name) {
  for (size_t i = 0; i < label_count; i++)
    if (strcmp(labels[i].name, name) == 0)
      return true;
  return false;
}

size_t vm_label_ip(const char *path, size_t line, const char *name) {
  for (size_t i = 0; i < label_count; i++)
    if (strcmp(labels[i].name, name) == 0)
      return labels[i].ip;
  vm_compile_errorf(path, line, "unknown label \"%s\"", name);
  return 0;
}

/* ================= opcode lookup ================= */
static OpMap ops[] = {
    {"nop", OP_NOP},
    {"pushi", OP_PUSH_INT},
    {"pushs", OP_PUSH_STR},
    {"pushd", OP_PUSH_DOUBLE},
    {"pushnil", OP_PUSH_NIL},
    {"pushfn", OP_PUSH_FN},
    {"add", OP_ADD},
    {"sub", OP_SUB},
    {"mul", OP_MUL},
    {"div", OP_DIV},
    {"and", OP_AND},
    {"or", OP_OR},
    {"not", OP_NOT},
    {"pop", OP_POP},
    {"dup", OP_DUP},
    {"swap", OP_SWAP},
    {"pick", OP_PICK},
    {"rot", OP_ROT},
    {"eq", OP_EQ},
    {"neq", OP_NEQ},
    {"lt", OP_LT},
    {"gt", OP_GT},
    {"lte", OP_LTE},
    {"gte", OP_GTE},
    {"jz", OP_JZ},
    {"jnz", OP_JNZ},
    {"jl", OP_JL},
    {"jle", OP_JLE},
    {"jgt", OP_JGT},
    {"jgte", OP_JGTE},
    {"jmp", OP_JMP},
    {"enter", OP_ENTER},
    {"typeof", OP_TYPEOF},
    {"load", OP_LOAD},
    {"store", OP_STORE},
    {"loadg", OP_LOAD_GLOBAL},
    {"storeg", OP_STORE_GLOBAL},
    {"array_new", OP_ARRAY_NEW},
    {"array_del", OP_ARRAY_DEL},
    {"array_get", OP_ARRAY_GET},
    {"array_set", OP_ARRAY_SET},
    {"array_len", OP_ARRAY_LEN},
    {"array_push", OP_ARRAY_PUSH},
    {"object_new", OP_OBJECT_NEW},
    {"object_del", OP_OBJECT_DEL},
    {"object_get", OP_OBJECT_GET},
    {"object_set", OP_OBJECT_SET},
    {"object_len", OP_OBJECT_LEN},
    {"call", OP_CALL},
    {"concat", OP_CONCAT},
    {"len", OP_LEN},
    {"ret", OP_RET},
    {"ret_void", OP_RET_VOID},
    {"load_lib", OP_LOAD_LIB},
    {"halt", OP_HALT},
};

int vm_find_op(const char *s, OpCode *out) {
  int n = sizeof(ops) / sizeof(ops[0]);
  for (int i = 0; i < n; i++)
    if (strcmp(ops[i].name, s) == 0) {
      *out = ops[i].op;
      return 1;
    }
  return 0;
}

/* ================= label/named builtins syntax ================= */

bool vm_is_label(const char *s, StrBuf *out) {
  const char *p = s;
  if (!isalpha((unsigned char)*p) && *p != '_' && *p != '$')
    return false;

  while (isalnum((unsigned char)*p) || *p == '_' || *p == '$')
    p++;

  if (*p != ':')
    return false;

  size_t len = (size_t)(p - s);
  vm_sb_reserve(out, len + 1);
  memcpy(out->buf, s, len);
  out->buf[len] = '\0';
  return true;
}

/* ================= assembler ================= */
char *vm_read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;

  fseek(f, 0, SEEK_END);
  long pos = ftell(f);
  if (pos < 0) {
    perror("read file error");
    exit(1);
  }
  rewind(f);
  size_t size = (size_t)(pos);
  char *buf = (char *)malloc(size + 1);
  if (!buf)
    exit(1);

  fread(buf, 1, size, f);
  buf[size] = '\0';

  fclose(f);
  return buf;
}
char *vm_find_eol(char *p) {
  while (*p && *p != '\n')
    p++;
  return p;
}

Inst *vm_load_program(VM *vm, const char *path) {
  char *source = vm_read_file(path);
  if (!source)
    return NULL;

  label_count = 0;

  size_t ip = 0;
  size_t line = 1;

  /* ======================================================
     PASS 1: collect labels and instruction count
     ====================================================== */
  // char *linebuf = NULL;
  // size_t linebufcap = 512;
  StrBuf linestr = {0};
  StrBuf tmplabelname = {0};
  vm_sb_init(&linestr);
  vm_sb_init(&tmplabelname);
  vm_sb_reserve(&linestr, 256);
  vm_sb_reserve(&tmplabelname, 256);
  for (char *p = source; *p;) {
    char *eol = vm_find_eol(p);
    size_t len = (size_t)(eol - p);
    vm_sb_reserve(&linestr, len);
    memcpy(linestr.buf, p, len);
    linestr.buf[len] = '\0';
    linestr.len = 0;
    char *s = vm_ltrim(linestr.buf);
    vm_rtrim(s);

    if (!vm_is_empty_or_comment(s)) {
      tmplabelname.buf[0] = '\0';
      tmplabelname.len = 0;
      if (vm_is_label(s, &tmplabelname)) {
        vm_add_label(path, line, tmplabelname.buf, ip);
      } else {
        ip++;
      }
    }

    if (*eol == '\n')
      eol++;

    p = eol;
    line++;
  }
  free(linestr.buf);
  linestr.len = 0;
  tmplabelname.buf[0] = '\0';
  tmplabelname.len = 0;
  Inst *code = (Inst *)calloc(ip, sizeof(Inst));

  Program *program = malloc(sizeof(Program));
  program->inst_count = 0;
  program->inst = code;
  if (!code)
    exit(1);

  /* ======================================================
     PASS 2: emit instructions
     ====================================================== */

  int pc = 0;
  line = 1;

  for (char *p = source; *p;) {
    char *line_start = p;
    char *next = strchr(p, '\n');
    if (next) {
      *next = '\0'; // destructive OK here
      p = next + 1;
    } else {
      p += strlen(p);
    }

    char *s = vm_ltrim(line_start);
    vm_rtrim(s);

    if (vm_is_empty_or_comment(s)) {
      line++;
      continue;
    }

    tmplabelname.buf[0] = '\0';
    if (vm_is_label(s, &tmplabelname) || *s == '@') {
      line++;
      continue;
    }

    const char *op_s = strtok(s, " \t");
    if (!op_s) {
      line++;
      continue;
    }

    OpCode op;
    if (!vm_find_op(op_s, &op))
      vm_compile_errorf(path, line, "unknown opcode \"%s\"", op_s);

    Inst in;
    memset(&in, 0, sizeof(in));
    in.type = op;
    in.source_line_num = line;
    /* ---------------- operands ---------------- */
    if (op == OP_PUSH_INT) {
      char *a = strtok(NULL, " \t");
      if (!a)
        vm_compile_errorf(path, line, "missing operand for \"%s\"", op_s);
      in.operand = strtoll(a, NULL, 10);
    } else if (op == OP_PUSH_FN) {
      char *t = strtok(NULL, " \t");
      if (!t)
        vm_compile_errorf(path, line, "%s needs label", op_s);

      in.u = vm_label_ip(path, line, t);
    } else if (op == OP_PUSH_DOUBLE) {
      char *a = strtok(NULL, " \t");
      if (!a)
        vm_compile_errorf(path, line, "missing operand for \"%s\"", op_s);
      in.d = strtod(a, NULL);
    } else if (op == OP_LOAD || op == OP_STORE || op == OP_ENTER ||
               op == OP_PICK || op == OP_STORE_GLOBAL || op == OP_LOAD_GLOBAL) {
      char *a = strtok(NULL, " \t");
      if (!a)
        vm_compile_errorf(path, line, "missing operand for \"%s\"", op_s);
      long number = strtoll(a, NULL, 10);
      if (number < 0) {
        vm_compile_errorf(path, line,
                          "%s operand must be equal or greater than 0", op_s);
      }
      if ((op == OP_STORE_GLOBAL || op == OP_LOAD_GLOBAL) &&
          number >= VM_GLOBALS_MAX) {
        vm_compile_errorf(path, line, "%s operand must be less then %d", op_s,
                          number);
      }
      in.u = (size_t)number;
    } else if (op == OP_CALL) {
      char *a = strtok(NULL, " \t");
      if (!a) {
        in.u = 0;
      } else {
        long parse = strtoll(a, NULL, 10);
        if (parse < 0) {
          vm_compile_errorf(path, line, "Call arity must be zero or greater");
        }
        in.u = (size_t)parse;
      }
    } else if (op == OP_JZ || op == OP_JNZ || op == OP_JL || op == OP_JLE ||
               op == OP_JGT || op == OP_JGTE || op == OP_JMP) {

      char *t = strtok(NULL, " \t");
      if (!t)
        vm_compile_errorf(path, line, "jump needs label for %s", op_s);

      size_t target = vm_label_ip(path, line, t);
      in.u = target;
    } else if (op == OP_PUSH_STR) {
      char *strbuf = NULL;
      while (!vm_is_eol_or_eof(*s) && *s != '"') {
        s++;
      }
      s++; // quote
      vm_parse_string_literal(s, &strbuf, path, line);
      in.chars = vm_strdup(strbuf);
      if (strbuf != NULL) {
        free(strbuf);
      }
    } else if (op == OP_ROT) {
      char *a = strtok(NULL, " \t");
      if (!a)
        vm_compile_errorf(path, line, "missing operand for %s", op_s);
      long amount = strtoll(a, NULL, 10);
      if (amount < 2) {
        vm_compile_errorf(path, line, "%s operand must be GTE than 2", op_s);
      }
      in.u = (size_t)amount;
    }

    code[pc++] = in;
    line++;
  }
  vm->program = program;
  vm->program->inst_count = ip;

  free(source);
  free(tmplabelname.buf);
  tmplabelname.len = 0;
  return code;
}
