#include <stdio.h>
#include "reph.h"
#include "table.h"
#include "../WAL/wal.h"

WAL *g_wal = NULL;

int main(void) {
    table_subsystem_init();
    g_wal = wal_open();
    if (!g_wal) {
        fprintf(stderr, "  [fatal] Could not open WAL\n");
        return 1;
    }
    wal_recover(g_wal);

    repl_run();

    wal_close(g_wal);
    
    return 0;
}