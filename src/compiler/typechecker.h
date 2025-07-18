#pragma once
#ifndef clox_typechecker_h
#define clox_typechecker_h

#include "ast.h"
#include "resolver.h"

typedef struct ClassTypeChecker ClassTypeChecker;
typedef struct FunctionTypeChecker FunctionTypeChecker;

typedef struct {
    VM* vm;
    Token currentToken;
    ObjString* currentNamespace;
    ClassTypeChecker* currentClass;
    FunctionTypeChecker* currentFunction;
    NameTable* nametab;

    TypeInfo* objectType;
    TypeInfo* nilType;
    TypeInfo* boolType;
    TypeInfo* numberType;
    TypeInfo* intType;
    TypeInfo* stringType;
    TypeInfo* classType;
    TypeInfo* functionType;
    TypeInfo* namespaceType;
    TypeInfo* traitType;
    TypeInfo* voidType;

    bool debugTypetab;
    bool hadError;
} TypeChecker;

void initTypeChecker(VM* vm, TypeChecker* typeChecker, NameTable* nametab, bool debugTypetab);
void typeCheckAst(TypeChecker* typeChecker, Ast* ast);
void typeCheckChild(TypeChecker* typeChecker, Ast* ast, int index);
void typeCheck(TypeChecker* typeChecker, Ast* ast);

#endif // !clox_typechecker_h