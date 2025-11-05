CC = gcc
CFLAGS = -Wall -Wextra

SRC_DIR = src
BIN_DIR = bin
COMMON_DIR = $(SRC_DIR)/common

SERVER_SRC = $(SRC_DIR)/server/server.c $(COMMON_DIR)/net.c $(COMMON_DIR)/game.c
CLIENT_SRC = $(SRC_DIR)/client/client.c

all: $(BIN_DIR)/server $(BIN_DIR)/client

$(BIN_DIR)/server: $(SERVER_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/client: $(CLIENT_SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
