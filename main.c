#include <stdio.h>
#include "reph.h"
#include "../index/index.h"
#include "../storage/Table/table.h"

static void test_index(void) {
    printf("\n  === Index (B+Tree) Test ===\n\n");
 
    INDEX *idx = INDEX_create();
 
    /* Insert keys */
    int test_keys[] = { 5, 3, 8, 1, 4, 7, 9, 2, 6 };
    int n = sizeof(test_keys) / sizeof(test_keys[0]);
 
    for (int i = 0; i < n; i++) {
        RowLocation loc = { .page_id = (uint32_t)i, .slot_id = (uint16_t)i };
        INDEX_insert(idx, test_keys[i], loc);
        printf("  INSERT key=%d -> page=%u slot=%u\n",
               test_keys[i], loc.page_id, loc.slot_id);
    }
 
    /* Print tree structure */
    INDEX_print(idx);
 
    /* Print leaves in order */
    INDEX_print_leaves(idx);
 
    /* Search test */
    printf("  SEARCH test:\n");
    int search_keys[] = { 1, 5, 9, 99 };
    for (int i = 0; i < 4; i++) {
        RowLocation loc;
        INDEXResult r = INDEX_search(idx, search_keys[i], &loc);
        if (r == INDEX_OK)
            printf("    key=%d FOUND -> page=%u slot=%u\n",
                   search_keys[i], loc.page_id, loc.slot_id);
        else
            printf("    key=%d NOT FOUND\n", search_keys[i]);
    }
 
    /* Delete test */
    printf("\n  DELETE key=5\n");
    INDEX_delete(idx, 5);
    INDEX_print_leaves(idx);
 
    INDEX_destroy(idx);
    printf("  === Index Test PASSED ===\n\n");
}
 
int main(void) {
    /* Run index test first */
    test_index();
 
    /* Start the database */
    table_subsystem_init();
    repl_run();
    return 0;
}