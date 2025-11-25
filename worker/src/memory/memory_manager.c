#include "memory_manager.h"
#include "../connections/storage.h"
#include <utils/logger.h>
#include <stdatomic.h>

static _Atomic uint64_t global_timestamp = 0;

//--Helper--
bool mm_find_page_for_frame(
    memory_manager_t *mm,
    uint32_t frame_idx,
    file_tag_entry_t **out_entry,
    page_table_t **out_pt,
    uint32_t *out_page_idx)
{
    for (uint32_t i = 0; i < mm->count; i++)
    {
        file_tag_entry_t *entry = &mm->entries[i];
        page_table_t *pt = entry->page_table;

        for (uint32_t j = 0; j < pt->page_count; j++)
        {
            pt_entry_t *page = &pt->entries[j];
            if (page->present && page->frame == frame_idx)
            {
                if (out_entry)
                    *out_entry = entry;
                if (out_pt)
                    *out_pt = pt;
                if (out_page_idx)
                    *out_page_idx = j;
                return true;
            }
        }
    }
    return false;
}
//--Helper--

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
    mm->query_id = -1;
    mm->last_victim_file = NULL;
    mm->last_victim_tag = NULL;
    mm->last_victim_page = 0;
    mm->last_victim_valid = false;
    mm->frame_table.clock_pointer = 0;

    mm->physical_memory = malloc(memory_size);
    if (!mm->physical_memory)
    {
        free(mm);
        return NULL;
    }

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

void mm_set_query_id(memory_manager_t *mm, int query_id)
{
    if (!mm)
        return;

    mm->query_id = query_id;
}

void mm_destroy(memory_manager_t *mm)
{
    if (!mm)
        return;

    for (uint32_t i = 0; i < mm->count; i++)
    {
        file_tag_entry_t *entry = &mm->entries[i];
        if (entry->file)
            free(entry->file);
        if (entry->tag)
            free(entry->tag);
        if (entry->page_table)
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

    t_log *logger = logger_get();
    if (logger)
    {
        log_info(logger, "Query %d: - Memoria Miss - File: %s - Tag: %s - Pagina: %d",
                 mm->query_id, file, tag, page_number);
    }

    int frame = mm_allocate_frame(mm);
    if (frame == -1)
        return -1;

    mm->frame_table.frames[frame].used = true;

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

    mm_update_page_access(mm, pt, page_number);

    if (logger)
    {
        log_info(logger,
                 "## Query %d: Se asigna el Marco: %d a la Página: %d perteneciente al - File: %s - Tag: %s",
                 mm->query_id, frame, page_number, file, tag);

        log_info(logger,
                 "## Query %d: - Memoria Add - File: %s - Tag: %s - Pagina: %d - Marco: %d",
                 mm->query_id, file, tag, page_number, frame);
    }

    if (mm->last_victim_valid)
    {
        if (logger)
        {
            log_info(logger,
                     "## Query %d: Se reemplaza la página %s:%s/%d por la %s:%s/%d",
                     mm->query_id,
                     mm->last_victim_file,
                     mm->last_victim_tag,
                     mm->last_victim_page,
                     file,
                     tag,
                     page_number);
        }
        mm->last_victim_valid = false;
    }

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
            entry = &pt->entries[current_page]; // Releer entry para asegurar que el entry->frame que se usa es el que se acaba de mapear.
        }

        mm_update_page_access(mm, pt, current_page);

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

        //  -- Armado de valor para log obligatorio --
        char valor_ascii[65];
        size_t log_len = (bytes_to_copy < 64) ? bytes_to_copy : 64;

        if (write)
            memcpy(valor_ascii, ptr, log_len);
        else
            memcpy(valor_ascii, frame_addr + offset, log_len);

        valor_ascii[log_len] = '\0';

        for (size_t k = 0; k < log_len; k++)
        {
            if (valor_ascii[k] < 32 || valor_ascii[k] > 126)
                valor_ascii[k] = '.';
        }
        //  ------

        t_log *logger = logger_get();

        if (logger)
        {
            uint32_t direccion_fisica = entry->frame * page_size + offset;
            log_info(logger,
                     "Query %d: Acción: %s - Dirección Física: %u - Valor: %s",
                     mm->query_id,
                     write ? "ESCRIBIR" : "LEER",
                     direccion_fisica,
                     valor_ascii);
        }

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

    t_log *logger = logger_get();
    if (logger)
    {
        log_info(logger, "## Query %d: - Memoria Llena - No hay marcos disponibles (Frame Count: %d)",
                 mm->query_id, mm->frame_table.frame_count);
    }

    if (mm->policy == LRU)
    {
        int victim_frame = mm_find_lru_victim(mm);
        if (victim_frame != -1)
        {
            if (logger)
            {
                log_info(logger, "## Query %d: Frame %d liberado usando algoritmo LRU",
                         mm->query_id, victim_frame);
            }
            return victim_frame;
        }
    }
    else if (mm->policy == CLOCK_M)
    {
        int victim_frame = mm_find_clockm_victim(mm);
        if (victim_frame != -1)
        {
            if (logger)
            {
                log_info(logger,
                         "## Query %d: Frame %d liberado usando algoritmo CLOCK-M",
                         mm->query_id,
                         victim_frame);
            }

            return victim_frame;
        }
    }

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

int mm_flush_query(memory_manager_t *mm, char *file, char *tag)
{
    if (!mm || !file || !tag)
        return -1;

    if (mm->storage_socket == -1 || mm->worker_id == -1)
        return -1;

    size_t dirty_count = 0;
    pt_entry_t *dirty_pages = mm_get_dirty_pages(mm, file, tag, &dirty_count);
    if (!dirty_pages || dirty_count == 0)
    {
        if (dirty_pages)
            free(dirty_pages);
        return 0;
    }

    page_table_t *pt = mm_find_page_table(mm, file, tag);
    if (!pt)
    {
        free(dirty_pages);
        return -1;
    }

    t_log *logger = logger_get();

    for (size_t i = 0; i < dirty_count; i++)
    {
        pt_entry_t *p = &dirty_pages[i];

        if (!p->present)
        {
            continue;
        }

        void *frame_addr = mm_get_frame_address(mm, p->frame);
        if (!frame_addr)
        {
            if (logger)
            {
                log_error(logger,
                          "## Query %d: Error al obtener dirección de marco para flush - File: %s - Tag: %s - Pagina: %d",
                          mm->query_id, file, tag, p->page_number);
            }
            free(dirty_pages);
            return -1;
        }

        int write_res = write_block_to_storage(mm->storage_socket,
                                               file, tag, p->page_number,
                                               frame_addr, mm->page_size,
                                               mm->worker_id);
        if (write_res != 0)
        {
            if (logger)
            {
                log_error(logger,
                          "## Query %d: Error al escribir página sucia en Storage - File: %s - Tag: %s - Pagina: %d",
                          mm->query_id, file, tag, p->page_number);
            }
            free(dirty_pages);
            return -1;
        }

        if (logger)
        {
            log_info(logger,
                     "## Query %d: Página sucia escrita en Storage - File: %s - Tag: %s - Pagina: %d",
                     mm->query_id, file, tag, p->page_number);
        }

        pt_set_dirty(pt, p->page_number, false);
    }

    free(dirty_pages);
    return 0;
}

int mm_flush_all_dirty(memory_manager_t *mm)
{
    if (!mm)
        return -1;

    if (mm->storage_socket == -1 || mm->worker_id == -1)
        return -1;

    t_log *logger = logger_get();
    int total_flushed = 0;

    // Recorrer todas las entradas de File:Tag
    for (uint32_t i = 0; i < mm->count; i++)
    {
        file_tag_entry_t *entry = &mm->entries[i];
        char *file = entry->file;
        char *tag = entry->tag;

        size_t dirty_count = 0;
        pt_entry_t *dirty_pages = mm_get_dirty_pages(mm, file, tag, &dirty_count);
        
        if (!dirty_pages || dirty_count == 0)
        {
            if (dirty_pages)
                free(dirty_pages);
            continue;
        }

        page_table_t *pt = entry->page_table;
        if (!pt)
        {
            free(dirty_pages);
            continue;
        }

        if (logger)
        {
            log_info(logger,
                     "## Query %d: Flushing %zu página(s) sucia(s) de File: %s - Tag: %s",
                     mm->query_id, dirty_count, file, tag);
        }

        // Escribir cada página sucia a Storage
        for (size_t j = 0; j < dirty_count; j++)
        {
            pt_entry_t *p = &dirty_pages[j];

            if (!p->present)
                continue;

            void *frame_addr = mm_get_frame_address(mm, p->frame);
            if (!frame_addr)
            {
                if (logger)
                {
                    log_error(logger,
                              "## Query %d: Error al obtener dirección de marco para flush - File: %s - Tag: %s - Pagina: %d",
                              mm->query_id, file, tag, p->page_number);
                }
                free(dirty_pages);
                return -1;
            }

            int write_res = write_block_to_storage(mm->storage_socket,
                                                   file, tag, p->page_number,
                                                   frame_addr, mm->page_size,
                                                   mm->worker_id);
            if (write_res != 0)
            {
                if (logger)
                {
                    log_error(logger,
                              "## Query %d: Error al escribir página sucia en Storage - File: %s - Tag: %s - Pagina: %d",
                              mm->query_id, file, tag, p->page_number);
                }
                free(dirty_pages);
                return -1;
            }

            if (logger)
            {
                log_info(logger,
                         "## Query %d: Página sucia escrita en Storage - File: %s - Tag: %s - Pagina: %d",
                         mm->query_id, file, tag, p->page_number);
            }

            pt_set_dirty(pt, p->page_number, false);
            total_flushed++;
        }

        free(dirty_pages);
    }

    if (logger && total_flushed > 0)
    {
        log_info(logger,
                 "## Query %d: Flush completo - Total de páginas escritas: %d",
                 mm->query_id, total_flushed);
    }

    return 0;
}

void mm_update_page_access(memory_manager_t *mm, page_table_t *pt, uint32_t page_number)
{
    if (!mm || !pt)
        return;

    if (page_number >= pt->page_count)
        return;

    if (mm->policy == LRU)
    {
        uint64_t new_ts = atomic_fetch_add(&global_timestamp, 1) + 1;
        pt_update_access_time(pt, page_number, new_ts);
        return;
    }

    if (mm->policy == CLOCK_M)
    {
        pt->entries[page_number].use_bit = true;
        return;
    }
}

int mm_find_lru_victim(memory_manager_t *mm)
{
    if (!mm)
        return -1;

    uint64_t oldest_time = UINT64_MAX;
    uint32_t victim_frame = (uint32_t)-1;
    char *victim_file = NULL;
    char *victim_tag = NULL;
    uint32_t victim_page = (uint32_t)-1;
    page_table_t *victim_pt = NULL;
    t_log *logger = logger_get();

    for (uint32_t i = 0; i < mm->count; i++)
    {
        file_tag_entry_t *entry = &mm->entries[i];
        page_table_t *pt = entry->page_table;

        for (uint32_t j = 0; j < pt->page_count; j++)
        {
            pt_entry_t *page_entry = &pt->entries[j];
            if (page_entry->present && page_entry->last_access_time < oldest_time)
            {
                oldest_time = page_entry->last_access_time;
                victim_frame = page_entry->frame;
                victim_file = entry->file;
                victim_tag = entry->tag;
                victim_page = j;
            }
        }
    }

    if (victim_frame != (uint32_t)-1)
    {
        if (logger)
        {
            log_info(logger,
                     "## Query %d: Se libera el Marco: %d perteneciente al - File: %s - Tag: %s",
                     mm->query_id, victim_frame, victim_file, victim_tag);
        }

        victim_pt = mm_find_page_table(mm, victim_file, victim_tag);

        if (victim_pt && victim_pt->entries[victim_page].dirty)
        {

            if (logger)
            {
                log_info(logger,
                         "## Query %d: Página sucia siendo reemplazada - File: %s - Tag: %s - Pagina: %d",
                         mm->query_id, victim_file, victim_tag, victim_page);
            }

            void *frame_addr = mm_get_frame_address(mm, victim_frame);

            int write_result = write_block_to_storage(
                mm->storage_socket,
                victim_file,
                victim_tag,
                victim_page,
                frame_addr,
                mm->page_size,
                mm->worker_id);

            if (write_result != 0)
            {
                if (logger)
                {
                    log_error(logger,
                              "## Query %d: Error al escribir página sucia en Storage - File: %s - Tag: %s - Pagina: %d",
                              mm->query_id, victim_file, victim_tag, victim_page);
                }
                return -1;
            }
            else
            {
                if (logger)
                {
                    log_info(logger,
                             "## Query %d: Página sucia escrita en Storage - File: %s - Tag: %s - Pagina: %d",
                             mm->query_id, victim_file, victim_tag, victim_page);
                }
            }
        }

        if (victim_pt)
        {
            pt_unmap(victim_pt, victim_page);
        }

        mm->frame_table.frames[victim_frame].used = false;

        mm->last_victim_file = victim_file;
        mm->last_victim_tag = victim_tag;
        mm->last_victim_page = victim_page;
        mm->last_victim_valid = true;
    }

    return victim_frame;
}

int mm_find_clockm_victim(memory_manager_t *mm)
{
    if (!mm || mm->policy != CLOCK_M)
        return -1;

    uint32_t frame_count = mm->frame_table.frame_count;
    t_log *logger = logger_get();

    if (frame_count == 0)
        return -1;

    while (1)
    {

        /* -------------- PASADA 1: buscar (U=0, M=0) -------------- */
        for (uint32_t k = 0; k < frame_count; k++)
        {

            uint32_t idx = mm->frame_table.clock_pointer;

            // si el marco está libre, avanzar
            if (!mm->frame_table.frames[idx].used)
            {
                mm->frame_table.clock_pointer = (idx + 1) % frame_count;
                continue;
            }

            file_tag_entry_t *entry = NULL;
            page_table_t *pt = NULL;
            uint32_t page_idx = 0;

            bool found = mm_find_page_for_frame(mm, idx, &entry, &pt, &page_idx);
            if (!found)
            {
                mm->frame_table.clock_pointer = (idx + 1) % frame_count;
                continue;
            }

            pt_entry_t *page = &pt->entries[page_idx];

            // condición ideal de la 1ra pasada
            if (page->use_bit == false && page->dirty == false)
            {

                // desmapear y liberar marco
                pt_unmap(pt, page_idx);
                mm->frame_table.frames[idx].used = false;

                if (logger)
                {
                    log_info(logger,
                             "## Query %d: Se libera el Marco: %d perteneciente al - File: %s - Tag: %s",
                             mm->query_id, idx, entry->file, entry->tag);
                }

                mm->last_victim_file = entry->file;
                mm->last_victim_tag = entry->tag;
                mm->last_victim_page = page_idx;
                mm->last_victim_valid = true;

                mm->frame_table.clock_pointer = (idx + 1) % frame_count;
                return (int)idx;
            }

            mm->frame_table.clock_pointer = (idx + 1) % frame_count;
        }

        /* -------------- PASADA 2: buscar (U=0, M=1) y limpiar U=1 -------------- */
        int dirty_candidate_frame = -1;
        file_tag_entry_t *dirty_entry = NULL;
        page_table_t *dirty_pt = NULL;
        uint32_t dirty_page_idx = 0;

        for (uint32_t k = 0; k < frame_count; k++)
        {

            uint32_t idx = mm->frame_table.clock_pointer;

            if (!mm->frame_table.frames[idx].used)
            {
                mm->frame_table.clock_pointer = (idx + 1) % frame_count;
                continue;
            }

            file_tag_entry_t *entry = NULL;
            page_table_t *pt = NULL;
            uint32_t page_idx = 0;

            bool found = mm_find_page_for_frame(mm, idx, &entry, &pt, &page_idx);
            if (!found)
            {
                mm->frame_table.clock_pointer = (idx + 1) % frame_count;
                continue;
            }

            pt_entry_t *page = &pt->entries[page_idx];

            // buscamos el primero con U=0 y M=1
            if (page->use_bit == false && page->dirty == true && dirty_candidate_frame == -1)
            {
                dirty_candidate_frame = idx;
                dirty_entry = entry;
                dirty_pt = pt;
                dirty_page_idx = page_idx;
            }

            // en la segunda pasada, SI vemos U=1 lo limpiamos
            if (page->use_bit == true)
            {
                page->use_bit = false;
            }

            mm->frame_table.clock_pointer = (idx + 1) % frame_count;
        }

        // si en la segunda pasada encontramos uno modificado con U=0, lo usamos
        if (dirty_candidate_frame != -1)
        {

            void *frame_addr = mm_get_frame_address(mm, dirty_candidate_frame);

            int write_result = write_block_to_storage(
                mm->storage_socket,
                dirty_entry->file,
                dirty_entry->tag,
                dirty_page_idx,
                frame_addr,
                mm->page_size,
                mm->worker_id);

            if (write_result != 0)
            {
                if (logger)
                {
                    log_error(logger,
                              "## Query %d: Error al escribir página sucia en Storage - File: %s - Tag: %s - Pagina: %d",
                              mm->query_id, dirty_entry->file, dirty_entry->tag, dirty_page_idx);
                }
                return -1;
            }
            else
            {
                if (logger)
                {
                    log_info(logger,
                             "## Query %d: Página sucia escrita en Storage - File: %s - Tag: %s - Pagina: %d",
                             mm->query_id, dirty_entry->file, dirty_entry->tag, dirty_page_idx);
                }
            }

            pt_unmap(dirty_pt, dirty_page_idx);
            mm->frame_table.frames[dirty_candidate_frame].used = false;

            if (logger)
            {
                log_info(logger,
                         "## Query %d: Se libera el Marco: %d perteneciente al - File: %s - Tag: %s",
                         mm->query_id,
                         dirty_candidate_frame,
                         dirty_entry->file,
                         dirty_entry->tag);
            }

            mm->last_victim_file = dirty_entry->file;
            mm->last_victim_tag = dirty_entry->tag;
            mm->last_victim_page = dirty_page_idx;
            mm->last_victim_valid = true;

            mm->frame_table.clock_pointer = (dirty_candidate_frame + 1) % frame_count;
            return dirty_candidate_frame;
        }
    }

    return -1;
}
