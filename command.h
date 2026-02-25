#ifndef COMMAND_H
#define COMMAND_H


#define MAX_INPUT_LENGTH 1024

typedef enum {
    CMD_META,
    CMD_SQL,
    CMD_EMPTY,
    CMD_UNKNOWN
} CommandType;

typedef enum {
    EXEC_SUCCESS,
    EXEC_EXIT,
    EXEC_UNRECOGNIZED,
    EXEC_ERROR
} ExecResult;

typedef struct {
    char          raw[MAX_INPUT_LENGTH];
    CommandType   type;
    char         *args;
} Command;

#endif