#include <string>
#include <vector>
#include <list>

struct ClassificationIndexStruct {
    unsigned int sectionNumber;
    int classificationIndex;
    ClassificationIndexStruct(unsigned int _sectionNumber, int _classificationIndex) : sectionNumber(_sectionNumber), classificationIndex(_classificationIndex) {}
};

struct HelpStruct {
    unsigned int locationCounter;
    unsigned int sectionNumber;
    HelpStruct(unsigned int _locationCounter, unsigned int _sectionNumber) : locationCounter(_locationCounter), sectionNumber(_sectionNumber) {}
};

class EquTableEntry {
public:
    unsigned int getSymbolNumber() {
        return symbolNumber;
    }

    std::vector<char> getSymbolSigns() {
        return symbolSigns;
    }
    void removeSymbolSign(int position) {
        symbolSigns.erase(symbolSigns.begin() + position);
    }

    std::vector<unsigned int> getSymbolDependencies() {
        return symbolDependencies;
    }
    void removeSymbolDependency(int position) {
        symbolDependencies.erase(symbolDependencies.begin() + position);
    }

    std::vector<ClassificationIndexStruct> getClassIndexTable() {
        return classificationIndexTable;
    }
    void setClassIndexTable(std::vector<ClassificationIndexStruct>& _classificationIndexTable) {
        classificationIndexTable = _classificationIndexTable;
    }
    void updateClassIndexTableEntry(unsigned int sectionNumber, int value) {
        auto iter = classificationIndexTable.begin();
        for (; iter != classificationIndexTable.end(); iter++)
            if (iter->sectionNumber == sectionNumber)
            {
                iter->classificationIndex += value;
                break;
            }
        if (iter == classificationIndexTable.end())
            classificationIndexTable.push_back(ClassificationIndexStruct(sectionNumber, value));
    }

    std::list<HelpStruct> getForwardReferences() {
        return forwardsReferences;
    }
    void addForwardReference(HelpStruct forwardRef) {
        forwardsReferences.insert(forwardsReferences.end(), forwardRef);
    }

    bool isExpressionValid() {
        bool entryNot0 = false;
        for (int i = 0; i < classificationIndexTable.size(); i++)
        {
            if (classificationIndexTable[i].classificationIndex > 1 || classificationIndexTable[i].classificationIndex < -1) return false;
            if (classificationIndexTable[i].classificationIndex == 1 || classificationIndexTable[i].classificationIndex == -1)
            {
                if (entryNot0 == false)
                    entryNot0 = true;
                else
                    return false;
            }
        }
        return true;
    }

    int entryNotZero() {
        for (int i = 0; i < classificationIndexTable.size(); i++)
            if (classificationIndexTable[i].classificationIndex == 1 || classificationIndexTable[i].classificationIndex == -1)
                return classificationIndexTable[i].sectionNumber;
        return -1;
    }

    EquTableEntry(unsigned int _symbolNumber, std::vector<char>& _symbolSigns, std::vector<unsigned int> _symbolDependencies, 
        std::vector<ClassificationIndexStruct> _classificationIndexTable) : symbolNumber(_symbolNumber), symbolSigns(_symbolSigns), symbolDependencies(_symbolDependencies), classificationIndexTable(_classificationIndexTable) {}

private:
    unsigned int symbolNumber;
    std::vector<char> symbolSigns;
    std::vector<unsigned int> symbolDependencies;
    std::vector<ClassificationIndexStruct> classificationIndexTable;
    std::list<HelpStruct> forwardsReferences;
};