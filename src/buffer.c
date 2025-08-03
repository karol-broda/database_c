#include "buffer.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

BufferPool* buffer_pool_init() {
    BufferPool* pool = (BufferPool*)malloc(sizeof(BufferPool));
    if (pool == NULL) {
        return NULL;
    }
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        pool->frames[i].page_id = -1;
        pool->frames[i].is_dirty = 0;
        pool->frames[i].pin_count = 0;
        pool->frames[i].lru_counter = 0;
    }
    pool->next_victim = 0;
    return pool;
}

// find victim frame using lru
static int find_victim_frame(BufferPool* pool) {
    int victim = -1;
    int min_lru_counter = -1;

    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (pool->frames[i].pin_count == 0) {
            if (victim == -1 || pool->frames[i].lru_counter < min_lru_counter) {
                victim = i;
                min_lru_counter = pool->frames[i].lru_counter;
            }
        }
    }

    return victim;
}

Page* buffer_pool_get_page(BufferPool* pool, Pager* pager, uint32_t page_id) {
    
    // check if page already in buffer
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (pool->frames[i].page_id == page_id) {
            pool->frames[i].pin_count++;
            pool->frames[i].lru_counter = 0;
            
            return &pool->frames[i].page;
        }
    }

    // page not in buffer, find victim
    int frame_idx = find_victim_frame(pool);
    if (frame_idx == -1) {
        
        return NULL;
    }

    // flush dirty victim
    if (pool->frames[frame_idx].is_dirty) {
        
        buffer_pool_flush(pool, pager, pool->frames[frame_idx].page_id);
    }


    
    lseek(pager->fd, page_id * PAGE_SIZE, SEEK_SET);
    read(pager->fd, &pool->frames[frame_idx].page, PAGE_SIZE);

    pool->frames[frame_idx].page_id = page_id;
    pool->frames[frame_idx].is_dirty = 0;
    pool->frames[frame_idx].pin_count = 1;
    pool->frames[frame_idx].lru_counter = 0;

    return &pool->frames[frame_idx].page;
}

void buffer_pool_unpin_page(BufferPool* pool, Pager* pager, uint32_t page_id, int is_dirty) {
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (pool->frames[i].page_id == page_id) {
            pool->frames[i].pin_count--;
            if (is_dirty) {
                pool->frames[i].is_dirty = 1;
                buffer_pool_flush(pool, pager, page_id);
            }
            break;
        }
    }
}

void buffer_pool_flush(BufferPool* pool, Pager* pager, uint32_t page_id) {
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (pool->frames[i].page_id == page_id) {
            if (pool->frames[i].is_dirty) {
                lseek(pager->fd, page_id * PAGE_SIZE, SEEK_SET);
                write(pager->fd, &pool->frames[i].page, PAGE_SIZE);
                pool->frames[i].is_dirty = 0;
            }
            break;
        }
    }
}

void buffer_pool_flush_all(BufferPool* pool, Pager* pager) {
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        buffer_pool_flush(pool, pager, pool->frames[i].page_id);
    }
}

Pager* pager_open(const char* filename) {
    int fd = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd == -1) {
        return NULL;
    }

    Pager* pager = (Pager*)malloc(sizeof(Pager));
    if (pager == NULL) {
        close(fd);
        return NULL;
    }

    pager->fd = fd;
    off_t file_length = lseek(fd, 0, SEEK_END);
    pager->next_page_id = file_length / PAGE_SIZE;
    strcpy(pager->filename, filename);

    return pager;
}

void pager_close(Pager* pager) {
    if (pager != NULL) {
        close(pager->fd);
        free(pager);
    }
}
