SRCS = src/main.c \
	src/backend/linux/sim_mem.c \
	src/sim_main.c

86em: $(SRCS)
	gcc -o $@ $^

testprog.bin: testprog.S
	nasm -O0 $^ -f bin -o $@
