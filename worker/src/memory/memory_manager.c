#include "memory_manager.h"
#include "../connections/storage.h"
#include <utils/logger.h>

static void mm_resize_entries(memory_manager_t *mm)
{
    if (mm->count < mm->capacity)
        return;

    uint32_t new_capacity = (mm->capacity == 0) ? 4 : mm->capacity * 2;
    file_tag_entry_t *new_entries = realloc(mm->entries, new_capacity * sizeof(file_tag_entry_t));
    if (!new_entries)
        return;

    mm->entries = new_entries;
    mm->capacity = new_capacity;
}

memory_manager_t *mm_create(size_t memory_size, size_t page_size, pt_replacement_t policy, int retardation_ms)
{
    if (memory_size == 0 || page_size == 0)
        return NULL;

    memory_manager_t *mm = calloc(1, sizeof(memory_manager_t));
    if (!mm)
        return NULL;

    mm->page_size = page_size;
    mm->policy = policy;
    mm->memory_retardation = retardation_ms;
    mm->capacity = 0;
    mm->count = 0;
    mm->entries = NULL;
    mm->storage_socket = -1;
    mm->worker_id = -1;

    mm->physical_memory = malloc(memory_size);
    if (!mm->physical_memory)
    {
        free(mm);
        return NULL;
    }

    // Inicializamos la tabla de frames
    mm->frame_table.frame_count = memory_size / page_size;
    mm->frame_table.frames = calloc(mm->frame_table.frame_count, sizeof(frame_t));
    if (!mm->frame_table.frames)
    {
        free(mm->physical_memory);
        free(mm);
        return NULL;
    }

    return mm;
}

void mm_set_storage_connection(memory_manager_t *mm, int storage_socket, int worker_id)
{
    if (!mm)
        return;

    mm->storage_socket = storage_socket;
    mm->worker_id = worker_id;
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
    free(mm->frame_table.frames);
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
            return entry->page_table;
    }
    return NULL;
}

page_table_t *mm_create_page_table(memory_manager_t *mm, char *file, char *tag)
{
    if (!mm || !file || !tag)
        return NULL;

    page_table_t *existing = mm_find_page_table(mm, file, tag);
    if (existing)
        return existing;

    mm_resize_entries(mm);
    file_tag_entry_t *entry = &mm->entries[mm->count++];

    entry->file = strdup(file);
    entry->tag = strdup(tag);
    entry->page_table = pt_create(1, mm->page_size);

    if (!entry->file || !entry->tag || !entry->page_table)
    {
        free(entry->file);
        free(entry->tag);
        if (entry->page_table)
            pt_destroy(entry->page_table);
        mm->count--;
        return NULL;
    }

    return entry->page_table;
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

int mm_handle_page_fault(memory_manager_t *mm, page_table_t *pt, char *file, char *tag, uint32_t page_number)
{
    if (!mm || !pt || !file || !tag || page_number >= pt->page_count)
        return -1;

    if (mm->storage_socket == -1 || mm->worker_id == -1)
        return -1;

    log_info(logger_get(), "Query %d: - Memoria Miss - File: %s - Tag: %s - Pagina: %d",
             mm->query_id, file, tag, page_number);

    int frame = mm_allocate_frame(mm);
    if (frame == -1)
        return -1;

    uint32_t block_number = page_number;

    void *data = NULL;
    size_t size = 0;
    int result = read_block_from_storage(mm->storage_socket, file, tag, block_number, &data, &size, mm->worker_id);

    if (result != 0 || !data)
    {
        mm_free_frame(mm, frame);
        return -1;
    }

    void *frame_addr = mm_get_frame_address(mm, frame);
    if (!frame_addr)
    {
        free(data);
        mm_free_frame(mm, frame);
        return -1;
    }

    memset(frame_addr, 0, mm->page_size);
    size_t copy_size = (size < mm->page_size) ? size : mm->page_size;
    memcpy(frame_addr, data, copy_size);

    free(data);

    if (pt_map(pt, page_number, frame) != 0)
    {
        mm_free_frame(mm, frame);
        return -1;
    }

    log_info(logger_get(),
             "## Query %d: Se asigna el Marco: %d a la Página: %d perteneciente al - File: %s - Tag: %s",
             mm->query_id, frame, page_number, file, tag);

    log_info(logger_get(),
             "## Query %d: - Memoria Add - File: %s - Tag: %s - Pagina: %d - Marco: %d",
             mm->query_id, file, tag, page_number, frame);

    return 0;
}

static int mm_access_memory(memory_manager_t *mm, page_table_t *pt, char *file, char *tag,
                            uint32_t base_address, void *buffer, size_t size,
                            bool write)
{
    if (!mm || !pt || !buffer || size == 0)
        return -1;

    uint32_t page_size = mm->page_size;
    uint32_t current_page = base_address / page_size;
    uint32_t offset = base_address % page_size;
    size_t remaining = size;
    uint8_t *ptr = buffer;

    while (remaining > 0)
    {
        if (current_page >= pt->page_count)
            return -1;

        usleep(mm->memory_retardation * 1000);  
        pt_entry_t *entry = &pt->entries[current_page];
        if (!entry->present)
        {
            // Intentar manejar el page fault
            if (mm_handle_page_fault(mm, pt, file, tag, current_page) != 0)
                return -1;
        }

        void *frame_addr = mm_get_frame_address(mm, entry->frame);
        size_t bytes_to_copy = page_size - offset;
        if (bytes_to_copy > remaining)
            bytes_to_copy = remaining;

        usleep(mm->memory_retardation * 1000);
        if (write)
            memcpy(frame_addr + offset, ptr, bytes_to_copy);
        else
            memcpy(ptr, frame_addr + offset, bytes_to_copy);

        if (write)
            pt_set_dirty(pt, current_page, true);

        ptr += bytes_to_copy;
        remaining -= bytes_to_copy;
        current_page++;
        offset = 0;
    }

    return 0;
}

int mm_write_to_memory(memory_manager_t *mm, page_table_t *pt, char *file, char *tag, uint32_t base_address, const void *data, size_t size)
{
    return mm_access_memory(mm, pt, file, tag, base_address, (void *)data, size, true);
}

int mm_read_from_memory(memory_manager_t *mm, page_table_t *pt, char *file, char *tag, uint32_t base_address, size_t size, void *out_buffer)
{
    return mm_access_memory(mm, pt, file, tag, base_address, out_buffer, size, false);
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

int mm_allocate_frame(memory_manager_t *mm)
{
    if (!mm)
        return -1;

    for (uint32_t i = 0; i < mm->frame_table.frame_count; i++)
    {
        if (!mm->frame_table.frames[i].used)
        {
            mm->frame_table.frames[i].used = true;
            return i;
        }
    }

    // TODO: aplicar algoritmo de reemplazo (LRU / CLOCK-M)
    return -1;
}

int mm_free_frame(memory_manager_t *mm, uint32_t frame)
{
    if (!mm || frame >= mm->frame_table.frame_count)
        return -1;
    mm->frame_table.frames[frame].used = false;
    return 0;
}

void *mm_get_frame_address(memory_manager_t *mm, uint32_t frame)
{
    if (!mm || frame >= mm->frame_table.frame_count)
        return NULL;
    return (uint8_t *)mm->physical_memory + (frame * mm->page_size);
}

void mm_mark_all_clean(memory_manager_t *mm, char *file, char *tag)
{
    if (!mm || !file || !tag)
        return;

    page_table_t *pt = mm_find_page_table(mm, file, tag);
    if (!pt)
        return;

    for (uint32_t i = 0; i < pt->page_count; i++)
    {
        pt_set_dirty(pt, i, false);
    }
}
