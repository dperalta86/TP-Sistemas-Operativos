// tests/criterion/smoke/test_smoke.c
#include <criterion/criterion.h>

// Test más básico posible
Test(smoke, sanity_check) {
    cr_assert(1 == 1);
}

// Test con mensaje custom
Test(smoke, basic_equality) {
    int a = 5;
    int b = 5;
    cr_assert_eq(a, b, "5 should equal 5");
}

// Test de strings
Test(smoke, string_comparison) {
    char *str1 = "Hello";
    char *str2 = "Hello";
    cr_assert_str_eq(str1, str2);
}

// Test con punteros
Test(smoke, null_pointer) {
    int *ptr = NULL;
    cr_assert_null(ptr);
    
    int value = 42;
    ptr = &value;
    cr_assert_not_null(ptr);
    cr_assert_eq(*ptr, 42);
}

// Test que verifica comparaciones
Test(smoke, comparisons) {
    cr_assert_lt(5, 10);  // Less than
    cr_assert_gt(10, 5);  // Greater than
    cr_assert_leq(5, 5);  // Less or equal
    cr_assert_geq(5, 5);  // Greater or equal
}