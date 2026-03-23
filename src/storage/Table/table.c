#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "table.h"
#include "page.h"
#include "buffer.h"
#include "index.h"
#include "../WAL/wal.h"
#include "../common/intent.h"

#define CATALOG_FILE  "data/catalog.db"

extern WAL *g_wal;
static void data_path(const char *name, char *out, size_t size) {
    snprintf(out, size, "data/%s.db", name);
}

static void schema_path(const char *name, char *out, size_t size) {
    snprintf(out, size, "data/%s.schema", name);
}

static void catalog_add(const char *name) {
    FILE *f = fopen(CATALOG_FILE, "a");
    if (!f) return;
    fprintf(f, "%s\n", name);
    fclose(f);
}

static void catalog_remove(const char *name) {
    FILE *f = fopen(CATALOG_FILE, "r");
    if (!f) return;
    FILE *tmp = fopen("data/catalog.tmp", "w");
    if (!tmp) { fclose(f); return; }
    char line[MAX_TABLE_NAME];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, name) != 0)
            fprintf(tmp, "%s\n", line);
    }
    fclose(f);
    fclose(tmp);
    rename("data/catalog.tmp", CATALOG_FILE);
}

static int save_schema(const char *name, const ColumnDef *cols, int count) {
    char path[256];
    schema_path(name, path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%d\n", count);
    for (int i = 0; i < count; i++)
        fprintf(f, "%s %s\n", cols[i].name, cols[i].type);
    fclose(f);
    return 0;
}

static int load_schema(const char *name, ColumnDef *cols, int *count) {
    char path[256];
    schema_path(name, path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    fscanf(f, "%d\n", count);
    for (int i = 0; i < *count; i++)
        fscanf(f, "%63s %63s\n", cols[i].name, cols[i].type);
    fclose(f);
    return 0;
}

static uint16_t serialise_row(const Intent *intent, uint8_t *buf, uint16_t bufsize) {
    int pos = 0;
    for (int i = 0; i < intent->value_count; i++) {
        int w = snprintf((char *)buf + pos, bufsize - pos, "%s", intent->values[i]);
        pos += w;
        if (i < intent->value_count - 1 && pos < bufsize - 1)
            buf[pos++] = '|';
    }
    buf[pos] = '\0';
    return (uint16_t)(pos + 1);
}

static int row_matches_where(const uint8_t *row_data, const Intent *intent, const Table *table) {
    if (!intent->where.active) return 1;
    int col_idx = -1;
    for (int i = 0; i < table->col_count; i++) {
        if (strcmp(table->col_defs[i].name, intent->where.column) == 0) {
            col_idx = i; break;
        }
    }
    if (col_idx < 0) return 0;
    char tmp[MAX_VALUE_LEN * MAX_COLUMNS];
    strncpy(tmp, (const char *)row_data, sizeof(tmp) - 1);
    char *tok = strtok(tmp, "|");
    for (int i = 0; i < col_idx && tok; i++)
        tok = strtok(NULL, "|");
    if (!tok) return 0;
    return strcmp(tok, intent->where.value) == 0;
}

static void get_col_value(const uint8_t *row_data, int col_idx, char *out, size_t out_size) {
    char tmp[MAX_VALUE_LEN * MAX_COLUMNS];
    strncpy(tmp, (const char *)row_data, sizeof(tmp) - 1);
    char *tok = strtok(tmp, "|");
    for (int i = 0; i < col_idx && tok; i++)
        tok = strtok(NULL, "|");

    if (tok)
        strncpy(out, tok, out_size - 1);
    else
        out[0] = '\0';
}

static int find_index_col(const Table *table) {
    for (int i = 0; i < table->col_count; i++) {
        if (strcmp(table->col_defs[i].type, "INT") == 0)
            return i;
    }
    return 0;
}

void table_subsystem_init(void) {
    struct stat st;
    if (stat("data", &st) != 0)
        mkdir("data", 0755);
}

int table_exists(const char *name) {
    char path[256];
    data_path(name, path, sizeof(path));
    return access(path, F_OK) == 0;
}

TableResult table_create(const Intent *intent) {
    if (table_exists(intent->table))
        return TABLE_ALREADY_EXISTS;
    char path[256];
    data_path(intent->table, path, sizeof(path));
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) { perror("table_create"); return TABLE_ERROR; }
    Page *page = page_alloc(0);
    if (!page) { close(fd); return TABLE_ERROR; }
    PageResult pr = page_write_disk(page, fd);
    page_free(page);
    close(fd);
    if (pr != PAGE_OK) return TABLE_ERROR;
    if (save_schema(intent->table, intent->columns, intent->column_count) < 0)
        return TABLE_ERROR;
    catalog_add(intent->table);
    printf("  [table] Created \"%s\" with %d column(s)\n\n", intent->table, intent->column_count);
    return TABLE_OK;
}

Table *table_open(const char *name) {
    if (!table_exists(name)) return NULL;
    Table *table = calloc(1, sizeof(Table));
    if (!table) return NULL;
    strncpy(table->name, name, MAX_TABLE_NAME - 1);
    if (load_schema(name, table->col_defs, &table->col_count) < 0) {
        free(table); return NULL;
    }
    char path[256];
    data_path(name, path, sizeof(path));
    table->fd = open(path, O_RDWR);
    if (table->fd < 0) { free(table); return NULL; }

    off_t size = lseek(table->fd, 0, SEEK_END);
    table->page_count = (uint32_t)(size / PAGE_SIZE);

    buf_init(&table->pool, table->fd);

    table->index  = INDEX_create();
    table->index_col = find_index_col(table);

    uint8_t  buf[PAGE_SIZE];
    uint16_t len;
    char key_str[MAX_VALUE_LEN];

    for (uint32_t pid = 0; pid < table->page_count; pid++) {
        BufferFrame *frame = buf_pin(&table->pool, pid);
        if (!frame) continue;
        PageHeader *hdr = (PageHeader *)frame->page->data;
        for (uint16_t s = 0; s < hdr->slot_count; s++) {
            if (page_read(frame->page, s, buf, sizeof(buf), &len) != PAGE_OK)
                continue;
            get_col_value(buf, table->index_col, key_str, sizeof(key_str));
            int key = atoi(key_str);
            RowLocation loc = { .page_id = pid, .slot_id = s };
            INDEX_insert(table->index, key, loc);
        }
    }

    return table;
}

void table_close(Table *table) {
    if (!table) return;
    buf_destroy(&table->pool);
    if (table->index) INDEX_destroy(table->index);
    close(table->fd);
    free(table);
}

TableResult table_drop(const char *name) {
    if (!table_exists(name)) return TABLE_NOT_FOUND;
    char path[256];
    data_path(name, path, sizeof(path));
    remove(path);
    schema_path(name, path, sizeof(path));
    remove(path);
    catalog_remove(name);
    printf("  [table] Dropped \"%s\"\n\n", name);
    return TABLE_OK;
}

TableResult table_insert(Table *table, const Intent *intent) {
    uint8_t  row_buf[PAGE_SIZE];
    uint16_t row_len = serialise_row(intent, row_buf, sizeof(row_buf));

    /* WAL: log before write — golden rule */
    if (g_wal) {
        uint32_t txn_id;
        wal_begin(g_wal, &txn_id);
        wal_write(g_wal, txn_id, WAL_INSERT, table->name, row_buf, row_len);
        wal_commit(g_wal, txn_id);
    }

    for (uint32_t pid = 0; pid < table->page_count; pid++) {
        BufferFrame *frame = buf_pin(&table->pool, pid);
        if (!frame) continue;

        uint16_t slot_out;
        if (page_insert(frame->page, row_buf, row_len, &slot_out) == PAGE_OK) {
            buf_mark_dirty(frame);

            char key_str[MAX_VALUE_LEN];
            get_col_value(row_buf, table->index_col, key_str, sizeof(key_str));
            RowLocation loc = { .page_id = pid, .slot_id = slot_out };
            INDEX_insert(table->index, atoi(key_str), loc);

            printf("  [table] Inserted into \"%s\" page=%u slot=%u\n\n", table->name, pid, slot_out);
            return TABLE_OK;
        }
    }

    uint32_t    new_pid = table->page_count;
    BufferFrame *frame  = buf_new_page(&table->pool, new_pid);
    if (!frame) return TABLE_ERROR;
    uint16_t slot_out;
    page_insert(frame->page, row_buf, row_len, &slot_out);
    buf_mark_dirty(frame);
    table->page_count++;
    char key_str[MAX_VALUE_LEN];
    get_col_value(row_buf, table->index_col, key_str, sizeof(key_str));
    RowLocation loc = { .page_id = new_pid, .slot_id = slot_out };
    INDEX_insert(table->index, atoi(key_str), loc);
    printf("  [table] Inserted into \"%s\" page=%u slot=%u (new page)\n\n", table->name, new_pid, slot_out);
    return TABLE_OK;
}

void table_scan(Table *table, const Intent *intent) {
    uint8_t  buf[PAGE_SIZE];
    uint16_t len;
    int      found = 0;

    printf("\n  ");
    for (int c = 0; c < table->col_count; c++)
        printf("%-16s", table->col_defs[c].name);
    printf("\n  ");
    for (int c = 0; c < table->col_count; c++)
        printf("%-16s", "----------------");
    printf("\n");

    if (intent->where.active && strcmp(table->col_defs[table->index_col].name,intent->where.column) == 0) {

        int key = atoi(intent->where.value);
        RowLocation loc;

        if (INDEX_search(table->index, key, &loc) == INDEX_OK) {
            BufferFrame *frame = buf_pin(&table->pool, loc.page_id);
            if (frame &&
                page_read(frame->page, loc.slot_id, buf, sizeof(buf), &len) == PAGE_OK) {
                char tmp[PAGE_SIZE];
                strncpy(tmp, (char *)buf, sizeof(tmp) - 1);
                printf("  ");
                char *tok = strtok(tmp, "|");
                while (tok) { printf("%-16s", tok); tok = strtok(NULL, "|"); }
                printf("\n");
                found++;
            }
        }
        printf("\n  %d row(s) found  [index lookup]\n\n", found);
        return;
    }

    for (uint32_t pid = 0; pid < table->page_count; pid++) {
        BufferFrame *frame = buf_pin(&table->pool, pid);
        if (!frame) continue;
        PageHeader *hdr = (PageHeader *)frame->page->data;
        for (uint16_t s = 0; s < hdr->slot_count; s++) {
            if (page_read(frame->page, s, buf, sizeof(buf), &len) != PAGE_OK)
                continue;
            if (!row_matches_where(buf, intent, table))
                continue;
            char tmp[PAGE_SIZE];
            strncpy(tmp, (char *)buf, sizeof(tmp) - 1);
            printf("  ");
            char *tok = strtok(tmp, "|");
            while (tok) { printf("%-16s", tok); tok = strtok(NULL, "|"); }
            printf("\n");
            found++;
        }
    }
    printf("\n  %d row(s) found  [full scan]\n\n", found);
}

TableResult table_delete(Table *table, const Intent *intent) {
    uint8_t  buf[PAGE_SIZE];
    uint16_t len;
    int deleted = 0;

    for (uint32_t pid = 0; pid < table->page_count; pid++) {
        BufferFrame *frame = buf_pin(&table->pool, pid);
        if (!frame) continue;

        PageHeader *hdr = (PageHeader *)frame->page->data;
        uint16_t original_count = hdr->slot_count;
        for (uint16_t s = 0; s < original_count; s++) {
            if (page_read(frame->page, s, buf, sizeof(buf), &len) != PAGE_OK)
                continue;
            if (!row_matches_where(buf, intent, table))
                continue;

            /* WAL: log before delete */
            if (g_wal) {
                uint32_t txn_id;
                wal_begin(g_wal, &txn_id);
                wal_write(g_wal, txn_id, WAL_DELETE, table->name, buf, len);
                wal_commit(g_wal, txn_id);
            }
            char key_str[MAX_VALUE_LEN];
            get_col_value(buf, table->index_col, key_str, sizeof(key_str));
            INDEX_delete(table->index, atoi(key_str));
            page_delete(frame->page, s);
            buf_mark_dirty(frame);
            deleted++;
        }
    }
    printf("  [table] Deleted %d row(s) from \"%s\"\n\n", deleted, table->name);
    return TABLE_OK;
}

TableResult table_update(Table *table, const Intent *intent) {
    uint8_t  buf[PAGE_SIZE];
    uint16_t len;
    int updated = 0;

    for (uint32_t pid = 0; pid < table->page_count; pid++) {
        BufferFrame *frame = buf_pin(&table->pool, pid);
        if (!frame) continue;
        PageHeader *hdr = (PageHeader *)frame->page->data;
        uint16_t original_count = hdr->slot_count;
        for (uint16_t s = 0; s < original_count; s++) {
            if (page_read(frame->page, s, buf, sizeof(buf), &len) != PAGE_OK)
                continue;
            if (!row_matches_where(buf, intent, table))
                continue;
            int col_idx = -1;
            for (int c = 0; c < table->col_count; c++) {
                if (strcmp(table->col_defs[c].name, intent->set.column) == 0) {
                    col_idx = c; break;
                }
            }
            if (col_idx < 0) continue;
            char old_key_str[MAX_VALUE_LEN];
            get_col_value(buf, table->index_col, old_key_str, sizeof(old_key_str));
            INDEX_delete(table->index, atoi(old_key_str));
            char tmp[PAGE_SIZE];
            strncpy(tmp, (char *)buf, sizeof(tmp) - 1);
            char new_row[PAGE_SIZE];
            int  pos = 0;
            char *tok = strtok(tmp, "|");
            int   ci  = 0;

            while (tok) {
                if (ci > 0) new_row[pos++] = '|';
                const char *val = (ci == col_idx) ? intent->set.value : tok;
                pos += snprintf(new_row + pos, sizeof(new_row) - pos, "%s", val);
                tok = strtok(NULL, "|");
                ci++;
            }
            new_row[pos++] = '\0';

            /* WAL: log before update */
            if (g_wal) {
                uint32_t txn_id;
                wal_begin(g_wal, &txn_id);
                wal_write(g_wal, txn_id, WAL_UPDATE, table->name, (uint8_t *)new_row, (uint16_t)pos);
                wal_commit(g_wal, txn_id);
            }

            page_delete(frame->page, s);
            uint16_t new_slot;
            page_insert(frame->page, (uint8_t *)new_row, (uint16_t)pos, &new_slot);
            buf_mark_dirty(frame);

            char new_key_str[MAX_VALUE_LEN];
            get_col_value((uint8_t *)new_row, table->index_col, new_key_str, sizeof(new_key_str));
            RowLocation loc = { .page_id = pid, .slot_id = new_slot };
            INDEX_insert(table->index, atoi(new_key_str), loc);

            updated++;
        }
    }
    printf("  [table] Updated %d row(s) in \"%s\"\n\n", updated, table->name);
    return TABLE_OK;
}