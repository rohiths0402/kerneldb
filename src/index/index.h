#ifndef INDEX_H
#define INDEX_H

#include <stdint.h>
#define INDEX_ORDER     4
#define INDEX_MIN_KEYS  (INDEX_ORDER / 2)

typedef struct {
    uint32_t page_id;
    uint16_t slot_id;
} RowLocation;

typedef struct INDEXNode {
    int is_leaf;
    int num_keys;
    int keys[INDEX_ORDER];
    struct INDEXNode *children[INDEX_ORDER + 1];
    RowLocation locations[INDEX_ORDER];
    struct INDEXNode *next;

} INDEXNode;

typedef struct {
    INDEXNode *root;
    int        node_count;
} INDEX;

typedef enum {
    INDEX_OK,
    INDEX_NOT_FOUND,
    INDEX_DUPLICATE,
    INDEX_ERROR
} INDEXResult;

INDEX *INDEX_create(void);

void INDEX_destroy(INDEX *tree);

INDEXResult INDEX_insert(INDEX *tree, int key, RowLocation loc);

INDEXResult INDEX_search(INDEX *tree, int key, RowLocation *loc_out);

INDEXResult INDEX_delete(INDEX *tree, int key);

void INDEX_print(const INDEX *tree);

void INDEX_print_leaves(const INDEX *tree);

#endif