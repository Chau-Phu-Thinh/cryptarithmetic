CC      = gcc
CFLAGS  = -std=c17 -Wall -Wextra -g -O2
LDFLAGS = -lm

SRC_DIR = src
BIN_DIR = bin

# OS Detection
ifeq ($(OS),Windows_NT)
    EXE     := .exe
    RM_DIR  := rmdir /s /q $(BIN_DIR)
    MKDIR   := mkdir $(BIN_DIR)
    RUN_CMD := $(BIN_DIR)\main$(EXE)
else
    EXE     :=
    RM_DIR  := rm -rf $(BIN_DIR)
    MKDIR   := mkdir -p $(BIN_DIR)
    RUN_CMD := ./$(BIN_DIR)/main$(EXE)
endif

# Existing CSP solver
CSP_SRC     = $(SRC_DIR)/main.c
TARGET      = $(BIN_DIR)/main$(EXE)

all: $(BIN_DIR)
	$(CC) $(CFLAGS) $(CSP_SRC) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	$(RUN_CMD) $(ARGS)

run-brute-force: $(TARGET)
	$(RUN_CMD) --brute-force

run-long-mul: $(TARGET)
	$(RUN_CMD) --long-mul

run-column-carry: $(TARGET)
	$(RUN_CMD) --column-carry

$(BIN_DIR)/%$(EXE): test/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -pthread

$(BIN_DIR):
	-$(MKDIR)

clean:
	-$(RM_DIR)

.PHONY: all clean run run-brute-force run-long-mul run-column-carry
