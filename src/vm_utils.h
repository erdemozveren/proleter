#ifndef VM_UTILS_H
#define VM_UTILS_H

#include "vm_structs.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HEAP_BLOCK_SIZE (1024 * 1024) // 1 MiB blocks
#define ASSERT_TYPE(vm, v, ...) assertType((vm), (v), __VA_ARGS__, VAL_TYPE_END)
#define VM_NIL                                                                 \
  (Value) { .type = VAL_NIL }
#define DEBUG 0

static void vm_runtime_errorf(const Inst *inst, const char *fmt, ...) {
  fprintf(stderr, "\n=== VM RUNTIME ERROR ===\n");

  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  if (inst) {
    printf("instruction at line %ld", inst->soure_line_num);
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "========================\n");
  exit(1);
}
static char *typeof_value(ValueType type) {
  if (!type)
    return "undefined";
  switch (type) {
  case VAL_NIL:
    return "nil";
  case VAL_INT:
    return "int";
  case VAL_DOUBLE:
    return "double";
  case VAL_STR:
    return "string";
  case VAL_ARRAY:
    return "array";
  case VAL_OBJECT:
    return "object";
  case VAL_CALLABLE:
    return "function";
  case VAL_TYPE_END:
    vm_runtime_errorf(NULL, "type_end should never fire in typeof_value");
    break;
  default:
    vm_runtime_errorf(NULL, "undeclared typeof_value");
    break;
  }
  return "undefined";
}

static const char *opcode_name(OpCode op) {
  switch (op) {
  case OP_NOP:
    return "nop";
  case OP_PUSH_NIL:
    return "pushnil";
  case OP_PUSH_INT:
    return "pushi";
  case OP_PUSH_DOUBLE:
    return "pushd";
  case OP_PUSH_STR:
    return "pushs";
  case OP_PUSH_FN:
    return "pushfn";

  case OP_ADD:
    return "add";
  case OP_SUB:
    return "sub";
  case OP_MUL:
    return "mul";
  case OP_DIV:
    return "div";
  case OP_AND:
    return "and";
  case OP_OR:
    return "or";
  case OP_NOT:
    return "not";
  case OP_POP:
    return "pop";
  case OP_DUP:
    return "dup";
  case OP_SWAP:
    return "swap";
  case OP_PICK:
    return "pick";
  case OP_ROT:
    return "rot";
  case OP_LEN:
    return "len";
  case OP_CONCAT:
    return "concat";
  case OP_TYPEOF:
    return "typeof";

  case OP_EQ:
    return "eq";
  case OP_NEQ:
    return "neq";
  case OP_LT:
    return "lt";
  case OP_GT:
    return "gt";
  case OP_LTE:
    return "lte";
  case OP_GTE:
    return "gte";

  case OP_JZ:
    return "jz";
  case OP_JNZ:
    return "jnz";
  case OP_JL:
    return "jl";
  case OP_JLE:
    return "jle";
  case OP_JGT:
    return "jgt";
  case OP_JGTE:
    return "jgte";
  case OP_JMP:
    return "jmp";

  case OP_ENTER:
    return "enter";
  case OP_LOAD:
    return "load";
  case OP_STORE:
    return "store";
  case OP_LOAD_GLOBAL:
    return "loadg";
  case OP_STORE_GLOBAL:
    return "storeg";

  case OP_ARRAY_NEW:
    return "array_new";
  case OP_ARRAY_PUSH:
    return "array_push";
  case OP_ARRAY_DEL:
    return "array_del";
  case OP_ARRAY_GET:
    return "array_get";
  case OP_ARRAY_SET:
    return "array_set";
  case OP_ARRAY_LEN:
    return "array_len";

  case OP_OBJECT_NEW:
    return "object_new";
  case OP_OBJECT_DEL:
    return "object_del";
  case OP_OBJECT_GET:
    return "object_get";
  case OP_OBJECT_SET:
    return "object_set";
  case OP_OBJECT_LEN:
    return "object_len";

  case OP_CALL:
    return "call";
  case OP_RET:
    return "ret";
  case OP_RET_VOID:
    return "ret_void";

  case OP_LOADDLL:
    return "loaddll";

  case OP_HALT:
    return "halt";

  default:
    return "unknown_op";
  }
}

static const char *value_type_name(ValueType t) {
  // same as typeof_value but for internals, doesnt panic
  switch (t) {
  case VAL_NIL:
    return "nil";
  case VAL_INT:
    return "int";
  case VAL_DOUBLE:
    return "double";
  case VAL_STR:
    return "string";
  case VAL_ARRAY:
    return "array";
  case VAL_OBJECT:
    return "object";
  case VAL_CALLABLE:
    return "function";
  case VAL_TYPE_END:
  default:
    printf("%d\n", t);
    return "unknown";
  }
}

static const Value *assertType(VM *vm, const Value *v, ...) {
  if (!v) {
    vm_runtime_errorf(NULL, "Value is NULL");
  }

  va_list ap;
  va_start(ap, v);

  bool ok = false;
  ValueType t;

  /* First pass: check if type matches */
  while ((t = va_arg(ap, ValueType)) != VAL_TYPE_END) {
    if (v->type == t) {
      ok = true;
      break;
    }
  }

  va_end(ap);

  if (ok) {
    return v;
  }

  /* Second pass: build error message */
  fprintf(stderr, "Runtime error: expected one of: ");

  va_start(ap, v);
  bool first = true;
  while ((t = va_arg(ap, ValueType)) != VAL_TYPE_END) {
    if (!first)
      fprintf(stderr, ", ");
    fprintf(stderr, "%s", value_type_name(t));
    first = false;
  }
  va_end(ap);

  fprintf(stderr, ", got %s", value_type_name(v->type));
  if (vm) {
    fprintf(stderr, ", at line %ld\n",
            vm->program->inst[vm->ip].soure_line_num);
  } else {
    fprintf(stderr, "\n");
  }

  exit(1);
}

static size_t get_rss_kb(void) {
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

static void vm_print_mem_usage(const VM *vm) {
  size_t total = 0;
  size_t used = 0;
  if (vm->heap.current) {
    for (HeapBlock *b = vm->heap.current; b; b = b->next) {
      used += b->used;
      total += b->capacity;
    }
  }

  printf("Vm Heap usage: %.2f / %.2f MiB\nRss: %.2f\n",
         (double)used / (1024.0 * 1024.0), (double)total / (1024.0 * 1024.0),
         (double)get_rss_kb() / (1024.0 * 1024.0));
}

static void vm_print_stack_top(const VM *vm) {
  printf("=== STACK (top 10) sp[%ld] ===\n", vm->sp);
  vm_print_mem_usage(vm);

  size_t count = vm->sp < 10 ? vm->sp : 10;
  for (size_t i = 0; i < count; i++) {
    size_t idx = vm->sp - 1 - i;
    Value v = vm->stack[idx];
    // +1 from idx
    if (idx == vm->fp - 1) {
      printf("---------fp[%ld]---------\n", vm->fp);
    }
    printf("[%2ld] ", idx);

    switch (v.type) {
    case VAL_CALLABLE:
      if (v.as.fn->type == CALLABLE_NATIVE) {
        printf("func %s()\n", v.as.fn->name);
      } else {
        printf("func ip:%ld()\n", v.as.fn->as.entry_ip);
      }
      break;
    case VAL_NIL:
      printf("NIL\n");
      break;
    case VAL_INT:
      printf("INT    %ld\n", v.as.i);
      break;
    case VAL_DOUBLE:
      printf("DOUBLE    %f\n", v.as.d);
      break;
    case VAL_ARRAY:
      printf("ARRAY(%ld)\n", v.as.arr->len);
      break;
    case VAL_OBJECT:
      printf("OBJECT(%ld)\n", v.as.obj->len);
      break;

    case VAL_STR:
      printf("STRING \"%s\"\n", v.as.str->chars);
      break;
    case VAL_TYPE_END:
    default:
      printf("UNKNOWN\n");
      break;
    }
  }

  if (vm->sp == 0)
    printf("(stack empty)\n");

  printf("======================\n");
}

static void assert_min_stack(VM *vm, size_t min) {
  if (vm->sp >= min)
    return;
  vm_runtime_errorf(NULL,
                    "Stack Underflow -- instruction need %ld values got %ld, "
                    "at line %ld (%s)",
                    min, vm->sp, vm->program->inst[vm->ip].soure_line_num,
                    opcode_name(vm->program->inst[vm->ip].type));
}

static void push(VM *vm, Value v) {
  assert(vm->sp < STACK_MAX);
  vm->stack[vm->sp++] = v;
}

static Value pop(VM *vm) {
  assert(vm->sp > 0);
  return vm->stack[--vm->sp];
}

static inline int val_is_numeric(Value v) {
  return v.type == VAL_INT || v.type == VAL_DOUBLE;
}

static inline double val_to_double(Value v) {
  return v.type == VAL_INT ? (double)v.as.i : v.as.d;
}
static inline int val_eq(Value a, Value b) {
  /* numeric cross-type equality */
  if (val_is_numeric(a) && val_is_numeric(b)) {
    return val_to_double(a) == val_to_double(b);
  }

  /* non-numeric types must match exactly */
  if (a.type != b.type) {
    return 0;
  }

  switch (a.type) {
  case VAL_CALLABLE:
    return a.as.fn == b.as.fn;

  case VAL_NIL:
    return 1;
  case VAL_ARRAY:
    return a.as.arr == b.as.arr;
  case VAL_OBJECT:
    return a.as.obj == b.as.obj;

  case VAL_STR:
    return strcmp(a.as.str->chars, b.as.str->chars) == 0;

  // Handled already,just to supress varning
  case VAL_INT:
  case VAL_DOUBLE:
  case VAL_TYPE_END:
    vm_runtime_errorf(NULL, "This should never fire");
    return 0;
  default:
    return 0;
  }
}

static size_t align_up(size_t n, size_t align) {
  return (n + align - 1) & ~(align - 1);
}

static HeapBlock *heap_new_block(size_t min_size) {
  size_t cap = min_size > HEAP_BLOCK_SIZE ? min_size : HEAP_BLOCK_SIZE;
  HeapBlock *b = (HeapBlock *)malloc(sizeof(HeapBlock));
  if (!b) {
    printf("Heap can not allocate memory\n");
    exit(1);
  }
  b->data = (char *)malloc(cap);
  b->capacity = cap;
  b->used = 0;
  b->next = NULL;
  return b;
}

static void *vm_heap_alloc(Heap *h, size_t size, size_t align) {
  HeapBlock *b = h->current;

  /* ensure we have a block */
  if (!b) {
    b = heap_new_block(size + align);
    b->next = NULL;
    h->current = b;
  }

  /* align the OFFSET, not the size */
  size_t off = align_up(b->used, align);

  /* check if current block fits */
  if (off + size > b->capacity) {
    /* global heap limit check */
    if (h->used + size > h->capacity) {
      printf("Heap memory limit reached (%.2f MiB)\n",
             (double)h->capacity / (1024.0 * 1024.0));
      exit(1);
    }

    /* allocate a new block with alignment slack */
    HeapBlock *nb = heap_new_block(size + align);
    nb->next = h->current;
    h->current = nb;
    b = nb;

    off = align_up(b->used, align);
  }

  void *ptr = b->data + off;
  b->used = off + size;
  h->used += size;

#ifdef DEBUG
  /* alignment sanity check */
  if (((uintptr_t)ptr & (align - 1)) != 0) {
    fprintf(stderr, "vm_heap_alloc returned misaligned pointer\n");
    abort();
  }
#endif

  return ptr;
}

static void free_program(Program *p) {
  // for (int i = 0; i < count; i++) {
  //      if (code[i].type == OP_PUSH_STRING) {
  //          free(code[i].chars);
  //      }
  //  }
  if (p->inst != NULL) {
    free(p->inst);
  }
  free(p);
}

static void vm_heap_free(VM *vm) {
  if (vm->heap.used == 0)
    return;
  HeapBlock *b = vm->heap.current;
  while (b) {
    HeapBlock *next = b->next;
    free(b->data);
    free(b);
    b = next;
  }
}

static Value vm_new_int(int64_t val) {
  return (Value){.type = VAL_INT, .as.i = val};
}

static Value vm_new_double(double val) {
  return (Value){.type = VAL_DOUBLE, .as.d = val};
}

static Value vm_object_new(VM *vm, size_t initial_cap) {
  Object *o = vm_heap_alloc(&vm->heap, sizeof(Object), alignof(Object));

  o->h.kind = OBJ_OBJECT;
  o->h.size = sizeof(Object);

  o->len = 0;
  o->cap = initial_cap;
  o->proto = NULL;

  if (initial_cap > 0) {
    o->entries = vm_heap_alloc(&vm->heap, initial_cap * sizeof(ObjEntry),
                               alignof(ObjEntry));
  } else {
    o->entries = NULL;
  }

  return (Value){.type = VAL_OBJECT, .as.obj = o};
}
static void vm_object_grow(VM *vm, Object *o, size_t needed_size) {
  assert(needed_size != 0);

  if (needed_size >= o->cap) {
    ObjEntry *new_entries = vm_heap_alloc(
        &vm->heap, needed_size * sizeof(ObjEntry), alignof(ObjEntry));

    if (o->entries) {
      memcpy(new_entries, o->entries, o->len * sizeof(ObjEntry));
    }

    o->entries = new_entries;
    o->cap = needed_size;

    o->h.size = sizeof(Object) + o->cap * sizeof(ObjEntry);
    // old buffer becomes garbage (GC later)
  }
}

static void vm_object_set(VM *vm, Object *o, const char *key, Value v) {
  // overwrite if exists
  for (size_t i = 0; i < o->len; i++) {
    if (strcmp(o->entries[i].key, key) == 0) {
      o->entries[i].value = v;
      return;
    }
  }

  // grow if needed
  if (o->len >= o->cap) {
    size_t new_cap = o->cap ? o->cap * 2 : 8;
    vm_object_grow(vm, o, new_cap);
  }
  size_t keylen = strlen(key);
  o->entries[o->len].key = vm_heap_alloc(&vm->heap, keylen + 1, alignof(char));
  memcpy(o->entries[o->len].key, key, keylen);
  o->entries[o->len].key[keylen] = '\0';
  o->entries[o->len].value = v;
  o->len++;
}

static bool vm_object_get(Object *o, const char *key, Value *out) {
  for (size_t i = 0; i < o->len; i++) {
    if (strcmp(o->entries[i].key, key) == 0) {
      *out = o->entries[i].value;
      return true;
    }
  }

  if (o->proto) {
    return vm_object_get(o->proto, key, out);
  }

  return false;
}

static void vm_object_del(Object *o, const char *key) {
  for (size_t i = 0; i < o->len; i++) {
    if (strcmp(o->entries[i].key, key) == 0) {

      for (size_t j = i; j < o->len - 1; j++) {
        o->entries[j] = o->entries[j + 1];
      }

      o->len--;
      return;
    }
  }

  // not found → no-op
}

static Value vm_array_new(VM *vm, size_t initial_cap) {
  Array *a = vm_heap_alloc(&vm->heap, sizeof(Array), alignof(Array));

  a->h.kind = OBJ_ARRAY;
  a->h.size = sizeof(Array);
  a->len = initial_cap;
  a->cap = initial_cap;

  a->items =
      vm_heap_alloc(&vm->heap, initial_cap * sizeof(Value), alignof(Value));

  return (Value){.type = VAL_ARRAY, .as.arr = a};
}

static void vm_array_grow(VM *vm, Array *a, size_t needed_size) {
  assert(needed_size != 0);
  if (needed_size >= a->cap) {

    Value *new_items =
        vm_heap_alloc(&vm->heap, needed_size * sizeof(Value), alignof(Value));

    memcpy(new_items, a->items, a->len * sizeof(Value));

    a->items = new_items;
    a->cap = needed_size;
    a->len = needed_size;
    a->h.size = sizeof(Array) + a->cap * sizeof(Value);

    // old buffer becomes garbage (GC later)
  }
}

static void vm_array_push(VM *vm, Array *a, Value v) {
  if (a->len >= a->cap) {
    size_t new_cap = a->cap ? a->cap * 2 : 8;
    vm_array_grow(vm, a, new_cap);
    // old buffer becomes garbage (GC later)
  }

  a->items[a->len++] = v;
}

static void vm_array_del(Array *a, size_t index) {
  if (index + 1 > a->len && a->len > 0) {
    vm_runtime_errorf(NULL,
                      "Array try to delete item out of bound, idx:%ld len:%ld",
                      index, a->len);
  }
  if (index != a->len - 1) {
    for (size_t i = index; i < a->len - 1; i++) {
      Value next = a->items[i + 1];
      a->items[i] = next;
    }
  }
  a->len -= 1;
}

static String *vm_alloc_string(VM *vm, const char *s) {
  const char *p = s;
  size_t len = 0;

  /* -------- pass 1: compute decoded length -------- */
  while (*p) {
    if (*p == '\\' && p[1]) {
      p++; // skip escape marker
    }
    len++;
    p++;
  }
  size_t total_size = sizeof(String) + len + 1;

  String *str = (String *)vm_heap_alloc(&vm->heap, total_size, alignof(String));
  str->h.kind = OBJ_STRING;
  str->h.size = total_size;
  str->len = len;
  /* -------- pass 2: decode escapes -------- */
  char *out = str->chars;
  p = s;

  while (*p) {
    if (*p == '\\') {
      p++;
      switch (*p) {
      case 'n':
        *out++ = '\n';
        break;
      case 't':
        *out++ = '\t';
        break;
      case '\\':
        *out++ = '\\';
        break;
      case '"':
        *out++ = '"';
        break;
      default:
        /* unknown escape → keep literal */
        *out++ = *p;
        break;
      }
    } else {
      *out++ = *p;
    }
    p++;
  }

  *out = '\0';

  return str;
}

static Value vm_new_string(VM *vm, char *str) {
  return (Value){.type = VAL_STR, .as.str = vm_alloc_string(vm, str)};
}

static void sb_init(StrBuf *b) {
  b->buf = NULL;
  b->len = 0;
  b->cap = 0;
}

static void sb_reserve(StrBuf *b, size_t need) {
  if (b->len + need <= b->cap)
    return;

  size_t new_cap = b->cap ? b->cap * 2 : 16;
  while (new_cap < b->len + need)
    new_cap *= 2;

  char *p = realloc(b->buf, new_cap);
  if (!p) {
    printf("sb_reserve realloc error");
    exit(1);
  }
  b->buf = p;
  b->cap = new_cap;
}

// static void sb_push_char(StrBuf *b, char c) {
//   sb_reserve(b, 1);
//   b->buf[b->len++] = c;
// }

// static void sb_append(StrBuf *b, const char *s) {
//   size_t n = strlen(s);
//   sb_reserve(b, n);
//   memcpy(b->buf + b->len, s, n);
//   b->len += n;
// }
static size_t value_char_len(Value *v) {
  if (!v) {
    vm_runtime_errorf(NULL, "value_char_len needs a value");
  }
  int len;
  switch (v->type) {
  case VAL_INT:
    len = snprintf(NULL, 0, "%d", (int)v->as.i);
    break;
  case VAL_DOUBLE:
    len = snprintf(NULL, 0, "%f", v->as.d);
    break;
  case VAL_STR:
    len = (int)v->as.str->len;
    break;
  case VAL_CALLABLE:
  case VAL_ARRAY:
  case VAL_OBJECT:
    // fallthrough
  case VAL_NIL:
  case VAL_TYPE_END:
  default:
    vm_runtime_errorf(NULL, "%s type as string length can not be used",
                      value_type_name(v->type));
    break;
  }
  if (len < 0) {
    vm_runtime_errorf(NULL, "unkown error at %s type string length conversion",
                      value_type_name(v->type));
  }
  return (size_t)len;
}

static size_t append_value_as_str(char *out, size_t cap, size_t off, Value *v) {
  switch (v->type) {
  case VAL_INT:
    return off + (size_t)snprintf(out + off, cap - off, "%d", (int)v->as.i);
  case VAL_DOUBLE:
    return off + (size_t)snprintf(out + off, cap - off, "%f", v->as.d);
  case VAL_STR:
    return off + (size_t)snprintf(out + off, cap - off, "%.*s",
                                  (int)v->as.str->len, v->as.str->chars);
  case VAL_CALLABLE:
  case VAL_ARRAY:
  case VAL_OBJECT:
    // fallthrough
  case VAL_NIL:
  case VAL_TYPE_END:
  default:
    return off;
  }
}

static void concat_val_as_string(Value *a, Value *b, char **out) {
  size_t alen = value_char_len(a);
  size_t blen = value_char_len(b);
  size_t size = sizeof(char) * (alen + blen + 1);
  size_t off = 0;
  *out = malloc(size);
  off = append_value_as_str(*out, size, off, a);
  off = append_value_as_str(*out, size, off, b);
}

/*
 * Loads a shared library and calls init_lib(vm).
 * uses memcpy isntead of casting because of ISO C standarts
 * Returns 0 on success, -1 on failure.
 */
static bool load_and_push_lib(const char *path, VM *vm) {
  void *handle;
  void *sym;
  init_lib_fn init_lib;

  dlerror();

  handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    fprintf(stderr, "dlopen failed: %s\n", dlerror());
    return false;
  }

  sym = dlsym(handle, "init_lib");
  if (!sym) {
    fprintf(stderr, "dlsym failed: %s\n", dlerror());
    dlclose(handle);
    return false;
  }

  memcpy(&init_lib, &sym, sizeof(init_lib));

  Value res = init_lib(vm);
  push(vm, res);

  return true;
}

#endif
