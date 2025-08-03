#include "btree.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static BTreeLeafNode* find_leaf(BufferPool* pool, Pager* pager, uint32_t root_page_id, int key) {
    Page* page = buffer_pool_get_page(pool, pager, root_page_id);
    if (page == NULL) {
        return NULL;
    }

    BTreeNodeHeader* header = (BTreeNodeHeader*)page;
    if (header->is_leaf) {
        return (BTreeLeafNode*)page;
    }

    BTreeInternalNode* node = (BTreeInternalNode*)page;
    int i = 0;
    while (i < node->header.num_keys && key >= node->keys[i]) {
        i++;
    }

    buffer_pool_unpin_page(pool, pager, root_page_id, 0);
    return find_leaf(pool, pager, node->children[i], key);
}

char* btree_search(BufferPool* pool, Pager* pager, uint32_t root_page_id, int key) {
    BTreeLeafNode* leaf = find_leaf(pool, pager, root_page_id, key);
    if (leaf == NULL) {
        return NULL;
    }

    for (int i = 0; i < leaf->header.num_keys; i++) {
        if (leaf->keys[i] != -1 && leaf->keys[i] == key) {
            char* value = (char*)malloc(strlen(leaf->values[i]) + 1);
            strcpy(value, leaf->values[i]);
            buffer_pool_unpin_page(pool, pager, root_page_id, 0);
            return value;
        }
    }

    buffer_pool_unpin_page(pool, pager, root_page_id, 0);
    return NULL;
}

void btree_insert(BufferPool* pool, Pager* pager, uint32_t root_page_id, int key, const char* value, int is_transaction_active) {
    // simplified - no node splits
    BTreeLeafNode* leaf = find_leaf(pool, pager, root_page_id, key);
    if (leaf == NULL) {
        return;
    }

    // check if key exists for update
    for (int i = 0; i < leaf->header.num_keys; i++) {
        if (leaf->keys[i] == key) {

            strcpy(leaf->values[i], value);
            buffer_pool_unpin_page(pool, pager, root_page_id, 1); 
            return;
        }
    }

    if (leaf->header.num_keys < BTREE_ORDER - 1) {
        int i = 0;
        while (i < leaf->header.num_keys && key > leaf->keys[i]) {
            i++;
        }

        for (int j = leaf->header.num_keys; j > i; j--) {
            leaf->keys[j] = leaf->keys[j - 1];
            strcpy(leaf->values[j], leaf->values[j - 1]);
        }

        leaf->keys[i] = key;
        strcpy(leaf->values[i], value);
        leaf->header.num_keys++;
        buffer_pool_unpin_page(pool, pager, root_page_id, 1); 
    }
}

void btree_delete(BufferPool* pool, Pager* pager, uint32_t root_page_id, int key, int is_transaction_active) {
    // simplified - no node merges
    BTreeLeafNode* leaf = find_leaf(pool, pager, root_page_id, key);
    if (leaf == NULL) {
        return;
    }

    int i = 0;
    while (i < leaf->header.num_keys && key != leaf->keys[i]) {
        i++;
    }

    if (i < leaf->header.num_keys) {
        leaf->keys[i] = -1; // mark as deleted
        memset(leaf->values[i], 0, sizeof(leaf->values[i]));
        leaf->header.num_keys--;
        buffer_pool_unpin_page(pool, pager, root_page_id, 1); 
    }
}
