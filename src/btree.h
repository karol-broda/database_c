#ifndef BTREE_H
#define BTREE_H

#include "buffer.h"

#define BTREE_ORDER 32


typedef struct {
    int is_leaf;
    int num_keys;
} BTreeNodeHeader;


typedef struct {
    BTreeNodeHeader header;
    int keys[BTREE_ORDER - 1];
    uint32_t children[BTREE_ORDER];
} BTreeInternalNode;


typedef struct {
    BTreeNodeHeader header;
    int keys[BTREE_ORDER - 1];
    char values[BTREE_ORDER - 1][256]; // fixed size values
    uint32_t next_leaf;
} BTreeLeafNode;

void btree_insert(BufferPool* pool, Pager* pager, uint32_t root_page_id, int key, const char* value, int is_transaction_active);
void btree_delete(BufferPool* pool, Pager* pager, uint32_t root_page_id, int key, int is_transaction_active);
char* btree_search(BufferPool* pool, Pager* pager, uint32_t root_page_id, int key);

#endif // BTREE_H
