#include <query_interpreter/query_interpreter.h>
#include <cspecs/cspec.h>

context(test_query_interpreter)
{
    describe("Decodificación de instrucciones")
    {
        describe("Instrucción válida")
        {
            it("debería interpretar correctamente la instrucción CREATE")
            {
                char *raw_instruction = "CREATE ARCHIVO:TAG";
                instruction_t expected_instruction;
                expected_instruction.operation = CREATE;
                expected_instruction.file_tag.file = "ARCHIVO";
                expected_instruction.file_tag.tag = "TAG";

                instruction_t decoded_instruction;
                int result = decode_instruction(raw_instruction, &decoded_instruction);

                should_int(result) be equal to(0);
                should_int(decoded_instruction.operation) be equal to(expected_instruction.operation);
                should_string(decoded_instruction.file_tag.file) be equal to(expected_instruction.file_tag.file);
                should_string(decoded_instruction.file_tag.tag) be equal to(expected_instruction.file_tag.tag);
            } end
            it("debería interpretar correctamente la instrucción TRUNCATE")
            {
                char *raw_instruction = "TRUNCATE ARCHIVO:TAG 1024";
                instruction_t expected_instruction;
                expected_instruction.operation = TRUNCATE;
                expected_instruction.truncate.file = "ARCHIVO";
                expected_instruction.truncate.tag = "TAG";
                expected_instruction.truncate.size = 1024;

                instruction_t decoded_instruction;
                int result = decode_instruction(raw_instruction, &decoded_instruction);

                should_int(result) be equal to(0);
                should_int(decoded_instruction.operation) be equal to(expected_instruction.operation);
                should_string(decoded_instruction.truncate.file) be equal to(expected_instruction.truncate.file);
                should_string(decoded_instruction.truncate.tag) be equal to(expected_instruction.truncate.tag);
                should_int(decoded_instruction.truncate.size) be equal to(expected_instruction.truncate.size);
            } end
            it("debería interpretar correctamente la instrucción WRITE")
            {
                char *raw_instruction = "WRITE ARCHIVO:TAG 1 datos";
                instruction_t expected_instruction;

                expected_instruction.operation = WRITE;
                expected_instruction.write.file = "ARCHIVO";
                expected_instruction.write.tag = "TAG";
                expected_instruction.write.base = 1;

                instruction_t decoded_instruction;
                int result = decode_instruction(raw_instruction, &decoded_instruction);

                should_int(result) be equal to(0);
                should_int(decoded_instruction.operation) be equal to(expected_instruction.operation);
                should_string(decoded_instruction.write.file) be equal to(expected_instruction.write.file);
                should_string(decoded_instruction.write.tag) be equal to(expected_instruction.write.tag);
                should_int(decoded_instruction.write.base) be equal to(expected_instruction.write.base);
                should_string((char*)decoded_instruction.write.data) be equal to("datos");
            } end
            it("debería interpretar correctamente la instrucción READ")
            {
                char *raw_instruction = "READ ARCHIVO:TAG 10 512";
                instruction_t expected_instruction;
                expected_instruction.operation = READ;
                expected_instruction.read.file = "ARCHIVO";
                expected_instruction.read.tag = "TAG";
                expected_instruction.read.base = 10;
                expected_instruction.read.size = 512;

                instruction_t decoded_instruction;
                int result = decode_instruction(raw_instruction, &decoded_instruction);

                should_int(result) be equal to(0);
                should_int(decoded_instruction.operation) be equal to(expected_instruction.operation);
                should_string(decoded_instruction.read.file) be equal to(expected_instruction.read.file);
                should_string(decoded_instruction.read.tag) be equal to(expected_instruction.read.tag);
                should_int(decoded_instruction.read.base) be equal to(expected_instruction.read.base);
                should_int(decoded_instruction.read.size) be equal to(expected_instruction.read.size);
            } end
            it("debería interpretar correctamente la instrucción TAG")
            {
                char *raw_instruction = "TAG ORIGEN:TAG1 DESTINO:TAG2";
                instruction_t expected_instruction;
                expected_instruction.operation = TAG;
                expected_instruction.tag.file_src = "ORIGEN";
                expected_instruction.tag.tag_src = "TAG1";
                expected_instruction.tag.file_dst = "DESTINO";
                expected_instruction.tag.tag_dst = "TAG2";

                instruction_t decoded_instruction;
                int result = decode_instruction(raw_instruction, &decoded_instruction);

                should_int(result) be equal to(0);
                should_int(decoded_instruction.operation) be equal to(expected_instruction.operation);
                should_string(decoded_instruction.tag.file_src) be equal to(expected_instruction.tag.file_src);
                should_string(decoded_instruction.tag.tag_src) be equal to(expected_instruction.tag.tag_src);
                should_string(decoded_instruction.tag.file_dst) be equal to(expected_instruction.tag.file_dst);
                should_string(decoded_instruction.tag.tag_dst) be equal to(expected_instruction.tag.tag_dst);
            } end
            it("debería interpretar correctamente la instrucción COMMIT")
            {
                char *raw_instruction = "COMMIT ARCHIVO:TAG";
                instruction_t expected_instruction;
                expected_instruction.operation = COMMIT;
                expected_instruction.file_tag.file = "ARCHIVO";
                expected_instruction.file_tag.tag = "TAG";

                instruction_t decoded_instruction;
                int result = decode_instruction(raw_instruction, &decoded_instruction);

                should_int(result) be equal to(0);
                should_int(decoded_instruction.operation) be equal to(expected_instruction.operation);
                should_string(decoded_instruction.file_tag.file) be equal to(expected_instruction.file_tag.file);
                should_string(decoded_instruction.file_tag.tag) be equal to(expected_instruction.file_tag.tag);
            } end
            it("debería interpretar correctamente la instrucción FLUSH")
            {
                char *raw_instruction = "FLUSH ARCHIVO:TAG";
                instruction_t expected_instruction;
                expected_instruction.operation = FLUSH;
                expected_instruction.file_tag.file = "ARCHIVO";
                expected_instruction.file_tag.tag = "TAG";

                instruction_t decoded_instruction;
                int result = decode_instruction(raw_instruction, &decoded_instruction);

                should_int(result) be equal to(0);
                should_int(decoded_instruction.operation) be equal to(expected_instruction.operation);
                should_string(decoded_instruction.file_tag.file) be equal to(expected_instruction.file_tag.file);
                should_string(decoded_instruction.file_tag.tag) be equal to(expected_instruction.file_tag.tag);
            } end
            it("debería interpretar correctamente la instrucción DELETE")
            {
                char *raw_instruction = "DELETE ARCHIVO:TAG";
                instruction_t expected_instruction;
                expected_instruction.operation = DELETE;
                expected_instruction.file_tag.file = "ARCHIVO";
                expected_instruction.file_tag.tag = "TAG";

                instruction_t decoded_instruction;
                int result = decode_instruction(raw_instruction, &decoded_instruction);

                should_int(result) be equal to(0);
                should_int(decoded_instruction.operation) be equal to(expected_instruction.operation);
                should_string(decoded_instruction.file_tag.file) be equal to(expected_instruction.file_tag.file);
                should_string(decoded_instruction.file_tag.tag) be equal to(expected_instruction.file_tag.tag);
            } end
            it("debería interpretar correctamente la instrucción END")
            {
                char *raw_instruction = "END";
                instruction_t expected_instruction;
                expected_instruction.operation = END;

                instruction_t decoded_instruction;
                int result = decode_instruction(raw_instruction, &decoded_instruction);

                should_int(result) be equal to(0);
                should_int(decoded_instruction.operation) be equal to(expected_instruction.operation);
            } end
        } end
    } end
    
    describe("Obtención de instrucciones")
    {
        before {
            FILE *file = fopen("tests/resources/test_instructions.txt", "w");
            fprintf(file, "CREATE ARCHIVO1:TAG1\n");
            fprintf(file, "END\n");
            fclose(file);
        } end

        after {
            remove("tests/resources/test_instructions.txt");
        } end
        
        describe("Archivo valido")
        {
            it("debería obtener la primera instrucción correctamente")
            {
                char *raw_instruction;
                int result = fetch_instruction("tests/resources/test_instructions.txt", 0, &raw_instruction);

                should_int(result) be equal to(0);
                should_string(raw_instruction) be equal to("CREATE ARCHIVO1:TAG1");
                
                free(raw_instruction);
            } end
            
            it("debería obtener la segunda instrucción correctamente")
            {
                char *raw_instruction;
                int result = fetch_instruction("tests/resources/test_instructions.txt", 1, &raw_instruction);

                should_int(result) be equal to(0);
                should_string(raw_instruction) be equal to("END");
                
                free(raw_instruction);
            } end
        } end
        
        describe("Casos de error")
        {
            it("debería fallar con archivo inexistente")
            {
                char *raw_instruction;
                int result = fetch_instruction("archivo_inexistente.txt", 0, &raw_instruction);

                should_int(result) be equal to(-1);
            } end
            
            it("debería fallar con program_counter fuera de rango")
            {
                char *raw_instruction;
                int result = fetch_instruction("tests/resources/test_instructions.txt", 10, &raw_instruction);

                should_int(result) be equal to(-1);
            } end
        } end
    } end
}
