#include <memory/page_table.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cspecs/cspec.h>

context(tests_page_table) {
    describe("Creacion de tabla de paginas") {
        it("Deberia crear una tabla de paginas con 16 entradas y tamaño de pagina 256 bytes") {
            size_t page_size = 256;
            uint32_t page_count = 16;

            page_table_t *pt = pt_create(page_count, page_size);

            should_int(pt->page_count) be equal to(page_count);
            should_int(pt->page_size) be equal to(page_size);
            for (uint32_t i = 0; i < page_count; i++) {
                should_int(pt->entries[i].frame) be equal to(0);
                should_bool(pt->entries[i].dirty) be equal to(false);
                should_bool(pt->entries[i].present) be equal to(false);
            }
            pt_destroy(pt);
        } end
        it("Deberia retornar NULL al intentar crear una tabla de paginas con 0 entradas") {
            size_t page_size = 256;
            uint32_t page_count = 0;

            page_table_t *pt = pt_create(page_count, page_size);

            should_ptr(pt) be equal to(NULL);
            pt_destroy(pt);
        } end
    } end
    describe("Marcar entrada como modificada") {
        it("Deberia marcar la entrada 5 como modificada") {
            size_t page_size = 256;
            uint32_t page_count = 16;
            page_table_t *pt = pt_create(page_count, page_size);

            pt_set_dirty(pt, 5, true);

            should_bool(pt->entries[5].dirty) be equal to(true);
            pt_destroy(pt);
        } end
        it("No deberia marcar ninguna entrada si el indice es invalido") {
            size_t page_size = 256;
            uint32_t page_count = 16;
            uint32_t invalid_index = 20;
            page_table_t *pt = pt_create(page_count, page_size);

            pt_set_dirty(pt, invalid_index, true);

            for (uint32_t i = 0; i < page_count; i++) {
                should_bool(pt->entries[i].dirty) be equal to(false);
            }
            pt_destroy(pt);
        } end
    } end
    describe("Marcar entrada como no modificada") {
        it("Deberia marcar la entrada 5 como no modificada") {
            size_t page_size = 256;
            uint32_t page_count = 16;
            page_table_t *pt = pt_create(page_count, page_size);
            pt_set_dirty(pt, 5, true);

            pt_set_dirty(pt, 5, false);

            should_bool(pt->entries[5].dirty) be equal to(false);
            pt_destroy(pt);
        } end
        it("No deberia cambiar ninguna entrada si el indice es invalido") {
            size_t page_size = 256;
            uint32_t page_count = 16;
            uint32_t invalid_index = 20;
            page_table_t *pt = pt_create(page_count, page_size);
            pt_set_dirty(pt, 5, true);

            pt_set_dirty(pt, invalid_index, false);

            should_bool(pt->entries[5].dirty) be equal to(true);
            pt_destroy(pt);
        } end
    } end
    describe("Obtener entradas modificadas") {
        it("Deberia obtener las entradas modificadas") {
            size_t page_size = 256;
            uint32_t page_count = 16;
            page_table_t *pt = pt_create(page_count, page_size);
            pt_set_dirty(pt, 2, true);
            pt_set_dirty(pt, 5, true);
            pt_set_dirty(pt, 7, true);

            size_t dirty_count = 0;
            pt_entry_t *dirty_entries = pt_get_dirty_entries(pt, &dirty_count);

            should_int(dirty_count) be equal to(3);
            for (size_t i = 0; i < dirty_count; i++) {
                should_bool(dirty_entries[i].dirty) be equal to(true);
            }
            free(dirty_entries);
            pt_destroy(pt);
        } end
        it("Deberia retornar count 0 y NULL si no hay entradas modificadas") {
            size_t page_size = 256;
            uint32_t page_count = 16;
            page_table_t *pt = pt_create(page_count, page_size);
            size_t dirty_count = 0;

            pt_entry_t *dirty_entries = pt_get_dirty_entries(pt, &dirty_count);
            
            should_int(dirty_count) be equal to(0);
            should_ptr(dirty_entries) be equal to(NULL);
            pt_destroy(pt);
        } end
    } end
}
