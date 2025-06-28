#pragma once
#ifndef clox_type_h
#define clox_type_h

#include "../common/buffer.h"
#include "../common/common.h"
#include "../vm/value.h"

typedef struct TypeInfo TypeInfo;
typedef struct TypeTable TypeTable;
DECLARE_BUFFER(TypeInfoArray, TypeInfo*)

#define IS_BEHAVIOR_TYPE(type) (type->category == TYPE_CATEGORY_CLASS || type->category == TYPE_CATEGORY_METACLASS || type->category == TYPE_CATEGORY_TRAIT)
#define IS_CALLABLE_TYPE(type) (type->category == TYPE_CATEGORY_FUNCTION || type->category == TYPE_CATEGORY_METHOD)
#define IS_FIELD_TYPE(type) (type->category == TYPE_CATEGORY_FIELD)
#define IS_VOID_TYPE(type) (type->category == TYPE_CATEGORY_VOID)

#define AS_BEHAVIOR_TYPE(type) ((BehaviorTypeInfo*)type)
#define AS_CALLABLE_TYPE(type) ((CallableTypeInfo*)type)
#define AS_FIELD_TYPE(type) ((FieldTypeInfo*)type)

typedef enum {
    TYPE_CATEGORY_NONE,
    TYPE_CATEGORY_CLASS,
    TYPE_CATEGORY_METACLASS,
    TYPE_CATEGORY_TRAIT,
    TYPE_CATEGORY_FUNCTION,
    TYPE_CATEGORY_FIELD,
    TYPE_CATEGORY_METHOD,
    TYPE_CATEGORY_VOID
} TypeCategory;

struct TypeInfo {
    int id;
    TypeCategory category;
    ObjString* shortName;
    ObjString* fullName;
};

typedef struct {
    TypeInfo baseType;
    TypeInfo* superclassType;
    TypeInfoArray* traitTypes;
    TypeTable* fields;
    TypeTable* methods;
} BehaviorTypeInfo;

typedef struct {
    bool isAsync;
    bool isClassMethod;
    bool isGenerator;
    bool isInitializer;
    bool isInstanceMethod;
    bool isLambda;
    bool isVariadic;
    bool isVoid;
} CallableTypeAttribute;

typedef struct {
    TypeInfo baseType;
    TypeInfo* returnType;
    TypeInfoArray* paramTypes;
    CallableTypeAttribute attribute;
} CallableTypeInfo;

typedef struct {
    TypeInfo baseType;
    TypeInfo* declaredType;
    int index;
    bool isMutable;
    bool hasInitializer;
} FieldTypeInfo;

typedef struct {
    ObjString* key;
    TypeInfo* value;
} TypeEntry;

struct TypeTable {
    int id;
    int count;
    int capacity;
    TypeEntry* entries;
};

static inline CallableTypeAttribute callableTypeInitModifier() {
    CallableTypeAttribute attribute = {
        .isAsync = false,
        .isClassMethod = false,
        .isGenerator = false,
        .isInitializer = false,
        .isInstanceMethod = false,
        .isLambda = false,
        .isVariadic = false,
        .isVoid = false
    };
    return attribute;
}

TypeInfo* newTypeInfo(int id, size_t size, TypeCategory category, ObjString* shortName, ObjString* fullName);
BehaviorTypeInfo* newBehaviorTypeInfo(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType);
BehaviorTypeInfo* newBehaviorTypeInfoWithTraits(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType, int numTraits, ...);
BehaviorTypeInfo* newBehaviorTypeInfoWithMethods(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType, TypeTable* methods);
CallableTypeInfo* newCallableTypeInfo(int id, TypeCategory category, ObjString* name, TypeInfo* returnType);
CallableTypeInfo* newCallableTypeInfoWithParams(int id, TypeCategory category, ObjString* name, TypeInfo* returnType, int numParams, ...);
FieldTypeInfo* newFieldTypeInfo(int id, ObjString* name, TypeInfo* declaredType, bool isMutable, bool hasInitializer);
void freeTypeInfo(TypeInfo* type);
TypeTable* newTypeTable(int id);
void freeTypeTable(TypeTable* typeTable);
TypeInfo* typeTableGet(TypeTable* typetab, ObjString* key);
bool typeTableSet(TypeTable* typetab, ObjString* key, TypeInfo* value);
TypeInfo* typeTableMethodLookup(TypeInfo* type, ObjString* key);
BehaviorTypeInfo* typeTableInsertBehavior(TypeTable* typetab, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType);
CallableTypeInfo* typeTableInsertCallable(TypeTable* typetab, TypeCategory category, ObjString* name, TypeInfo* returnType);
FieldTypeInfo* typeTableInsertField(TypeTable* typetab, ObjString* name, TypeInfo* declaredType, bool isMutable, bool hasInitializer);
void typeTableOutput(TypeTable* typetab);

bool isEqualType(TypeInfo* type, TypeInfo* type2);
bool isSubtypeOfType(TypeInfo* type, TypeInfo* type2);

#endif // !clox_type_h