#include "page_table.h"

static pt_entry_t *pt_get_entry(page_table_t *pt, uint32_t page_number)
{
    if (!pt || page_number >= pt->page_count)
        return NULL;
    return &pt->entries[page_number];
}

page_table_t *pt_create(uint32_t page_count, size_t page_size)
{
    if (page_count == 0 || page_size == 0)
        return NULL;

    page_table_t *pt = malloc(sizeof(page_table_t));
    if (!pt)
        return NULL;

    pt->entries = calloc(page_count, sizeof(pt_entry_t));
    if (!pt->entries)
    {
        free(pt);
        return NULL;
    }

    pt->page_count = page_count;
    pt->page_size = page_size;

    for (uint32_t i = 0; i < page_count; i++)
        pt->entries[i].page_number = i;

    return pt;
}

void pt_destroy(page_table_t *pt)
{
    if (!pt)
        return;
    free(pt->entries);
    free(pt);
}

int pt_map(page_table_t *pt, uint32_t page_number, uint32_t frame)
{
    pt_entry_t *entry = pt_get_entry(pt, page_number);
    if (!entry)
        return -1;

    entry->frame = frame;
    entry->present = true;
    entry->dirty = false;
    return 0;
}

int pt_unmap(page_table_t *pt, uint32_t page_number)
{
    pt_entry_t *entry = pt_get_entry(pt, page_number);
    if (!entry)
        return -1;

    entry->frame = 0;
    entry->present = false;
    entry->dirty = false;
    return 0;
}

void pt_set_dirty(page_table_t *pt, uint32_t page_number, bool dirty)
{
    pt_entry_t *entry = pt_get_entry(pt, page_number);
    if (entry)
        entry->dirty = dirty;
}

void pt_set_present(page_table_t *pt, uint32_t page_number, bool present)
{
    pt_entry_t *entry = pt_get_entry(pt, page_number);
    if (entry)
        entry->present = present;
}

pt_entry_t *pt_get_dirty_entries(page_table_t *pt, size_t *count)
{
    if (!pt || !count)
        return NULL;

    size_t dirty_count = 0;
    for (uint32_t i = 0; i < pt->page_count; i++)
        if (pt->entries[i].dirty)
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

    size_t idx = 0;
    for (uint32_t i = 0; i < pt->page_count; i++)
        if (pt->entries[i].dirty)
            dirty_entries[idx++] = pt->entries[i];

    return dirty_entries;
}
