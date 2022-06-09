#include <string>
#include <list>

struct ForwardReferenceStruct {
    unsigned int patch;
    unsigned int sectionNumber;
    char sign;
    ForwardReferenceStruct(unsigned int _patch, unsigned int _sectionNumber, char _sign = '+') : patch(_patch), sectionNumber(_sectionNumber), sign(_sign) {}
};

class SymbolTableEntry {
public:
    enum Scope {
        LOCAL,
        GLOBAL,
        EXTERN
    };
    static unsigned int numGenerator; // Generates numbers starting from 1, ascending
    static const unsigned int UNDEFINED_SECTION_NUMBER;
   
    SymbolTableEntry(std::string _name) : name(_name), scope(LOCAL), defined(false), sectionNumber(0) {}
    
    SymbolTableEntry(std::string _name, ForwardReferenceStruct forwardReference) : name(_name), scope(LOCAL), defined(false), sectionNumber(0) { // Used when symbol is not yet defined, but it is referenced (for the first time)
        forwardReferences.push_back(forwardReference);
    }
    // Used when symbol is being defined and referenced (for the first time) at the same time
    SymbolTableEntry(std::string _name, unsigned int _sectionNumber, int _value) : name(_name), sectionNumber(_sectionNumber), value(_value), scope(LOCAL), defined(true) {}
    // Used when symbol is being referenced (for the first time) within an .global/.extern
    SymbolTableEntry(std::string _name, bool isExtern) : name(_name), defined(false) {
        if (isExtern == true) {
            sectionNumber = 0;
            scope = EXTERN;
        }
        else
            scope = GLOBAL;
    }

    unsigned int getNumber() {
        return number;
    }

    std::string getSymbolName() {
        return name;
    }

    void setSectionNumber(unsigned int _sectionNumber) {
        sectionNumber = _sectionNumber;
    }
    unsigned int getSectionNumber() {
        return sectionNumber;
    }

    void setSymbolValue(int _value) {
        value = _value;
    }
    int getSymbolValue() {
        return value;
    }

    void setSymbolScopeToExtern() {
        scope = EXTERN;
    }
    void setSymbolScopeToGlobal() {
        scope = GLOBAL;
    }
    Scope getSymbolScope() {
        return scope;
    }

    void setDefinedToTrue() {
        defined = true;
    }
    bool getDefined() {
        return defined;
    }

    std::list<ForwardReferenceStruct> getFowardsReferences() {
        return forwardReferences;
    }
    void addForwardReference(ForwardReferenceStruct forwardReference) {
        forwardReferences.push_back(forwardReference);
    }
    void resetForwardRefernces() {
        forwardReferences = std::list<ForwardReferenceStruct>();
    }

    void setEquToTrue() {
        isEqu = true;
    }
    bool getEqu() {
        return isEqu;
    }

private:
    unsigned int number = numGenerator++;
    std::string name;
    unsigned int sectionNumber;
    int value = 0;
    Scope scope;
    bool defined;
    // All of the forward references towards a symbol that is not yet defined are stored within a list;
    // each element of the list contains one unsigned int value which equals to the value of the location counter when referencing a non-defined symbol
    // once that symbol is defined (it has to be, otherwise it is an error), value of the symbol is being added on this very unsigned int value
    std::list<ForwardReferenceStruct> forwardReferences;
    bool isEqu = false; 
};

const unsigned int SymbolTableEntry::UNDEFINED_SECTION_NUMBER = 0;
unsigned int SymbolTableEntry::numGenerator = 1;