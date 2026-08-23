# ============================================================
# RichMan A3_A17_A21 - gcc / MinGW 构建脚本
#   make        编译生成 bin/A3_A17_A21(.exe)
#   make run    编译并运行
#   make demo   编译并运行破产机制演示
#   make clean  清理 obj/ 与 bin/
# ============================================================

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -O2 -Iinclude
LDFLAGS ?=

TARGET  := bin/A3_A17_A21
ifeq ($(OS),Windows_NT)
TARGET  := bin/A3_A17_A21.exe
endif

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,obj/%.o,$(SRCS))

.PHONY: all clean run demo

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

demo: all
	./$(TARGET) --demo
