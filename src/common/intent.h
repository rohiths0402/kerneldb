#ifndef INTENT_H
#define INTENT_H

#define MAX_COLUMNS     32
#define MAX_NAME_LEN    64
#define MAX_VALUE_LEN   256
#define MAX_VALUES      32

typedef enum {
    INTENT_SELECT,
    INTENT_INSERT,
    INTENT_UPDATE,
    INTENT_DELETE,
    INTENT_CREATE_TABLE,
    INTENT_DROP_TABLE,
    INTENT_CREATE_DB,
    INTENT_USE_DB,
    INTENT_UNKNOWN
} IntentType;

typedef struct {
    char column[MAX_NAME_LEN];
    char value[MAX_VALUE_LEN];
    int  active;
} WhereClause;

typedef struct {
    char name[MAX_NAME_LEN];
    char type[MAX_NAME_LEN];
} ColumnDef;

typedef struct {
    char column[MAX_NAME_LEN];
    char value[MAX_VALUE_LEN];
} SetClause;

typedef struct {
    IntentType  type;
    char table[MAX_NAME_LEN];
    char db_name[MAX_NAME_LEN];
    int select_all;
    char select_cols[MAX_COLUMNS][MAX_NAME_LEN];
    int select_col_count;
    char values[MAX_VALUES][MAX_VALUE_LEN];
    int value_count;
    SetClause set;
    ColumnDef columns[MAX_COLUMNS];
    int column_count;
    WhereClause where;

}Intent;

#endif