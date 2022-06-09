#include <string>
#include <unordered_map>

// Operation codes for all supported instructions
std::unordered_map<std::string, unsigned int> instructionOperationCodes = std::unordered_map<std::string, unsigned int>({
    { "halt", 0 },
    { "iret", 1 },
    { "ret", 2 },
    { "int", 3 },
    { "call", 4 },
    { "jmp", 5 },
    { "jeq", 6 },
    { "jne", 7 },
    { "jgt", 8 },
    { "push", 9 },
    { "pop", 10 },
    { "xchg", 11 },
    { "mov", 12 },
    { "add", 13 },
    { "sub", 14 },
    { "mul", 15 },
    { "div", 16 },
    { "cmp", 17 },
    { "not", 18 },
    { "and", 19 },
    { "or", 20 },
    { "xor", 21 },
    { "test", 22 },
    { "shl", 23 },
    { "shr", 24 }
});

// Operation codes for all supported ways of addressing an operand
std::unordered_map<std::string, unsigned int> addressingOperationCodes = std::unordered_map<std::string, unsigned int>({
    { "immed", 0 },
    { "regdir", 1 },
    { "regind", 2 },
    { "regindoff", 3 },
    { "mem", 4 }
});