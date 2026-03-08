#ifndef PARSER_H
#define PARSER_H

#include ".lexer.h"
#include "../common/intent.h"

typedef enum {
    PARSE_OK,
    PARSE_ERROR
} ParseResult;

ParseResult parse(const char *sql, Intent *intent);

void intent_print(const Intent *intent);

#endif