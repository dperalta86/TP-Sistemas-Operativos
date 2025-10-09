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
        } end
    } end
}
