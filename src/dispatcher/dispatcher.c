#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include "dispatcher.h"
#include "command.h"
#include "parser.h"
#include "intent.h"
#include "../storage/Table/table.h"
#include "../WAL/wal.h"

char current_db[64] = "default";
extern WAL *g_wal;

static int txn_active = 0;
static uint32_t current_txn = 0;

static const char *ltrim(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static int starts_with_ci(const char *str, const char *prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*str) != tolower((unsigned char)*prefix)){
            return 0;
        }
        str++; prefix++;
    }
    return 1;
}

static ExecResult handle_show_db(void) {
    printf("\n  Databases available:\n");
    printf("  %-20s  %s\n", "Name", "Status");
    printf("  %-20s  %s\n", "--------------------", "------");

    DIR *dir = opendir("data");
    if (!dir) {
        printf("  (no databases yet)\n\n");
        return EXEC_SUCCESS;
    }

    struct dirent *entry;
    int found = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
                continue;
            }
            printf("  %-20s  online\n", entry->d_name);
            found++;
        }
    }

    closedir(dir);

    if (!found)
        printf("  (no databases yet)\n");

    printf("\n");
    return EXEC_SUCCESS;
}

static ExecResult handle_help(void) {
    printf("\n  .exit/.quit  Exit\n");
    printf("  .help        This help\n");
    printf("  show db      List databases\n");
    printf("  SELECT * FROM <table> [WHERE col=val]\n");
    printf("  INSERT INTO <table> VALUES (...)\n");
    printf("  UPDATE <table> SET col=val [WHERE col=val]\n");
    printf("  DELETE FROM <table> [WHERE col=val]\n");
    printf("  CREATE TABLE <table> (col type, ...)\n");
    printf("  CREATE DATABASE <name>\n");
    printf("  USE <name>\n");
    printf("  DROP TABLE <table>\n\n");
    return EXEC_SUCCESS;
}

static ExecResult handle_sql(Command *cmd) {
    Intent intent;
    ParseResult result = parse(cmd->raw, &intent);
    if (result != PARSE_OK) {
        printf("  [error] Could not parse SQL.\n\n");
        return EXEC_ERROR;
    }

    switch (intent.type) {
        case INTENT_CREATE_DB: {
            char path[128];
            snprintf(path, sizeof(path), "data/%s", intent.db_name);
            struct stat st;
            if (stat(path, &st) == 0) {
                printf("  [error] Database already exists: %s\n\n", intent.db_name);
                return EXEC_SUCCESS;
            }
            if (mkdir(path, 0755) == 0) {
                printf("  [db] created database %s\n\n", intent.db_name);
            } else {
                perror("mkdir");
            }

            return EXEC_SUCCESS;
        }
        case INTENT_USE_DB: {
            char path[128];
            snprintf(path, sizeof(path), "data/%s", intent.db_name);
            struct stat st;
            if (stat(path, &st) != 0) {
                printf("  [error] Database not found: %s\n\n", intent.db_name);
                return EXEC_SUCCESS;
            }
            strncpy(current_db, intent.db_name, sizeof(current_db) - 1);
            current_db[sizeof(current_db) - 1] = '\0';
            FILE *f = fopen("data/.current_db", "w");
            if (f) {
                fprintf(f, "%s", current_db);
                fclose(f);
            }
            printf("  [db] switched to %s\n\n", current_db);
            return EXEC_SUCCESS;
        }

        case INTENT_CREATE_TABLE: {
            TableResult tr = table_create(&intent);
            if (tr == TABLE_ALREADY_EXISTS)
                printf("  [error] Table already exists: %s\n\n", intent.table);
            else if (tr != TABLE_OK)
                printf("  [error] Could not create table.\n\n");

            return EXEC_SUCCESS;
        }
        case INTENT_DROP_TABLE: {
            TableResult tr = table_drop(intent.table);
            if (tr == TABLE_NOT_FOUND)
                printf("  [error] Table not found: %s\n\n", intent.table);
            return EXEC_SUCCESS;
        }
        case INTENT_INSERT: {
            uint32_t txn = current_txn;
            int auto_commit = 0;
            if (!txn_active) {
                wal_begin(g_wal, &txn);
                auto_commit = 1;
            }
            char buffer[WAL_MAX_DATA];
            int written = snprintf(buffer, sizeof(buffer), "%s|%s", intent.values[0], intent.values[1]);
            if (written < 0 || written >= sizeof(buffer)) {
                printf("  [error] Data too long for WAL.\n\n");
                return EXEC_ERROR;
            }
            wal_write(g_wal, txn, WAL_INSERT, intent.table, (uint8_t *)buffer, written);
            Table *table = table_open(intent.table);
            if (!table) {
                printf("  [error] Table not found: %s\n\n", intent.table);
                return EXEC_ERROR;
            }
            table_insert_raw(table, (uint8_t *)buffer, written);
            table_close(table);
            if (auto_commit) {
                wal_commit(g_wal, txn);
            }

            return EXEC_SUCCESS;
        }
        case INTENT_SELECT: {
            Table *table = table_open(intent.table);
            if (!table) {
                printf("  [error] Table not found: %s\n\n", intent.table);
                return EXEC_ERROR;
            }
            table_scan(table, &intent);
            table_close(table);
            return EXEC_SUCCESS;
        }

        case INTENT_UPDATE: {
            Table *table = table_open(intent.table);
            if (!table) {
                printf("  [error] Table not found: %s\n\n", intent.table);
                return EXEC_ERROR;
            }
            table_update(table, &intent);
            table_close(table);
            return EXEC_SUCCESS;
        }

        case INTENT_DELETE: {
            Table *table = table_open(intent.table);
            if (!table) {
                printf("  [error] Table not found: %s\n\n", intent.table);
                return EXEC_ERROR;
            }
            table_delete(table, &intent);
            table_close(table);
            return EXEC_SUCCESS;
        }

        case INTENT_BEGIN: {
            wal_begin(g_wal, &current_txn);
            txn_active = 1;
            printf("  [txn] BEGIN %d\n\n", current_txn);
            return EXEC_SUCCESS;
        }
        
        case INTENT_COMMIT: {
            wal_commit(g_wal, current_txn);
            txn_active = 0;
            printf("  [txn] COMMIT %d\n\n", current_txn);
            return EXEC_SUCCESS;
        }
        default:
            printf("  [error] Unknown intent.\n\n");
            return EXEC_ERROR;
    }


}

static const char *SQL_KEYWORDS[] = {
"select","insert","update","delete","create","drop","alter","use", "begin", "commit", NULL
};

static void classify(Command *cmd) {
    const char *s = ltrim(cmd->raw);


    if (*s == 0) {
        cmd->type = CMD_EMPTY;
        return;
    }

    if (*s == '.') {
        cmd->type = CMD_META;
        cmd->args = (char *)(s + 1);
        return;
    }

    if (starts_with_ci(s, "show")) {
        cmd->type = CMD_META;
        cmd->args = (char *)ltrim(s + 4);
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

            if (strcmp(arg, "exit") == 0 || strcmp(arg, "quit") == 0)
                return EXEC_EXIT;

            if (strcmp(arg, "help") == 0)
                return handle_help();

            if (starts_with_ci(arg, "db"))
                return handle_show_db();

            printf("  Unknown meta-command: .%s\n\n", arg);
            return EXEC_UNRECOGNIZED;
        }

        case CMD_SQL:
            return handle_sql(cmd);

        default:
            printf("  Unrecognized: [%s]\n\n", cmd->raw);
            return EXEC_UNRECOGNIZED;
    }
}
