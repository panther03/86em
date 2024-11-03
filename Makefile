SRCS = src/main.c \
	src/backend/linux/vm_mem.c \
	src/vm.c \
	src/dbg.c

vm: 86EM TESTPROG

86EM: build/86em
build/86em: $(SRCS)
	gcc -I src/ -g -o $@ $^ -lreadline

TESTPROG: build/testprog.bin
build/testprog.bin: build tests/$(PROG).asm
	nasm -O0 $^ -f bin -o $@

build:
	@mkdir -p build/