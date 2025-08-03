CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
LDFLAGS =

# directories
BUILD_DIR = build
BIN_DIR = bin

# source files
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

TARGET = $(BIN_DIR)/c_database

TEST_SRCS = $(wildcard tests/*.c)
TEST_OBJS = $(TEST_SRCS:tests/%.c=$(BUILD_DIR)/%.o)
TEST_TARGET = $(BIN_DIR)/test_btree

all: $(TARGET)

# create directories if they don't exist
$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: tests/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(filter-out $(BUILD_DIR)/main.o,$(OBJS)) $(TEST_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) *.db *.log

.PHONY: all clean test
