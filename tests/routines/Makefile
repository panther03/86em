PROG = add

TESTROUTINE: ../../build/testprog.bin
build/testprog.bin: tests/$(PROG).asm
	@mkdir -p ../../build/
	nasm -O0 $^ -f bin -o $@