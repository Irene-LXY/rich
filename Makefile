# ============================================================
# RichMan 大富翁 - gcc / MinGW 构建脚本
#   make        编译生成 bin/richman(.exe)
#   make run    编译并运行
#   make clean  清理 obj/ 与 bin/
# ============================================================

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -O2 -Iinclude
LDFLAGS ?=

TARGET  := bin/richman
ifeq ($(OS),Windows_NT)
TARGET  := bin/richman.exe
endif

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,obj/%.o,$(SRCS))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS) | bin
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

bin obj:
	mkdir -p $@

clean:
	rm -rf obj bin

run: all
	./$(TARGET)
