.PHONY: clean debug
CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
DEBUG_FLAGS := -g -O0 -Werror -Wswitch-enum -Wmissing-prototypes -Wstrict-prototypes \
               -fsanitize=address,undefined
RELEASE_FLAGS := -O2

vm: src/vm.h | build
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) src/vm.h -o build/vm.o

bindings:
	gcc -fPIC -shared ./src/natives/langstd.c -o build/libmystd.so

debug: bindings src/main.c| build
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) src/main.c -o build/vm

build:
	mkdir -p build

clean:
	rm -rf build
