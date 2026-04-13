#ifndef TABLE_H
#define TABLE_H

#include <stdint.h>
#include "../buffer/buffer.h"
#include "../page/page.h"
#include "../index/index.h"
#include "../common/intent.h"

#define MAX_TABLE_NAME 64
#define MAX_TABLES 16
#define TABLE_DIR "data"

typedef struct {
    char name[MAX_TABLE_NAME];
    int fd;
    BufferPool pool;
    INDEX *index;
    uint32_t page_count;
    ColumnDef col_defs[MAX_COLUMNS];
    int col_count;
    int index_col;
} Table;

typedef enum {
    TABLE_OK,
    TABLE_NOT_FOUND,
    TABLE_ALREADY_EXISTS,
    TABLE_FULL,
    TABLE_ERROR
} TableResult;

void table_subsystem_init(void);
TableResult table_create(const Intent *intent);
Table *table_open(const char *name);
void table_close(Table *table);
void table_flush(Table *table);
TableResult table_drop(const char *name);
TableResult table_insert(Table *table, const Intent *intent);
TableResult table_insert_raw(Table *table, const uint8_t *row_buf, uint16_t row_len);
void table_scan(Table *table, const Intent *intent);
TableResult table_delete(Table *table, const Intent *intent);
TableResult table_update(Table *table, const Intent *intent);
int table_exists(const char *name);

#endif 