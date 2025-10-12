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

/**
 * Mapea un número de página a un marco de la página.
 *
 * Estructura: page_table[page_number]->frame = frame
 *
 * @param page_table La tabla de páginas.
 * @param page_number El número de página a mapear.
 * @param frame El marco de la página al que mapear.
 * @return 0 si el mapeo fue exitoso, -1 si hubo un error
 */
int pt_map(page_table_t *page_table, uint32_t page_number, uint32_t frame);

/**
 * Desmapea un número de página.
 *
 * Estructura: page_table[page_number]->frame = 0
 *
 * @param page_table La tabla de páginas.
 * @param page_number El número de página a desmapear.
 * @return 0 si el desmapeo fue exitoso, -1 si hubo un error
 */
int pt_unmap(page_table_t *page_table, uint32_t page_number);

/**
 * Traduce una dirección virtual a una dirección física.
 *
 * @param page_table La tabla de páginas.
 * @param virtual_address La dirección virtual a traducir.
 * @param out_frame Puntero para almacenar el marco de la página resultante.
 * @param out_offset Puntero para almacenar el offset dentro del marco de la página.
 * @return 0 si la traducción fue exitosa, -1 si hubo un error (página no mapeada o fuera de rango)
 */
int pt_translate(page_table_t *page_table,
                 uintptr_t virtual_address,
                 uint32_t *out_frame,
                 size_t *out_offset);

void pt_mark_dirty(page_table_t *page_table, uint32_t page_number);
void pt_mark_clean(page_table_t *page_table, uint32_t page_number);
void pt_mark_present(page_table_t *page_table, uint32_t page_number);
void pt_mark_absent(page_table_t *page_table, uint32_t page_number);

pt_entry_t *pt_get_dirty_entries(page_table_t *page_table, size_t *count);

#endif
