CC      = gcc
CFLAGS  = -std=c17 -Wall -Wextra -g -O2
LDFLAGS = -lm

SRC_DIR = src
BIN_DIR = bin

# Existing CSP solver (unchanged)
CSP_SRC     = $(SRC_DIR)/main.c

TARGET      = $(BIN_DIR)/main

all: $(BIN_DIR)
	$(CC) $(CFLAGS) $(CSP_SRC) -o $(TARGET) -lm


run: $(TARGET)
	./$(TARGET)

$(BIN_DIR)/%: test/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -pthread

$(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf  $(BIN_DIR)

.PHONY: all clean run 
