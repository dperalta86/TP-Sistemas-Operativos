#ifndef PAGE_TABLE_H
#define PAGE_TABLE_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct {
    uint32_t page_number;
    uint32_t frame;
    bool dirty;
    bool present;
} pt_entry_t;

typedef struct {
    pt_entry_t *entries;
    uint32_t page_count;
    size_t page_size;
} page_table_t;

page_table_t *pt_create(uint32_t page_count, size_t page_size);
void pt_destroy(page_table_t *page_table);

int pt_map(page_table_t *page_table, uint32_t page_number, uint32_t frame);

int pt_unmap(page_table_t *page_table, uint32_t page_number);

void pt_set_dirty(page_table_t *page_table, uint32_t page_number, bool dirty);
void pt_set_present(page_table_t *page_table, uint32_t page_number, bool present);

pt_entry_t *pt_get_dirty_entries(page_table_t *page_table, size_t *count);

#endif
