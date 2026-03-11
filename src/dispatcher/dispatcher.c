#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "dispatcher.h"
#include "command.h"
#include "parser.h"
#include "intent.h"

static const char *DB_CATALOG[] = { "users.db", "products.db", "orders.db", NULL };

static const char *ltrim(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static int starts_with_ci(const char *str, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*str) != tolower((unsigned char)*prefix)) return 0;
        str++; prefix++;
    }
    return 1;
}

static ExecResult handle_show_db(void) {
    printf("Databases available:\n");
    printf("  %-20s  %s\n", "Name", "Status");
    printf("  %-20s  %s\n", "--------------------", "------");

    for (int i = 0; DB_CATALOG[i] != NULL; i++)
        printf("  %-20s  online\n", DB_CATALOG[i]);

    return EXEC_SUCCESS;
}

static ExecResult handle_help(void) {
    printf(".exit / .quit       Exit\n");
    printf("  .help               This help\n");
    printf("  show db             List databases\n");
    printf("  SELECT * FROM <table> [WHERE col=val]\n");
    printf("  INSERT INTO <table> VALUES (...)\n");
    printf("  UPDATE <table> SET col=val [WHERE col=val]\n");
    printf("  DELETE FROM <table> [WHERE col=val]\n");
    printf("  CREATE TABLE <table> (col type, ...)\n");
    printf("  DROP TABLE <table>\n");

    return EXEC_SUCCESS;
}

static ExecResult handle_sql(Command *cmd) {
    Intent intent;
    ParseResult result = parse(cmd->raw, &intent);

    if (result != PARSE_OK) {
        printf("  [error] Could not parse SQL.\n\n");
        return EXEC_ERROR;
    }

    intent_print(&intent);

    switch (intent.type) {

        case INTENT_SELECT:
            printf("  -> SELECT from \"%s\"", intent.table);
            if (intent.where.active)
                printf(" WHERE %s = %s", intent.where.column, intent.where.value);

            printf("\n  (executor arrives in Layer 3+)\n\n");
            break;

        case INTENT_INSERT:
            printf("  -> INSERT into \"%s\" with %d value(s)\n",
                   intent.table, intent.value_count);

            printf("  (executor arrives in Layer 3+)\n\n");
            break;

        case INTENT_UPDATE:
            printf("  -> UPDATE \"%s\" SET %s = %s",
                   intent.table, intent.set.column, intent.set.value);

            if (intent.where.active)
                printf(" WHERE %s = %s", intent.where.column, intent.where.value);

            printf("\n  (executor arrives in Layer 3+)\n\n");
            break;

        case INTENT_DELETE:
            printf("  -> DELETE from \"%s\"", intent.table);

            if (intent.where.active)
                printf(" WHERE %s = %s", intent.where.column, intent.where.value);

            printf("\n  (executor arrives in Layer 3+)\n\n");
            break;

        case INTENT_CREATE_TABLE:
            printf("  -> CREATE TABLE \"%s\" with %d column(s)\n",
                   intent.table, intent.column_count);

            printf("  (executor arrives in Layer 3+)\n\n");
            break;

        case INTENT_DROP_TABLE:
            printf("  -> DROP TABLE \"%s\"\n", intent.table);
            printf("  (executor arrives in Layer 3+)\n\n");
            break;

        default:
            printf("  [error] Unknown intent.\n\n");
            return EXEC_ERROR;
    }

    return EXEC_SUCCESS;
}

static const char *SQL_KEYWORDS[] = {
    "select","insert","update","delete","create","drop","alter","explain",NULL
};

static void classify(Command *cmd) {
    const char *s = ltrim(cmd->raw);

    if (*s == 0) {
        cmd->type = CMD_EMPTY;
        return;
    }

    if (*s == '.') {
        cmd->type = CMD_META;
        cmd->args = (char *)(s+1);
        return;
    }

    if (starts_with_ci(s, "show")) {
        cmd->type = CMD_META;
        cmd->args = (char *)ltrim(s+4);
        return;
    }

    for (int i = 0; SQL_KEYWORDS[i]; i++) {
        if (starts_with_ci(s, SQL_KEYWORDS[i])) {
            cmd->type = CMD_SQL;
            cmd->args = (char *)s;
            return;
        }
    }

    cmd->type = CMD_UNKNOWN;
}

ExecResult dispatch(Command *cmd) {
    classify(cmd);

    switch (cmd->type) {

        case CMD_EMPTY:
            return EXEC_SUCCESS;

        case CMD_META: {
            const char *arg = ltrim(cmd->args ? cmd->args : "");

            if (strcmp(arg,"exit")==0 || strcmp(arg,"quit")==0)
                return EXEC_EXIT;

            if (strcmp(arg,"help")==0)
                return handle_help();

            if (starts_with_ci(arg,"db"))
                return handle_show_db();

            printf("  Unknown meta-command: .%s\n\n", arg);
            return EXEC_UNRECOGNIZED;
        }

        case CMD_SQL:
            return handle_sql(cmd);

        default:
            printf("  Unrecognized: \"%s\"\n\n", cmd->raw);
            return EXEC_UNRECOGNIZED;
    }
}