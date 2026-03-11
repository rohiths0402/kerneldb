CC      := gcc
CFLAGS  := -std=c17 -Wall -Wextra
LDFLAGS := -lpthread
SRC_DIR := src
BIN     := kerneldb

SRCS := main.c \
        src/reph/reph.c \
        src/dispatcher/dispatcher.c \
        src/monitor/monitor.c \
        src/Parser/lexer/lexer.c \
        src/Parser/parser.c

INCLUDES := -Isrc/reph \
            -Isrc/dispatcher \
            -Isrc/monitor \
            -Isrc/common \
            -Isrc/Parser \
            -Isrc/Parser/lexer

all: $(BIN)

$(BIN): $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(BIN)