#include "query_interpreter.h"

static operation_t get_operation_type(char *operation_str) {
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

static char **get_file_tag(char *file_tag_str) {
    return string_split(file_tag_str, ":");
}

int fetch_instruction(char *instructions_path, uint32_t program_counter, char **raw_instruction) {
    FILE *file = fopen(instructions_path, "r");
    if (file == NULL) {
        return -1;
    }

    char line[256];
    uint32_t current_line = 0;

    while (fgets(line, sizeof(line), file)) {
        if (current_line == program_counter) {
            line[strcspn(line, "\n")] = 0;
            *raw_instruction = string_duplicate(line);
            fclose(file);
            return 0;
        }
        current_line++;
    }

    fclose(file);
    return -1;
}

int decode_instruction(char *raw_instruction, instruction_t *instruction) {
    char **instruction_splited = string_split(raw_instruction, " ");
    
    if (instruction_splited == NULL || instruction_splited[0] == NULL) {
        string_array_destroy(instruction_splited);
        return -1;
    }

    operation_t op_type = get_operation_type(instruction_splited[0]);
    
    if (op_type == UNKNOWN) {
        string_array_destroy(instruction_splited);
        return -1;
    }

    instruction->operation = op_type;

    switch(op_type) {
        case CREATE:
        case COMMIT:
        case FLUSH:
        case DELETE:
            if (instruction_splited[1] == NULL) {
                string_array_destroy(instruction_splited);
                return -1;
            }
            char **file_tag_simple = get_file_tag(instruction_splited[1]);
            if (file_tag_simple == NULL || file_tag_simple[0] == NULL || file_tag_simple[1] == NULL) {
                string_array_destroy(file_tag_simple);
                string_array_destroy(instruction_splited);
                return -1;
            }
            instruction->file_tag.file = string_duplicate(file_tag_simple[0]);
            instruction->file_tag.tag = string_duplicate(file_tag_simple[1]);
            string_array_destroy(file_tag_simple);
            break;
        case TRUNCATE:
            if (instruction_splited[1] == NULL || instruction_splited[2] == NULL) {
                string_array_destroy(instruction_splited);
                return -1;
            }
            char **file_tag_trunc = get_file_tag(instruction_splited[1]);
            if (file_tag_trunc == NULL || file_tag_trunc[0] == NULL || file_tag_trunc[1] == NULL) {
                string_array_destroy(file_tag_trunc);
                string_array_destroy(instruction_splited);
                return -1;
            }
            instruction->truncate.file = string_duplicate(file_tag_trunc[0]);
            instruction->truncate.tag = string_duplicate(file_tag_trunc[1]);
            instruction->truncate.size = (size_t)atoi(instruction_splited[2]);
            string_array_destroy(file_tag_trunc);
            break;

        case WRITE:
            if (instruction_splited[1] == NULL || instruction_splited[2] == NULL || instruction_splited[3] == NULL) {
                string_array_destroy(instruction_splited);
                return -1;
            }
            char **file_tag_write = get_file_tag(instruction_splited[1]);
            if (file_tag_write == NULL || file_tag_write[0] == NULL || file_tag_write[1] == NULL) {
                string_array_destroy(file_tag_write);
                string_array_destroy(instruction_splited);
                return -1;
            }
            instruction->write.file = string_duplicate(file_tag_write[0]);
            instruction->write.tag = string_duplicate(file_tag_write[1]);
            instruction->write.base = (uint32_t)atoi(instruction_splited[2]);
            instruction->write.data = string_duplicate(instruction_splited[3]);
            string_array_destroy(file_tag_write);
            break;

        case READ:
            if (instruction_splited[1] == NULL || instruction_splited[2] == NULL || instruction_splited[3] == NULL) {
                string_array_destroy(instruction_splited);
                return -1;
            }
            char **file_tag_read = get_file_tag(instruction_splited[1]);
            if (file_tag_read == NULL || file_tag_read[0] == NULL || file_tag_read[1] == NULL) {
                string_array_destroy(file_tag_read);
                string_array_destroy(instruction_splited);
                return -1;
            }
            instruction->read.file = string_duplicate(file_tag_read[0]);
            instruction->read.tag = string_duplicate(file_tag_read[1]);
            instruction->read.base = (uint32_t)atoi(instruction_splited[2]);
            instruction->read.size = (size_t)atoi(instruction_splited[3]);
            string_array_destroy(file_tag_read);
            break;

        case TAG:
            if (instruction_splited[1] == NULL || instruction_splited[2] == NULL) {
                string_array_destroy(instruction_splited);
                return -1;
            }
            char **file_tag_src = get_file_tag(instruction_splited[1]);
            char **file_tag_dst = get_file_tag(instruction_splited[2]);
            if (file_tag_src == NULL || file_tag_src[0] == NULL || file_tag_src[1] == NULL ||
                file_tag_dst == NULL || file_tag_dst[0] == NULL || file_tag_dst[1] == NULL) {
                string_array_destroy(file_tag_src);
                string_array_destroy(file_tag_dst);
                string_array_destroy(instruction_splited);
                return -1;
            }
            instruction->tag.file_src = string_duplicate(file_tag_src[0]);
            instruction->tag.tag_src = string_duplicate(file_tag_src[1]);
            instruction->tag.file_dst = string_duplicate(file_tag_dst[0]);
            instruction->tag.tag_dst = string_duplicate(file_tag_dst[1]);
            string_array_destroy(file_tag_src);
            string_array_destroy(file_tag_dst);
            break;
        case END:
            break;
        default:
            string_array_destroy(instruction_splited);
            return -1;
    }

    string_array_destroy(instruction_splited);
    return 0;
}

void free_instruction(instruction_t *instruction) {
    if (instruction == NULL) {
        return;
    }

    switch(instruction->operation) {
        case CREATE:
        case COMMIT:
        case FLUSH:
        case DELETE:
            free(instruction->file_tag.file);
            free(instruction->file_tag.tag);
            break;
        case TRUNCATE:
            free(instruction->truncate.file);
            free(instruction->truncate.tag);
            break;
        case WRITE:
            free(instruction->write.file);
            free(instruction->write.tag);
            free(instruction->write.data);
            break;
        case READ:
            free(instruction->read.file);
            free(instruction->read.tag);
            break;
        case TAG:
            free(instruction->tag.file_src);
            free(instruction->tag.tag_src);
            free(instruction->tag.file_dst);
            free(instruction->tag.tag_dst);
            break;
        case END:
        case UNKNOWN:
            break;
    }
}

int execute_instruction(instruction_t *instruction, int socket_storage, int socket_master, memory_manager_t *memory_manager, int query_id, int worker_id) {
    if (instruction == NULL || memory_manager == NULL) {
        return -1;
    }

    switch(instruction->operation) {
        case CREATE: {
            int result = create_file_in_storage(socket_storage, worker_id, instruction->file_tag.file, instruction->file_tag.tag);
            if (result != 0) {
                return -1;
            }
            break;
        }
        case TRUNCATE: {
            if (instruction->truncate.size % memory_manager->page_size != 0) {
                return -1;
            }
            int result = truncate_file_in_storage(socket_storage, instruction->truncate.file, instruction->truncate.tag, instruction->truncate.size, worker_id);
            if (result != 0) {
                return -1;
            }
            page_table_t *page_table = mm_find_page_table(memory_manager, instruction->truncate.file, instruction->truncate.tag);
            if (page_table != NULL) {
                uint32_t new_page_count = instruction->truncate.size / memory_manager->page_size;
                mm_resize_page_table(memory_manager, instruction->truncate.file, instruction->truncate.tag, new_page_count);
            }
            break;
        }
        case WRITE: {
            page_table_t *page_table = mm_create_page_table(memory_manager, instruction->write.file, instruction->write.tag);
            if (page_table == NULL) {
                return -1;
            }
            int result = mm_write_to_memory(memory_manager, page_table, instruction->write.file, instruction->write.tag, instruction->write.base, instruction->write.data, strlen((char*)instruction->write.data));
            if (result != 0) {
                return -1;
            }
            break;
        }
        case READ: {
            page_table_t *page_table = mm_find_page_table(memory_manager, instruction->read.file, instruction->read.tag);
            if (page_table == NULL) {
                page_table = mm_create_page_table(memory_manager, instruction->read.file, instruction->read.tag);
                if (page_table == NULL) {
                    return -1;
                }
            }
            uint8_t *buffer = malloc(instruction->read.size);
            if (buffer == NULL) {
                return -1;
            }
            int result = mm_read_from_memory(memory_manager, page_table, instruction->read.file, instruction->read.tag, instruction->read.base, instruction->read.size, buffer);
            if (result != 0) {
                free(buffer);
                return -1;
            }
            int send_result = send_read_content_to_master(socket_master, query_id, buffer, instruction->read.size, instruction->read.file, instruction->read.tag, worker_id);
            if (send_result != 0) {
                free(buffer);
                return -1;
            }
            free(buffer);
            break;
        }
        case TAG:
            fork_file_in_storage(socket_storage, instruction->tag.file_src, instruction->tag.tag_src, instruction->tag.file_dst, instruction->tag.tag_dst, worker_id);
            break;
        case COMMIT: {
            int flush_result = mm_flush_query(memory_manager, instruction->file_tag.file, instruction->file_tag.tag);
            if (flush_result != 0) {
                return -1;
            }
            int result = commit_file_in_storage(socket_storage, instruction->file_tag.file, instruction->file_tag.tag, worker_id);
            if (result != 0) {
                return -1;
            }
            break;
        }
        case FLUSH: {
            int result = mm_flush_query(memory_manager, instruction->file_tag.file, instruction->file_tag.tag);
            if (result != 0) {
                return -1;
            }
            break;
        }
        case DELETE: {
            if (mm_has_page_table(memory_manager, instruction->file_tag.file, instruction->file_tag.tag)) {
                mm_remove_page_table(memory_manager, instruction->file_tag.file, instruction->file_tag.tag);
            }
            int result = delete_file_in_storage(socket_storage, instruction->file_tag.file, instruction->file_tag.tag, worker_id);
            if (result != 0) {
                return -1;
            }
            break;
        }
        case END:
            end_query_in_master(socket_master, worker_id, query_id);
            break;
        default:
            return -1;
    }

    return 0;
}
