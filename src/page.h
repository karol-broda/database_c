#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>

#define PAGE_SIZE 4096


typedef enum {
    PAGE_TYPE_INTERNAL,
    PAGE_TYPE_LEAF,
    PAGE_TYPE_METADATA
} PageType;


typedef struct {
    PageType page_type;
    uint16_t free_space_offset;
    uint16_t num_cells;
} PageHeader;


typedef struct {
    PageHeader header;
    char data[PAGE_SIZE - sizeof(PageHeader)];
} Page;

#endif // PAGE_H
