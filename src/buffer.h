#ifndef BUFFER_H
#define BUFFER_H

#include "page.h"
#include <stdint.h>

#define BUFFER_POOL_SIZE 100


typedef struct {
    Page page;
    uint32_t page_id;
    int is_dirty;
    int pin_count;
    int lru_counter;
} Frame;


typedef struct {
    Frame frames[BUFFER_POOL_SIZE];
    int next_victim;
} BufferPool;


typedef struct {
    int fd;
    uint32_t next_page_id;
    char filename[256];
} Pager;

BufferPool* buffer_pool_init();
void buffer_pool_flush(BufferPool* pool, Pager* pager, uint32_t page_id);
void buffer_pool_flush_all(BufferPool* pool, Pager* pager);
Page* buffer_pool_get_page(BufferPool* pool, Pager* pager, uint32_t page_id);
void buffer_pool_unpin_page(BufferPool* pool, Pager* pager, uint32_t page_id, int is_dirty);

Pager* pager_open(const char* filename);
void pager_close(Pager* pager);

#endif // BUFFER_H
