#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include "page_table.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum
{
    CLOCK_M,
    LRU
} pt_replacement_t;

typedef struct
{
    bool used;
} frame_t;

typedef struct
{
    frame_t *frames;
    uint32_t frame_count;
    uint32_t clock_pointer;  // Para el algoritmo CLOCK
} frame_table_t;

typedef struct
{
    char *file;
    char *tag;
    page_table_t *page_table;
} file_tag_entry_t;

typedef struct
{
    file_tag_entry_t *entries;
    uint32_t count;
    uint32_t capacity;
    size_t page_size;
    pt_replacement_t policy;
    void *physical_memory;
    int memory_retardation;
    int storage_socket;
    int worker_id;
    int query_id;
    int master_socket;
    frame_table_t frame_table;
    char *last_victim_file;
    char *last_victim_tag;
    uint32_t last_victim_page;
    bool last_victim_valid;

} memory_manager_t;

memory_manager_t *mm_create(size_t memory_size, size_t page_size, pt_replacement_t policy, int retardation_ms);
void mm_destroy(memory_manager_t *mm);
void mm_set_storage_connection(memory_manager_t *mm, int storage_socket, int worker_id);
void mm_set_master_connection(memory_manager_t *mm, int master_socket);
void mm_set_query_id(memory_manager_t *mm, int query_id);

page_table_t *mm_find_page_table(memory_manager_t *mm, char *file, char *tag);
page_table_t *mm_create_page_table(memory_manager_t *mm, char *file, char *tag);
void mm_remove_page_table(memory_manager_t *mm, char *file, char *tag);
int mm_resize_page_table(memory_manager_t *mm, char *file, char *tag, uint32_t new_page_count);

int mm_write_to_memory(memory_manager_t *mm, page_table_t *pt, char *file, char *tag, uint32_t base_address, const void *data, size_t size);
int mm_read_from_memory(memory_manager_t *mm, page_table_t *pt, char *file, char *tag, uint32_t base_address, size_t size, void *out_buffer);

int mm_allocate_frame(memory_manager_t *mm);
int mm_free_frame(memory_manager_t *mm, uint32_t frame);
void *mm_get_frame_address(memory_manager_t *mm, uint32_t frame);

pt_entry_t *mm_get_dirty_pages(memory_manager_t *mm, char *file, char *tag, size_t *count);
bool mm_has_page_table(memory_manager_t *mm, char *file, char *tag);
void mm_mark_all_clean(memory_manager_t *mm, char *file, char *tag);
int mm_flush_query(memory_manager_t *mm, char *file, char *tag);
int mm_flush_all_dirty(memory_manager_t *mm);
int mm_handle_page_fault(memory_manager_t *mm, page_table_t *pt, char *file, char *tag, uint32_t page_number);

int mm_find_lru_victim(memory_manager_t *mm);
void mm_update_page_access(memory_manager_t *mm, page_table_t *pt, uint32_t page_number);

int mm_find_lru_victim(memory_manager_t *mm);
int mm_find_clockm_victim(memory_manager_t *mm);   
bool mm_find_page_for_frame(memory_manager_t *mm, uint32_t frame_idx, file_tag_entry_t **out_entry, page_table_t **out_pt, uint32_t *out_page_idx );

#endif
