#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "index.h"

static INDEXNode *node_alloc(int is_leaf) {
    INDEXNode *node = calloc(1, sizeof(INDEXNode));
    if (!node) return NULL;
    node->is_leaf  = is_leaf;
    node->num_keys = 0;
    node->next     = NULL;
    return node;
}

static void node_free(INDEXNode *node) {
    if (!node) return;
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++)
            node_free(node->children[i]);
    }
    free(node);
}

static INDEXNode *find_leaf(INDEXNode *root, int key) {
    INDEXNode *cur = root;
    while (!cur->is_leaf) {
        int i = 0;
        while (i < cur->num_keys && key >= cur->keys[i])
            i++;
        cur = cur->children[i];
    }
    return cur;
}

static INDEXNode *split_leaf(INDEXNode *left, int *new_key) {
    INDEXNode *right = node_alloc(1);
    if (!right) return NULL;

    int mid = INDEX_ORDER / 2;

    right->num_keys = left->num_keys - mid;
    for (int i = 0; i < right->num_keys; i++) {
        right->keys[i]      = left->keys[mid + i];
        right->locations[i] = left->locations[mid + i];
    }

    left->num_keys = mid;
    right->next    = left->next;
    left->next     = right;
    *new_key       = right->keys[0];

    return right;
}

static INDEXNode *split_internal(INDEXNode *left, int *new_key) {
    INDEXNode *right = node_alloc(0);
    if (!right) return NULL;

    int mid = INDEX_ORDER / 2;
    *new_key = left->keys[mid];

    right->num_keys = left->num_keys - mid - 1;
    for (int i = 0; i < right->num_keys; i++)
        right->keys[i] = left->keys[mid + 1 + i];
    for (int i = 0; i <= right->num_keys; i++)
        right->children[i] = left->children[mid + 1 + i];

    left->num_keys = mid;
    return right;
}

typedef struct {
    int split;
    int  promoted_key;
    INDEXNode *right_child;
} InsertResult;

static InsertResult insert_recursive(INDEXNode *node, int key, RowLocation loc) {
    InsertResult res = { 0, 0, NULL };

    if (node->is_leaf) {
        int i = node->num_keys - 1;
        while (i >= 0 && node->keys[i] > key) {
            node->keys[i + 1]      = node->keys[i];
            node->locations[i + 1] = node->locations[i];
            i--;
        }
        node->keys[i + 1]      = key;
        node->locations[i + 1] = loc;
        node->num_keys++;

        if (node->num_keys == INDEX_ORDER) {
            int new_key;
            INDEXNode *right = split_leaf(node, &new_key);
            res.split        = 1;
            res.promoted_key = new_key;
            res.right_child  = right;
        }
        return res;
    }

    int i = 0;
    while (i < node->num_keys && key >= node->keys[i])
        i++;

    InsertResult child_res = insert_recursive(node->children[i], key, loc);
    if (!child_res.split) return res;

    int pos = node->num_keys - 1;
    while (pos >= 0 && node->keys[pos] > child_res.promoted_key) {
        node->keys[pos + 1]     = node->keys[pos];
        node->children[pos + 2] = node->children[pos + 1];
        pos--;
    }
    node->keys[pos + 1]     = child_res.promoted_key;
    node->children[pos + 2] = child_res.right_child;
    node->num_keys++;

    if (node->num_keys == INDEX_ORDER) {
        int new_key;
        INDEXNode *right = split_internal(node, &new_key);
        res.split        = 1;
        res.promoted_key = new_key;
        res.right_child  = right;
    }
    return res;
}

INDEX *INDEX_create(void) {
    INDEX *tree = calloc(1, sizeof(INDEX));
    if (!tree) return NULL;
    tree->root = node_alloc(1);
    if (!tree->root) { free(tree); return NULL; }
    tree->node_count = 1;
    return tree;
}

void INDEX_destroy(INDEX *tree) {
    if (!tree) return;
    node_free(tree->root);
    free(tree);
}

INDEXResult INDEX_insert(INDEX *tree, int key, RowLocation loc) {
    if (!tree || !tree->root) return INDEX_ERROR;

    InsertResult res = insert_recursive(tree->root, key, loc);

    if (res.split) {
        INDEXNode *new_root = node_alloc(0);
        if (!new_root) return INDEX_ERROR;
        new_root->keys[0]     = res.promoted_key;
        new_root->children[0] = tree->root;
        new_root->children[1] = res.right_child;
        new_root->num_keys    = 1;
        tree->root = new_root;
        tree->node_count++;
    }

    tree->node_count++;
    return INDEX_OK;
}

INDEXResult INDEX_search(INDEX *tree, int key, RowLocation *loc_out) {
    if (!tree || !tree->root) return INDEX_NOT_FOUND;

    INDEXNode *leaf = find_leaf(tree->root, key);
    for (int i = 0; i < leaf->num_keys; i++) {
        if (leaf->keys[i] == key) {
            if (loc_out) *loc_out = leaf->locations[i];
            return INDEX_OK;
        }
    }
    return INDEX_NOT_FOUND;
}

INDEXResult INDEX_delete(INDEX *tree, int key) {
    if (!tree || !tree->root) return INDEX_NOT_FOUND;

    INDEXNode *leaf = find_leaf(tree->root, key);
    for (int i = 0; i < leaf->num_keys; i++) {
        if (leaf->keys[i] == key) {
            for (int j = i; j < leaf->num_keys - 1; j++) {
                leaf->keys[j]      = leaf->keys[j + 1];
                leaf->locations[j] = leaf->locations[j + 1];
            }
            leaf->num_keys--;
            return INDEX_OK;
        }
    }
    return INDEX_NOT_FOUND;
}

static void print_node(const INDEXNode *node, int level) {
    if (!node) return;
    printf("  ");
    for (int i = 0; i < level; i++) printf("  ");
    if (node->is_leaf) {
        printf("[LEAF] keys: ");
        for (int i = 0; i < node->num_keys; i++)
            printf("%d(p%u,s%u) ", node->keys[i],
                   node->locations[i].page_id,
                   node->locations[i].slot_id);
    } else {
        printf("[NODE] keys: ");
        for (int i = 0; i < node->num_keys; i++)
            printf("%d ", node->keys[i]);
    }
    printf("\n");
    if (!node->is_leaf)
        for (int i = 0; i <= node->num_keys; i++)
            print_node(node->children[i], level + 1);
}

void INDEX_print(const INDEX *tree) {
    if (!tree || !tree->root) { printf("  [index] (empty)\n"); return; }
    printf("\n  [index] nodes=%d\n", tree->node_count);
    print_node(tree->root, 0);
    printf("\n");
}

void INDEX_print_leaves(const INDEX *tree) {
    if (!tree || !tree->root) return;
    INDEXNode *leaf = tree->root;
    while (!leaf->is_leaf)
        leaf = leaf->children[0];
    printf("\n  [index leaves] ");
    while (leaf) {
        for (int i = 0; i < leaf->num_keys; i++)
            printf("%d ", leaf->keys[i]);
        printf("| ");
        leaf = leaf->next;
    }
    printf("\n\n");
}