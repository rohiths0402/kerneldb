CC      := gcc
CFLAGS  := -std=c17 -Wall -Wextra
LDFLAGS := -lpthread
SRC_DIR := src
BIN     := kerneldb

SRCS := main.c \
        $(SRC_DIR)/reph/reph.c \
        $(SRC_DIR)/dispatcher/dispatcher.c \
        $(SRC_DIR)/monitor/monitor.c

INCLUDES := -I$(SRC_DIR)/reph \
            -I$(SRC_DIR)/dispatcher \
            -I$(SRC_DIR)/monitor \
            -I$(SRC_DIR)/common

all: $(BIN)

$(BIN): $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(BIN)
