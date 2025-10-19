#include "page_table.h"

page_table_t *pt_create(uint32_t page_count, size_t page_size)
{
    if (page_count == 0 || page_size == 0)
        return NULL;

    page_table_t *page_table = malloc(sizeof(page_table_t));
    if (!page_table)
        return NULL;

    page_table->entries = calloc(page_count, sizeof(pt_entry_t));
    if (!page_table->entries)
    {
        free(page_table);
        return NULL;
    }

    page_table->page_count = page_count;
    page_table->page_size = page_size;

    for (uint32_t i = 0; i < page_count; i++)
        page_table->entries[i].page_number = i;

    return page_table;
}


void pt_destroy(page_table_t *page_table)
{
    if (page_table)
    {
        free(page_table->entries);
        free(page_table);
    }
}

int pt_map(page_table_t *page_table, uint32_t page_number, uint32_t frame)
{
    if (!page_table || page_number >= page_table->page_count)
        return -1;

    pt_entry_t *entry = &page_table->entries[page_number];
    entry->frame = frame;
    entry->present = true;
    entry->dirty = false;
    entry->page_number = page_number;
    return 0;
}

int pt_unmap(page_table_t *page_table, uint32_t page_number)
{
    if (!page_table || page_number >= page_table->page_count)
        return -1;

    pt_entry_t *entry = &page_table->entries[page_number];
    entry->frame = 0;
    entry->present = false;
    entry->dirty = false;
    return 0;
}

void pt_mark_dirty(page_table_t *page_table, uint32_t page_number)
{
    if (!page_table || page_number >= page_table->page_count)
        return;

    pt_entry_t *entry = &page_table->entries[page_number];
    entry->dirty = true;
    entry->page_number = page_number;
}

void pt_mark_clean(page_table_t *page_table, uint32_t page_number)
{
    if (!page_table || page_number >= page_table->page_count)
        return;

    pt_entry_t *entry = &page_table->entries[page_number];
    entry->dirty = false;
    entry->page_number = page_number;
}

void pt_mark_present(page_table_t *page_table, uint32_t page_number)
{
    if (!page_table || page_number >= page_table->page_count)
        return;

    pt_entry_t *entry = &page_table->entries[page_number];
    entry->present = true;
    entry->page_number = page_number;
}

void pt_mark_absent(page_table_t *page_table, uint32_t page_number)
{
    if (!page_table || page_number >= page_table->page_count)
        return;

    pt_entry_t *entry = &page_table->entries[page_number];
    entry->present = false;
    entry->page_number = page_number;
}

pt_entry_t *pt_get_dirty_entries(page_table_t *page_table, size_t *count)
{
    if (!page_table || !count)
        return NULL;

    size_t dirty_count = 0;
    for (uint32_t i = 0; i < page_table->page_count; i++)
        if (page_table->entries[i].dirty)
            dirty_count++;

    *count = dirty_count;
    if (dirty_count == 0)
        return NULL;

    pt_entry_t *dirty_entries = malloc(dirty_count * sizeof(pt_entry_t));
    if (!dirty_entries)
    {
        *count = 0;
        return NULL;
    }

    size_t index = 0;
    for (uint32_t i = 0; i < page_table->page_count; i++)
    {
        if (page_table->entries[i].dirty)
        {
            dirty_entries[index] = page_table->entries[i];
            dirty_entries[index].page_number = i;
            index++;
        }
    }

    return dirty_entries;
}

