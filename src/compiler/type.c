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

CallableTypeInfo* newCallableTypeInfoWithParams(int id, TypeCategory category, ObjString* name, TypeInfo* returnType, int numParams, ...) {
    CallableTypeInfo* callableType = (CallableTypeInfo*)newTypeInfo(id, sizeof(CallableTypeInfo), category, name, name);
    if (callableType != NULL) {
        callableType->returnType = returnType;
        callableType->paramTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        callableType->attribute = callableTypeInitAttribute();

        if (callableType->paramTypes != NULL) {
            TypeInfoArrayInit(callableType->paramTypes);
            va_list args;
            va_start(args, numParams);
            for (int i = 0; i < numParams; i++) {
                TypeInfo* type = va_arg(args, TypeInfo*);
                TypeInfoArrayAdd(callableType->paramTypes, type);
            }
            va_end(args);
        }
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
    else if (IS_FIELD_TYPE(type)) {
        FieldTypeInfo* fieldType = AS_FIELD_TYPE(type);
        free(fieldType);
    }
    else {
        free(type);
    }
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
        if (entry != NULL) {
            freeTypeInfo(entry->value);
        }
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

static void typeTableOutputMethod(TypeTable* methods) {
    printf("    methods:\n");
    for (int i = 0; i < methods->capacity; i++) {
        TypeEntry* entry = &methods->entries[i];
        if (entry != NULL && entry->key != NULL) {
            CallableTypeInfo* method = AS_CALLABLE_TYPE(entry->value);
            printf("      %s", method->attribute.isAsync ? "async " : "");

            if (method->returnType == NULL) printf("dynamic ");
            else if (method->attribute.isVoid) printf("void ");
            else printf("%s ", method->returnType->shortName->chars);
            printf("%s(", entry->key->chars);

            if (method->paramTypes->count > 0) {
                printf("%s", (method->paramTypes->elements[0] == NULL) ? "dynamic" : method->paramTypes->elements[0]->shortName->chars);
                for (int i = 1; i < method->paramTypes->count; i++) {
                    printf(", %s", (method->paramTypes->elements[i] == NULL) ? "dynamic" : method->paramTypes->elements[i]->shortName->chars);
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
        typeTableOutputMethod(behavior->methods);
    }
}

static void typeTableOutputFunction(CallableTypeInfo* function) {
    printf("    signature: ");
    if (function->returnType == NULL) printf("dynamic ");
    else if (function->attribute.isVoid) printf("void ");
    else printf("%s ", function->returnType->shortName->chars);
    printf("%s(", function->baseType.shortName->chars);

    if (function->paramTypes != NULL && function->paramTypes->count > 0) {
        printf("%s", function->paramTypes->elements[0]->shortName->chars);
        for (int i = 1; i < function->paramTypes->count; i++) {
            printf(", %s", function->paramTypes->elements[i]->shortName->chars);
        }
    } 
    printf(")\n");
}

static void typeTableOutputEntry(TypeEntry* entry) {
    printf("  %s", entry->value->shortName->chars);
    if (IS_BEHAVIOR_TYPE(entry->value)) printf("(%s)", entry->value->fullName->chars);
    printf("\n    id: %d\n    category: ", entry->value->id);
    typeTableOutputCategory(entry->value->category);

    if (IS_BEHAVIOR_TYPE(entry->value)) typeTableOutputBehavior(AS_BEHAVIOR_TYPE(entry->value));
    else if (IS_CALLABLE_TYPE(entry->value)) typeTableOutputFunction(AS_CALLABLE_TYPE(entry->value));
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
    return (type->id == type2->id);
}

bool isSubtypeOfType(TypeInfo* type, TypeInfo* type2) {
    if (isEqualType(type, type2)) return true;
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
