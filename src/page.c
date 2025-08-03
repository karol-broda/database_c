#include "page.h"

#include <string.h>

// initialize a new page
void page_init(Page* page, PageType page_type) {
    if (page == NULL) {
        return;
    }
    memset(page, 0, PAGE_SIZE);
    page->header.page_type = page_type;
    page->header.free_space_offset = sizeof(PageHeader);
    page->header.num_cells = 0;
}
