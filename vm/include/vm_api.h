#ifndef VM_API_H
#define VM_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROLETER_API_VERSION 1

#ifndef PROLETER_DEBUG
#define PROLETER_DEBUG 0
#endif

#ifndef PROLETER_LIB_INIT_FN
#define PROLETER_LIB_INIT_FN init_lib
#endif

#ifndef PROLETER_LIB_INIT_NAME
#define PROLETER_LIB_INIT_NAME "init_lib"
#endif

typedef struct VM VM;
typedef struct String String;
typedef struct Array Array;
typedef struct Object Object;
typedef struct Callable Callable;
typedef struct Value Value;

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

typedef int (*vm_api_version_fn)(void);
typedef Value (*init_lib_fn)(VM *vm);
typedef Value (*NativeFn)(VM *vm, size_t argc, Value *argv);
typedef void (*vm_object_iter_fn)(const char *key, Value val, void *user_data);

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

#define VM_NIL ((Value){.type = VAL_NIL})

/* =========================
 * Value helpers / constructors
 * ========================= */

char *vm_value_type_name(ValueType t);

int vm_val_is_numeric(Value v);
double vm_val_to_double(Value v);
int vm_val_is_eq(Value a, Value b);

const char *vm_string_chars(String *s);
size_t vm_string_len(String *s);

Value vm_new_int(int64_t val);
Value vm_new_double(double val);
Value vm_new_string(VM *vm, const char *str);
Value vm_make_native(VM *vm, const char *name, NativeFn fn);

/* =========================
 * Object values
 * ========================= */

Value vm_object_new(VM *vm, size_t initial_cap);
void vm_object_set(VM *vm, Object *o, const char *key, Value v);
bool vm_object_get(Object *o, const char *key, Value *out);
void vm_object_del(Object *o, const char *key);
void vm_object_iter(Object *o, vm_object_iter_fn fn, void *ud);
size_t vm_object_len(Object *a);

/* =========================
 * Array values
 * ========================= */

Value vm_array_new(VM *vm, size_t initial_cap);
void vm_array_set(VM *vm, Array *a, size_t index, Value v);
void vm_array_push(VM *vm, Array *a, Value v);
Value vm_array_get(Array *a, size_t index);
void vm_array_del(Array *a, size_t index);
size_t vm_array_len(Array *a);

/* =========================
 * Public helper
 * ========================= */

void vm_print_stack_top(const VM *vm);
void vm_panic(const char *fmt, ...);
void vm_halt(VM *vm);

size_t vm_gc_allocated(VM *vm);
size_t vm_gc_next_bytes(VM *vm);
void vm_gc_collect(VM *vm);
void vm_gc_pause(VM *vm);
void vm_gc_resume(VM *vm);
void vm_gc_collect_if_needed(VM *vm);

#endif
