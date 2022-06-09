#include <regex>
#include <string>

const std::string LABEL_REGEXP(R"(^\s*([a-zA-Z]\w*):[ \t]*(.*))");
const std::string GLOBAL_REGEXP(R"(^\s*\.global[ \t]+((,[a-zA-Z]\w*[ \t]*|[a-zA-Z]\w*[ \t]*,[ \t]*)*[a-zA-Z]\w*)[ \t]*$)");
const std::string EXTERN_REGEXP(R"(^\s*\.extern[ \t]+((,[a-zA-Z]\w*[ \t]*|[a-zA-Z]\w*[ \t]*,[ \t]*)*[a-zA-Z]\w*)[ \t]*$)");
const std::string SECTION_REGEXP(R"(^\s*\.section[ \t]+([a-zA-Z]\w*):[ \t]*$)");
const std::string BYTE_REGEXP(R"(^\s*\.byte[ \t]+((,[a-zA-Z]\w*[ \t]*|[a-zA-Z]\w*[ \t]*,[ \t]*|,[1-9][0-9]*[ \t]*|[1-9][0-9]*[ \t]*,[ \t]*|,0[ \t]*|0[ \t]*,[ \t]*|,0x[0-9]+[ \t]*|0x[0-9]+[ \t]*,[ \t]*|,0x[a-fA-F]+[ \t]*|0x[a-fA-F]+[ \t]*,[ \t]*)*([a-zA-Z]\w*|[1-9][0-9]*|0|0x[0-9]+|0x[a-fA-F]+))[ \t]*$)");
const std::string WORD_REGEXP(R"(^\s*\.word[ \t]+((,[a-zA-Z]\w*[ \t]*|[a-zA-Z]\w*[ \t]*,[ \t]*|,[1-9][0-9]*[ \t]*|[1-9][0-9]*[ \t]*,[ \t]*|,0[ \t]*|0[ \t]*,[ \t]*|,0x[0-9]+[ \t]*|0x[0-9]+[ \t]*,[ \t]*|,0x[a-fA-F]+[ \t]*|0x[a-fA-F]+[ \t]*,[ \t]*)*([a-zA-Z]\w*|[1-9][0-9]*|0|0x[0-9]+|0x[a-fA-F]+))[ \t]*$)");
const std::string SKIP_REGEXP(R"(^\s*\.skip[ \t]+([1-9][0-9]*|0|0x[0-9]+|0x[a-fA-F]+){1}[ \t]*$)");
const std::string EQU_REGEXP(R"(^\s*\.equ[ \t]+([a-zA-Z]\w*){1}[ \t]*,[ \t]*(([a-zA-Z]\w*[ \t]*\+[ \t]*|\+[a-zA-Z]\w*[ \t]*|[a-zA-Z]\w*[ \t]*-[ \t]*|-[a-zA-Z]\w*[ \t]*|[1-9][0-9]*[ \t]*\+[ \t]*|\+[1-9][0-9]*[ \t]*|[1-9][0-9]*[ \t]*-[ \t]*|-[1-9][0-9]*\w*[ \t]*|0[ \t]*\+[ \t]*|\+0[ \t]*|0[ \t]*-[ \t]*|-0[ \t]*|0x[0-9]+[ \t]*\+[ \t]*|\+0x[0-9]+[ \t]*|0x[0-9]+[ \t]*-[ \t]*|-0x[0-9]+[ \t]*|0x[a-fA-F]+[ \t]*\+[ \t]*|\+0x[a-fA-F]+[ \t]*|0x[a-fA-F]+[ \t]*-[ \t]*|-0x[a-fA-F]+[ \t]*)*([a-zA-Z]\w*|[1-9][0-9]*|0x[0-9]+|0x[a-fA-F]+|0))[ \t]*$)");
const std::string LIST_REGEXP(R"(([a-zA-Z]\w*|[1-9][0-9]*|0x[0-9]|0x[a-fA-F]|0)[,]?)"); // Used to separate symbols and literals within a list of symbols and literals
const std::string EXPRESSION_REGEXP(R"(([a-zA-Z]\w*|[1-9][0-9]*|0x[0-9]|0x[a-fA-F]|0)([\+-])?)"); // Used to separate symbols and literals within the expression
const std::string NOADDR_INSTRUCTION_REGEXP(R"(^\s*(halt|iret|ret)[ \t]*$)");
const std::string BRANCH_INSTRUCTION_REGEXP(R"(^\s*(int|call|jmp|jeq|jne|jgt)[ \t]+((\*?([1-9][0-9]*|0x[0-9]+|0x[a-fA-F]+|0))|(\*?[a-zA-Z]\w*)|\*%r[0-7]|\*\(%r[0-7]\)|\*([1-9][0-9]*|0x[0-9]+|0x[a-fA-F]+|0)\(%r[0-7]\)|\*[a-zA-Z]\w*\((%r[0-7]|%pc\/%r7)\)){1}[ \t]*$)");
const std::string ONEADDR_INSTRUCTION_REGEXP(R"(^\s*(push|pop)[ \t]+((\$?([1-9][0-9]*|0x[0-9]+|0x[a-fA-F]+|0))|(\$?[a-zA-Z]\w*)|%r[0-7]|\(%r[0-7]\)|([1-9][0-9]*|0x[0-9]+|0x[a-fA-F]+|0)\(%r[0-7]\)|[a-zA-Z]\w*\((%r[0-7]|%pc\/%r7)\)){1}[ \t]*$)");
const std::string TWOADDR_INSTRUCTION_REGEXP(R"(^\s*(xchg|mov|add|sub|mul|div|cmp|not|and|or|xor|test|shl|shr)[ \t]+((\$?([1-9][0-9]*|0x[0-9]+|0x[a-fA-F]+|0))|(\$?[a-zA-Z]\w*)|%r[0-7]|\(%r[0-7]\)|([1-9][0-9]*|0x[0-9]+|0x[a-fA-F]+|0)\(%r[0-7]\)|[a-zA-Z]\w*\((%r[0-7]|%pc\/%r7)\)){1}[ \t]*,[ \t]*((\$?([1-9][0-9]*|0x[0-9]+|0x[a-fA-F]+|0))|(\$?[a-zA-Z]\w*)|%r[0-7]|\(%r[0-7]\)|([1-9][0-9]*|0x[0-9]+|0x[a-fA-F]+|0)\(%r[0-7]\)|[a-zA-Z]\w*\((%r[0-7]|%pc\/%r7)\)){1}[ \t]*$)");
const std::string LITERAL_REGEXP(R"(^\s*(\$|\*)?([1-9][0-9]*|0x[0-9]+|0x[a-fA-F]+|0)[ \t]*$)");
const std::string SYMBOL_REGEXP(R"(^\s*(\*|\$)?([a-zA-Z]\w*)[ \t]*$)");
const std::string REGISTER_REGEXP(R"(^\s*(\*)?(%r[0-7]|\(%r[0-7]\)))");
const std::string LITREG_REGEXP(R"(^\s*(\*)?^\s*(\$|\*)?([1-9][0-9]*|0x[0-9]+|0x[a-fA-F]+|0)\(%r([0-7])\))");
const std::string SYMREG_REGEXP(R"(^\s*(\*)?^\s*(\$|\*)?([a-zA-Z]\w*)\((%r[0-7]|%pc\/%r7)\))");
const std::string HEX_REGEXP(R"(0x.*)");
const std::string DEC_REGEXP(R"(0|[1-9][0-9]*)");

const std::regex LABEL_REGEX(LABEL_REGEXP);
const std::regex GLOBAL_REGEX(GLOBAL_REGEXP);
const std::regex EXTERN_REGEX(EXTERN_REGEXP);
const std::regex SECTION_REGEX(SECTION_REGEXP);
const std::regex BYTE_REGEX(BYTE_REGEXP);
const std::regex WORD_REGEX(WORD_REGEXP);
const std::regex SKIP_REGEX(SKIP_REGEXP);
const std::regex EQU_REGEX(EQU_REGEXP);
const std::regex LIST_REGEX(LIST_REGEXP);
const std::regex EXPRESSION_REGEX(EXPRESSION_REGEXP);
const std::regex NOADDR_INSTURCTION_REGEX(NOADDR_INSTRUCTION_REGEXP);
const std::regex BRANCH_INSTRUCTION_REGEX(BRANCH_INSTRUCTION_REGEXP);
const std::regex ONEADDR_INSTRUCTION_REGEX(ONEADDR_INSTRUCTION_REGEXP);
const std::regex TWOADDR_INSTRUCTION_REGEX(TWOADDR_INSTRUCTION_REGEXP);
const std::regex LITERAL_REGEX(LITERAL_REGEXP);
const std::regex SYMBOL_REGEX(SYMBOL_REGEXP);
const std::regex REGISTER_REGEX(REGISTER_REGEXP);
const std::regex LITREG_REGEX(LITREG_REGEXP);
const std::regex SYMREG_REGEX(SYMREG_REGEXP);
const std::regex HEX_REGEX(HEX_REGEXP);
const std::regex DEC_REGEX(DEC_REGEXP);