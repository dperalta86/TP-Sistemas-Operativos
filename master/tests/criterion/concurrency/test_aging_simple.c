// tests/criterion/concurrency/test_aging_simple.c
#include <criterion/criterion.h>
#include <unistd.h>
#include <stdio.h>
#include "../../helpers/test_helpers.h"
#include "../../../src/init_master.h"
#include "../../../src/aging.h"

Test(aging_simple, thread_starts_and_stops, .timeout = 3) {
    printf("1. Creando master...\n");
    t_master *master = init_fake_master("PRIORITY", 500);
    
    printf("2. Iniciando aging thread...\n");
    master->running = true;
    int result = pthread_create(&master->aging_thread, NULL, aging_thread_func, master);
    cr_assert_eq(result, 0, "Thread creation should succeed");
    
    printf("3. Thread corriendo, esperando 100ms...\n");
    usleep(100 * 1000); // 100ms
    
    printf("4. Deteniendo thread...\n");
    master->running = false;
    
    printf("5. Esperando join...\n");
    pthread_join(master->aging_thread, NULL);
    
    printf("6. Thread finalizado, limpiando...\n");
    destroy_fake_master(master);
    
    printf("7. Test completado!\n");
}