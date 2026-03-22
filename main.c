#include "reph.h"
#include "table.h"
 
int main(void) {
    table_subsystem_init();
    repl_run();
    return 0;
}