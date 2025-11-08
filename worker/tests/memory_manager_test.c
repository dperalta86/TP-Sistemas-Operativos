#include <memory/memory_manager.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <cspecs/cspec.h>
#include <commons/log.h>


static int mm_test_write(memory_manager_t *mm, page_table_t *pt,
                         uint32_t base, const void *data, size_t size)
{
    return mm_write_to_memory(mm, pt, "TEST_FILE", "TEST_TAG",
                              base, data, size);
}

static int mm_test_read(memory_manager_t *mm, page_table_t *pt,
                        uint32_t base, size_t size, void *out)
{
    return mm_read_from_memory(mm, pt, "TEST_FILE", "TEST_TAG",
                               base, size, out);
}


context(memory_manager_tests) {
    describe("Crear administrador de memoria") {
        memory_manager_t *mm = NULL;
        after {
            mm_destroy(mm);
        } end
        it("debería crear un administrador de memoria con parámetros válidos") {
            size_t memory_size = 1024 * 1024;
            size_t page_size = 4096;
            pt_replacement_t policy = LRU;
            int retardation_ms = 100;

            memory_manager_t *mm = mm_create(memory_size, page_size, policy, retardation_ms);

            should_ptr(mm) not be equal to(NULL);
            should_int(mm->count) be equal to(0);
            should_int(mm->capacity) be equal to(0);
            should_ptr(mm->physical_memory) not be equal to(NULL);
            should_int(mm->page_size) be equal to(4096);
            should_int(mm->policy) be equal to(LRU);
            should_int(mm->memory_retardation) be equal to(100);
        } end
        it("debería retornar NULL al crear un administrador de memoria con tamaño 0") {
            memory_manager_t *mm = mm_create(0, 4096, LRU, 0);
            should_ptr(mm) be equal to(NULL);
        } end
        it("debería retornar NULL al crear un administrador de memoria con tamaño de página 0") {
            memory_manager_t *mm = mm_create(1024 * 1024, 0, LRU, 0);
            should_ptr(mm) be equal to(NULL);
        } end
    } end
    describe("Crear tabla de páginas") {
        memory_manager_t *mm = NULL;

        before {
            mm = mm_create(1024 * 1024, 4096, LRU, 0);
        } end

        after {
            mm_destroy(mm);
        } end

        it("debería crear una nueva tabla de páginas si no existe") {
            page_table_t *pt = mm_create_page_table(mm, "file1", "tag1");

            should_ptr(pt) not be equal to(NULL);
            should_int(mm->count) be equal to(1);
            should_ptr(mm->entries[0].file) not be equal to(NULL);
            should_ptr(mm->entries[0].tag) not be equal to(NULL);
            should_ptr(mm->entries[0].page_table) be equal to(pt);
            should_int(mm->count) be equal to(1);
        } end
        it("debería retornar la tabla de páginas existente si ya existe") {
            page_table_t *pt1 = mm_create_page_table(mm, "file1", "tag1");
            page_table_t *pt2 = mm_create_page_table(mm, "file1", "tag1");

            should_ptr(pt1) be equal to(pt2);
            should_int(mm->count) be equal to(1);
        } end
        it("debería resizear si no hay memoria suficiente para una nueva tabla de páginas") {
            int i = 0;
            char filename[32];
            char tag[32];
            
            while (mm->count < mm->capacity) {
                sprintf(filename, "file_%d", i);
                sprintf(tag, "tag_%d", i);
                mm_create_page_table(mm, filename, tag);
                i++;
            }
            page_table_t *pt = mm_create_page_table(mm, "file_overflow", "tag_overflow");

            file_tag_entry_t *last_entry = &mm->entries[mm->count - 1];
            should_ptr(pt) not be equal to(NULL);
            should_bool(mm_has_page_table(mm, "file_overflow", "tag_overflow")) be equal to(true);
            should_string(last_entry->file) be equal to("file_overflow");
            should_string(last_entry->tag) be equal to("tag_overflow");
        } end
    } end
    describe("Escribir en la memoria") {
        memory_manager_t *mm = NULL;
        before {
            mm = mm_create(1024 * 1024, 4096, LRU, 0);
        } end

        after {
            mm_destroy(mm);
        } end

        it("debería retornar error al intentar escribir en una tabla de páginas inexistente") {
            char data[100] = "prueba";

            int result = mm_test_write(mm, NULL, 0, data, sizeof(data));

            should_int(result) be equal to(-1);
        } end
        it("debería retornar error al intentar escribir datos nulos") {
            page_table_t *pt = mm_create_page_table(mm, "file1", "tag1");

            int result = mm_test_write(mm, pt, 0, NULL, 100);

            should_int(result) be equal to(-1);
        } end
        it("debería retornar error al intentar escribir tamaño 0") {
            page_table_t *pt = mm_create_page_table(mm, "file1", "tag1");
            char data[100] = "prueba";

            int result = mm_test_write(mm, pt, 0, data, 0);

            should_int(result) be equal to(-1);
        } end
        it("debería retornar error al intentar escribir fuera de los límites de la tabla de páginas") {
            page_table_t *pt = mm_create_page_table(mm, "file1", "tag1");
            char data[100] = "prueba";

            int result = mm_test_write(mm, pt, 4096 * pt->page_count, data, sizeof(data));

            should_int(result) be equal to(-1);
        } end
        it("debería escribir datos correctamente en la memoria") {
            page_table_t *pt = mm_create_page_table(mm, "file1", "tag1");
            char data[100] = "prueba";

            for (size_t i = 0; i < 1; i++) {
                pt_map(pt, i, i);
            }

            int result = mm_test_write(mm, pt, 0, data, strlen(data));

            should_int(result) be equal to(0);
            should_bool(pt->entries[0].dirty) be equal to(true);

            for (size_t i = 0; i < strlen(data); i++) {
                should_char(((char *)mm->physical_memory)[i]) be equal to(data[i]);
            }
        } end
    } end
    describe("Leer de la memoria") {
        memory_manager_t *mm = NULL;
        page_table_t *pt = NULL;
        char data[100] = "prueba";
        char buffer[100] = {0};

        before {
            mm = mm_create(1024 * 1024, 4096, LRU, 0);
            pt = mm_create_page_table(mm, "file1", "tag1");
            for (size_t i = 0; i < 1; i++) {
                pt_map(pt, i, i);
            }
            mm_test_write(mm, pt, 0, data, strlen(data));
        } end

        after {
            mm_destroy(mm);
        } end

        it("debería retornar error al intentar leer de una tabla de páginas inexistente") {
            int result = mm_test_read(mm, NULL, 0, sizeof(buffer), buffer);

            should_int(result) be equal to(-1);
        } end
        it("debería retornar error al intentar leer en un buffer nulo") {
            page_table_t *pt = mm_create_page_table(mm, "file1", "tag1");

            int result = mm_test_read(mm, pt, 0, 100, NULL);

            should_int(result) be equal to(-1);
        } end
        it("debería retornar error al intentar leer tamaño 0") {
            page_table_t *pt = mm_create_page_table(mm, "file1", "tag1");
            char buffer[100];

            int result = mm_test_read(mm, pt, 0, 0, buffer);

            should_int(result) be equal to(-1);
        } end
        it("debería retornar error al intentar leer fuera de los límites de la tabla de páginas") {
            page_table_t *pt = mm_create_page_table(mm, "file1", "tag1");
            char buffer[100];

            int result = mm_test_read(mm, pt, 4096 * pt->page_count, sizeof(buffer), buffer);

            should_int(result) be equal to(-1);
        } end
        it("debería leer datos correctamente de la memoria") {
            page_table_t *pt = mm_create_page_table(mm, "file1", "tag1");
            char data[100] = "prueba";
            char buffer[100] = {0};
            for (size_t i = 0; i < 1; i++) {
                pt_map(pt, i, i);
            }
            mm_test_write(mm, pt, 0, data, strlen(data));
            mm_test_read(mm, pt, 0, sizeof(buffer), buffer);
            should_string(buffer) be equal to(data);
        } end
    } end
    describe("Obtener páginas modificadas") {
        memory_manager_t *mm = NULL;
        page_table_t *pt = NULL;
        char data[100] = "prueba";
        size_t dirty_count = 0;

        before {
            mm = mm_create(1024 * 1024, 4096, LRU, 0);
            pt = mm_create_page_table(mm, "file1", "tag1");
            for (size_t i = 0; i < 1; i++) {
                pt_map(pt, i, i);
            }
            mm_test_write(mm, pt, 0, data, strlen(data));
        } end

        after {
            mm_destroy(mm);
        } end

        it("debería retornar NULL al intentar obtener páginas sucias de una tabla de páginas inexistente") {
            pt_entry_t *dirty_pages = mm_get_dirty_pages(mm, "nonexistent_file", "nonexistent_tag", &dirty_count);

            should_ptr(dirty_pages) be equal to(NULL);
            should_int(dirty_count) be equal to(0);
        } end
        it("debería retornar las páginas sucias correctamente") {
            pt_entry_t *dirty_pages = mm_get_dirty_pages(mm, "file1", "tag1", &dirty_count);

            should_ptr(dirty_pages) not be equal to(NULL);
            should_int(dirty_count) be equal to(1);
            should_int(dirty_pages[0].page_number) be equal to(0);
            should_bool(dirty_pages[0].dirty) be equal to(true);
        } end
    } end
    describe("Algoritmo de reemplazo LRU") {
        memory_manager_t *mm = NULL;

        before {
            mm = mm_create(4096 * 4, 1024, LRU, 0);
            for (int i = 0; i < 4; i++) {
                char file[16], tag[16];
                sprintf(file, "file_%d", i);
                sprintf(tag, "tag_%d", i);

                page_table_t *pt = mm_create_page_table(mm, file, tag);
                pt_map(pt, 0, i);

                mm->frame_table.frames[i].used = true;
                pt->entries[0].present = true;
                pt->entries[0].frame = i;
                pt->entries[0].last_access_time = i + 1;
            }
        } end

        after {
            mm_destroy(mm);
        } end

        it("debería seleccionar la página con menor last_access_time como víctima") {
            int victim_frame = mm_find_lru_victim(mm);
            should_int(victim_frame) be equal to(0);
        } end

        it("debería marcar el marco de la víctima como libre") {
            int victim_frame = mm_find_lru_victim(mm);
            should_bool(mm->frame_table.frames[victim_frame].used) be equal to(false);
        } end

    } end

    describe("Algoritmo de reemplazo CLOCK-M") {

        memory_manager_t *mm = NULL;

        before {
            mm = mm_create(4096 * 4, 4096, CLOCK_M, 0);

            for (int i = 0; i < 4; i++) {
                char file[16], tag[16];
                sprintf(file, "f%d", i);
                sprintf(tag, "t%d", i);

                page_table_t *pt = mm_create_page_table(mm, file, tag);
                pt_map(pt, 0, i);

                pt->entries[0].present = true;
                pt->entries[0].frame   = i;
                pt->entries[0].dirty   = false;
                // solo el 2 tiene U=0, el resto U=1
                pt->entries[0].use_bit = (i != 2);

                mm->frame_table.frames[i].used = true;
            }

            mm->frame_table.clock_pointer = 0;
        } end

        after {
            mm_destroy(mm);
        } end

        it("elige el primer frame con U=0 y M=0") {
            int victim = mm_find_clockm_victim(mm);
            should_int(victim) be equal to(2);
        } end

    } end
}   // cierra context(memory_manager_tests)


