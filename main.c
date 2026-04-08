#include <stdio.h>
#include <string.h>
#include "reph.h"
#include "table.h"
#include "../WAL/wal.h"

WAL *g_wal = NULL;
extern char current_db[64];

int main(void) {
    table_subsystem_init();

    g_wal = wal_open();
    if (!g_wal) {
        fprintf(stderr, "  [fatal] Could not open WAL\n");
        return 1;
    }
    FILE *f = fopen("data/.current_db", "r");
    if (f) {
        fgets(current_db, sizeof(current_db), f);
        current_db[strcspn(current_db, "\n")] = '\0';
        fclose(f);

        printf("[db] restored: %s\n", current_db);
    }
    wal_recover(g_wal);
    repl_run();
    wal_close(g_wal);
    
    return 0;
}