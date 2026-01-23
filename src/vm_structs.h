#ifndef VM_STRUCTS_H
#define VM_STRUCTS_H
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define STACK_MAX 65536
#define GLOBALS_MAX 65536
#define CALL_MAX 256
#define FN_MAX 256

typedef enum { OBJ_STRING = 1, OBJ_ARRAY, OBJ_OBJECT } ObjKind;

typedef struct VM VM;
typedef struct Value Value;
typedef struct Array Array;
typedef struct Object Object;

typedef struct HeapObj {
  uint8_t kind; // stores ObjKind value
  // uint8_t flags; // reserved (GC mark bit later)
  // uint16_t _pad; // for memory alignment of strucutre, not used in logic
  size_t size; // total object size in bytes
} HeapObj;

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} StrBuf;

typedef struct {
  HeapObj h;
  size_t len;
  char chars[];
} String;

typedef enum {
  VAL_NIL = 0,
  VAL_INT,
  VAL_DOUBLE,
  VAL_CALLABLE,
  VAL_STR,
  VAL_ARRAY,
  VAL_OBJECT,
  VAL_TYPE_END
} ValueType;

typedef enum {
  CALLABLE_NATIVE,
  CALLABLE_USER,
} CallableType;

typedef Value (*init_lib_fn)(VM *vm);

typedef Value (*NativeFn)(VM *vm, size_t argc, Value *argv);

typedef struct {
  const char *name;
  CallableType type;
  union {
    NativeFn native;
    size_t entry_ip;
  } as;
} Callable;
typedef struct Value {
  ValueType type;
  union {
    String *str;
    Array *arr;
    Object *obj;
    Callable *fn;
    int64_t i;
    size_t u;
    double d;
  } as;
} Value;

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

typedef struct Object {
  HeapObj h;
  size_t len;
  size_t cap;
  ObjEntry *entries;
  struct Object *proto; // optional, can be NULL
} Object;

typedef struct {
  const char *name;
  size_t id;
} BuiltinMap;

typedef struct HeapBlock {
  char *data;
  size_t capacity;
  size_t used;
  struct HeapBlock *next;
} HeapBlock;

typedef struct {
  HeapBlock *current;
  size_t used;
  size_t capacity;
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

  OP_JZ,  // jump if zero (false)
  OP_JNZ, // jump if non-zero (true)
  OP_JL,  // jump less
  OP_JLE, // jump less then equal
  OP_JGT, // jump greater then
  OP_JGTE,
  OP_JMP,

  OP_ENTER,
  OP_LOAD,  // load local
  OP_STORE, // store local
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

  OP_LOADDLL,

  OP_HALT,
} OpCode;

typedef struct {
  union {
    int64_t operand;
    size_t u;
    double d;
    String *str;
  };
  OpCode type;
  size_t soure_line_num;
} Inst;

typedef struct {
  Inst *inst;
  size_t inst_count;
} Program;

struct VM {
  Value stack[STACK_MAX];
  Value globals[GLOBALS_MAX];
  Program *program;
  CallFrame frames[CALL_MAX];
  BuiltinMap builtin_map[FN_MAX];
  Heap heap;
  size_t builtin_count;
  size_t symbol_count;
  size_t frame_top;
  size_t sp;
  size_t fp;
  size_t ip;
  bool halt;
};
#endif
