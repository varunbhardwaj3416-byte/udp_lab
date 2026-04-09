CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2 -Iinclude
LDFLAGS = -lssl -lcrypto

SRC_DIR = src
OBJ_DIR = build

COMMON_SRCS = $(SRC_DIR)/packet.c $(SRC_DIR)/crypto.c \
              $(SRC_DIR)/arq.c    $(SRC_DIR)/socket.c

SENDER_SRCS   = $(COMMON_SRCS) $(SRC_DIR)/sender.c
RECEIVER_SRCS = $(COMMON_SRCS) $(SRC_DIR)/receiver.c

.PHONY: all clean

all: sender receiver

sender: $(SENDER_SRCS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/sender $(SENDER_SRCS) $(LDFLAGS)

receiver: $(RECEIVER_SRCS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/receiver $(RECEIVER_SRCS) $(LDFLAGS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR)
