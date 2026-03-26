CC      := gcc
CFLAGS  := -std=c17 -Wall -Wextra
LDFLAGS := -lpthread -luring    
SRC_DIR := src
BIN     := kerneldb

SRCS := main.c \
        src/reph/reph.c \
        src/dispatcher/dispatcher.c \
        src/monitor/monitor.c \
        src/Parser/lexer/lexer.c \
        src/Parser/parser.c \
        src/storage/page/page.c \
        src/storage/buffer/buffer.c \
        src/storage/Table/table.c \
        src/index/index.c \
        src/wal/wal.c \
        src/concurrency/rwlock.c \
        src/io/aio.c \


INCLUDES := -Isrc/reph \
            -Isrc/dispatcher \
            -Isrc/monitor \
            -Isrc/common \
            -Isrc/Parser \
            -Isrc/Parser/lexer \
            -Isrc/storage \
            -Isrc/storage/page \
            -Isrc/storage/buffer \
            -Isrc/storage/Table \
            -Isrc/index \
            -Isrc/wal \
            -Isrc/concurrency \
            -Isrc/io \

all: $(BIN)

$(BIN): $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(BIN)