#ifndef VM_FS_H
#define VM_FS_H

#include "vm_api.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define fs_mkdir(path) _mkdir(path)
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define fs_mkdir(path) mkdir(path, 0755)
#endif

int vm_api_version(void) { return PROLETER_API_VERSION; }

/* -------------------------
   helpers
   ------------------------- */

static const char *fs_expect_string(Value v, const char *name) {
  if (v.type != VAL_STR) {
    vm_panic("%s must be string, got '%s'", name, vm_value_type_name(v.type));
  }

  return vm_string_chars(v.as.str);
}

static Value fs_bool(int ok) { return vm_new_int(ok ? 1 : 0); }

static int fs_path_exists(const char *path) {
#ifdef _WIN32
  DWORD attr = GetFileAttributesA(path);
  return attr != INVALID_FILE_ATTRIBUTES;
#else
  struct stat st;
  return stat(path, &st) == 0;
#endif
}

static int fs_path_is_file(const char *path) {
#ifdef _WIN32
  DWORD attr = GetFileAttributesA(path);
  return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
  struct stat st;
  if (stat(path, &st) != 0)
    return 0;
  return S_ISREG(st.st_mode);
#endif
}

static int fs_path_is_dir(const char *path) {
#ifdef _WIN32
  DWORD attr = GetFileAttributesA(path);
  return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
  struct stat st;
  if (stat(path, &st) != 0)
    return 0;
  return S_ISDIR(st.st_mode);
#endif
}

/* -------------------------
   fs.readFile(path)
   ------------------------- */

static Value native_fs_read_file(VM *vm, size_t argc, Value *argv) {
  if (argc != 1) {
    vm_panic("fs.readFile(path) expects 1 argument");
  }

  const char *path = fs_expect_string(argv[0], "path");

  FILE *fp = fopen(path, "rb");
  if (!fp) {
    vm_panic("failed to open file '%s': %s", path, strerror(errno));
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    vm_panic("failed to seek file '%s'", path);
  }

  long size = ftell(fp);
  if (size < 0) {
    fclose(fp);
    vm_panic("failed to tell file size '%s'", path);
  }

  rewind(fp);

  char *buffer = malloc((size_t)size + 1);
  if (!buffer) {
    fclose(fp);
    vm_panic("out of memory reading file '%s'", path);
  }

  size_t read_size = fread(buffer, 1, (size_t)size, fp);
  fclose(fp);

  buffer[read_size] = '\0';

  Value result = vm_new_string(vm, buffer);
  free(buffer);

  return result;
}

/* -------------------------
   fs.writeFile(path, data)
   ------------------------- */

static Value native_fs_write_file(VM *vm, size_t argc, Value *argv) {
  (void)vm;
  if (argc != 2) {
    vm_panic("fs.writeFile(path, data) expects 2 arguments");
  }

  const char *path = fs_expect_string(argv[0], "path");
  const char *data = fs_expect_string(argv[1], "data");

  FILE *fp = fopen(path, "wb");
  if (!fp) {
    vm_panic("failed to open file '%s': %s", path, strerror(errno));
  }

  size_t len = strlen(data);
  size_t written = fwrite(data, 1, len, fp);

  fclose(fp);

  if (written != len) {
    vm_panic("failed to write file '%s'", path);
  }

  return VM_NIL;
}

/* -------------------------
   fs.appendFile(path, data)
   ------------------------- */

static Value native_fs_append_file(VM *vm, size_t argc, Value *argv) {
  if (argc != 2) {
    vm_panic("fs.appendFile(path, data) expects 2 arguments");
  }

  const char *path = fs_expect_string(argv[0], "path");
  const char *data = fs_expect_string(argv[1], "data");

  FILE *fp = fopen(path, "ab");
  if (!fp) {
    vm_panic("failed to open file '%s': %s", path, strerror(errno));
  }

  size_t len = strlen(data);
  size_t written = fwrite(data, 1, len, fp);

  fclose(fp);

  if (written != len) {
    vm_panic("failed to append file '%s'", path);
  }

  return VM_NIL;
}

/* -------------------------
   fs.exists(path)
   ------------------------- */

static Value native_fs_exists(VM *vm, size_t argc, Value *argv) {
  (void)vm;

  if (argc != 1) {
    vm_panic("fs.exists(path) expects 1 argument");
  }

  const char *path = fs_expect_string(argv[0], "path");
  return fs_bool(fs_path_exists(path));
}

/* -------------------------
   fs.isFile(path)
   ------------------------- */

static Value native_fs_is_file(VM *vm, size_t argc, Value *argv) {
  (void)vm;

  if (argc != 1) {
    vm_panic("fs.isFile(path) expects 1 argument");
  }

  const char *path = fs_expect_string(argv[0], "path");
  return fs_bool(fs_path_is_file(path));
}

/* -------------------------
   fs.isDir(path)
   ------------------------- */

static Value native_fs_is_dir(VM *vm, size_t argc, Value *argv) {
  (void)vm;

  if (argc != 1) {
    vm_panic("fs.isDir(path) expects 1 argument");
  }

  const char *path = fs_expect_string(argv[0], "path");
  return fs_bool(fs_path_is_dir(path));
}

/* -------------------------
   fs.mkdir(path)
   ------------------------- */

static Value native_fs_mkdir(VM *vm, size_t argc, Value *argv) {
  (void)vm;

  if (argc != 1) {
    vm_panic("fs.mkdir(path) expects 1 argument");
  }

  const char *path = fs_expect_string(argv[0], "path");

  if (fs_mkdir(path) != 0) {
    vm_panic("failed to create directory '%s': %s", path, strerror(errno));
  }

  return VM_NIL;
}

/* -------------------------
   fs.remove(path)
   ------------------------- */

static Value native_fs_remove(VM *vm, size_t argc, Value *argv) {
  (void)vm;

  if (argc != 1) {
    vm_panic("fs.remove(path) expects 1 argument");
  }

  const char *path = fs_expect_string(argv[0], "path");

#ifdef _WIN32
  if (fs_path_is_dir(path)) {
    if (!RemoveDirectoryA(path)) {
      vm_panic("failed to remove directory '%s'", path);
    }
  } else {
    if (!DeleteFileA(path)) {
      vm_panic("failed to remove file '%s'", path);
    }
  }
#else
  if (remove(path) != 0) {
    vm_panic("failed to remove '%s': %s", path, strerror(errno));
  }
#endif

  return VM_NIL;
}

/* -------------------------
   fs.readDir(path)
   returns array<string>
   ------------------------- */

static Value native_fs_read_dir(VM *vm, size_t argc, Value *argv) {
  if (argc != 1) {
    vm_panic("fs.readDir(path) expects 1 argument");
  }

  const char *path = fs_expect_string(argv[0], "path");
  Value arr = vm_array_new(vm, 0);

#ifdef _WIN32
  char search_path[MAX_PATH];

  snprintf(search_path, sizeof(search_path), "%s\\*", path);

  WIN32_FIND_DATAA data;
  HANDLE h = FindFirstFileA(search_path, &data);

  if (h == INVALID_HANDLE_VALUE) {
    vm_panic("failed to read directory '%s'", path);
  }

  do {
    const char *name = data.cFileName;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      continue;
    }

    vm_array_push(vm, arr.as.arr, vm_new_string(vm, name));
  } while (FindNextFileA(h, &data));

  FindClose(h);
#else
  DIR *dir = opendir(path);

  if (!dir) {
    vm_panic("failed to read directory '%s': %s", path, strerror(errno));
  }

  struct dirent *entry;

  while ((entry = readdir(dir)) != NULL) {
    const char *name = entry->d_name;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      continue;
    }

    vm_array_push(vm, arr.as.arr, vm_new_string(vm, name));
  }

  closedir(dir);
#endif

  return arr;
}

/* -------------------------
   module init
   ------------------------- */

extern Value PROLETER_LIB_INIT_FN(VM *vm) {
  Value val = vm_object_new(vm, 9);
  Object *o = val.as.obj;

  vm_object_set(vm, o, "readFile",
                vm_make_native(vm, "readFile", native_fs_read_file));

  vm_object_set(vm, o, "writeFile",
                vm_make_native(vm, "writeFile", native_fs_write_file));

  vm_object_set(vm, o, "appendFile",
                vm_make_native(vm, "appendFile", native_fs_append_file));

  vm_object_set(vm, o, "exists",
                vm_make_native(vm, "exists", native_fs_exists));

  vm_object_set(vm, o, "isFile",
                vm_make_native(vm, "isFile", native_fs_is_file));

  vm_object_set(vm, o, "isDir", vm_make_native(vm, "isDir", native_fs_is_dir));

  vm_object_set(vm, o, "mkdir", vm_make_native(vm, "mkdir", native_fs_mkdir));

  vm_object_set(vm, o, "remove",
                vm_make_native(vm, "remove", native_fs_remove));

  vm_object_set(vm, o, "readDir",
                vm_make_native(vm, "readDir", native_fs_read_dir));

  return val;
}

#endif
