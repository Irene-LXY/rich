CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -I third_party/cjson -I src
TARGET  = rich_test.exe
SRC     = src/test_runner.c src/game_engine_stub.c third_party/cjson/cJSON.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
