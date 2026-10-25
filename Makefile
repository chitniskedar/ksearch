CC := gcc
CFLAGS ?= -std=gnu11 -Wall -Wextra -Werror -O2 -Iinclude
LDFLAGS ?=

ifeq ($(OS),Windows_NT)
BIN := ksearch.exe
TEST_BIN := tests.exe
MKDIR_BUILD = powershell -NoProfile -Command "New-Item -ItemType Directory -Force build | Out-Null"
CLEAN_FILES = powershell -NoProfile -Command "Remove-Item -Path build,ksearch.exe,tests.exe -Recurse -Force -ErrorAction SilentlyContinue; exit 0"
LDFLAGS += -lshell32
else
BIN := ksearch
TEST_BIN := ksearch-tests
MKDIR_BUILD = mkdir -p build
CLEAN_FILES = rm -rf build $(BIN) $(TEST_BIN)
endif

SRC := $(wildcard src/*.c)
CORE_SRC := $(filter-out src/main.c src/cli.c,$(SRC))
OBJ := $(SRC:src/%.c=build/%.o)
TEST_OBJ := $(CORE_SRC:src/%.c=build/test_%.o)

.PHONY: all clean debug test

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/test_%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/tests.o: tests/tests.c | build
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BIN): $(TEST_OBJ) build/tests.o
	$(CC) $(TEST_OBJ) build/tests.o $(LDFLAGS) -o $@

build:
	$(MKDIR_BUILD)

debug: CFLAGS += -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
debug: LDFLAGS += -fsanitize=address,undefined
debug: clean all test

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	$(CLEAN_FILES)
