#include "opcodes.h"
#include "regexes.h"
#include "symtabentry.h"
#include "reltabentry.h"
#include "equtabentry.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>

int currentSectionNumber = -1; // -1 for a section number means no section is currently being processed
// Location counter is being reset back to 0, for each new section
unsigned int locationCounter = 0;
// One section can be split into multiple .section directives; therefore, when continuing one section, location counter must not be reset back to 0
// sectionLocationCounters stores location counters for all processed sections, so they can be restored if one of those sections continues
std::unordered_map<int, unsigned int> sectionLocationCounters;
// Symbol table
std::vector<SymbolTableEntry> symbolTable;
// Relocation table - one for each section
std::unordered_map<int, std::vector<RelocationTableEntry>> sectionRelocationTables;
// For each section, ...
std::unordered_map<int, std::vector<char>> outputFileData;
// EQU Symbol table
std::vector<EquTableEntry> equSymbolTable;

void processLabelDefinition(std::string &label)
{
    auto it = symbolTable.begin();
    for (; it != symbolTable.end(); it++)
    {
        if (it->getSymbolName() == label)
        { // Label already exists within the symbol table
            if (it->getDefined() == true)
            { // Multiple definitions of the same label are not allowed
                std::cout << "Multiple definitions of the same label are not allowed!\n";
                // exit(2);
            }
            else
            { // Label is being defined
                it->setDefinedToTrue();
                it->setSectionNumber(currentSectionNumber);
                it->setSymbolValue(locationCounter);
            }
            return;
        }
    }
    // Label does not exist within the symbol table
    symbolTable.push_back(SymbolTableEntry(label, currentSectionNumber, locationCounter));
}

void processGlobal(std::string &symbol, bool isExtern)
{
    auto it = symbolTable.begin();
    for (; it != symbolTable.end(); it++)
    {
        if (it->getSymbolName() == symbol)
        { // Symbol already exists within the symbol table
            if (isExtern == false)
                it->setSymbolScopeToGlobal();
            else
            {
                if (it->getDefined())
                {
                    std::cout << "Symbol is already defined as non-extern symbol!\n";
                    return;
                }
                it->setSymbolScopeToExtern();
            }
            return;
        }
    }
    // Symbol does not exist within the symbol table
    symbolTable.push_back(SymbolTableEntry(symbol, isExtern));
}

void processSection(std::string &section)
{
    if (currentSectionNumber != -1)
    { // Not the first section
        // Saving the current section's location counter
        if (sectionLocationCounters.find(currentSectionNumber) == sectionLocationCounters.end())
        { // Section is being processed for the first time
            sectionLocationCounters.insert({{currentSectionNumber, locationCounter}});
        }
        else
        { // A part of a section has been processed before
            sectionLocationCounters[currentSectionNumber] = locationCounter;
        }
    }
    // Check if the new section is already in the symbol table
    auto it = symbolTable.begin();
    for (; it != symbolTable.end(); it++)
    {
        if (it->getSymbolName() == section)
        { // Section is already in the symbol table?
            if (it->getNumber() != it->getSectionNumber())
            { // There already is a symbol with such name
                std::cout << "Invalid section name! There already is a symbol with such name!\n";
                return; // exit(3);
            }
            if (currentSectionNumber != it->getSectionNumber())
            {                                                                      // Are current and new section the same section?
                locationCounter = sectionLocationCounters[it->getSectionNumber()]; // Restore location counter for the new section
                currentSectionNumber = it->getSectionNumber();
            }
            return;
        }
    }
    // Section is not in the symbol table
    locationCounter = 0;
    symbolTable.push_back(SymbolTableEntry(section, symbolTable.size() + 1, 0));
    currentSectionNumber = symbolTable.size();
}

void processMemoryAllocation(unsigned int option, const std::vector<std::string> &symbols)
{ // 1 - .byte; 2 - .word; 3 - .skip
    std::smatch matches;
    if (option == 3)
    { // .skip directive
        unsigned long literalValue;
        if (std::regex_search(symbols[0], matches, HEX_REGEX)) // Hexadecimal value
            literalValue = std::stoul(symbols[0], nullptr, 16);
        else // Decimal value
            literalValue = std::stoul(symbols[0]);
        std::cout << "Literal's value is: " << literalValue << "\n";
        for (int i = 0; i < literalValue; i++)
            if (outputFileData.find(currentSectionNumber) != outputFileData.end())
                outputFileData[currentSectionNumber].push_back(0);
            else
                outputFileData.insert({{currentSectionNumber, std::vector<char>({0})}});
        locationCounter += literalValue;
        return;
    }
    unsigned short size; // In bytes
    if (option == 1)
        size = 1;
    else if (option == 2)
        size = 2;
    for (const std::string &symbol : symbols)
    {
        if (std::regex_search(symbol, matches, HEX_REGEX))
        { // Hexadecimal literal
            unsigned long literalValue = std::stoul(symbol, nullptr, 16);
            if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(literalValue & 0xFF)})}});
            else
                outputFileData[currentSectionNumber].push_back((char)(literalValue & 0xFF));
            if (size == 2)
                outputFileData[currentSectionNumber].push_back((char)((literalValue >> 8) & 0xFF));
        }
        else if (std::regex_search(symbol, matches, DEC_REGEX))
        { // Decimal literal
            unsigned long literalValue = std::stoul(symbol);
            if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(literalValue & 0xFF)})}});
            else
                outputFileData[currentSectionNumber].push_back((char)(literalValue & 0xFFUL));
            if (size == 2)
                outputFileData[currentSectionNumber].push_back((literalValue >> 8) & 0xFF);
        }
        else
        { // Symbol
            auto it = symbolTable.begin();
            for (; it != symbolTable.end(); it++)
            {
                if (it->getSymbolName() == symbol)
                {
                    if (it->getNumber() == it->getSectionNumber())
                    {
                        std::cout << "Section names are not allowed inside of memory allocation directives!\n";
                        return; // exit(4);
                    }
                    else
                    {
                        if (it->getDefined() == false)
                        { // Symbol is not yet defined
                            if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                                outputFileData.insert({{currentSectionNumber, std::vector<char>({0})}});
                            else
                                outputFileData[currentSectionNumber].push_back(0);
                            if (size == 2)
                                outputFileData[currentSectionNumber].push_back(0);
                            if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                                sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber())})}});
                            else
                                sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber()));
                            it->addForwardReference(ForwardReferenceStruct(locationCounter, currentSectionNumber)); // Adding forward reference
                        }
                        else
                        { // Symbol is already defined
                            int symbolValue = it->getSymbolValue();
                            if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                                outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(symbolValue & 0xFF)})}});
                            else
                                outputFileData[currentSectionNumber].push_back((char)(symbolValue & 0xFF));
                            if (size == 2)
                                outputFileData[currentSectionNumber].push_back((char)((symbolValue >> 8) & 0xFF));
                            if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                                sectionRelocationTables.insert({{currentSectionNumber,
                                                                 std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber())})}});
                            else
                                sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber()));
                        }
                    }
                    break;
                }
            }
            if (it == symbolTable.end()) {
                // Symbol has not yet been refernced
                symbolTable.push_back(SymbolTableEntry(symbol, ForwardReferenceStruct(locationCounter, currentSectionNumber)));
                if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                    outputFileData.insert({{currentSectionNumber, std::vector<char>({0})}});
                else
                    outputFileData[currentSectionNumber].push_back(0);
                if (size == 2)
                    outputFileData[currentSectionNumber].push_back(0);
                if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                    sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber())})}});
                else
                    sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber()));
                            
            }
        }
        locationCounter += size;
    }
}

void processInstruction(std::string &name, const std::vector<std::string> &operands = std::vector<std::string>(), bool isBranch = false)
{
    if (currentSectionNumber == -1)
    {
        std::cout << "Instruction directive must be a part of a section!\n";
        return; // exit(...);
    }
    if (operands.size() == 0)
    { // Non-address instruction
        short data = instructionOperationCodes[name] << 3;
        if (outputFileData.find(currentSectionNumber) == outputFileData.end())
            outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
        else
            outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
        locationCounter++;
    }
    else if (operands.size() == 1)
    {
        std::smatch matches;
        short data = instructionOperationCodes[name] << 3;
        if (isBranch == true)
        { // Branch instruction
            if (std::regex_search(operands[0], matches, LITERAL_REGEX))
            {
                std::string literal = matches.str(2);
                unsigned long literalValue;
                if (std::regex_search(operands[0], matches, HEX_REGEX))
                    literalValue = std::stoul(literal, nullptr, 16);
                else
                    literalValue = std::stoul(literal);
                if (operands[0][0] == '*')
                {
                    std::cout << "Branch instruction operand is a literal (actual operand is in memory): " << matches.str(2) << "\n";
                    data |= 1; // Set size bit to 1 - operand's size is 2 bytes for memory addressing
                    // OC and size byte
                    if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                        outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
                    else
                        outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
                    // Operand description byte
                    outputFileData[currentSectionNumber].push_back((char)(addressingOperationCodes["mem"] << 5));
                    // Operand bytes
                    outputFileData[currentSectionNumber].push_back((char)(literalValue & 0xFF));
                    outputFileData[currentSectionNumber].push_back((char)((literalValue >> 8) & 0xFF));
                    locationCounter += 4;
                }
                else
                {
                    std::cout << "Branch instruction operand is a literal: " << matches.str(2) << "\n";
                    if (literalValue > 255) // 2 bytes are needed for the operand
                        data |= 1;
                    // OC and size bits
                    if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                        outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
                    else
                        outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
                    // Operand description byte
                    outputFileData[currentSectionNumber].push_back((char)(addressingOperationCodes["immed"] << 5));
                    // Operand byte(s)
                    outputFileData[currentSectionNumber].push_back((char)(literalValue & 0xFF));
                    if (literalValue > 255) // 2 bytes are needed for the operand
                        outputFileData[currentSectionNumber].push_back((char)((literalValue >> 8) & 0xFF));
                    locationCounter += (literalValue > 255) ? 4 : 3;
                }
            }
            else if (std::regex_search(operands[0], matches, SYMBOL_REGEX))
            {
                data |= 1; // Set size bit to 1 - operand's size is 2 bytes for memory addressing
                // OC and size byte
                if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                    outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
                else
                    outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
                if (matches.str(1) == "*")
                {
                    std::cout << "Branch instruction operand is a symbol (actual operand is in memory): " << matches.str(2) << "\n";
                    // Operand description byte
                    outputFileData[currentSectionNumber].push_back((char)(addressingOperationCodes["mem"] << 5));
                }
                else
                {
                    std::cout << "Branch instruction operand is a symbol: " << matches.str(2) << "\n";
                    // Operand description byte
                    outputFileData[currentSectionNumber].push_back((char)(addressingOperationCodes["immed"] << 5));
                }
                locationCounter += 2;
                std::string symbolName = matches.str(2);
                auto it = symbolTable.begin();
                for (; it != symbolTable.end(); it++)
                    if (it->getSymbolName() == symbolName)
                        break;
                if (it == symbolTable.end())
                { // Symbol has not yet been defined/referenced
                    symbolTable.push_back(SymbolTableEntry(symbolName, ForwardReferenceStruct(locationCounter, currentSectionNumber)));
                    if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                        sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, symbolTable.size())})}});
                    else
                        sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, symbolTable.size()));
                    // Operand bytes
                    outputFileData[currentSectionNumber].push_back(0);
                    outputFileData[currentSectionNumber].push_back(0);
                }
                else
                {
                    if (it->getDefined() == false)
                    {
                        it->addForwardReference(ForwardReferenceStruct(locationCounter, currentSectionNumber));
                        if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                            sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber())})}});
                        else
                            sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber()));
                        // Operand bytes
                        outputFileData[currentSectionNumber].push_back(0);
                        outputFileData[currentSectionNumber].push_back(0);
                    }
                    else // Symbol is defined
                    {
                        if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                            sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber())})}});
                        else
                            sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber()));
                        // Operand bytes
                        outputFileData[currentSectionNumber].push_back((char)(it->getSymbolValue() & 0xFF));
                        outputFileData[currentSectionNumber].push_back((char)((it->getSymbolValue() >> 8) & 0xFF));
                    }
                }
                locationCounter += 2;
            }
            else if (std::regex_search(operands[0], matches, REGISTER_REGEX))
            {
                std::cout << "Branch instruction operand is a register!\n";
                // OC and size byte
                if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                    outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
                else
                    outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
                if (matches.str(2)[0] == '(')
                {
                    std::cout << "Register indirect! Register number: " << matches.str(2)[3] << "\n";
                    char regNum = matches.str(2)[3];
                    int registerNumber = std::atoi(&regNum);
                    outputFileData[currentSectionNumber].push_back((char)((addressingOperationCodes["regind"] << 5) | (registerNumber << 1)));
                }
                else
                {
                    std::cout << "Register direct! Register number: " << matches.str(2)[2] << "\n";
                    char regNum = matches.str(2)[2];
                    int registerNumber = std::atoi(&regNum);
                    outputFileData[currentSectionNumber].push_back((char)((addressingOperationCodes["regdir"] << 5) | (registerNumber << 1)));
                }
                locationCounter += 2;
            }
            else if (std::regex_search(operands[0], matches, LITREG_REGEX))
            {
                std::cout << "Branch instruction operand is a register with literal offset!\n";
                std::cout << "Literal offset is: " << matches.str(3) << "\n";
                std::cout << "Register number is: " << matches.str(4) << "\n";
                std::string literal = matches.str(3);
                int registerNumber = std::stoi(matches.str(4));
                data |= 1; // Operand size for register indirect with offset is 2 bytes
                // OC and size byte
                if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                    outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
                else
                    outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
                unsigned long literalValue;
                if (std::regex_search(literal, matches, HEX_REGEX))
                    literalValue = std::stoul(literal, nullptr, 16);
                else
                    literalValue = std::stoul(literal);
                // Operand description byte
                outputFileData[currentSectionNumber].push_back((char)((addressingOperationCodes["regindoff"] << 5) | (registerNumber << 1)));
                // Operand bytes
                outputFileData[currentSectionNumber].push_back((char)(literalValue & 0xFF));
                outputFileData[currentSectionNumber].push_back((char)((literalValue >> 8) & 0xFF));
                locationCounter += 4;
            }
            else if (std::regex_search(operands[0], matches, SYMREG_REGEX))
            {
                std::cout << "Branch instruction operand is a register? with symbol's value offset!\n";
                std::cout << "Symbol is: " << matches.str(3) << "\n";
                data |= 1; // Operand size for register indirect with offset is 2 bytes
                // OC and size byte
                if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                    outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
                else
                    outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
                int registerNumber;
                if (matches.str(4)[1] == 'p')
                {
                    std::cout << "PC relative!\n";
                    registerNumber = 7;
                }
                else
                {
                    std::cout << "Register number is: " << matches.str(4)[2] << "\n";
                    char regNum = matches.str(4)[2];
                    registerNumber = std::atoi(&regNum);
                }
                // Operand description byte
                outputFileData[currentSectionNumber].push_back((char)((addressingOperationCodes["regindoff"] << 5) | (registerNumber << 1)));
                locationCounter += 2;
                std::string symbolName = matches.str(3);
                auto it = symbolTable.begin();
                for (; it != symbolTable.end(); it++)
                    if (it->getSymbolName() == symbolName)
                        break;
                RelocationTableEntry::Type type = (registerNumber == 7) ? RelocationTableEntry::RELATIVE : RelocationTableEntry::ABSOLUTE;
                int dataValue = (registerNumber == 7) ? -2 : 0;
                if (it == symbolTable.end())
                { // Symbol has not yet been defined/referenced
                    symbolTable.push_back(SymbolTableEntry(symbolName, ForwardReferenceStruct(locationCounter, currentSectionNumber)));
                    if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                        sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, type, symbolTable.size())})}});
                    else
                        sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, type, symbolTable.size()));
                }
                else
                {
                    if (it->getDefined() == false)
                    {
                        it->addForwardReference(ForwardReferenceStruct(locationCounter, currentSectionNumber));
                        if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                            sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, type, it->getNumber())})}});
                        else
                            sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, type, it->getNumber()));
                    }
                    else // Symbol is defined
                    {
                        if (it->getSymbolScope() == SymbolTableEntry::LOCAL)
                        {
                            if (currentSectionNumber == it->getSectionNumber() && registerNumber == 7) // Constant offset - no relocation data needed
                                dataValue = it->getSymbolValue() - locationCounter - 2;
                            else
                            { // Relocation data is needed
                                dataValue += it->getSymbolValue();
                                if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                                    sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, type, it->getNumber())})}});
                                else
                                    sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, type, it->getNumber()));
                            }
                        }
                        else if (it->getSymbolScope() == SymbolTableEntry::GLOBAL)
                        {
                            if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                                sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, type, it->getNumber())})}});
                            else
                                sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, type, it->getNumber()));
                        }
                    }
                }
                // Operand bytes
                outputFileData[currentSectionNumber].push_back((char)(dataValue & 0xFF));
                outputFileData[currentSectionNumber].push_back((char)((dataValue >> 8) & 0xFF));
                locationCounter += 2;
            }
        }
        else
        { // One address, non-branch instruction
            if (std::regex_search(operands[0], matches, LITERAL_REGEX))
            {
                std::string literal = matches.str(2);
                unsigned long literalValue;
                if (std::regex_search(literal, matches, HEX_REGEX))
                    literalValue = std::stoul(literal, nullptr, 16);
                else
                    literalValue = std::stoul(literal);
                if (operands[0][0] == '$')
                {
                    std::cout << "One address instruction operand is an immediate value: " << matches.str(2) << "\n";
                    if (name == "pop")
                    {
                        std::cout << "Immediate addressing is not allowed for the destination operand!\n";
                        return;
                    }
                    if (literalValue > 255)
                        data |= 1;
                    // OC and size byte
                    if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                        outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
                    else
                        outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
                    // Operand description byte
                    outputFileData[currentSectionNumber].push_back((char)(addressingOperationCodes["imm"] << 5));
                    // Operand byte(s)
                    outputFileData[currentSectionNumber].push_back((char)(literalValue & 0xFF));
                    if (literalValue > 255)
                    {
                        outputFileData[currentSectionNumber].push_back((char)((literalValue >> 8) & 0xFF));
                        locationCounter += 4;
                    }
                    else
                        locationCounter += 3;
                }
                else
                {
                    std::cout << "One address instruction operand is in memory (literal stores the location): " << matches.str(2) << "\n";
                    data |= 1;
                    // OC and size byte
                    if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                        outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
                    else
                        outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
                    // Operand description byte
                    outputFileData[currentSectionNumber].push_back((char)(addressingOperationCodes["mem"] << 5));
                    // Operand byte(s)
                    outputFileData[currentSectionNumber].push_back((char)(literalValue & 0xFF));
                    outputFileData[currentSectionNumber].push_back((char)((literalValue >> 8) & 0xFF));
                    locationCounter += 4;
                }
            }
            else if (std::regex_search(operands[0], matches, SYMBOL_REGEX))
            {
                data |= 1; // Set size bit to 1 - operand's size is 2 bytes for memory addressing
                // OC and size byte
                if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                    outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
                else
                    outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
                if (matches.str(1) == "$")
                {
                    std::cout << "One address instruction operand is an immediate value (equals to the symbol's value): " << matches.str(2) << "\n";
                    if (name == "pop")
                    {
                        std::cout << "Immediate addressing is not allowed for the destination operand!\n";
                        return;
                    }
                    outputFileData[currentSectionNumber].push_back((char)(addressingOperationCodes["immed"] << 5));
                }
                else
                {
                    std::cout << "One address instruction operand is in memory (symbol's value is the location): " << matches.str(2) << "\n";
                    outputFileData[currentSectionNumber].push_back((char)(addressingOperationCodes["mem"] << 5));
                }
                locationCounter += 2;
                std::string symbolName = matches.str(2);
                auto it = symbolTable.begin();
                for (; it != symbolTable.end(); it++)
                    if (it->getSymbolName() == symbolName)
                        break;
                if (it == symbolTable.end())
                { // Symbol has not yet been defined/referenced
                    symbolTable.push_back(SymbolTableEntry(symbolName, ForwardReferenceStruct(locationCounter, currentSectionNumber)));
                    if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                        sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, symbolTable.size())})}});
                    else
                        sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, symbolTable.size()));
                    // Operand bytes
                    outputFileData[currentSectionNumber].push_back(0);
                    outputFileData[currentSectionNumber].push_back(0);
                }
                else
                {
                    if (it->getDefined() == false)
                    {
                        it->addForwardReference(ForwardReferenceStruct(locationCounter, currentSectionNumber));
                        if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                            sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber())})}});
                        else
                            sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber()));
                        // Operand bytes
                        outputFileData[currentSectionNumber].push_back(0);
                        outputFileData[currentSectionNumber].push_back(0);
                    }
                    else // Symbol is defined
                    {
                        if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                            sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber())})}});
                        else
                            sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber()));
                        // Operand bytes
                        outputFileData[currentSectionNumber].push_back((char)(it->getSymbolValue() & 0xFF));
                        outputFileData[currentSectionNumber].push_back((char)((it->getSymbolValue() >> 8) & 0xFF));
                    }
                }
                locationCounter += 2;
            }
            else if (std::regex_search(operands[0], matches, REGISTER_REGEX))
            {
                std::cout << "One address instruction operand is a register!\n";
                // OC and size byte
                if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                    outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
                else
                    outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
                if (matches.str(2)[0] == '(')
                {
                    std::cout << "Register indirect! Register number: " << matches.str(2)[3] << "\n";
                    char regNum = matches.str(2)[3];
                    int registerNumber = std::atoi(&regNum);
                    outputFileData[currentSectionNumber].push_back((char)((addressingOperationCodes["regind"] << 5) | (registerNumber << 1)));
                }
                else
                {
                    std::cout << "Register direct! Register number: " << matches.str(2)[2] << "\n";
                    char regNum = matches.str(2)[2];
                    int registerNumber = std::atoi(&regNum);
                    outputFileData[currentSectionNumber].push_back((char)((addressingOperationCodes["regdir"] << 5) | (registerNumber << 1)));
                }
                locationCounter += 2;
            }
            else if (std::regex_search(operands[0], matches, LITREG_REGEX))
            {
                std::cout << "One address instruction operand is a register with literal offset!\n";
                std::cout << "Literal offset is: " << matches.str(3) << "\n";
                std::cout << "Register number is: " << matches.str(4) << "\n";
                std::string literal = matches.str(3);
                int registerNumber = std::stoi(matches.str(4));
                data |= 1; // Operand size for register indirect with offset is 2 bytes
                // OC and size byte
                if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                    outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
                else
                    outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
                unsigned long literalValue;
                if (std::regex_search(literal, matches, HEX_REGEX))
                    literalValue = std::stoul(literal, nullptr, 16);
                else
                    literalValue = std::stoul(literal);
                // Operand description byte
                outputFileData[currentSectionNumber].push_back((char)((addressingOperationCodes["regindoff"] << 5) | (registerNumber << 1)));
                // Operand bytes
                outputFileData[currentSectionNumber].push_back((char)(literalValue & 0xFF));
                outputFileData[currentSectionNumber].push_back((char)((literalValue >> 8) & 0xFF));
                locationCounter += 4;
            }
            else if (std::regex_search(operands[0], matches, SYMREG_REGEX))
            {
                std::cout << "One address instruction operand is a register? with symbol's value offset!\n";
                std::cout << "Symbol is: " << matches.str(3) << "\n";
                data |= 1; // Operand size for register indirect with offset is 2 bytes
                // OC and size byte
                if (outputFileData.find(currentSectionNumber) == outputFileData.end())
                    outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
                else
                    outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
                int registerNumber;
                if (matches.str(4)[1] == 'p')
                {
                    std::cout << "PC relative!\n";
                    registerNumber = 7;
                }
                else
                {
                    std::cout << "Register number is: " << matches.str(4)[2] << "\n";
                    char regNum = matches.str(4)[2];
                    registerNumber = std::atoi(&regNum);
                }
                // Operand description byte
                outputFileData[currentSectionNumber].push_back((char)((addressingOperationCodes["regindoff"] << 5) | (registerNumber << 1)));
                locationCounter += 2;
                std::string symbolName = matches.str(3);
                auto it = symbolTable.begin();
                for (; it != symbolTable.end(); it++)
                    if (it->getSymbolName() == symbolName)
                        break;
                RelocationTableEntry::Type type = (registerNumber == 7) ? RelocationTableEntry::RELATIVE : RelocationTableEntry::ABSOLUTE;
                int dataValue = (registerNumber == 7) ? -2 : 0;
                if (it == symbolTable.end())
                { // Symbol has not yet been defined/referenced
                    symbolTable.push_back(SymbolTableEntry(symbolName, ForwardReferenceStruct(locationCounter, currentSectionNumber)));
                    if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                        sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, type, symbolTable.size())})}});
                    else
                        sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, type, symbolTable.size()));
                }
                else
                {
                    if (it->getDefined() == false)
                    {
                        it->addForwardReference(ForwardReferenceStruct(locationCounter, currentSectionNumber));
                        if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                            sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, type, it->getNumber())})}});
                        else
                            sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, type, it->getNumber()));
                    }
                    else // Symbol is defined
                    {
                        if (it->getSymbolScope() == SymbolTableEntry::LOCAL)
                        {
                            if (currentSectionNumber == it->getSectionNumber() && registerNumber == 7) // Constant offset - no relocation data needed
                                dataValue = it->getSymbolValue() - locationCounter - 2;
                            else
                            { // Relocation data is needed
                                dataValue += it->getSymbolValue();
                                if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                                    sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, type, it->getNumber())})}});
                                else
                                    sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, type, it->getNumber()));
                            }
                        }
                        else if (it->getSymbolScope() == SymbolTableEntry::GLOBAL)
                        {
                            if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                                sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, type, it->getNumber())})}});
                            else
                                sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, type, it->getNumber()));
                        }
                    }
                }
                // Operand bytes
                outputFileData[currentSectionNumber].push_back((char)(dataValue & 0xFF));
                outputFileData[currentSectionNumber].push_back((char)((dataValue >> 8) & 0xFF));
                locationCounter += 2;
            }
        }
    }
    else if (operands.size() == 2)
    { // Two-address instruction
        std::smatch matches;
        short data = (instructionOperationCodes[name] << 3) | 1;
        // OC and size byte
        if (outputFileData.find(currentSectionNumber) == outputFileData.end())
            outputFileData.insert({{currentSectionNumber, std::vector<char>({(char)(data & 0xFF)})}});
        else
            outputFileData[currentSectionNumber].push_back((char)(data & 0xFF));
        locationCounter++;
        for (int i = 0; i < 2; i++)
        {
            if (std::regex_search(operands[i], matches, LITERAL_REGEX))
            {
                std::string literal = matches.str(2);
                unsigned long literalValue;
                if (std::regex_search(literal, matches, HEX_REGEX))
                    literalValue = std::stoul(literal, nullptr, 16);
                else
                    literalValue = std::stoul(literal);
                if (operands[i][0] == '$')
                {
                    std::cout << "One address instruction operand is an immediate value: " << matches.str(2) << "\n";
                    if (i == 1 || (i == 0 && name == "xchg"))
                    {
                        std::cout << "Immediate addressing is not allowed for the destination operand nor for the source operands if the instruction is xchg!\n";
                        return;
                    }
                    // Operand description byte
                    outputFileData[currentSectionNumber].push_back((char)(addressingOperationCodes["imm"] << 5));
                    // Operand byte(s)
                    outputFileData[currentSectionNumber].push_back((char)(literalValue & 0xFF));
                    if (literalValue > 255)
                    {
                        outputFileData[currentSectionNumber].push_back((char)((literalValue >> 8) & 0xFF));
                        locationCounter += 3;
                    }
                    else
                        locationCounter += 2;
                }
                else
                {
                    std::cout << "One address instruction operand is in memory (literal stores the location): " << matches.str(2) << "\n";
                    // Operand description byte
                    outputFileData[currentSectionNumber].push_back((char)(addressingOperationCodes["mem"] << 5));
                    // Operand byte(s)
                    outputFileData[currentSectionNumber].push_back((char)(literalValue & 0xFF));
                    outputFileData[currentSectionNumber].push_back((char)((literalValue >> 8) & 0xFF));
                    locationCounter += 3;
                }
            }
            else if (std::regex_search(operands[i], matches, SYMBOL_REGEX))
            {
                if (matches.str(1) == "$")
                {
                    std::cout << "One address instruction operand is an immediate value (equals to the symbol's value): " << matches.str(2) << "\n";
                    if (i == 1 || (i == 0 && name == "xchg"))
                    {
                        std::cout << "Immediate addressing is not allowed for the destination operand nor for the source operands if the instruction is xchg!\n";
                        return;
                    }
                    outputFileData[currentSectionNumber].push_back((char)(addressingOperationCodes["immed"] << 5));
                }
                else
                {
                    std::cout << "One address instruction operand is in memory (symbol's value is the location): " << matches.str(2) << "\n";
                    outputFileData[currentSectionNumber].push_back((char)(addressingOperationCodes["mem"] << 5));
                }
                locationCounter++;
                std::string symbolName = matches.str(2);
                auto it = symbolTable.begin();
                for (; it != symbolTable.end(); it++)
                    if (it->getSymbolName() == symbolName)
                        break;
                if (it == symbolTable.end())
                { // Symbol has not yet been defined/referenced
                    symbolTable.push_back(SymbolTableEntry(symbolName, ForwardReferenceStruct(locationCounter, currentSectionNumber)));
                    if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                        sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, symbolTable.size())})}});
                    else
                        sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, symbolTable.size()));
                    // Operand bytes
                    outputFileData[currentSectionNumber].push_back(0);
                    outputFileData[currentSectionNumber].push_back(0);
                }
                else
                {
                    if (it->getDefined() == false)
                    {
                        it->addForwardReference(ForwardReferenceStruct(locationCounter, currentSectionNumber));
                        if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                            sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber())})}});
                        else
                            sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber()));
                        // Operand bytes
                        outputFileData[currentSectionNumber].push_back(0);
                        outputFileData[currentSectionNumber].push_back(0);
                    }
                    else // Symbol is defined
                    {
                        if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                            sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber())})}});
                        else
                            sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, RelocationTableEntry::ABSOLUTE, it->getNumber()));
                        // Operand bytes
                        outputFileData[currentSectionNumber].push_back((char)(it->getSymbolValue() & 0xFF));
                        outputFileData[currentSectionNumber].push_back((char)((it->getSymbolValue() >> 8) & 0xFF));
                    }
                }
                locationCounter += 2;
            }
            else if (std::regex_search(operands[i], matches, REGISTER_REGEX))
            {
                std::cout << "One address instruction operand is a register!\n";
                if (matches.str(2)[0] == '(')
                {
                    std::cout << "Register indirect! Register number: " << matches.str(2)[3] << "\n";
                    char regNum = matches.str(2)[3];
                    int registerNumber = std::atoi(&regNum);
                    outputFileData[currentSectionNumber].push_back((char)((addressingOperationCodes["regind"] << 5) | (registerNumber << 1)));
                }
                else
                {
                    std::cout << "Register direct! Register number: " << matches.str(2)[2] << "\n";
                    char regNum = matches.str(2)[2];
                    int registerNumber = std::atoi(&regNum);
                    outputFileData[currentSectionNumber].push_back((char)((addressingOperationCodes["regdir"] << 5) | (registerNumber << 1)));
                }
                locationCounter++;
            }
            else if (std::regex_search(operands[i], matches, LITREG_REGEX))
            {
                std::cout << "One address instruction operand is a register with literal offset!\n";
                std::cout << "Literal offset is: " << matches.str(3) << "\n";
                std::cout << "Register number is: " << matches.str(4) << "\n";
                std::string literal = matches.str(3);
                int registerNumber = std::stoi(matches.str(4));
                unsigned long literalValue;
                if (std::regex_search(literal, matches, HEX_REGEX))
                    literalValue = std::stoul(literal, nullptr, 16);
                else
                    literalValue = std::stoul(literal);
                // Operand description byte
                outputFileData[currentSectionNumber].push_back((char)((addressingOperationCodes["regindoff"] << 5) | (registerNumber << 1)));
                // Operand bytes
                outputFileData[currentSectionNumber].push_back((char)(literalValue & 0xFF));
                outputFileData[currentSectionNumber].push_back((char)((literalValue >> 8) & 0xFF));
                locationCounter += 3;
            }
            else if (std::regex_search(operands[i], matches, SYMREG_REGEX))
            {
                std::cout << "One address instruction operand is a register? with symbol's value offset!\n";
                std::cout << "Symbol is: " << matches.str(3) << "\n";
                int registerNumber;
                if (matches.str(4)[1] == 'p')
                {
                    std::cout << "PC relative!\n";
                    registerNumber = 7;
                }
                else
                {
                    std::cout << "Register number is: " << matches.str(4)[2] << "\n";
                    char regNum = matches.str(4)[2];
                    registerNumber = std::atoi(&regNum);
                }
                // Operand description byte
                outputFileData[currentSectionNumber].push_back((char)((addressingOperationCodes["regindoff"] << 5) | (registerNumber << 1)));
                locationCounter++;
                std::string symbolName = matches.str(3);
                auto it = symbolTable.begin();
                for (; it != symbolTable.end(); it++)
                    if (it->getSymbolName() == symbolName)
                        break;
                RelocationTableEntry::Type type = (registerNumber == 7) ? RelocationTableEntry::RELATIVE : RelocationTableEntry::ABSOLUTE;
                int dataValue = (registerNumber == 7) ? -2 : 0;
                if (it == symbolTable.end())
                { // Symbol has not yet been defined/referenced
                    symbolTable.push_back(SymbolTableEntry(symbolName, ForwardReferenceStruct(locationCounter, currentSectionNumber)));
                    if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                        sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, type, symbolTable.size())})}});
                    else
                        sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, type, symbolTable.size()));
                }
                else
                {
                    if (it->getDefined() == false)
                    {
                        it->addForwardReference(ForwardReferenceStruct(locationCounter, currentSectionNumber));
                        if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                            sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, type, it->getNumber())})}});
                        else
                            sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, type, it->getNumber()));
                    }
                    else // Symbol is defined
                    {
                        if (it->getSymbolScope() == SymbolTableEntry::LOCAL)
                        {
                            if (currentSectionNumber == it->getSectionNumber() && registerNumber == 7) // Constant offset - no relocation data needed
                                dataValue = it->getSymbolValue() - locationCounter - 2;
                            else
                            { // Relocation data is needed
                                dataValue += it->getSymbolValue();
                                if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                                    sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, type, it->getNumber())})}});
                                else
                                    sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, type, it->getNumber()));
                            }
                        }
                        else if (it->getSymbolScope() == SymbolTableEntry::GLOBAL)
                        {
                            if (sectionRelocationTables.find(currentSectionNumber) == sectionRelocationTables.end())
                                sectionRelocationTables.insert({{currentSectionNumber, std::vector<RelocationTableEntry>({RelocationTableEntry(locationCounter, type, it->getNumber())})}});
                            else
                                sectionRelocationTables[currentSectionNumber].push_back(RelocationTableEntry(locationCounter, type, it->getNumber()));
                        }
                    }
                }
                // Operand bytes
                outputFileData[currentSectionNumber].push_back((char)(dataValue & 0xFF));
                outputFileData[currentSectionNumber].push_back((char)((dataValue >> 8) & 0xFF));
                locationCounter += 2;
            }
        }
    }
}

void processEqu(std::string& symbolName, std::vector<std::string>& exprOperands, std::vector<std::string>& operandSigns)
{
    std::vector<unsigned int> symbols;
    std::vector<char> symbolSigns;
    unsigned int size = exprOperands.size();
    int symbolValue = 0;
    unsigned int symbolNumber;
    std::vector<ClassificationIndexStruct> classifictionIndexTable;
    for (int i = 0; i < size; i++)
    { // Go through the operands within the expression
        std::smatch matches;
        std::string currOperand = exprOperands[i];
        if (std::regex_search(currOperand, matches, LITERAL_REGEX))
        {
            int literalValue;
            if (std::regex_search(currOperand, matches, HEX_REGEX))
                literalValue = std::stoi(currOperand, nullptr, 16);
            else
                literalValue = std::stoi(currOperand);
            if (operandSigns[i] == "+")
                symbolValue += literalValue;
            else
                symbolValue -= literalValue;       
        }
        else
        { // Operand is a symbol
            auto it = symbolTable.begin();
            for (; it != symbolTable.end(); it++)
            {
                if (it->getSymbolName() == currOperand)
                {
                    if (it->getDefined() == true && it->getEqu() == false)
                    { // Symbol is defined -> update classification index for symbols's section and its value
                        if (operandSigns[i] == "+")
                            symbolValue += it->getSymbolValue();
                        else
                            symbolValue -= it->getSymbolValue();
                        auto iter = classifictionIndexTable.begin();
                        for (; iter != classifictionIndexTable.end(); iter++)
                        {
                            if (iter->sectionNumber == it->getSectionNumber())
                            {
                                iter->classificationIndex += (operandSigns[i] == "+") ? 1 : -1;
                                break;
                            }
                        }
                        if (iter == classifictionIndexTable.end())
                            classifictionIndexTable.push_back(ClassificationIndexStruct(it->getSectionNumber(), (operandSigns[i] == "+") ? 1 : -1));
                    }
                    else
                    {
                        symbols.push_back(it->getNumber());
                        symbolSigns.push_back(operandSigns[i][0]);
                    }
                    break;
                }
            }
            if (it == symbolTable.end())
            { // Symbol is being "referenced" for the first time -> add it into the symbol table (without forward references)
                symbolTable.push_back(SymbolTableEntry(currOperand));
                symbols.push_back(symbolTable.size());
                symbolSigns.push_back(operandSigns[i][0]);
            }    
        } 
    }
    auto it = symbolTable.begin();
    for (; it != symbolTable.end(); it++)
    {
        if (it->getSymbolName() == symbolName)
        {
            symbolNumber = it->getNumber();
            if (it->getDefined() == true)
            {
                std::cout << "Multiple definitions of the symbol!\n";
                return;
            }
            else {
                it->setSymbolValue(symbolValue);
                it->setEquToTrue();
            }
            break;
        }
    }
    if (it == symbolTable.end())
    {
        // Symbol is not in the symbol table -> add it...
        symbolTable.push_back(SymbolTableEntry(symbolName));
        symbolNumber = symbolTable.size();
        symbolTable[symbolTable.size() - 1].setEquToTrue();
        symbolTable[symbolTable.size() - 1].setSymbolValue(symbolValue);
    }
    // Add the symbol to the EQU symbol table
    equSymbolTable.push_back(EquTableEntry(symbolNumber, symbolSigns, symbols, classifictionIndexTable));
}

void processLine(std::string line)
{
    std::smatch matches;
    std::smatch matchesOperand;
    if (std::regex_search(line, matches, LABEL_REGEX))
    { // Is it a label?
        std::cout << "Found a label!\n";
        std::cout << "Label name: " << matches.str(1) << "\n";
        if (currentSectionNumber == -1)
        { // Label must be a part of a section!
            std::cout << "Label must be a part of a section!\n";
            // exit(1);
        }
        else
        {
            std::string labelName = matches.str(1);
            processLabelDefinition(labelName);
        }
    }
    else if (std::regex_search(line, matches, GLOBAL_REGEX))
    { // Is it a global?
        std::cout << "Found a global!\n";
        std::cout << "List of global symbols: " << matches.str(1) << "\n";
        std::string symbolList = matches.str(1);
        symbolList.erase(std::remove(symbolList.begin(), symbolList.end(), ' '), symbolList.end()); // Remove all spaces
        while (std::regex_search(symbolList, matches, LIST_REGEX))
        {
            std::string symbolName = matches.str(1);
            processGlobal(symbolName, false);
            symbolList = matches.suffix();
        }
    }
    else if (std::regex_search(line, matches, EXTERN_REGEX))
    { // Is it an extern?
        std::cout << "Found an extern!\n";
        std::cout << "List of extern symbols: " << matches.str(1) << "\n";
        std::string symbolList = matches.str(1);
        symbolList.erase(std::remove(symbolList.begin(), symbolList.end(), ' '), symbolList.end()); // Remove all spaces
        while (std::regex_search(symbolList, matches, LIST_REGEX))
        {
            std::string symbolName = matches.str(1);
            processGlobal(symbolName, true);
            symbolList = matches.suffix();
        }
    }
    else if (std::regex_search(line, matches, SECTION_REGEX))
    { // Is it a section?
        std::cout << "Found a section!\n";
        std::cout << "Section name: " << matches.str(1) << "\n";
        std::string sectionName = matches.str(1);
        processSection(sectionName);
    }
    else if (std::regex_search(line, matches, BYTE_REGEX))
    { // Is it a byte?
        std::cout << "Found a byte!\n";
        std::cout << "List of symbols/literals: " << matches.str(1) << "\n";
        if (currentSectionNumber == -1)
        {
            std::cout << "Memory allocation directive must be a part of a section!\n";
            // exit(1);
        }
        else
        {
            std::string symbolList = matches.str(1);
            symbolList.erase(std::remove(symbolList.begin(), symbolList.end(), ' '), symbolList.end()); // Remove all spaces
            std::vector<std::string> symbols;
            while (std::regex_search(symbolList, matches, LIST_REGEX))
            {
                symbols.push_back(matches.str(1));
                symbolList = matches.suffix();
            }
            processMemoryAllocation(1, symbols);
        }
    }
    else if (std::regex_search(line, matches, WORD_REGEX))
    { // Is it a word?
        std::cout << "Found a word!\n";
        std::cout << "List of symbols/literals: " << matches.str(1) << "\n";
        if (currentSectionNumber == -1)
        {
            std::cout << "Memory allocation directive must be a part of a section!\n";
            // exit(1);
        }
        else
        {
            std::string symbolList = matches.str(1);
            symbolList.erase(std::remove(symbolList.begin(), symbolList.end(), ' '), symbolList.end()); // Remove all spaces
            std::vector<std::string> symbols;
            while (std::regex_search(symbolList, matches, LIST_REGEX))
            {
                symbols.push_back(matches.str(1));
                symbolList = matches.suffix();
            }
            processMemoryAllocation(2, symbols);
        }
    }
    else if (std::regex_search(line, matches, SKIP_REGEX))
    { // Is it a skip?
        std::cout << "Found a skip!\n";
        std::cout << "Literal: " << matches.str(1) << "\n";
        if (currentSectionNumber == -1)
        {
            std::cout << "Memory allocation directive must be a part of a section!\n";
            // exit(1);
        }
        else
        {
            std::vector<std::string> symbols;
            symbols.push_back(matches.str(1));
            processMemoryAllocation(3, symbols);
        }
    }
    else if (std::regex_search(line, matches, EQU_REGEX))
    { // Is it an equ?
        std::cout << "Found an equ!\n";
        std::cout << "Symbol name: " << matches.str(1) << "\n";
        std::string symbolName = matches.str(1);
        std::cout << "Expression: " << matches.str(2) << "\n";
        std::string expression = matches.str(2);
        expression.erase(std::remove(expression.begin(), expression.end(), ' '), expression.end()); // Remove all spaces
        std::vector<std::string> exprOperands;
        std::vector<std::string> operandSigns({ "+" });
        while (std::regex_search(expression, matches, EXPRESSION_REGEX))
        {
            exprOperands.push_back(matches.str(1));
            if (matches.str(2) != "")
                operandSigns.push_back(matches.str(2));
            expression = matches.suffix();
        }
        processEqu(symbolName, exprOperands, operandSigns);
    }
    else
    { // Is it an instruction or an error?
        if (std::regex_search(line, matches, NOADDR_INSTURCTION_REGEX))
        { // Is it a non-address instruction?
            std::cout << "Non-address instruction name: " << matches.str(1) << "\n";
            std::string instructionName = matches.str(1);
            processInstruction(instructionName, std::vector<std::string>());
        }
        else if (std::regex_search(line, matches, BRANCH_INSTRUCTION_REGEX))
        { // Is it a branch instruction?
            std::cout << "Branch instruction name: " << matches.str(1) << "\n";
            std::cout << "Branch instruction operand: " << matches.str(2) << "\n";
            std::string instructionName = matches.str(1);
            std::string operand = matches.str(2);
            processInstruction(instructionName, std::vector<std::string>({operand}), true);
        }
        else if (std::regex_search(line, matches, ONEADDR_INSTRUCTION_REGEX))
        {
            std::cout << "One operand instruction name: " << matches.str(1) << "\n";
            std::cout << "One operand instruction operand: " << matches.str(2) << "\n";
            std::string instructionName = matches.str(1);
            std::string operand = matches.str(2);
            processInstruction(instructionName, std::vector<std::string>({operand}));
        }
        else if (std::regex_search(line, matches, TWOADDR_INSTRUCTION_REGEX))
        {
            std::cout << "Two operand instruction name: " << matches.str(1) << "\n";
            std::cout << "Two operand instruction operand #1: " << matches.str(2) << "\n";
            std::cout << "Two operand instruction operand #2: " << matches.str(8) << "\n";
            std::string instructionName = matches.str(1);
            std::string operand1 = matches.str(2);
            std::string operand2 = matches.str(8);
            processInstruction(instructionName, std::vector<std::string>({operand1, operand2}));
        }
    }
}

void processNonEquForwardReferences()
{
    auto it = symbolTable.begin();
    for (; it != symbolTable.end(); it++)
    {
        if (it->getEqu() == true || it->getSymbolScope() == SymbolTableEntry::EXTERN) continue;
        if (it->getDefined() == false)
        {
            std::cout << "Non-equ and non-extern symbol is not defined!\n";
            return; // Error - non-equ and non-extern symbol is not defined
        }
        std::list<ForwardReferenceStruct> forwardReferences = it->getFowardsReferences();
        auto iter = forwardReferences.begin();
        for (; iter != forwardReferences.end(); iter++)
        {
            char lowerByte = (char)(outputFileData[iter->sectionNumber][iter->patch] & 0xFF);
            char higherByte = (char)(outputFileData[iter->sectionNumber][iter->patch + 1] & 0xFF);
            int dataValue = lowerByte | (higherByte << 8);
            int symbolValue = it->getSymbolValue();
            dataValue += (iter->sign == '+') ? symbolValue : -symbolValue;
            // Get the new value back in the file
            outputFileData[iter->sectionNumber][iter->patch] = (char)(dataValue & 0xFF);
            outputFileData[iter->sectionNumber][iter->patch + 1] = (char)((dataValue >> 8) & 0xFF);
        }
        if (it->getNumber() == it->getSectionNumber() || it->getSymbolScope() != SymbolTableEntry::LOCAL) continue; // Symbol is a section, or it is a global/extern symbol
        // Update all of the relocation data for the symbol
        auto iterator = sectionRelocationTables.begin();
        for (; iterator != sectionRelocationTables.end(); iterator++)
        {
            for (int i = 0; i < iterator->second.size(); i++)
            {
                if (iterator->second[i].getSymbolNumber() != it->getNumber()) continue;
                int num = it->getNumber();
                // Relocation data is for the current symbol
                if (iterator->second[i].getType() == RelocationTableEntry::ABSOLUTE)
                    iterator->second[i].setSymbolNumber(it->getSectionNumber()); // Relocation data 
                else 
                { // PC relative relocation data
                    if (iterator->first == it->getSectionNumber()) 
                    { // No relocation data is needed
                        char lowerByte = (char)(outputFileData[iterator->first][iterator->second[i].getOffset()] & 0xFF);
                        char higherByte = (char)(outputFileData[iterator->first][iterator->second[i].getOffset() + 1] & 0xFF);
                        int dataValue = lowerByte | (higherByte << 8);
                        dataValue -= iterator->second[i].getOffset();
                        outputFileData[iterator->first][iterator->second[i].getOffset()] = (char)(dataValue & 0xFF);
                        outputFileData[iterator->first][iterator->second[i].getOffset() + 1] = (char)((dataValue >> 8) & 0xFF);
                        iterator->second.erase(iterator->second.begin() + i);
                        i--;
                    }
                    else
                        iterator->second[i].setSymbolNumber(it->getSectionNumber());
                }
            }
        }
        it->resetForwardRefernces();
    }
}

void processEquValue1()
{ // Updating EQU symbol values, based off of only non-EQU symbols
    auto it = equSymbolTable.begin();
    for (; it != equSymbolTable.end(); it++)
    {
        std::vector<unsigned int> symbolDependencies = it->getSymbolDependencies();
        std::vector<char> symbolSigns = it->getSymbolSigns();
        int equSymbolValue = symbolTable[it->getSymbolNumber() - 1].getSymbolValue();
        int size = symbolDependencies.size();
        for (int i = 0; i < size; i++)
        {
            if (symbolTable[symbolDependencies[i] - 1].getEqu() == false)
            {
                int symbolValue = symbolTable[symbolDependencies[i] - 1].getSymbolValue();
                equSymbolValue += (symbolSigns[i] == '+') ? symbolValue : -symbolValue;
                it->updateClassIndexTableEntry(symbolTable[symbolDependencies[i] - 1].getSectionNumber(), (symbolSigns[i] == '+') ? 1 : -1);
                it->removeSymbolDependency(i);
                it->removeSymbolSign(i); 
            }
        }
        if (it->getSymbolDependencies().size() == 0)
        { 
           if (it->isExpressionValid() == false) {
               std::cout << "Equ expression is invalid!\n";
               return;
           }
           else {
               if (it->entryNotZero() == 0)
                symbolTable[it->getSymbolNumber() - 1].setSymbolScopeToExtern();
               symbolTable[it->getSymbolNumber() - 1].setDefinedToTrue();
           }
           
        }
        symbolTable[it->getSymbolNumber() - 1].setSymbolValue(equSymbolValue);
    }
}

int getEntryNot0(unsigned int equSymbolNumber)
{
    auto it = equSymbolTable.begin();
    for (; it != equSymbolTable.end(); it++)
        if (it->getSymbolNumber() == equSymbolNumber)
            return it->entryNotZero();
    return -1; // Unreachable code
}

void processEquValue2()
{ // Updating EQU symbol values, based off of EQU symbols... At this point at least one EQU symbol must be defined, otherwise we have circular dependency
// In one iteration, at least one EQU symbol must get defined, up until all EQU symbols are defined
    int numOfUndefinedEquSymbols = 0;
    auto iter = equSymbolTable.begin();
    for (; iter != equSymbolTable.end(); iter++)
        if (symbolTable[iter->getSymbolNumber() - 1].getDefined() == false)
            numOfUndefinedEquSymbols++;
    bool keepGoing = true;
    while (keepGoing == true)
    {
        keepGoing = false;
        auto it = equSymbolTable.begin();
        for (; it != equSymbolTable.end(); it++)
        {
            if (symbolTable[it->getSymbolNumber() - 1].getDefined() == false) {
                std::vector<unsigned int> symbolDependencies = it->getSymbolDependencies();
                std::vector<char> symbolSigns = it->getSymbolSigns();
                int equSymbolValue = symbolTable[it->getSymbolNumber() - 1].getSymbolValue();
                for (int i = 0; i < symbolDependencies.size(); i++)
                    if (symbolTable[symbolDependencies[i] - 1].getDefined() == true) {
                        int symbolValue = symbolTable[symbolDependencies[i] - 1].getSymbolValue();
                        equSymbolValue += (symbolSigns[i] == '+') ? symbolValue : -symbolValue;
                        int entryNot0 = getEntryNot0(symbolDependencies[i]);
                        if (entryNot0 != -1) 
                            it->updateClassIndexTableEntry(entryNot0, (symbolSigns[i] == '+') ? 1 : -1);
                        it->removeSymbolDependency(i);
                        it->removeSymbolSign(i);
                    }
                if (it->getSymbolDependencies().size() == 0) {
                    if (it->isExpressionValid() == true)
                    {
                        if (it->entryNotZero() == 0)
                            symbolTable[it->getSymbolNumber() - 1].setSymbolScopeToExtern();
                        symbolTable[it->getSymbolNumber() - 1].setSymbolValue(equSymbolValue);
                        symbolTable[it->getSymbolNumber() - 1].setDefinedToTrue();
                        keepGoing = true;
                        numOfUndefinedEquSymbols--;
                    }
                    else {
                        std::cout << "Equ expression is invalid!\n";
                        return;
                    }
                }
                else
                    symbolTable[it->getSymbolNumber() - 1].setSymbolValue(equSymbolValue);
            }
        }
    }
    if (numOfUndefinedEquSymbols > 0) {
        std::cout << "Equ expression(s) is(are) invalid!\n";
        return;
    }
}

void processEquForwardReferences()
{
    auto it = symbolTable.begin();
    for (; it != symbolTable.end(); it++)
    {
        if (it->getEqu() == false) continue;
        std::list<ForwardReferenceStruct> forwardReferences = it->getFowardsReferences();
        auto iter = forwardReferences.begin();
        for (; iter != forwardReferences.end(); iter++)
        {
            char lowerByte = (char)(outputFileData[iter->sectionNumber][iter->patch] & 0xFF);
            char higherByte = (char)(outputFileData[iter->sectionNumber][iter->patch + 1] & 0xFF);
            int dataValue = lowerByte | (higherByte << 8);
            int symbolValue = it->getSymbolValue();
            dataValue += (iter->sign == '+') ? symbolValue : -symbolValue;
            // Get the new value back in the file
            outputFileData[iter->sectionNumber][iter->patch] = (char)(dataValue & 0xFF);
            outputFileData[iter->sectionNumber][iter->patch + 1] = (char)((dataValue >> 8) & 0xFF);
        }
        // Update all of the relocation data for the symbol
        auto iterator = sectionRelocationTables.begin();
        for (; iterator != sectionRelocationTables.end(); iterator++)
        {
            for (int i = 0; i < iterator->second.size(); i++)
            {
                if (iterator->second[i].getSymbolNumber() != it->getNumber()) continue;
                int num = it->getNumber();
                // Relocation data is for the current symbol
                int entryNot0 = getEntryNot0(it->getNumber());
                if (entryNot0 != -1) {
                    if (entryNot0 != 0)
                    {
                        if (iterator->second[i].getType() == RelocationTableEntry::ABSOLUTE)
                            iterator->second[i].setSymbolNumber(entryNot0); // Relocation data 
                        else 
                        { // PC relative relocation data
                            if (iterator->first == entryNot0) 
                            { // No relocation data is needed
                                iterator->second.erase(iterator->second.begin() + i);
                                i--;
                            }
                            else
                                iterator->second[i].setSymbolNumber(entryNot0);
                        }
                    }
                }
                else {
                    iterator->second.erase(iterator->second.begin() + i);
                    i--;
                }
            }
        }
        it->resetForwardRefernces();
    }
}

int main()
{
    std::ifstream assemblyFile;
    assemblyFile.open("/home/student/Desktop/asm_program.txt");
    bool isOpen = assemblyFile.is_open();
    std::string line;
    while (getline(assemblyFile, line))
    {
        processLine(line);
    }
    processNonEquForwardReferences();
    processEquValue1();
    processEquValue2();
    processEquForwardReferences();
    assemblyFile.close();
    std::ofstream outputFile;
    outputFile.open("/home/student/Desktop/output_file.txt");
    outputFile << "Symbol Table:\n";
    outputFile << "Symbol Number\tSymbol Name\tSection Number\tSymbol Value\tSymbol Scope\n";
    auto symbolTableIterator = symbolTable.begin();
    for (; symbolTableIterator != symbolTable.end(); symbolTableIterator++)
    {
        outputFile << symbolTableIterator->getNumber() << "\t" << symbolTableIterator->getSymbolName() << "\t"
            << symbolTableIterator->getSectionNumber() << "\t" << symbolTableIterator->getSymbolValue() << "\t";
        if (symbolTableIterator->getSymbolScope() == SymbolTableEntry::LOCAL)
            outputFile << "LOCAL\n\n";
        else
            outputFile << "GLOBAL\n\n";
    }
    auto outputFileIterator = outputFileData.begin();
    for (; outputFileIterator != outputFileData.end(); outputFileIterator++)
    {
        outputFile << symbolTable[outputFileIterator->first - 1].getSymbolName() << ":\n";
        int size = outputFileIterator->second.size();
        for (int i = 0; i < size; i++)
        {
            char buffer[100];
            sprintf(buffer, "%d : %.2X", i, (unsigned char)outputFileIterator->second[i]);
            std::string outputFileLine = buffer;
            outputFile << outputFileLine << "\n";
        }
        outputFile << "\n";
        if (sectionRelocationTables.find(outputFileIterator->first) != sectionRelocationTables.end() &&
            sectionRelocationTables[outputFileIterator->first].size() > 0)
        {
            outputFile << symbolTable[outputFileIterator->first - 1].getSymbolName() << "'s Relocation Data:\n";
            outputFile << "Offset\tType\tSymbol Number\n";
            int size = sectionRelocationTables[outputFileIterator->first].size();
            for (int i = 0; i < size; i++)
            {
                char buffer[100];
                sprintf(buffer, "%X\t", sectionRelocationTables[outputFileIterator->first][i].getOffset());
                std::string outputFileLine = buffer;
                if (sectionRelocationTables[outputFileIterator->first][i].getType() == RelocationTableEntry::ABSOLUTE)
                    outputFileLine += "R_386_32\t";
                else
                    outputFileLine += "R_386_PC32\t";
                outputFile << outputFileLine << sectionRelocationTables[outputFileIterator->first][i].getSymbolNumber() << "\n";
            }
            outputFile << "\n";
        }
    }
    outputFile.close();
    return 0;
}