BACKEND ?= LINUX
TESTROUTINE ?= 

BUILD = build/
SRCS = src/vm.c \
	   src/dbg.c \
	   src/util.c
OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

CFLAGS = -Wall -Wextra -Iinclude/

SRCS_LINUX = $(wildcard ./src/backend/linux/*.c)
OBJS_LINUX = $(patsubst ./src/%.c,$(BUILD)/%.o,$(SRCS_LINUX))
CFLAGS_LINUX = -Iinclude/backend/linux $(shell sdl2-config --cflags) -g 
LDFLAGS_LINUX = -lreadline $(shell sdl2-config --libs) 

all: 86EM

86EM: $(BUILD)/86em
$(BUILD)/86em: $(OBJS) $(OBJS_$(BACKEND)) $(BUILD)/main.o
	@mkdir -p $(BUILD)/
	$(CC) -o $@ $^ $(LDFLAGS) $(LDFLAGS_$(BACKEND))

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CFLAGS_$(BACKEND)) -c -o $@ $<

