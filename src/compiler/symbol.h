#pragma once
#ifndef clox_symbol_h
#define clox_symbol_h

#include "token.h"
#include "type.h"
#include "../vm/value.h"

typedef struct SymbolTable SymbolTable;

typedef enum {
    SYMBOL_CATEGORY_NONE,
    SYMBOL_CATEGORY_LOCAL,
    SYMBOL_CATEGORY_UPVALUE,
    SYMBOL_CATEGORY_GLOBAL,
    SYMBOL_CATEGORY_FIELD,
    SYMBOL_CATEGORY_METHOD
} SymbolCategory;

typedef enum {
    SYMBOL_SCOPE_GLOBAL,
    SYMBOL_SCOPE_MODULE,
    SYMBOL_SCOPE_CLASS,
    SYMBOL_SCOPE_TRAIT,
    SYMBOL_SCOPE_FUNCTION,
    SYMBOL_SCOPE_METHOD,
    SYMBOL_SCOPE_BLOCK
} SymbolScope;

typedef enum {
    SYMBOL_STATE_DECLARED,
    SYMBOL_STATE_DEFINED,
    SYMBOL_STATE_ACCESSED,
    SYMBOL_STATE_MODIFIED
} SymbolState;

typedef struct {
    Token token;
    SymbolCategory category;
    SymbolState state;
    bool isMutable;
    TypeInfo* type;
} SymbolItem;

typedef struct {
    ObjString* key;
    SymbolItem* value;
} SymbolEntry;

struct SymbolTable {
    int id;
    struct SymbolTable* parent;
    SymbolScope scope;
    uint8_t depth;
    int count;
    int capacity;
    SymbolEntry* entries;
};

SymbolItem* newSymbolItem(Token token, SymbolCategory category, SymbolState state, bool isMutable);
SymbolItem* newSymbolItemWithType(Token token, SymbolCategory category, SymbolState state, bool isMutable, TypeInfo* type);
void freeSymbolItem(SymbolItem* item);
SymbolTable* newSymbolTable(int id, SymbolTable* parent, SymbolScope scope, uint8_t depth);
void freeSymbolTable(SymbolTable* symTab);
SymbolItem* symbolTableGet(SymbolTable* symtab, ObjString* key);
bool symbolTableSet(SymbolTable* symtab, ObjString* key, SymbolItem* value);
SymbolItem* symbolTableLookup(SymbolTable* symtab, ObjString* key);
void symbolTableOutput(SymbolTable* symtab);

static inline bool isClassScope(SymbolScope scope) {
    return (scope == SYMBOL_SCOPE_CLASS || scope == SYMBOL_SCOPE_TRAIT);
}

static inline bool isFunctionScope(SymbolScope scope) {
    return (scope == SYMBOL_SCOPE_FUNCTION || scope == SYMBOL_SCOPE_METHOD);
}

#endif // !clox_symbol_h