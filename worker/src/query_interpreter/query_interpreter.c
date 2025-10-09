#include "query_interpreter.h"

static operation_t get_operation_type(const char *operation_str) {
    if (string_equals_ignore_case(operation_str, "CREATE")) {
        return CREATE;
    } else if (string_equals_ignore_case(operation_str, "TRUNCATE")) {
        return TRUNCATE;
    } else if (string_equals_ignore_case(operation_str, "WRITE")) {
        return WRITE;
    } else if (string_equals_ignore_case(operation_str, "READ")) {
        return READ;
    } else if (string_equals_ignore_case(operation_str, "TAG")) {
        return TAG;
    } else if (string_equals_ignore_case(operation_str, "COMMIT")) {
        return COMMIT;
    } else if (string_equals_ignore_case(operation_str, "FLUSH")) {
        return FLUSH;
    } else if (string_equals_ignore_case(operation_str, "DELETE")) {
        return DELETE;
    } else if (string_equals_ignore_case(operation_str, "END")) {
        return END;
    } else {
        return UNKNOWN;
    }
}

int decode_instruction(const char *raw_instruction, instruction_t *instruction) {

    char *operation_str = string_new();
    for (int i = 0; i < string_length(raw_instruction); i++) {
        if (raw_instruction[i] == ' ') {
            break;
        }
        char tmp[2] = { raw_instruction[i], '\0' };
        string_append(&operation_str, tmp); 
    }

    operation_t op_type = get_operation_type(operation_str);
    if (op_type == -1) {
        return -1;
    }

    switch(op_type) {
        case CREATE:
            instruction->operation = CREATE;
            instruction->file_tag.file = "ARCHIVO";
            instruction->file_tag.tag = "TAG";
            break;
        case TRUNCATE:
            instruction->operation = TRUNCATE;
            instruction->truncate.file = "ARCHIVO";
            instruction->truncate.tag = "TAG";
            instruction->truncate.size = 1024;
            break;
    }

    
    return 0;
}
