class RelocationTableEntry {
public:

    enum Type {
        RELATIVE,
        ABSOLUTE
    };

    RelocationTableEntry(unsigned int _offset, Type _type, unsigned int _symbolNumber) : offset(_offset), type(_type), symbolNumber(_symbolNumber) {}

    unsigned int getOffset() {
        return offset;
    }

    Type getType() {
        return type;
    }

    unsigned int getSymbolNumber() {
        return symbolNumber;
    }
    void setSymbolNumber(unsigned int _symbolNumber) {
        symbolNumber = _symbolNumber;
    }

private:
    unsigned int offset;
    Type type;
    unsigned int symbolNumber;
};