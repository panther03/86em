SRCS = src/main.c \
	src/backend/linux/sim_mem.c \
	src/sim_main.c

sim: 86EM TESTPROG

86EM: build/86em
build/86em: build $(SRCS)
	gcc -g -o $@ $^

TESTPROG: build/testprog.bin
build/testprog.bin: build tests/$(PROG).asm
	nasm -O0 $^ -f bin -o $@

build:
	@mkdir -p build/