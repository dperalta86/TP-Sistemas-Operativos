#ifndef QUERY_INTERPRETER_H
#define QUERY_INTERPRETER_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <commons/string.h>
#include <connections/storage.h>
#include <connections/master.h>
#include <memory/memory_manager.h>

typedef enum {
    CREATE,
    TRUNCATE,
    WRITE,
    READ,
    TAG,
    COMMIT,
    FLUSH,
    DELETE,
    END,
    UNKNOWN
} operation_t;

typedef struct {
    char *file;
    char *tag;
} file_tag_t;

typedef struct {
    char *file;
    char *tag;
    size_t size;
} truncate_params_t;

typedef struct {
    char *file;
    char *tag;
    uint32_t base;
    char *data;
} write_params_t;

typedef struct {
    char *file;
    char *tag;
    uint32_t base;
    size_t size;
} read_params_t;

typedef struct {
    char *file_src;
    char *tag_src;
    char *file_dst;
    char *tag_dst;
} tag_params_t;


typedef struct {
    operation_t operation;
    union {
        file_tag_t file_tag;
        truncate_params_t truncate;
        write_params_t write;
        read_params_t read;
        tag_params_t tag;
    };
} instruction_t;

int fetch_instruction(char *instructions_path, uint32_t program_counter, char **raw_instruction);
int decode_instruction(char *raw_instruction, instruction_t *instruction);
void free_instruction(instruction_t *instruction);
int execute_instruction(instruction_t *instruction, int socket_storage, int socket_master, memory_manager_t *memory_manager, int query_id, int worker_id);

#endif
