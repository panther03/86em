PROG ?= add
SRCS = src/main.c \
	src/backend/linux/vm_mem.c \
	src/backend/linux/vm_io.c \
	src/vm.c \
	src/dbg.c

vm: 86EM TESTPROG

86EM: build/86em
build/86em: $(SRCS)
	@mkdir -p build/
	gcc -I src/ -g -o $@ $^ -lreadline

TESTPROG: build/testprog.bin
build/testprog.bin: tests/$(PROG).asm
	@mkdir -p build/
	nasm -O0 $^ -f bin -o $@
	