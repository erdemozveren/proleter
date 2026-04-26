#ifndef VM_H
#define VM_H

#include "vm_api.h"

#define VM_STACK_MAX 65536
#define VM_GLOBALS_MAX 65536
#define VM_CALL_MAX 256
#define VM_FN_MAX 256

#define VM_HEAP_BLOCK_SIZE (1024 * 1024)
#define VM_GC_START_THRESHOLD (1024 * 1024)
#define VM_GC_GROW_FACTOR 2
#define VM_ASSERT_TYPE(vm, v, ...)                                             \
  vm_assertType((vm), (v), __VA_ARGS__, VAL_TYPE_END)

/* =========================
 * Decoding
 * ========================= */

#define VM_LABELS_MAX 512

typedef struct {
  char *name;
  size_t ip;
} Label;

/* =========================
 * Internal object layouts
 * ========================= */

typedef enum { OBJ_STRING = 1, OBJ_ARRAY, OBJ_OBJECT, OBJ_CALLABLE } ObjKind;

typedef struct HeapObj {
  ObjKind kind;
  bool marked;
  size_t size;
  struct HeapObj *next;
} HeapObj;

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} StrBuf;

struct String {
  HeapObj h;
  size_t len;
  char chars[];
};

struct Callable {
  HeapObj h;
  const char *name;
  CallableType type;
  union {
    NativeFn native;
    size_t entry_ip;
  } as;
};

struct Array {
  HeapObj h;
  size_t len;
  size_t cap;
  Value *items;
};

typedef struct {
  char *key;
  Value value;
} ObjEntry;

struct Object {
  HeapObj h;
  size_t len;
  size_t cap;
  ObjEntry *entries;
  struct Object *proto;
};

/* =========================
 * VM internals
 * ========================= */

typedef struct {
  const char *name;
  size_t id;
} BuiltinMap;

typedef struct {
  HeapObj *objects;
  size_t object_count;
  size_t bytes_allocated;
  size_t next_gc;
} Heap;

typedef struct {
  char *data;
  size_t capacity;
  size_t offset;
} Arena;

typedef struct {
  size_t return_ip;
  size_t old_fp;
  size_t argc;
  size_t locals;
} CallFrame;

typedef enum {
  OP_NOP = 1,
  OP_PUSH_NIL,
  OP_PUSH_INT,
  OP_PUSH_DOUBLE,
  OP_PUSH_STR,
  OP_PUSH_FN,

  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_AND,
  OP_OR,
  OP_NOT,
  OP_POP,
  OP_DUP,
  OP_SWAP,
  OP_ROT,
  OP_PICK,
  OP_CONCAT,
  OP_LEN,

  OP_EQ,
  OP_NEQ,
  OP_LT,
  OP_GT,
  OP_LTE,
  OP_GTE,
  OP_TYPEOF,

  OP_JZ,
  OP_JNZ,
  OP_JL,
  OP_JLE,
  OP_JGT,
  OP_JGTE,
  OP_JMP,

  OP_ENTER,
  OP_LOAD,
  OP_STORE,
  OP_LOAD_GLOBAL,
  OP_STORE_GLOBAL,

  OP_ARRAY_NEW,
  OP_ARRAY_PUSH,
  OP_ARRAY_DEL,
  OP_ARRAY_GET,
  OP_ARRAY_SET,
  OP_ARRAY_LEN,

  OP_OBJECT_NEW,
  OP_OBJECT_DEL,
  OP_OBJECT_GET,
  OP_OBJECT_SET,
  OP_OBJECT_LEN,

  OP_CALL,
  OP_RET,
  OP_RET_VOID,

  OP_LOAD_LIB,

  OP_HALT,
} OpCode;

typedef struct {
  const char *name;
  OpCode op;
} OpMap;

typedef struct {
  union {
    int64_t operand;
    size_t u;
    double d;
    char *chars;
  };
  OpCode type;
  size_t source_line_num;
} Inst;

typedef struct {
  Inst *inst;
  size_t inst_count;
} Program;

struct VM {
  Value stack[VM_STACK_MAX];
  Value globals[VM_GLOBALS_MAX];
  Program *program;
  CallFrame frames[VM_CALL_MAX];
  BuiltinMap builtin_map[VM_FN_MAX];
  Heap heap;
  size_t gc_pause_count;
  bool gc_requested;
  size_t frame_top;
  size_t sp;
  size_t fp;
  size_t ip;
  bool halt;
};

/* =========================
 * VM execution / instruction
 * ========================= */

int vm_inst_execute(VM *vm, const Inst inst);
void vm_run_program(VM *vm);

void vm_print_inst(const Inst *in);
const char *vm_opcode_name(OpCode op);

/* =========================
 * VM stack / runtime checks
 * ========================= */

void vm_push(VM *vm, Value v);
Value vm_pop(VM *vm);

void vm_assert_min_stack(VM *vm, size_t min);
const Value *vm_assertType(VM *vm, const Value *v, ...);

void vm_inst_errorf(const Inst *inst, const char *fmt, ...);

/* =========================
 * Internal string helpers
 * ========================= */

char *vm_strdup(const char *src);
String *vm_malloc_string(VM *vm, const char *s);

void vm_sb_init(StrBuf *b);
void vm_sb_reserve(StrBuf *b, size_t need);

size_t vm_value_char_len(Value *v);
size_t vm_append_value_as_str(char *out, size_t cap, size_t off, Value *v);
void vm_concat_val_as_string(Value *a, Value *b, char **out);

void vm_object_grow(VM *vm, Object *o, size_t needed_size);
void vm_array_grow_capacity(VM *vm, Array *a, size_t needed_size);

/* =========================
 * Heap / memory management
 * ========================= */

size_t vm_align_up(size_t n, size_t align);
void *vm_gc_alloc(VM *vm, size_t size, ObjKind kind);
void vm_gc_free_obj(VM *vm, HeapObj *obj);
void vm_gc_mark_roots(VM *vm);
void vm_gc_sweep(VM *vm);
void vm_gc_sweep_all(VM *vm);
void vm_gc_mark_obj(HeapObj *obj);
void vm_gc_mark_value(Value v);

void vm_heap_free(VM *vm);
void vm_free_program(Program *p);

/* =========================
 * Dynamic library loading
 * ========================= */

bool vm_has_shared_ext(const char *path);
bool vm_load_and_push_lib(const char *path, VM *vm);

/* =========================
 * Compiler / parser
 * ========================= */

void vm_compile_errorf(const char *path, size_t line, const char *fmt, ...);
void vm_rtrim(char *s);
char *vm_ltrim(char *s);
int vm_is_empty_or_comment(const char *s);
bool vm_is_eol_or_eof(char c);
bool vm_parse_string_literal(const char *p, char **out, const char *path,
                             size_t line);
void vm_add_label(const char *path, size_t line, const char *name, size_t ip);
bool vm_vm_is_label_exist(const char *name);
size_t vm_label_ip(const char *path, size_t line, const char *name);
int vm_find_op(const char *s, OpCode *out);
bool vm_is_label(const char *s, StrBuf *out);
char *vm_read_file(const char *path);
char *vm_find_eol(char *p);
Inst *vm_load_program(VM *vm, const char *path);

#endif
