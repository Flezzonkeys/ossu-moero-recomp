CC?=gcc
BUILD?=build
GEN:=$(wildcard gen/*.c)
SRC:=src/cpu.c src/mem.c src/io.c src/ppu.c src/gx.c src/spu.c src/sched.c src/main.c
OBJ:=$(patsubst %.c,$(BUILD)/%.o,$(GEN) $(SRC))
LIBS?=-lm
CFLAGS_SRC?=
TARGET?=ossu
$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LIBS)
$(BUILD)/gen/%.o: gen/%.c src/rt.h src/recomp.h
	@mkdir -p $(dir $@)
	$(CC) -O1 -w -fno-strict-aliasing -c $< -o $@
$(BUILD)/src/%.o: src/%.c src/rt.h
	@mkdir -p $(dir $@)
	$(CC) -O2 -g -w -fno-strict-aliasing $(CFLAGS_SRC) -c $< -o $@
