#ifndef LEXER_H
#define LEXER_H

#define MAX_TOKEN_LEN   256
#define MAX_TOKENS      128

typedef enum {
    TOKEN_IDENT,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_SELECT,
    TOKEN_INSERT,
    TOKEN_UPDATE,
    TOKEN_DELETE,
    TOKEN_CREATE,
    TOKEN_DROP,
    TOKEN_TABLE,
    TOKEN_INTO,
    TOKEN_FROM,
    TOKEN_WHERE,
    TOKEN_SET,
    TOKEN_VALUES,
    TOKEN_AND,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_COMMA,
    TOKEN_STAR,
    TOKEN_EQUALS,
    TOKEN_SEMICOLON,
    TOKEN_EOF,
    TOKEN_USE,
    TOKEN_BEGIN,
    TOKEN_COMMIT,
    TOKEN_UNKNOWN
    
} TokenType;

typedef struct {
    TokenType type;
    char      value[MAX_TOKEN_LEN];
} Token;

typedef struct {
    Token tokens[MAX_TOKENS];
    int   count;
} TokenList;

void lexer_tokenize(const char *input, TokenList *out);

void lexer_print(const TokenList *list);

#endif