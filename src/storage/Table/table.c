#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "table.h"
#include "../buffer/buffer.h"
#include "../page/page.h"
#include "../common/intent.h"

static void data_path(const char *name, char *out, size_t size) {
    snprintf(out, size, "%s/%s.db", TABLE_DIR, name);
}

static void schema_path(const char *name, char *out, size_t size) {
    snprintf(out, size, "%s/%s.schema", TABLE_DIR, name);
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
        int written = snprintf((char *)buf + pos, bufsize - pos,"%s", intent->values[i]);
        pos += written;
        if (i < intent->value_count - 1 && pos < bufsize - 1)
            buf[pos++] = '|';
    }
    buf[pos] = '\0';
    return (uint16_t)(pos + 1);
}

static int row_matches_where(const uint8_t *row_data, uint16_t len, const Intent *intent, const Table *table) {
    if (!intent->where.active) return 1;

    /* Find column index for WHERE column */
    int col_idx = -1;
    for (int i = 0; i < table->col_count; i++) {
        if (strcmp(table->col_defs[i].name, intent->where.column) == 0) {
            col_idx = i;
            break;
        }
    }
    if (col_idx < 0) return 0;

    /* Split row by '|' and get the value at col_idx */
    char tmp[MAX_VALUE_LEN * MAX_COLUMNS];
    strncpy(tmp, (const char *)row_data, sizeof(tmp) - 1);

    char *tok = strtok(tmp, "|");
    for (int i = 0; i < col_idx && tok; i++)
        tok = strtok(NULL, "|");

    if (!tok) return 0;
    return strcmp(tok, intent->where.value) == 0;
}

void table_subsystem_init(void) {
    struct stat st;
    if (stat(TABLE_DIR, &st) != 0)
        mkdir(TABLE_DIR, 0755);
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

    /* Create the data file */
    int fd = open(path, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        perror("table_create: open");
        return TABLE_ERROR;
    }

    /* Write a first empty page */
    Page *page = page_alloc(0);
    if (!page) { close(fd); return TABLE_ERROR; }

    PageResult pr = page_write_disk(page, fd);
    page_free(page);
    close(fd);

    if (pr != PAGE_OK) return TABLE_ERROR;

    /* Save schema */
    if (save_schema(intent->table, intent->columns, intent->column_count) < 0)
        return TABLE_ERROR;

    printf("  [table] Created \"%s\" with %d column(s)\n\n", intent->table, intent->column_count);
    return TABLE_OK;
}

Table *table_open(const char *name) {
    if (!table_exists(name)) return NULL;

    Table *table = calloc(1, sizeof(Table));
    if (!table) return NULL;

    strncpy(table->name, name, MAX_TABLE_NAME - 1);

    /* Load schema */
    if (load_schema(name, table->col_defs, &table->col_count) < 0) {
        free(table);
        return NULL;
    }

    /* Open data file */
    char path[256];
    data_path(name, path, sizeof(path));
    table->fd = open(path, O_RDWR);
    if (table->fd < 0) {
        free(table);
        return NULL;
    }
    off_t size = lseek(table->fd, 0, SEEK_END);
    table->page_count = (uint32_t)(size / PAGE_SIZE);
    buf_init(&table->pool, table->fd);

    return table;
}

void table_close(Table *table) {
    if (!table) return;
    buf_destroy(&table->pool);
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

    printf("  [table] Dropped \"%s\"\n\n", name);
    return TABLE_OK;
}

TableResult table_insert(Table *table, const Intent *intent) {
    uint8_t  row_buf[PAGE_SIZE];
    uint16_t row_len = serialise_row(intent, row_buf, sizeof(row_buf));

    /* Try inserting into existing pages via buffer pool */
    for (uint32_t pid = 0; pid < table->page_count; pid++) {
        BufferFrame *frame = buf_pin(&table->pool, pid);
        if (!frame) continue;

        uint16_t slot_out;
        PageResult pr = page_insert(frame->page, row_buf, row_len, &slot_out);

        if (pr == PAGE_OK) {
            buf_mark_dirty(frame);
            printf("  [table] Inserted into \"%s\" page=%u slot=%u\n\n", table->name, pid, slot_out);
            return TABLE_OK;
        }
    }

    /* All pages full — allocate a new page */
    uint32_t new_pid = table->page_count;
    BufferFrame *frame = buf_new_page(&table->pool, new_pid);
    if (!frame) return TABLE_ERROR;

    uint16_t slot_out;
    page_insert(frame->page, row_buf, row_len, &slot_out);
    buf_mark_dirty(frame);
    table->page_count++;

    printf("  [table] Inserted into \"%s\" page=%u slot=%u (new page)\n\n", table->name, new_pid, slot_out);
    return TABLE_OK;
}

void table_scan(Table *table, const Intent *intent) {
    uint8_t  buf[PAGE_SIZE];
    uint16_t len;
    int      found = 0;

    /* Print column headers */
    printf("\n  ");
    for (int c = 0; c < table->col_count; c++) {
        printf("%-16s", table->col_defs[c].name);
    }
    printf("\n  ");
    for (int c = 0; c < table->col_count; c++)
        printf("%-16s", "----------------");
    printf("\n");

    for (uint32_t pid = 0; pid < table->page_count; pid++) {
        BufferFrame *frame = buf_pin(&table->pool, pid);
        if (!frame) continue;

        PageHeader *hdr = (PageHeader *)frame->page->data;
        for (uint16_t s = 0; s < hdr->slot_count; s++) {
            if (page_read(frame->page, s, buf, sizeof(buf), &len) != PAGE_OK)
                continue;

            if (!row_matches_where(buf, len, intent, table))
                continue;

            /* Print row values */
            char tmp[PAGE_SIZE];
            strncpy(tmp, (char *)buf, sizeof(tmp) - 1);
            printf("  ");
            char *tok = strtok(tmp, "|");
            while (tok) {
                printf("%-16s", tok);
                tok = strtok(NULL, "|");
            }
            printf("\n");
            found++;
        }
    }

    printf("\n  %d row(s) found\n\n", found);
}

TableResult table_delete(Table *table, const Intent *intent) {
    uint8_t  buf[PAGE_SIZE];
    uint16_t len;
    int      deleted = 0;

    for (uint32_t pid = 0; pid < table->page_count; pid++) {
        BufferFrame *frame = buf_pin(&table->pool, pid);
        if (!frame) continue;

        PageHeader *hdr = (PageHeader *)frame->page->data;
        for (uint16_t s = 0; s < hdr->slot_count; s++) {
            if (page_read(frame->page, s, buf, sizeof(buf), &len) != PAGE_OK)
                continue;

            if (!row_matches_where(buf, len, intent, table))
                continue;

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
    int      updated = 0;

    for (uint32_t pid = 0; pid < table->page_count; pid++) {
        BufferFrame *frame = buf_pin(&table->pool, pid);
        if (!frame) continue;

        PageHeader *hdr = (PageHeader *)frame->page->data;
        for (uint16_t s = 0; s < hdr->slot_count; s++) {
            if (page_read(frame->page, s, buf, sizeof(buf), &len) != PAGE_OK)
                continue;

            if (!row_matches_where(buf, len, intent, table))
                continue;

            /* Find column to update and replace its value */
            int col_idx = -1;
            for (int c = 0; c < table->col_count; c++) {
                if (strcmp(table->col_defs[c].name, intent->set.column) == 0) {
                    col_idx = c;
                    break;
                }
            }
            if (col_idx < 0) continue;
            /* Rebuild row with updated value */
            char tmp[PAGE_SIZE];
            strncpy(tmp, (char *)buf, sizeof(tmp) - 1);
            char new_row[PAGE_SIZE];
            int  pos = 0;
            char *tok = strtok(tmp, "|");
            int   ci  = 0;

            while (tok) {
                if (ci > 0) new_row[pos++] = '|';
                const char *val = (ci == col_idx) ? intent->set.value : tok;
                int written = snprintf(new_row + pos, sizeof(new_row) - pos, "%s", val);
                pos += written;
                tok = strtok(NULL, "|");
                ci++;
            }
            new_row[pos++] = '\0';
            page_delete(frame->page, s);
            uint16_t new_slot;
            page_insert(frame->page, (uint8_t *)new_row, (uint16_t)pos, &new_slot);
            buf_mark_dirty(frame);
            updated++;
        }
    }

    printf("  [table] Updated %d row(s) in \"%s\"\n\n", updated, table->name);
    return TABLE_OK;
}