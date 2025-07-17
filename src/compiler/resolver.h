#pragma once
#ifndef clox_resolver_h
#define clox_resolver_h

#include "ast.h"

typedef struct ClassResolver ClassResolver;
typedef struct FunctionResolver FunctionResolver;

typedef struct {
    ObjString* shortName;
    ObjString* fullName;
} NameResolutionEntry;

typedef struct {
    int count;
    int capacity;
    NameResolutionEntry* entries;
} NameResolutionTable;

typedef struct {
    VM* vm;
    Token currentToken;
    ObjString* currentNamespace;
    ClassResolver* currentClass;
    FunctionResolver* currentFunction;
    SymbolTable* currentSymtab;
    SymbolTable* globalSymtab;
    SymbolTable* rootSymtab;
    NameResolutionTable* nameResolutionTable;
    Token rootClass;
    Token thisVar;
    Token superVar;
    int loopDepth;
    int switchDepth;
    int tryDepth;
    bool isTopLevel;
    bool debugSymtab;
    bool hadError;
} Resolver;

NameResolutionTable* newNameResolutionTable();
void freeNameResolutionTable(NameResolutionTable* table);
ObjString* nameResolutionTableGet(NameResolutionTable* table, ObjString* shortName);

void initResolver(VM* vm, Resolver* resolver, bool debugSymtab);
void resolveAst(Resolver* resolver, Ast* ast);
void resolveChild(Resolver* resolver, Ast* ast, int index);
NameResolutionTable* resolve(Resolver* resolver, Ast* ast);

#endif // !clox_resolver_h
