#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "dispatcher.h"
#include "command.h"


static const char *DB_CATALOG[] = {
    "users.db",
    "products.db",
    "orders.db",
    NULL
};

static const char *ltrim(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/* Case-insensitive prefix check */
static int starts_with_ci(const char *str, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*str) != tolower((unsigned char)*prefix))
            return 0;
        str++; prefix++;
    }
    return 1;
}

static ExecResult handle_show_db(void) {
    printf("\n  Databases available:\n");
    printf("  %-20s  %s\n", "Name", "Status");
    printf("  %-20s  %s\n", "--------------------", "------");
    for (int i = 0; DB_CATALOG[i] != NULL; i++) {
        printf("  %-20s  online\n", DB_CATALOG[i]);
    }
    printf("\n");
    return EXEC_SUCCESS;
}

static ExecResult handle_help(void) {
    printf("\n  Meta-commands:\n");
    printf("    .exit / .quit    Exit the REPL\n");
    printf("    .help            Show this help\n");
    printf("    show db          List all databases\n");
    printf("\n  SQL (stub — parser coming in Layer 2):\n");
    printf("    SELECT ...       Query data\n");
    printf("    INSERT ...       Insert a row\n");
    printf("    CREATE TABLE ... Define a table\n\n");
    return EXEC_SUCCESS;
}

static ExecResult handle_sql(Command *cmd) {
    printf("  [SQL stub] Received query: \"%s\"\n", cmd->raw);
    printf("  → Would route to: lexer → parser → optimizer → executor\n");
    printf("  (Layer 2+ will make this real)\n\n");
    return EXEC_SUCCESS;
}

static void classify(Command *cmd) {
    const char *s = ltrim(cmd->raw);

    if (*s == '\0') {
        cmd->type = CMD_EMPTY;
        return;
    }

    /* Meta: starts with '.' */
    if (*s == '.') {
        cmd->type = CMD_META;
        cmd->args = (char *)(s + 1);
        return;
    }

    /* Meta: "show db" keyword */
    if (starts_with_ci(s, "show")) {
        cmd->type = CMD_META;
        cmd->args = (char *)ltrim(s + 4);
        return;
    }

    /* SQL keywords */
    const char *sql_keywords[] = {
        "select", "insert", "update", "delete",
        "create", "drop", "alter", "explain", NULL
    };
    for (int i = 0; sql_keywords[i]; i++) {
        if (starts_with_ci(s, sql_keywords[i])) {
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

            if (strcmp(arg, "exit") == 0 || strcmp(arg, "quit") == 0)
                return EXEC_EXIT;

            if (strcmp(arg, "help") == 0)
                return handle_help();

            if (starts_with_ci(arg, "db"))
                return handle_show_db();

            printf("  Unknown meta-command: .%s\n  Type .help for options.\n\n", arg);
            return EXEC_UNRECOGNIZED;
        }

        case CMD_SQL:
            return handle_sql(cmd);

        default:
            printf("  Unrecognized input: \"%s\"\n  Type .help for options.\n\n", cmd->raw);
            return EXEC_UNRECOGNIZED;
    }
}