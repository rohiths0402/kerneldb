#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "parser.h"
#include "lexer/lexer.h"
#include "../common/intent.h"

typedef struct {
    const TokenList *list;
    int pos;
} Parser;

static const Token *current(Parser *p) {
    if (p->pos < p->list->count){
        return &p->list->tokens[p->pos];
    }
    return &p->list->tokens[p->list->count - 1]; 
}

static const Token *advance(Parser *p) {
    const Token *t = current(p);
    if (t->type != TOKEN_EOF){
        p->pos++;
    }
    return t;
}

static int check(Parser *p, TokenType type) {
    return current(p)->type == type;
}

static const Token *expect(Parser *p, TokenType type) {
    if (check(p, type)) return advance(p);
    return NULL;
}

static void parse_where(Parser *p, WhereClause *where) {
    where->active = 0;
    if (!check(p, TOKEN_WHERE)) return;
    advance(p);

    const Token *col = expect(p, TOKEN_IDENT);
    if (!col) return;
    strncpy(where->column, col->value, MAX_NAME_LEN - 1);

    expect(p, TOKEN_EQUALS);

    const Token *val = current(p);
    if (val->type == TOKEN_NUMBER || val->type == TOKEN_STRING ||
        val->type == TOKEN_IDENT) {
        strncpy(where->value, val->value, MAX_VALUE_LEN - 1);
        advance(p);
        where->active = 1;
    }
}

static ParseResult parse_select(Parser *p, Intent *intent) {
    intent->type = INTENT_SELECT;
    if (check(p, TOKEN_STAR)) {
        advance(p);
        intent->select_all = 1;
    } else {
        intent->select_all = 0;
        while (check(p, TOKEN_IDENT)) {
            const Token *col = advance(p);
            if (intent->select_col_count < MAX_COLUMNS) {
                strncpy(intent->select_cols[intent->select_col_count++],
                        col->value, MAX_NAME_LEN - 1);
            }
            if (!check(p, TOKEN_COMMA)) break;
            advance(p);
        }
    }

    if (!expect(p, TOKEN_FROM)) {
        printf("  [parser] Error: expected FROM after SELECT\n");
        return PARSE_ERROR;
    }

    const Token *tbl = expect(p, TOKEN_IDENT);
    if (!tbl) {
        printf("  [parser] Error: expected table name after FROM\n");
        return PARSE_ERROR;
    }
    strncpy(intent->table, tbl->value, MAX_NAME_LEN - 1);

    parse_where(p, &intent->where);
    return PARSE_OK;
}

static ParseResult parse_insert(Parser *p, Intent *intent) {
    intent->type = INTENT_INSERT;

    if (!expect(p, TOKEN_INTO)) {
        printf("  [parser] Error: expected INTO after INSERT\n");
        return PARSE_ERROR;
    }

    const Token *tbl = expect(p, TOKEN_IDENT);
    if (!tbl) {
        printf("  [parser] Error: expected table name\n");
        return PARSE_ERROR;
    }
    strncpy(intent->table, tbl->value, MAX_NAME_LEN - 1);

    if (!expect(p, TOKEN_VALUES)) {
        printf("  [parser] Error: expected VALUES\n");
        return PARSE_ERROR;
    }
    if (!expect(p, TOKEN_LPAREN)) {
        printf("  [parser] Error: expected '('\n");
        return PARSE_ERROR;
    }

    while (!check(p, TOKEN_RPAREN) && !check(p, TOKEN_EOF)) {
        const Token *val = current(p);
        if (val->type == TOKEN_NUMBER || val->type == TOKEN_STRING ||
            val->type == TOKEN_IDENT) {
            if (intent->value_count < MAX_VALUES) {
                strncpy(intent->values[intent->value_count++],
                        val->value, MAX_VALUE_LEN - 1);
            }
            advance(p);
        }
        if (check(p, TOKEN_COMMA)) advance(p);
    }
    expect(p, TOKEN_RPAREN);
    return PARSE_OK;
}

static ParseResult parse_update(Parser *p, Intent *intent) {
    intent->type = INTENT_UPDATE;

    const Token *tbl = expect(p, TOKEN_IDENT);
    if (!tbl) {
        printf("  [parser] Error: expected table name after UPDATE\n");
        return PARSE_ERROR;
    }
    strncpy(intent->table, tbl->value, MAX_NAME_LEN - 1);

    if (!expect(p, TOKEN_SET)) {
        printf("  [parser] Error: expected SET\n");
        return PARSE_ERROR;
    }

    const Token *col = expect(p, TOKEN_IDENT);
    if (!col) {
        printf("  [parser] Error: expected column name after SET\n");
        return PARSE_ERROR;
    }
    strncpy(intent->set.column, col->value, MAX_NAME_LEN - 1);

    expect(p, TOKEN_EQUALS);

    const Token *val = current(p);
    if (val->type == TOKEN_NUMBER || val->type == TOKEN_STRING ||
        val->type == TOKEN_IDENT) {
        strncpy(intent->set.value, val->value, MAX_VALUE_LEN - 1);
        advance(p);
    }

    parse_where(p, &intent->where);
    return PARSE_OK;
}

static ParseResult parse_delete(Parser *p, Intent *intent) {
    intent->type = INTENT_DELETE;

    if (!expect(p, TOKEN_FROM)) {
        printf("  [parser] Error: expected FROM after DELETE\n");
        return PARSE_ERROR;
    }

    const Token *tbl = expect(p, TOKEN_IDENT);
    if (!tbl) {
        printf("  [parser] Error: expected table name after FROM\n");
        return PARSE_ERROR;
    }
    strncpy(intent->table, tbl->value, MAX_NAME_LEN - 1);

    parse_where(p, &intent->where);
    return PARSE_OK;
}

static ParseResult parse_create(Parser *p, Intent *intent) {

    if (check(p, TOKEN_IDENT) &&
        strcasecmp(current(p)->value, "DATABASE") == 0) {

        advance(p);

        intent->type = INTENT_CREATE_DB;

        const Token *db = expect(p, TOKEN_IDENT);
        if (!db) {
            printf("  [parser] Error: expected database name\n");
            return PARSE_ERROR;
        }

        strncpy(intent->db_name, db->value, MAX_NAME_LEN - 1);
        return PARSE_OK;
    }

    // 🔥 OTHERWISE: CREATE TABLE
    intent->type = INTENT_CREATE_TABLE;

    if (!expect(p, TOKEN_TABLE)) {
        printf("  [parser] Error: expected TABLE after CREATE\n");
        return PARSE_ERROR;
    }

    const Token *tbl = expect(p, TOKEN_IDENT);
    if (!tbl) {
        printf("  [parser] Error: expected table name\n");
        return PARSE_ERROR;
    }
    strncpy(intent->table, tbl->value, MAX_NAME_LEN - 1);

    if (!expect(p, TOKEN_LPAREN)) {
        printf("  [parser] Error: expected '('\n");
        return PARSE_ERROR;
    }

    while (!check(p, TOKEN_RPAREN) && !check(p, TOKEN_EOF)) {
        const Token *col_name = expect(p, TOKEN_IDENT);
        const Token *col_type = expect(p, TOKEN_IDENT);

        if (col_name && col_type && intent->column_count < MAX_COLUMNS) {
            ColumnDef *cd = &intent->columns[intent->column_count++];
            strncpy(cd->name, col_name->value, MAX_NAME_LEN - 1);
            strncpy(cd->type, col_type->value, MAX_NAME_LEN - 1);
        }
        if (check(p, TOKEN_COMMA)) advance(p);
    }

    expect(p, TOKEN_RPAREN);
    return PARSE_OK;
}

static ParseResult parse_drop(Parser *p, Intent *intent) {
    intent->type = INTENT_DROP_TABLE;

    if (!expect(p, TOKEN_TABLE)) {
        printf("  [parser] Error: expected TABLE after DROP\n");
        return PARSE_ERROR;
    }

    const Token *tbl = expect(p, TOKEN_IDENT);
    if (!tbl) {
        printf("  [parser] Error: expected table name\n");
        return PARSE_ERROR;
    }
    strncpy(intent->table, tbl->value, MAX_NAME_LEN - 1);
    return PARSE_OK;
}

ParseResult parse(const char *sql, Intent *intent) {
    memset(intent, 0, sizeof(Intent));
    intent->type = INTENT_UNKNOWN;
    TokenList tokens;
    lexer_tokenize(sql, &tokens);
    Parser p = { .list = &tokens, .pos = 0 };
    const Token *first = current(&p);

    if (first->type == TOKEN_BEGIN) {
        advance(&p);
        intent->type = INTENT_BEGIN;
        return PARSE_OK;
    }

    if (first->type == TOKEN_COMMIT) {
        advance(&p);
        intent->type = INTENT_COMMIT;
        return PARSE_OK;
    }

    switch (first->type) {
        case TOKEN_SELECT: {
            advance(&p);
            return parse_select(&p, intent);
        }
        case TOKEN_INSERT: {
            advance(&p);
            return parse_insert(&p, intent);
        }
        case TOKEN_UPDATE: {
            advance(&p);
            return parse_update(&p, intent);
        }
        case TOKEN_DELETE: {
            advance(&p);
            return parse_delete(&p, intent);
        }
        case TOKEN_CREATE:{
            advance(&p);
            return parse_create(&p, intent);
        }
        case TOKEN_DROP:{
            advance(&p);
            return parse_drop(&p, intent);
        }
        case TOKEN_BEGIN: {
            intent->type = INTENT_BEGIN;
            return PARSE_OK;
        }
        case TOKEN_COMMIT: {
            intent->type = INTENT_COMMIT;
            return PARSE_OK;
        }
        case TOKEN_USE: {
            advance(&p);
            intent->type = INTENT_USE_DB;
            const Token *db = expect(&p, TOKEN_IDENT);
            if (!db) {
                printf("  [parser] Error: expected database name after USE\n");
                return PARSE_ERROR;
            }
            strncpy(intent->db_name, db->value, MAX_NAME_LEN - 1);
            return PARSE_OK;
        }
        default:
            printf("  [parser] Unknown statement: \"%s\"\n", first->value);
            return PARSE_ERROR;
    }
    printf("FIRST TOKEN TYPE=%d VALUE=[%s]\n", first->type, first->value);
}

void intent_print(const Intent *intent) {
    static const char *TYPE_NAMES[] = {
        "SELECT", "INSERT", "UPDATE", "DELETE",
        "CREATE_TABLE", "DROP_TABLE", "BEGIN", "COMMIT", "UNKNOWN"
    };

    printf("\n  [parser] Intent:\n");
    printf("    type  : %s\n", TYPE_NAMES[intent->type]);
    printf("    table : %s\n", intent->table);

    if (intent->type == INTENT_INSERT) {
        printf("    values: ");
        for (int i = 0; i < intent->value_count; i++)
            printf("\"%s\" ", intent->values[i]);
        printf("\n");
    }

    if (intent->type == INTENT_UPDATE)
        printf("    set   : %s = %s\n", intent->set.column, intent->set.value);

    if (intent->type == INTENT_CREATE_TABLE) {
        printf("    cols  : ");
        for (int i = 0; i < intent->column_count; i++)
            printf("%s(%s) ", intent->columns[i].name, intent->columns[i].type);
        printf("\n");
    }

    if (intent->where.active)
        printf("    where : %s = %s\n", intent->where.column, intent->where.value);

    printf("\n");
}