#include <cspecs/cspec.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../src/utils/utils.h"

context(test_utils) {
    describe("get_stringified_array function") {
        it("convierte array vacío a string correctamente") {
            char* empty_array[] = {NULL};
            char* result = get_stringified_array(empty_array);

            should_ptr(result) not be null;
            should_string(result) be equal to("[]");

            free(result);
        } end

        it("convierte array con un elemento a string correctamente") {
            char* single_array[] = {"elemento1", NULL};
            char* result = get_stringified_array(single_array);

            should_ptr(result) not be null;
            should_string(result) be equal to("[elemento1]");

            free(result);
        } end

        it("convierte array con múltiples elementos a string correctamente") {
            char* multi_array[] = {"1", "2", "3", "4", NULL};
            char* result = get_stringified_array(multi_array);

            should_ptr(result) not be null;
            should_string(result) be equal to("[1,2,3,4]");

            free(result);
        } end
    } end
}
