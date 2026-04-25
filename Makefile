.PHONY: all release debug runtime-libs clean

CC := gcc
VM_SRCS := vm/main.c vm/vm.c vm/vm_decoding.c
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -rdynamic 
LIB_FLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wshadow \
             -Wno-conversion -Wno-unused-parameter -Wno-unused-function \
             -shared -I./vm/include
DEBUG_FLAGS := -g -O0 -Werror -Wswitch-enum -Wmissing-prototypes -Wstrict-prototypes \
               -fsanitize=address,undefined -I./vm/include
RELEASE_FLAGS := -O2 -I./vm/include

all: release runtime-libs

release: clean vm/main.c vm/include/vm.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) $(VM_SRCS) -o build/proleter-vm

debug: vm/main.c runtime-libs
	@mkdir -p build
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(VM_SRCS) -o build/proleter-vm

runtime-libs:
	@echo "Compiling runtime modules..."
	@mkdir -p modules
	@for f in ./vm/runtime-libs/*.c; do \
		name=$$(basename $$f .c); \
		if [ "$(OS)" = "Windows_NT" ]; then \
			$(CC) $(LIB_FLAGS) $$f -o modules/$$name.dll; \
		else \
			$(CC) -fPIC $(LIB_FLAGS) $$f -o modules/$$name.so; \
		fi; \
	done

clean:
	@echo "Cleaning build and modules folders"
	@rm -rf ./build ./modules
