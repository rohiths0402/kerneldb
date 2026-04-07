#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

static const struct{
    const char *word;
    TokenType type;
} KEYWORDS[] ={
   { "SELECT", TOKEN_SELECT },
   { "INSERT", TOKEN_INSERT },
   { "UPDATE", TOKEN_UPDATE },
   { "DELETE", TOKEN_DELETE },
   { "CREATE", TOKEN_CREATE },
   { "DROP", TOKEN_DROP   },
   { "TABLE", TOKEN_TABLE  },
   { "INTO", TOKEN_INTO   },
   { "FROM", TOKEN_FROM   },
   { "WHERE", TOKEN_WHERE  },
   { "SET", TOKEN_SET    },
   { "VALUES", TOKEN_VALUES },
   { "AND", TOKEN_AND    },
   { "USE", TOKEN_USE    },
   { "BEGIN", TOKEN_BEGIN  },
   { "COMMIT", TOKEN_COMMIT },
   { NULL, TOKEN_UNKNOWN }
};

static TokenType classify_word(const char *word){
    char upper[MAX_TOKEN_LEN];
    size_t i;
    for(i = 0; word[i] && i < MAX_TOKEN_LEN - 1; i++)
        upper[i] =(char)toupper((unsigned char)word[i]);
    upper[i] = '\0';

    for(int k = 0; KEYWORDS[k].word != NULL; k++){
        if(strcmp(upper, KEYWORDS[k].word) == 0)
            return KEYWORDS[k].type;
    }
    return TOKEN_IDENT;
}

static void push(TokenList *list, TokenType type, const char *value){
    if(list->count >= MAX_TOKENS) return;
    Token *t = &list->tokens[list->count++];
    t->type = type;
    strncpy(t->value, value ? value : "", MAX_TOKEN_LEN - 1);
    t->value[MAX_TOKEN_LEN - 1] = '\0';
}

void lexer_tokenize(const char *input, TokenList *out){
    out->count = 0;
    const char *p = input;

    while(*p){
        if(isspace((unsigned char)*p)){ p++; continue; }
        if(*p == ';'){ 
            push(out, TOKEN_SEMICOLON, ";"); p++; continue; 
        }
        if(*p == '('){
            push(out, TOKEN_LPAREN,  "("); p++; continue; 
        }
        if(*p == ')'){ 
            push(out, TOKEN_RPAREN,  ")"); p++; continue; 
        }
        if(*p == ','){ 
            push(out, TOKEN_COMMA,   ","); p++; continue; 
        }
        if(*p == '*'){ 
            push(out, TOKEN_STAR,    "*"); p++; continue; 
        }
        if(*p == '='){ 
            push(out, TOKEN_EQUALS,  "="); p++; continue; 
        }
        if(*p == '\''){
            p++;
            char buf[MAX_TOKEN_LEN];
            int  len = 0;
            while(*p && *p != '\'' && len < MAX_TOKEN_LEN - 1)
                buf[len++] = *p++;
            buf[len] = '\0';
            if(*p == '\'') p++;
            push(out, TOKEN_STRING, buf);
            continue;
        }
        if(isdigit((unsigned char)*p)){
            char buf[MAX_TOKEN_LEN];
            int  len = 0;
            while(isdigit((unsigned char)*p) && len < MAX_TOKEN_LEN - 1){
                buf[len++] = *p++;
            }
            buf[len] = '\0';
            push(out, TOKEN_NUMBER, buf);
            continue;
        }
        if(isalpha((unsigned char)*p) || *p == '_'){
            char buf[MAX_TOKEN_LEN];
            int  len = 0;
            while((isalnum((unsigned char)*p) || *p == '_') && len < MAX_TOKEN_LEN - 1){
                buf[len++] = *p++;
            }
            buf[len] = '\0';
            push(out, classify_word(buf), buf);
            continue;
        }
        p++;
    }

    push(out, TOKEN_EOF, "");
}

void lexer_print(const TokenList *list){
    static const char *TYPE_NAMES[] ={
        "IDENT", "STRING", "NUMBER",
        "SELECT", "INSERT", "UPDATE", "DELETE",
        "CREATE", "DROP", "TABLE", "INTO", "FROM",
        "WHERE", "SET", "VALUES", "AND",
        "LPAREN", "RPAREN", "COMMA", "STAR", "EQUALS",
        "SEMICOLON", "EOF", "UNKNOWN"
    };
    printf("\n  [lexer] %d token(s):\n", list->count);
    for(int i = 0; i < list->count; i++){
        const Token *t = &list->tokens[i];
        printf("    [%2d]  %-12s  \"%s\"\n", i, TYPE_NAMES[t->type], t->value);
    }
    printf("\n");
}