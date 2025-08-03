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
TEST_BTREE_TARGET = $(BIN_DIR)/test_btree
TEST_INDEXES_TARGET = $(BIN_DIR)/test_indexes
TEST_INTEGRATION_TARGET = $(BIN_DIR)/test_integration

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

test: test-btree test-indexes test-integration

test-btree: $(TEST_BTREE_TARGET)
	@echo "Running B-tree tests..."
	./$(TEST_BTREE_TARGET)

test-indexes: $(TEST_INDEXES_TARGET)
	@echo "Running index tests..."
	./$(TEST_INDEXES_TARGET)

test-integration: $(TEST_INTEGRATION_TARGET)
	@echo "Running integration tests..."
	./$(TEST_INTEGRATION_TARGET)

$(TEST_BTREE_TARGET): $(filter-out $(BUILD_DIR)/main.o,$(OBJS)) $(BUILD_DIR)/test_btree.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST_INDEXES_TARGET): $(filter-out $(BUILD_DIR)/main.o,$(OBJS)) $(BUILD_DIR)/test_indexes.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST_INTEGRATION_TARGET): $(filter-out $(BUILD_DIR)/main.o,$(OBJS)) $(BUILD_DIR)/test_integration.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) *.db *.log

.PHONY: all clean test test-btree test-indexes test-integration
