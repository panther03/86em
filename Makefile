PROG ?= add
SRCS = src/main.c \
	src/backend/linux/vm_mem.c \
	src/backend/linux/vm_io.c \
	src/backend/linux/cga.c \
	src/backend/linux/i8253.c \
	src/backend/linux/i8259.c \
	src/vm.c \
	src/dbg.c \
	src/util.c

vm: 86EM TESTPROG

86EM: build/86em
build/86em: $(SRCS)
	@mkdir -p build/
	gcc -Wall -Wextra -Isrc/ $(shell sdl2-config --cflags) -g -o $@ $^ -lreadline $(shell sdl2-config --libs) 

TESTPROG: build/testprog.bin
build/testprog.bin: tests/$(PROG).asm
	@mkdir -p build/
	nasm -O0 $^ -f bin -o $@
	