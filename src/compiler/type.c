#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "type.h"
#include "../vm/object.h"

#define TYPE_TABLE_MAX_LOAD 0.75
DEFINE_BUFFER(TypeInfoArray, TypeInfo*)

TypeInfo* newTypeInfo(int id, size_t size, TypeCategory category, ObjString* shortName, ObjString* fullName) {
    TypeInfo* type = (TypeInfo*)malloc(size);
    if (type != NULL) {
        type->id = id;
        type->category = category;
        type->shortName = shortName;
        type->fullName = fullName;
    }
    return type;
}

BehaviorTypeInfo* newBehaviorTypeInfo(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType) {
    BehaviorTypeInfo* behaviorType = (BehaviorTypeInfo*)newTypeInfo(id, sizeof(BehaviorTypeInfo), category, shortName, fullName);
    if (behaviorType != NULL) {
        behaviorType->superclassType = superclassType;
        behaviorType->traitTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        if (behaviorType->traitTypes != NULL) TypeInfoArrayInit(behaviorType->traitTypes);

        behaviorType->fields = newTypeTable(-1);
        behaviorType->methods = newTypeTable(id);
    }
    return behaviorType;
}

BehaviorTypeInfo* newBehaviorTypeInfoWithTraits(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType, int numTraits, ...) {
    BehaviorTypeInfo* behaviorType = (BehaviorTypeInfo*)newTypeInfo(id, sizeof(BehaviorTypeInfo), category, shortName, fullName);
    if (behaviorType != NULL) {
        behaviorType->superclassType = superclassType;
        behaviorType->traitTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        behaviorType->fields = newTypeTable(-1);
        behaviorType->methods = newTypeTable(id);

        if (behaviorType->traitTypes != NULL) {
            TypeInfoArrayInit(behaviorType->traitTypes);
            va_list args;
            va_start(args, numTraits);

            for (int i = 0; i < numTraits; i++) {
                TypeInfo* type = va_arg(args, TypeInfo*);
                TypeInfoArrayAdd(behaviorType->traitTypes, type);
            }
            va_end(args);
        }
    }
    return behaviorType;
}

BehaviorTypeInfo* newBehaviorTypeInfoWithMethods(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType, TypeTable* methods) {
    BehaviorTypeInfo* behaviorType = (BehaviorTypeInfo*)newTypeInfo(id, sizeof(BehaviorTypeInfo), category, shortName, fullName);
    if (behaviorType != NULL) {
        behaviorType->superclassType = superclassType;
        behaviorType->traitTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        if (behaviorType->traitTypes != NULL) TypeInfoArrayInit(behaviorType->traitTypes);

        behaviorType->fields = newTypeTable(-1);
        behaviorType->methods = methods;
    }
    return behaviorType;
}

CallableTypeInfo* newCallableTypeInfo(int id, TypeCategory category, ObjString* name, TypeInfo* returnType) {
    CallableTypeInfo* callableType = (CallableTypeInfo*)newTypeInfo(id, sizeof(CallableTypeInfo), category, name, name);
    if (callableType != NULL) {
        callableType->returnType = returnType;
        callableType->paramTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        callableType->attribute = callableTypeInitAttribute();
        if (callableType->paramTypes != NULL) TypeInfoArrayInit(callableType->paramTypes);
    }
    return callableType;
}

FieldTypeInfo* newFieldTypeInfo(int id, ObjString* name, TypeInfo* declaredType, bool isMutable, bool hasInitializer) {
    FieldTypeInfo* fieldType = (FieldTypeInfo*)newTypeInfo(id, sizeof(FieldTypeInfo), TYPE_CATEGORY_FIELD, name, name);
    if (fieldType != NULL) {
        fieldType->declaredType = declaredType;
        fieldType->index = id - 1;
        fieldType->isMutable = isMutable;
        fieldType->hasInitializer = hasInitializer;
    }
    return fieldType;
}

MethodTypeInfo* newMethodTypeInfo(int id, ObjString* name, TypeInfo* returnType, bool isClass, bool isInitializer) {
    MethodTypeInfo* methodType = (MethodTypeInfo*)newTypeInfo(id, sizeof(MethodTypeInfo), TYPE_CATEGORY_METHOD, name, name);
    if (methodType != NULL) {
        methodType->declaredType = newCallableTypeInfo(-1, TYPE_CATEGORY_FUNCTION, name, returnType);
        methodType->isClass = isClass;
        methodType->isInitializer = isInitializer;
    }
    return methodType;
}

GenericTypeInfo* newGenericTypeInfo(int id, ObjString* shortName, ObjString* fullName, TypeInfo* rawType) {
    GenericTypeInfo* genericType = (GenericTypeInfo*)newTypeInfo(id, sizeof(GenericTypeInfo), TYPE_CATEGORY_GENERIC, shortName, fullName);
    if (genericType != NULL) {
        genericType->rawType = rawType;
        genericType->parameters = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        if (genericType->parameters != NULL) TypeInfoArrayInit(genericType->parameters);
    }
    return genericType;
}

GenericTypeInfo* newGenericTypeInfoWithParameters(int id, ObjString* shortName, ObjString* fullName, TypeInfo* rawType, int numParameters, ...) {
    GenericTypeInfo* genericType = (GenericTypeInfo*)newTypeInfo(id, sizeof(GenericTypeInfo), TYPE_CATEGORY_GENERIC, shortName, fullName);
    if (genericType != NULL) {
        genericType->rawType = rawType;
        genericType->parameters = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        
        if (genericType->parameters != NULL) {
            TypeInfoArrayInit(genericType->parameters);
            va_list args;
            va_start(args, numParameters);

            for (int i = 0; i < numParameters; i++) {
                TypeInfo* type = va_arg(args, TypeInfo*);
                TypeInfoArrayAdd(genericType->parameters, type);
            }
            va_end(args);
        }
    }
}

AliasTypeInfo* newAliasTypeInfo(int id, ObjString* shortName, ObjString* fullName, TypeInfo* targetType) {
    AliasTypeInfo* aliasType = (AliasTypeInfo*)newTypeInfo(id, sizeof(AliasTypeInfo), TYPE_CATEGORY_ALIAS, shortName, fullName);
    if (aliasType != NULL) aliasType->targetType = targetType;
    return aliasType;
}

char* createCallableTypeName(CallableTypeInfo* callableType) {
    char* callableName = bufferNewCString(UINT16_MAX);
    size_t length = 0;

    if (callableType->returnType != NULL) {
        char* returnTypeName = IS_CALLABLE_TYPE(callableType->returnType) ? createCallableTypeName(AS_CALLABLE_TYPE(callableType->returnType)) : callableType->returnType->shortName->chars;
        size_t returnTypeLength = strlen(returnTypeName);
        memcpy(callableName, returnTypeName, returnTypeLength);
        length += returnTypeLength;
        if (IS_CALLABLE_TYPE(callableType->returnType)) free(returnTypeName);
    }
    else {
        memcpy(callableName, "dynamic", 7);
        length += 7;
    }

    memcpy(callableName + length, " fun(", 5);
    length += 5;
    if (callableType->attribute.isVariadic) {
        memcpy(callableName + length, "...", 3);
        length += 3;
    }

    for (int i = 0; i < callableType->paramTypes->count; i++) {
        TypeInfo* paramType = callableType->paramTypes->elements[i];
        if (i > 0) {
            callableName[length++] = ',';
            callableName[length++] = ' ';
        }
        if (paramType != NULL) {
            char* paramTypeName = IS_CALLABLE_TYPE(paramType) ? createCallableTypeName(AS_CALLABLE_TYPE(paramType)) : paramType->shortName->chars;
            size_t paramTypeLength = strlen(paramTypeName);
            memcpy(callableName + length, paramTypeName, paramTypeLength);
            length += paramTypeLength;
            if (IS_CALLABLE_TYPE(paramType)) free(paramTypeName);
        }
        else {
            memcpy(callableName + length, "dynamic", 7);
            length += 7;
        }
    }

    callableName[length++] = ')';
    callableName[length] = '\0';
    return callableName;
}

void freeTypeInfo(TypeInfo* type) {
    if (type == NULL) return;
    if (IS_BEHAVIOR_TYPE(type)) {
        BehaviorTypeInfo* behaviorType = AS_BEHAVIOR_TYPE(type);
        if (behaviorType->traitTypes != NULL) TypeInfoArrayFree(behaviorType->traitTypes);
        freeTypeTable(behaviorType->fields);
        freeTypeTable(behaviorType->methods);
        free(behaviorType);
    }
    else if (IS_CALLABLE_TYPE(type)) {
        CallableTypeInfo* callableType = AS_CALLABLE_TYPE(type);
        if (callableType->paramTypes != NULL) TypeInfoArrayFree(callableType->paramTypes);
        free(callableType);
    }
    else {
        free(type);
    }
}

void freeTempTypes(TypeInfoArray* typeArray) {
    for (int i = 0; i < typeArray->count; i++) {
        TypeInfo* type = typeArray->elements[i];
        if (type != NULL) freeTypeInfo(type);
    }
    free(typeArray);
}

void freeAliasTypes(TypeInfoArray* aliasTypes) {
    for (int i = 0; i < aliasTypes->count; i++) {
        TypeInfo* type = aliasTypes->elements[i];
        if (type != NULL) freeTypeInfo(type);
    }
    free(aliasTypes);
}

TypeTable* newTypeTable(int id) {
    TypeTable* typetab = (TypeTable*)malloc(sizeof(TypeTable));
    if (typetab != NULL) {
        typetab->id = id;
        typetab->count = 0;
        typetab->capacity = 0;
        typetab->entries = NULL;
    }
    return typetab;
}

void freeTypeTable(TypeTable* typetab) {
    for (int i = 0; i < typetab->capacity; i++) {
        TypeEntry* entry = &typetab->entries[i];
        if (entry != NULL) freeTypeInfo(entry->value);
    }

    if (typetab->entries != NULL) free(typetab->entries);
    free(typetab);
}

static TypeEntry* findTypeEntry(TypeEntry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash & (capacity - 1);
    for (;;) {
        TypeEntry* entry = &entries[index];
        if (entry->key == key || entry->key == NULL) {
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

static void typeTableAdjustCapacity(TypeTable* typetab, int capacity) {
    int oldCapacity = typetab->capacity;
    TypeEntry* entries = (TypeEntry*)malloc(sizeof(TypeTable) * capacity);
    if (entries == NULL) exit(1);

    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NULL;
    }
    typetab->count = 0;

    for (int i = 0; i < typetab->capacity; i++) {
        TypeEntry* entry = &typetab->entries[i];
        if (entry->key == NULL) continue;
        TypeEntry* dest = findTypeEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        typetab->count++;
    }

    free(typetab->entries);
    typetab->capacity = capacity;
    typetab->entries = entries;
}

TypeInfo* typeTableGet(TypeTable* typetab, ObjString* key) {
    if (typetab->count == 0) return NULL;
    TypeEntry* entry = findTypeEntry(typetab->entries, typetab->capacity, key);
    if (entry->key == NULL) return NULL;
    return entry->value;
}

bool typeTableSet(TypeTable* typetab, ObjString* key, TypeInfo* value) {
    if (typetab->count + 1 > typetab->capacity * TYPE_TABLE_MAX_LOAD) {
        int capacity = bufferGrowCapacity(typetab->capacity);
        typeTableAdjustCapacity(typetab, capacity);
    }

    TypeEntry* entry = findTypeEntry(typetab->entries, typetab->capacity, key);
    if (entry->key != NULL) return false;
    typetab->count++;

    entry->key = key;
    entry->value = value;
    return true;
}

void typeTableAddAll(TypeTable* from, TypeTable* to) {
    for (int i = 0; i < from->capacity; i++) {
        TypeEntry* entry = &from->entries[i];
        if (entry != NULL && entry->key != NULL) {
            FieldTypeInfo* fromFieldType = AS_FIELD_TYPE(entry->value);
            FieldTypeInfo* fieldType = newFieldTypeInfo(fromFieldType->baseType.id, entry->key, fromFieldType->declaredType, fromFieldType->isMutable, fromFieldType->hasInitializer);
            typeTableSet(to, entry->key, (TypeInfo*)fieldType);       
        }
    }
}

TypeInfo* typeTableMethodLookup(TypeInfo* type, ObjString* key) {
    if (type == NULL || !IS_BEHAVIOR_TYPE(type)) return NULL;
    BehaviorTypeInfo* behaviorType = AS_BEHAVIOR_TYPE(type);
    TypeInfo* methodType = typeTableGet(behaviorType->methods, key);
    if (methodType != NULL) return methodType;

    if (behaviorType->traitTypes != NULL) {
        for (int i = 0; i < behaviorType->traitTypes->count; i++) {
            BehaviorTypeInfo* traitType = AS_BEHAVIOR_TYPE(behaviorType->traitTypes->elements[i]);
            methodType = typeTableGet(traitType->methods, key);
            if (methodType != NULL) return methodType;
        }
    }

    return typeTableMethodLookup(behaviorType->superclassType, key);
}

BehaviorTypeInfo* typeTableInsertBehavior(TypeTable* typetab, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType) {
    int id = typetab->count + 1;
    BehaviorTypeInfo* behaviorType = newBehaviorTypeInfo(id, category, shortName, fullName, superclassType);
    typeTableSet(typetab, fullName, (TypeInfo*)behaviorType);
    return behaviorType;
}

CallableTypeInfo* typeTableInsertCallable(TypeTable* typetab, TypeCategory category, ObjString* name, TypeInfo* returnType) {
    int id = typetab->count + 1;
    CallableTypeInfo* callableType = newCallableTypeInfo(id, category, name, returnType);
    typeTableSet(typetab, name, (TypeInfo*)callableType);
    return callableType;
}

FieldTypeInfo* typeTableInsertField(TypeTable* typetab, ObjString* name, TypeInfo* declaredType, bool isMutable, bool hasInitializer) {
    int id = typetab->count + 1;
    FieldTypeInfo* fieldType = newFieldTypeInfo(id, name, declaredType, isMutable, hasInitializer);
    typeTableSet(typetab, name, (TypeInfo*)fieldType);
    return fieldType;
}

MethodTypeInfo* typeTableInsertMethod(TypeTable* typetab, ObjString* name, TypeInfo* returnType, bool isClass, bool isInitializer) {
    int id = typetab->count + 1;
    MethodTypeInfo* methodType = newMethodTypeInfo(id, name, returnType, isClass, isInitializer);
    typeTableSet(typetab, name, (TypeInfo*)methodType);
    return methodType;
}

AliasTypeInfo* typeTableInsertAlias(TypeTable* typetab, ObjString* shortName, ObjString* fullName, TypeInfo* targetType) {
    int id = typetab->count + 1;
    AliasTypeInfo* aliasType = newAliasTypeInfo(id, shortName, fullName, targetType);
    typeTableSet(typetab, fullName, (TypeInfo*)aliasType);
    return aliasType;
}

static void typeTableOutputCategory(TypeCategory category) {
    switch (category) {
        case TYPE_CATEGORY_CLASS:
            printf("class");
            break;
        case TYPE_CATEGORY_METACLASS:
            printf("metaclass");
            break;
        case TYPE_CATEGORY_TRAIT:
            printf("trait");
            break;
        case TYPE_CATEGORY_FUNCTION:
            printf("function");
            break;
        case TYPE_CATEGORY_METHOD:
            printf("method");
            break;
        case TYPE_CATEGORY_GENERIC:
            printf("generic");
            break;
        case TYPE_CATEGORY_ALIAS:
            printf("typealias");
            break;
        case TYPE_CATEGORY_VOID:
            printf("void");
            break;
        default:
            printf("none");
    }
    printf("\n");
}

static void typeTableOutputFields(TypeTable* fields) {
    printf("    fields:\n");
    for (int i = 0; i < fields->capacity; i++) {
        TypeEntry* entry = &fields->entries[i];
        if (entry != NULL && entry->key != NULL) {
            FieldTypeInfo* field = AS_FIELD_TYPE(entry->value);
            printf("      %s", field->isMutable ? "var " : "val ");
            if (field->declaredType != NULL) printf("%s ", field->declaredType->shortName->chars);
            printf("%s(index: %d)\n", entry->key->chars, field->index);
        }
    }
}

static void typeTableOutputMethods(TypeTable* methods) {
    printf("    methods:\n");
    for (int i = 0; i < methods->capacity; i++) {
        TypeEntry* entry = &methods->entries[i];
        if (entry != NULL && entry->key != NULL) {
            MethodTypeInfo* method = AS_METHOD_TYPE(entry->value);
            printf("      %s", method->declaredType->attribute.isAsync ? "async " : "");

            if (method->declaredType->returnType == NULL) printf("dynamic ");
            else if (method->declaredType->attribute.isVoid) printf("void ");
            else printf("%s ", method->declaredType->returnType->shortName->chars);
            printf("%s(", entry->key->chars);

            if (method->declaredType->paramTypes->count > 0) {
                printf("%s", (method->declaredType->paramTypes->elements[0] == NULL) ? "dynamic" : method->declaredType->paramTypes->elements[0]->shortName->chars);
                for (int i = 1; i < method->declaredType->paramTypes->count; i++) {
                    printf(", %s", (method->declaredType->paramTypes->elements[i] == NULL) ? "dynamic" : method->declaredType->paramTypes->elements[i]->shortName->chars);
                }
            }
            printf(")\n");
        }
    }
}

static void typeTableOutputBehavior(BehaviorTypeInfo* behavior) {
    if (behavior->superclassType != NULL) {
        printf("    superclass: %s\n", behavior->superclassType->fullName->chars);
    }

    if (behavior->traitTypes != NULL && behavior->traitTypes->count > 0) {
        printf("    traits: %s", behavior->traitTypes->elements[0]->fullName->chars);
        for (int i = 1; i < behavior->traitTypes->count; i++) {
            printf(", %s", behavior->traitTypes->elements[i]->fullName->chars);
        }
        printf("\n");
    }

    if (behavior->fields != NULL && behavior->fields->count > 0) {
        typeTableOutputFields(behavior->fields);
    }

    if (behavior->methods != NULL && behavior->methods->count > 0) {
        typeTableOutputMethods(behavior->methods);
    }
}

static void typeTableOutputFunction(CallableTypeInfo* function) {
    printf("    signature: ");
    if (function->returnType == NULL) printf("dynamic ");
    else if (function->attribute.isVoid) printf("void ");
    else printf("%s ", function->returnType->shortName->chars);
    printf("%s(", function->baseType.shortName->chars);

    if (function->paramTypes != NULL && function->paramTypes->count > 0) {
        printf("%s", (function->paramTypes->elements[0] != NULL) ? function->paramTypes->elements[0]->shortName->chars : "dynamic");
        for (int i = 1; i < function->paramTypes->count; i++) {
            printf(", %s", (function->paramTypes->elements[i] != NULL) ? function->paramTypes->elements[i]->shortName->chars : "dynamic");
        }
    } 
    printf(")\n");
}

static void typeTableOutputAlias(AliasTypeInfo* alias) {
    if (alias->targetType != NULL) {
        printf("    target: %s\n", alias->targetType->shortName->chars);
    }
}

static void typeTableOutputEntry(TypeEntry* entry) {
    printf("  %s", entry->value->shortName->chars);
    if (IS_BEHAVIOR_TYPE(entry->value)) printf("(%s)", entry->value->fullName->chars);
    printf("\n    id: %d\n    category: ", entry->value->id);
    typeTableOutputCategory(entry->value->category);

    if (IS_BEHAVIOR_TYPE(entry->value)) typeTableOutputBehavior(AS_BEHAVIOR_TYPE(entry->value));
    else if (IS_CALLABLE_TYPE(entry->value)) typeTableOutputFunction(AS_CALLABLE_TYPE(entry->value));
    else if (IS_ALIAS_TYPE(entry->value)) typeTableOutputAlias(AS_ALIAS_TYPE(entry->value));
    printf("\n");
}

void typeTableOutput(TypeTable* typetab) {
    printf("type table(count: %d)\n", typetab->count);

    for (int i = 0; i < typetab->capacity; i++) {
        TypeEntry* entry = &typetab->entries[i];
        if (entry != NULL && entry->key != NULL) {
            typeTableOutputEntry(entry);
        }
    }

    printf("\n");
}

bool isEqualType(TypeInfo* type, TypeInfo* type2) {
    if (type == NULL || type2 == NULL) return true;
    type = IS_ALIAS_TYPE(type) ? AS_ALIAS_TYPE(type)->targetType : type;
    type2 = IS_ALIAS_TYPE(type2) ? AS_ALIAS_TYPE(type2)->targetType : type2;

    if (IS_BEHAVIOR_TYPE(type) && IS_BEHAVIOR_TYPE(type2)) return (type->id == type2->id);
    else if (IS_CALLABLE_TYPE(type) && IS_CALLABLE_TYPE(type2)) {
        CallableTypeInfo* callableType = AS_CALLABLE_TYPE(type);
        CallableTypeInfo* callableType2 = AS_CALLABLE_TYPE(type2);
        if (!isEqualType(callableType->returnType, callableType2->returnType)) return false;
        if (callableType->paramTypes->count != callableType2->paramTypes->count) return false;

        for (int i = 0; i < callableType->paramTypes->count; i++) {
            if (!isEqualType(callableType->paramTypes->elements[i], callableType2->paramTypes->elements[i])) return false;
        }
        return true;
    }
    else return false;
}

bool isSubtypeOfType(TypeInfo* type, TypeInfo* type2) {
    if (isEqualType(type, type2)) return true;
    type = IS_ALIAS_TYPE(type) ? AS_ALIAS_TYPE(type)->targetType : type;
    type2 = IS_ALIAS_TYPE(type2) ? AS_ALIAS_TYPE(type2)->targetType : type2;

    if (memcmp(type->shortName->chars, "Nil", 3) == 0) return true;
    if (memcmp(type2->shortName->chars, "Object", 3) == 0) return true;
    if (IS_CALLABLE_TYPE(type) && IS_BEHAVIOR_TYPE(type2) && (strcmp(type2->shortName->chars, "Function") == 0 || strcmp(type2->shortName->chars, "TCallable") == 0)) return true;
    if (!IS_BEHAVIOR_TYPE(type) || !IS_BEHAVIOR_TYPE(type2)) return false; 
   
    BehaviorTypeInfo* subtype = AS_BEHAVIOR_TYPE(type);
    BehaviorTypeInfo* supertype = AS_BEHAVIOR_TYPE(type2);
    TypeInfo* superclassType = subtype->superclassType;
    while (superclassType != NULL) {
        if (superclassType->id == type2->id) return true;
        superclassType = AS_BEHAVIOR_TYPE(superclassType)->superclassType;
    }

    if (subtype->traitTypes != NULL && subtype->traitTypes->count > 0) {
        for (int i = 0; i < subtype->traitTypes->count; i++) {
            if (subtype->traitTypes->elements[i]->id == type2->id) return true;
        }
    }
    return false;
}