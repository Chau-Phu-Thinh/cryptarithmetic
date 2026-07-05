CC      = gcc
CFLAGS  = -std=c17 -Wall -Wextra -g -O2
LDFLAGS = -lm

SRC_DIR = src
BIN_DIR = bin

# Existing CSP solver (unchanged)
CSP_SRC     = $(SRC_DIR)/main.c

# GA solver sources
GA_SRC      = $(SRC_DIR)/ga_solver.c $(SRC_DIR)/ga_main.c

TEST_SRC = $(wildcard test/*.c)
TEST_TARGETS = $(patsubst test/%.c,$(BIN_DIR)/%,$(TEST_SRC))

TARGET      = $(BIN_DIR)/main
GA_TARGET   = $(BIN_DIR)/ga

all: $(BIN_DIR)
	$(CC) $(CFLAGS) $(CSP_SRC) -o $(TARGET) -lm

ga: $(BIN_DIR)
	$(CC) $(CFLAGS) $(GA_SRC) -o $(GA_TARGET) -lm -pthread

run: $(TARGET)
	./$(TARGET)

run-ga: $(GA_TARGET)
	./$(GA_TARGET)

test: $(TEST_TARGETS)
	@for test_bin in $(TEST_TARGETS); do \
		echo "Running $$test_bin"; \
		./$$test_bin; \
	done

$(BIN_DIR)/%: test/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -pthread

$(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf  $(BIN_DIR)

.PHONY: all ga test clean run run-ga
