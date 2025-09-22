
CC = gcc
CFLAGS = -g -std=gnu11 -Werror -Wall -Wextra -Wpedantic -Wmissing-declarations -Wmissing-prototypes -Wold-style-definition -Iinclude

SRC_DIR = src
OBJ_DIR = build
INC_DIR = include

TARGET = mmake

SRC = $(SRC_DIR)/mmake.c $(SRC_DIR)/parser.c
OBJ = $(OBJ_DIR)/mmake.o $(OBJ_DIR)/parser.o

all: $(OBJ_DIR) $(TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

$(OBJ_DIR)/mmake.o: $(SRC_DIR)/mmake.c $(INC_DIR)/parser.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/parser.o: $(SRC_DIR)/parser.c $(INC_DIR)/parser.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

