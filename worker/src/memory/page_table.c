#include "page_table.h"

static pt_entry_t *pt_find_entry(page_table_t *page_table, uint32_t page_number)
{
    if (!page_table)
        return NULL;

    for (uint32_t i = 0; i < page_table->page_count; i++)
        if (page_table->entries[i].page_number == page_number)
            return &page_table->entries[i];

    return NULL;
}

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
    {
        page_table->entries[i].page_number = i;
        page_table->entries[i].frame = 0;
        page_table->entries[i].dirty = false;
        page_table->entries[i].present = false;
    }

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
    if (!page_table)
        return -1;

    pt_entry_t *entry = pt_find_entry(page_table, page_number);
    if (!entry)
        return -1;

    entry->frame = frame;
    entry->present = true;
    entry->dirty = false;
    return 0;
}

int pt_unmap(page_table_t *page_table, uint32_t page_number)
{
    if (!page_table)
        return -1;

    pt_entry_t *entry = pt_find_entry(page_table, page_number);
    if (!entry)
        return -1;

    entry->frame = 0;
    entry->present = false;
    entry->dirty = false;
    return 0;
}

void pt_mark_dirty(page_table_t *page_table, uint32_t page_number)
{
    pt_entry_t *entry = pt_find_entry(page_table, page_number);
    if (entry)
        entry->dirty = true;
}

void pt_mark_clean(page_table_t *page_table, uint32_t page_number)
{
    pt_entry_t *entry = pt_find_entry(page_table, page_number);
    if (entry)
        entry->dirty = false;
}

void pt_mark_present(page_table_t *page_table, uint32_t page_number)
{
    pt_entry_t *entry = pt_find_entry(page_table, page_number);
    if (entry)
        entry->present = true;
}

void pt_mark_absent(page_table_t *page_table, uint32_t page_number)
{
    pt_entry_t *entry = pt_find_entry(page_table, page_number);
    if (entry)
        entry->present = false;
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
        if (page_table->entries[i].dirty)
            dirty_entries[index++] = page_table->entries[i];

    return dirty_entries;
}
