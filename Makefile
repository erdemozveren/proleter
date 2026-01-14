.PHONY: clean debug
CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
DEBUG_FLAGS := -g -O0 -Werror -Wswitch-enum -Wmissing-prototypes -Wstrict-prototypes \
               -fsanitize=address,undefined
RELEASE_FLAGS := -O2

vm: src/vm.c | build
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) src/vm.c -o build/vm

debug: src/vm.c | build
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) src/vm.c -o build/vm

build:
	mkdir -p build

clean:
	rm -rf build
