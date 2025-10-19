#include "memory_manager.h"

memory_manager_t *mm_create(size_t memory_size, size_t page_size, pt_replacement_t policy)
{
    memory_manager_t *mm = malloc(sizeof(memory_manager_t));
    if (!mm)
        return NULL;

    mm->entries = NULL;
    mm->count = 0;
    mm->capacity = 0;
    mm->page_size = page_size;
    mm->policy = policy;
    mm->physical_memory = malloc(memory_size);

    return mm;
}

void mm_destroy(memory_manager_t *mm)
{
    if (!mm)
        return;

    for (uint32_t i = 0; i < mm->count; i++)
    {
        file_tag_entry_t *entry = &mm->entries[i];
        free(entry->file);
        free(entry->tag);
        pt_destroy(entry->page_table);
    }
    free(mm->entries);
    free(mm->physical_memory);
    free(mm);
}

page_table_t *mm_find_page_table(memory_manager_t *mm, char *file, char *tag)
{
    if (!mm || !file || !tag)
        return NULL;

    for (uint32_t i = 0; i < mm->count; i++)
    {
        file_tag_entry_t *entry = &mm->entries[i];
        if (strcmp(entry->file, file) == 0 && strcmp(entry->tag, tag) == 0)
        {
            return entry->page_table;
        }
    }
    return NULL;
}

page_table_t *mm_get_or_create_page_table(memory_manager_t *mm, char *file, char *tag)
{
    if (!mm || !file || !tag)
        return NULL;

    page_table_t *page_table = mm_find_page_table(mm, file, tag);
    if (page_table)
        return page_table;

    if (mm->count == mm->capacity)
    {
        uint32_t new_capacity = mm->capacity == 0 ? 4 : mm->capacity * 2;
        file_tag_entry_t *new_entries = realloc(mm->entries, new_capacity * sizeof(file_tag_entry_t));
        if (!new_entries)
            return NULL;
        mm->entries = new_entries;
        mm->capacity = new_capacity;
    }

    file_tag_entry_t *new_entry = &mm->entries[mm->count++];
    new_entry->file = strdup(file);
    new_entry->tag = strdup(tag);
    new_entry->page_table = pt_create(1, mm->page_size);
    if (!new_entry->file || !new_entry->tag || !new_entry->page_table)
    {
        free(new_entry->file);
        free(new_entry->tag);
        free(new_entry->page_table);
        mm->count--;
        return NULL;
    }

    return new_entry->page_table;
}

void mm_remove_page_table(memory_manager_t *mm, char *file, char *tag)
{
    if (!mm || !file || !tag)
        return;

    for (uint32_t i = 0; i < mm->count; i++)
    {
        file_tag_entry_t *entry = &mm->entries[i];
        if (strcmp(entry->file, file) == 0 && strcmp(entry->tag, tag) == 0)
        {
            free(entry->file);
            free(entry->tag);
            pt_destroy(entry->page_table);

            mm->entries[i] = mm->entries[mm->count - 1];
            mm->count--;
            return;
        }
    }
}

bool mm_has_page_table(memory_manager_t *mm, char *file, char *tag)
{
    return mm_find_page_table(mm, file, tag) != NULL;
}

int mm_write_to_memory(memory_manager_t *mm,
                       page_table_t *page_table,
                       uint32_t base_address,
                       void *data,
                       size_t size)
{
    uint32_t page_size = mm->page_size;
    uint32_t current_page = base_address / page_size;
    uint32_t offset = base_address % page_size;
    size_t bytes_remaining = size;
    const uint8_t *data_ptr = data;

    while (bytes_remaining > 0)
    {
        if (current_page >= page_table->page_count)
            return -1;

        pt_entry_t *entry = &page_table->entries[current_page];

        if (!entry->present)
        {
            // TODO: Manejar page fault
            return -1;
        }

        void *frame_addr = mm->physical_memory + (entry->frame * page_size);

        size_t bytes_to_copy = page_size - offset;
        if (bytes_to_copy > bytes_remaining)
            bytes_to_copy = bytes_remaining;

        memcpy(frame_addr + offset, data_ptr, bytes_to_copy);

        pt_mark_dirty(page_table, current_page);

        data_ptr += bytes_to_copy;
        bytes_remaining -= bytes_to_copy;
        current_page++;
        offset = 0;
    }

    return 0;
}

int mm_read_from_memory(memory_manager_t *mm,
                        page_table_t *page_table,
                        uint32_t base_address,
                        size_t size,
                        void *out_buffer)
{
    uint32_t page_size = mm->page_size;
    uint32_t current_page = base_address / page_size;
    uint32_t offset = base_address % page_size;
    size_t bytes_remaining = size;
    uint8_t *out_ptr = out_buffer;

    while (bytes_remaining > 0)
    {
        if (current_page >= page_table->page_count)
            return -1;

        pt_entry_t *entry = &page_table->entries[current_page];

        if (!entry->present)
        {
            // TODO: Manejar page fault
            return -1;
        }

        void *frame_addr = mm->physical_memory + (entry->frame * page_size);

        size_t bytes_to_copy = page_size - offset;
        if (bytes_to_copy > bytes_remaining)
            bytes_to_copy = bytes_remaining;

        memcpy(out_ptr, frame_addr + offset, bytes_to_copy);

        out_ptr += bytes_to_copy;
        bytes_remaining -= bytes_to_copy;
        current_page++;
        offset = 0;
    }

    return 0;
}

pt_entry_t *mm_get_dirty_pages(memory_manager_t *mm, char *file, char *tag, size_t *count)
{
    if (!mm || !file || !tag || !count)
        return NULL;

    page_table_t *pt = mm_find_page_table(mm, file, tag);
    if (!pt)
    {
        *count = 0;
        return NULL;
    }

    return pt_get_dirty_entries(pt, count);
}

void mm_mark_all_clean(memory_manager_t *mm, char *file, char *tag)
{
    if (!mm || !file || !tag)
        return;

    page_table_t *pt = mm_find_page_table(mm, file, tag);
    for (uint32_t i = 0; pt && i < pt->page_count; i++)
    {
        pt_mark_clean(pt, i);
    }
}
