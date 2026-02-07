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

        behaviorType->formalTypeParams = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        if (behaviorType->formalTypeParams != NULL) TypeInfoArrayInit(behaviorType->formalTypeParams);
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
        behaviorType->formalTypeParams = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));

        if (behaviorType->formalTypeParams != NULL) TypeInfoArrayInit(behaviorType->formalTypeParams);
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

BehaviorTypeInfo* newBehaviorTypeInfoWithFormalParameters(int id, TypeCategory category, ObjString* shortName, ObjString* fullName, TypeInfo* superclassType, int numParameters, ...) {
    BehaviorTypeInfo* behaviorType = (BehaviorTypeInfo*)newTypeInfo(id, sizeof(BehaviorTypeInfo), category, shortName, fullName);
    if (behaviorType != NULL) {
        behaviorType->superclassType = superclassType;
        behaviorType->traitTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        if (behaviorType->traitTypes != NULL) TypeInfoArrayInit(behaviorType->traitTypes);

        behaviorType->formalTypeParams = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        behaviorType->fields = newTypeTable(-1);
        behaviorType->methods = newTypeTable(id);

        if (behaviorType->formalTypeParams != NULL) {
            TypeInfoArrayInit(behaviorType->formalTypeParams);
            va_list args;
            va_start(args, numParameters);

            for (int i = 0; i < numParameters; i++) {
                TypeInfo* type = va_arg(args, TypeInfo*);
                TypeInfoArrayAdd(behaviorType->formalTypeParams, type);
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

        behaviorType->formalTypeParams = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        if (behaviorType->formalTypeParams != NULL) TypeInfoArrayInit(behaviorType->formalTypeParams);
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
        callableType->formalTypeParams = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        callableType->attribute = callableTypeInitAttribute();

        if (callableType->paramTypes != NULL) TypeInfoArrayInit(callableType->paramTypes);
        if (callableType->formalTypeParams != NULL) TypeInfoArrayInit(callableType->formalTypeParams);
    }
    return callableType;
}

CallableTypeInfo* newCallableTypeInfoWithFormalParameters(int id, TypeCategory category, ObjString* name, TypeInfo* returnType, int numParameters, ...) {
    CallableTypeInfo* callableType = (CallableTypeInfo*)newTypeInfo(id, sizeof(CallableTypeInfo), category, name, name);
    if (callableType != NULL) {
        callableType->returnType = returnType;
        callableType->paramTypes = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        callableType->formalTypeParams = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        callableType->attribute = callableTypeInitAttribute();
        if (callableType->paramTypes != NULL) TypeInfoArrayInit(callableType->paramTypes);

        if (callableType->formalTypeParams != NULL) {
            TypeInfoArrayInit(callableType->formalTypeParams);
            va_list args;
            va_start(args, numParameters);
            for (int i = 0; i < numParameters; i++) {
                TypeInfo* type = va_arg(args, TypeInfo*);
                TypeInfoArrayAdd(callableType->formalTypeParams, type);
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

MethodTypeInfo* newMethodTypeInfo(int id, ObjString* name, TypeInfo* returnType, bool isClass, bool isInitializer) {
    MethodTypeInfo* methodType = (MethodTypeInfo*)newTypeInfo(id, sizeof(MethodTypeInfo), TYPE_CATEGORY_METHOD, name, name);
    if (methodType != NULL) {
        methodType->declaredType = newCallableTypeInfo(-1, TYPE_CATEGORY_FUNCTION, name, returnType);
        methodType->isClass = isClass;
        methodType->isInitializer = isInitializer;
    }
    return methodType;
}

TypeInfo* newFormalTypeInfo(int id, ObjString* name) {
    return newTypeInfo(id, sizeof(TypeInfo), TYPE_CATEGORY_FORMAL, name, name);
}

GenericTypeInfo* newGenericTypeInfo(int id, ObjString* shortName, ObjString* fullName, TypeInfo* rawType) {
    GenericTypeInfo* genericType = (GenericTypeInfo*)newTypeInfo(id, sizeof(GenericTypeInfo), TYPE_CATEGORY_GENERIC, shortName, fullName);
    if (genericType != NULL) {
        genericType->rawType = rawType;
        genericType->isFullyInstantiated = false;
        genericType->actualTypeParams = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        if (genericType->actualTypeParams != NULL) TypeInfoArrayInit(genericType->actualTypeParams);
    }
    return genericType;
}

GenericTypeInfo* newGenericTypeInfoWithParameters(int id, ObjString* shortName, ObjString* fullName, TypeInfo* rawType, int numParameters, ...) {
    GenericTypeInfo* genericType = (GenericTypeInfo*)newTypeInfo(id, sizeof(GenericTypeInfo), TYPE_CATEGORY_GENERIC, shortName, fullName);
    if (genericType != NULL) {
        genericType->rawType = rawType;
        genericType->isFullyInstantiated = true;
        genericType->actualTypeParams = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        
        if (genericType->actualTypeParams != NULL) {
            TypeInfoArrayInit(genericType->actualTypeParams);
            va_list args;
            va_start(args, numParameters);

            for (int i = 0; i < numParameters; i++) {
                TypeInfo* type = va_arg(args, TypeInfo*);
                TypeInfoArrayAdd(genericType->actualTypeParams, type);
                if (IS_FORMAL_TYPE(type)) genericType->isFullyInstantiated = false;
            }
            va_end(args);
        }
    }
    return genericType;
}

AliasTypeInfo* newAliasTypeInfo(int id, ObjString* shortName, ObjString* fullName, TypeInfo* targetType) {
    AliasTypeInfo* aliasType = (AliasTypeInfo*)newTypeInfo(id, sizeof(AliasTypeInfo), TYPE_CATEGORY_ALIAS, shortName, fullName);
    if (aliasType != NULL) {
        aliasType->targetType = targetType;
        aliasType->formalTypeParams = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        if (aliasType->formalTypeParams != NULL) TypeInfoArrayInit(aliasType->formalTypeParams);
    }
    return aliasType;
}

AliasTypeInfo* newAliasTypeInfoWithParameters(int id, ObjString* shortName, ObjString* fullName, TypeInfo* targetType, int numParameters, ...) {
    AliasTypeInfo* aliasType = (AliasTypeInfo*)newTypeInfo(id, sizeof(AliasTypeInfo), TYPE_CATEGORY_ALIAS, shortName, fullName);
    if (aliasType != NULL) {
        aliasType->targetType = targetType;
        aliasType->formalTypeParams = (TypeInfoArray*)malloc(sizeof(TypeInfoArray));
        if (aliasType->formalTypeParams != NULL) {
            TypeInfoArrayInit(aliasType->formalTypeParams);
			va_list args;
            va_start(args, numParameters);
            for (int i = 0; i < numParameters; i++) {
                TypeInfo* type = va_arg(args, TypeInfo*);
                TypeInfoArrayAdd(aliasType->formalTypeParams, type);
            }
			va_end(args);
        }
    }
    return aliasType;
}

static char* createTempTypeName(TypeInfo* paramType) {
    if (IS_CALLABLE_TYPE(paramType)) return createCallableTypeName(AS_CALLABLE_TYPE(paramType));
    else if (IS_GENERIC_TYPE(paramType)) return createGenericTypeName(AS_GENERIC_TYPE(paramType));
    else return paramType->shortName->chars;
}

char* createCallableTypeName(CallableTypeInfo* callableType) {
    char* callableName = bufferNewCString(UINT16_MAX);
    size_t length = 0;

    if (callableType->returnType != NULL) {
        char* returnTypeName = createTempTypeName(callableType->returnType);
        size_t returnTypeLength = strlen(returnTypeName);
        memcpy(callableName, returnTypeName, returnTypeLength);
        length += returnTypeLength;
        if (isTempType(callableType->returnType)) free(returnTypeName);
    }
    else {
        memcpy(callableName, "dynamic", 7);
        length += 7;
    }

    memcpy(callableName + length, " fun", 4);
    length += 4;
    if (callableType->attribute.isGeneric) {
        callableName[length++] = '<';
        for (int i = 0; i < callableType->formalTypeParams->count; i++) {
            TypeInfo* formalType = callableType->formalTypeParams->elements[i];
            if (i > 0) {
                callableName[length++] = ',';
                callableName[length++] = ' ';
            }
            if (formalType != NULL) {
                char* formalTypeName = createTempTypeName(formalType);
                size_t formalTypeLength = strlen(formalTypeName);
                memcpy(callableName + length, formalTypeName, formalTypeLength);
                length += formalTypeLength;
                if (isTempType(formalType)) free(formalTypeName);
            }
            else {
                memcpy(callableName + length, "dynamic", 7);
                length += 7;
            }
        }
		callableName[length++] = '>';
    }

    callableName[length++] = '(';
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
            char* paramTypeName = createTempTypeName(paramType);
            size_t paramTypeLength = strlen(paramTypeName);
            memcpy(callableName + length, paramTypeName, paramTypeLength);
            length += paramTypeLength;
            if (isTempType(paramType)) free(paramTypeName);
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

char* createGenericTypeName(GenericTypeInfo* genericType) {
    char* genericName = bufferNewCString(UINT16_MAX);
    size_t length = 0;

    if (genericType->rawType != NULL) {
        char* rawTypeName = genericType->rawType->shortName->chars;
        size_t rawTypeLength = strlen(genericType->rawType->shortName->chars);
        memcpy(genericName, rawTypeName, rawTypeLength);
        length += rawTypeLength;
    }
    else {
        memcpy(genericName, "dynamic", 7);
        length += 7;
    }
	genericName[length++] = '<';


    for (int i = 0; i < genericType->actualTypeParams->count; i++) {
        TypeInfo* paramType = genericType->actualTypeParams->elements[i];
        if (i > 0) {
            genericName[length++] = ',';
            genericName[length++] = ' ';
        }
        if (paramType != NULL) {
            char* paramTypeName = createTempTypeName(paramType);
            size_t paramTypeLength = strlen(paramTypeName);
            memcpy(genericName + length, paramTypeName, paramTypeLength);
            length += paramTypeLength;
            if (isTempType(paramType)) free(paramTypeName);
        }
        else {
            memcpy(genericName + length, "dynamic", 7);
            length += 7;
        }
    }

    genericName[length++] = '>';
    genericName[length] = '\0';
    return genericName;
}

char* createAliasTypeName(AliasTypeInfo* aliasType) {
    char* aliasName = bufferNewCString(UINT16_MAX);
    size_t length = 0;

    if (aliasType->targetType != NULL) {
        char* targetTypeName = aliasType->targetType->shortName->chars;
        size_t targetTypeLength = strlen(aliasType->targetType->shortName->chars);
        memcpy(aliasName, targetTypeName, targetTypeLength);
        length += targetTypeLength;
    }
    else {
        memcpy(aliasName, "dynamic", 7);
        length += 7;
    }
    aliasName[length++] = '<';


    for (int i = 0; i < aliasType->formalTypeParams->count; i++) {
        TypeInfo* paramType = aliasType->formalTypeParams->elements[i];
        if (i > 0) {
            aliasName[length++] = ',';
            aliasName[length++] = ' ';
        }
        if (paramType != NULL) {
            char* paramTypeName = paramType->shortName->chars;
            size_t paramTypeLength = strlen(paramTypeName);
            memcpy(aliasName + length, paramTypeName, paramTypeLength);
            length += paramTypeLength;
            if (isTempType(paramType)) free(paramTypeName);
        }
        else {
            memcpy(aliasName + length, "dynamic", 7);
            length += 7;
        }
    }

    aliasName[length++] = '>';
    aliasName[length] = '\0';
    return aliasName;
}

void freeTypeInfo(TypeInfo* type) {
    if (type == NULL) return;

    if (IS_BEHAVIOR_TYPE(type)) {
        BehaviorTypeInfo* behaviorType = AS_BEHAVIOR_TYPE(type);
        if (behaviorType->traitTypes != NULL) TypeInfoArrayFree(behaviorType->traitTypes);
        if (behaviorType->formalTypeParams != NULL) TypeInfoArrayFree(behaviorType->formalTypeParams);
        freeTypeTable(behaviorType->fields);
        freeTypeTable(behaviorType->methods);
        free(behaviorType);
    }
    else if (IS_CALLABLE_TYPE(type)) {
        CallableTypeInfo* callableType = AS_CALLABLE_TYPE(type);
        if (callableType->paramTypes != NULL) TypeInfoArrayFree(callableType->paramTypes);
        if (callableType->formalTypeParams != NULL) TypeInfoArrayFree(callableType->formalTypeParams);
        free(callableType);
    }
    else if (IS_GENERIC_TYPE(type)) {
        GenericTypeInfo* genericType = AS_GENERIC_TYPE(type);
        if (genericType->actualTypeParams != NULL) TypeInfoArrayFree(genericType->actualTypeParams);
        free(genericType);
    }
    else if (IS_ALIAS_TYPE(type)) {
        AliasTypeInfo* aliasType = AS_ALIAS_TYPE(type);
        if (aliasType->formalTypeParams != NULL) TypeInfoArrayFree(aliasType->formalTypeParams);
        free(aliasType);
	}
    else free(type);
}

void freeTempTypes(TypeInfoArray* typeArray) {
    for (int i = 0; i < typeArray->count; i++) {
        TypeInfo* type = typeArray->elements[i];
        if (type != NULL) freeTypeInfo(type);
    }
    free(typeArray);
}

TypeInfo* getFormalTypeByName(TypeInfo* type, ObjString* name) {
    if (type == NULL) return NULL;
    else if (IS_BEHAVIOR_TYPE(type)) {
        BehaviorTypeInfo* behaviorType = AS_BEHAVIOR_TYPE(type);
        for (int i = 0; i < behaviorType->formalTypeParams->count; i++) {
            TypeInfo* formalType = behaviorType->formalTypeParams->elements[i];
            if (formalType != NULL && formalType->shortName == name) return formalType;
        }
    }
    else if (IS_CALLABLE_TYPE(type)) {
        CallableTypeInfo* callableType = AS_CALLABLE_TYPE(type);
        for (int i = 0; i < callableType->formalTypeParams->count; i++) {
            TypeInfo* formalType = callableType->formalTypeParams->elements[i];
            if (formalType != NULL && formalType->shortName == name) return formalType;
        }
    }
	else if (IS_GENERIC_TYPE(type)) {
        GenericTypeInfo* genericType = AS_GENERIC_TYPE(type);
        if (genericType->isFullyInstantiated) {
            for (int i = 0; i < genericType->actualTypeParams->count; i++) {
                TypeInfo* formalType = genericType->actualTypeParams->elements[i];
				if (formalType == NULL || !IS_FORMAL_TYPE(formalType)) continue;
                if (formalType->shortName == name) return formalType;
            }
        }
    }
    return NULL;
}

TypeInfo* getAliasTargetType(TypeInfo* type) {
    if (IS_ALIAS_TYPE(type)) {
        return getAliasTargetType(AS_ALIAS_TYPE(type)->targetType);
	}
    return type;
}

TypeInfo* getInnerBaseType(TypeInfo* type) {
    if (type == NULL) return NULL;
    else if (IS_GENERIC_TYPE(type)) {
        return getInnerBaseType(AS_GENERIC_TYPE(type)->rawType);
    }
    else if (IS_ALIAS_TYPE(type)) {
        return getInnerBaseType(AS_ALIAS_TYPE(type)->targetType);
    }
    else return type;
}

bool hasCallableTypeParameters(TypeInfo* type) {
    if (type == NULL || !IS_CALLABLE_TYPE(type)) return false;
    CallableTypeInfo* callableType = AS_CALLABLE_TYPE(type);
    if (callableType->formalTypeParams->count > 0) return true;
    if (hasGenericParameters(callableType->returnType)) return true;
    for (int i = 0; i < callableType->paramTypes->count; i++) {
        TypeInfo* paramType = callableType->paramTypes->elements[i];
        if (hasGenericParameters(paramType)) return true;
    }
    return false;
}

bool hasGenericParameters(TypeInfo* type) {
    if (type == NULL) return false;
    else if (IS_FORMAL_TYPE(type) || IS_GENERIC_TYPE(type)) return true;
    else if (IS_BEHAVIOR_TYPE(type)) return AS_BEHAVIOR_TYPE(type)->formalTypeParams->count > 0;
    else if (IS_CALLABLE_TYPE(type)) return hasCallableTypeParameters(type);
    else if (IS_METHOD_TYPE(type)) return hasCallableTypeParameters((TypeInfo*)AS_METHOD_TYPE(type)->declaredType);
    else if (IS_ALIAS_TYPE(type)) return AS_ALIAS_TYPE(type)->formalTypeParams->count > 0;
    else return false;
}

static TypeInfo* instantiateFormalTypeParameter(TypeInfo* type, TypeInfoArray* formalParams, TypeInfoArray* actualParams) {
    for (int i = 0; i < formalParams->count; i++) {
        TypeInfo* formalTypeParam = formalParams->elements[i];
        if (formalTypeParam != NULL && formalTypeParam->shortName == type->shortName) {
            return actualParams->elements[i];
        }
    }
    return NULL;
}

TypeInfo* instantiateTypeParameter(TypeInfo* type, TypeInfoArray* formalParams, TypeInfoArray* actualParams) {
	if (type == NULL) return NULL;
	else if (IS_FORMAL_TYPE(type)) {
        return instantiateFormalTypeParameter(type, formalParams, actualParams);
    }
    else if (IS_BEHAVIOR_TYPE(type)){
		BehaviorTypeInfo* behaviorType = AS_BEHAVIOR_TYPE(type);
		GenericTypeInfo* genericType = newGenericTypeInfo(-1, behaviorType->baseType.shortName, behaviorType->baseType.fullName, type);
		
        for (int i = 0; i < behaviorType->formalTypeParams->count; i++) {
            TypeInfo* formalTypeParam = behaviorType->formalTypeParams->elements[i];
            if (formalTypeParam != NULL && IS_FORMAL_TYPE(formalTypeParam)) {
                TypeInfo* instantiatedType = instantiateFormalTypeParameter(formalTypeParam, formalParams, actualParams);
                TypeInfoArrayAdd(genericType->actualTypeParams, instantiatedType);
            }
        }

        if (genericType->actualTypeParams->count == behaviorType->formalTypeParams->count) {
            genericType->isFullyInstantiated = true;
		}
        return (TypeInfo*)genericType;
    }
    else if (IS_CALLABLE_TYPE(type)) {
		CallableTypeInfo* callableType = AS_CALLABLE_TYPE(type);
        if (callableType->formalTypeParams->count > 0) {
            GenericTypeInfo* genericType = newGenericTypeInfo(-1, callableType->baseType.shortName, callableType->baseType.fullName, type);            
            for (int i = 0; i < callableType->formalTypeParams->count; i++) {
                TypeInfo* formalTypeParam = callableType->formalTypeParams->elements[i];
                if (formalTypeParam != NULL && IS_FORMAL_TYPE(formalTypeParam)) {
                    TypeInfo* instantiatedType = instantiateFormalTypeParameter(formalTypeParam, formalParams, actualParams);
                    TypeInfoArrayAdd(genericType->actualTypeParams, instantiatedType);
                }
            }

            if (genericType->actualTypeParams->count == callableType->formalTypeParams->count) {
                genericType->isFullyInstantiated = true;
            }
            return (TypeInfo*)genericType;
        }
        else {
			CallableTypeInfo* instantiatedCallableType = newCallableTypeInfo(-1, callableType->baseType.category, callableType->baseType.shortName, NULL);
            instantiatedCallableType->attribute = callableType->attribute;
            TypeInfo* instantiatedReturnType = callableType->returnType;
            if (hasGenericParameters(instantiatedReturnType)) {
                instantiatedReturnType = instantiateTypeParameter(instantiatedReturnType, formalParams, actualParams);
            }
            instantiatedCallableType->returnType = instantiatedReturnType;
            
            for (int i = 0; i < callableType->paramTypes->count; i++) {
                TypeInfo* paramType = callableType->paramTypes->elements[i];
                if (hasGenericParameters(paramType)) {
                    paramType = instantiateTypeParameter(paramType, formalParams, actualParams);
                }
                TypeInfoArrayAdd(instantiatedCallableType->paramTypes, paramType);
            }
			return (TypeInfo*)instantiatedCallableType;
        }
        
    }
    else if (IS_ALIAS_TYPE(type)) {
		TypeInfo* targetType = getInnerBaseType(type);
		formalParams = IS_CALLABLE_TYPE(targetType) ? AS_CALLABLE_TYPE(targetType)->formalTypeParams : AS_BEHAVIOR_TYPE(targetType)->formalTypeParams;
		return instantiateTypeParameter(targetType, formalParams, actualParams);
    }
	return type;
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


static void typeTableFieldsInheritBehavior(TypeTable* from, TypeTable* to) {
    for (int i = 0; i < from->capacity; i++) {
        TypeEntry* entry = &from->entries[i];
        if (entry != NULL && entry->key != NULL) {
            FieldTypeInfo* fromFieldType = AS_FIELD_TYPE(entry->value);
            FieldTypeInfo* toFieldType = newFieldTypeInfo(fromFieldType->baseType.id, entry->key, fromFieldType->declaredType, fromFieldType->isMutable, fromFieldType->hasInitializer);
            typeTableSet(to, entry->key, (TypeInfo*)toFieldType);
        }
    }
}

static void typeTableFieldsInheritGeneric(TypeTable* from, TypeTable* to, TypeInfoArray* formalParams, TypeInfoArray* actualParams) {
    for (int i = 0; i < from->capacity; i++) {
        TypeEntry* entry = &from->entries[i];
        if (entry != NULL && entry->key != NULL) {
            FieldTypeInfo* fromFieldType = AS_FIELD_TYPE(entry->value);
            if (fromFieldType->declaredType != NULL && (hasGenericParameters(fromFieldType->declaredType) || hasCallableTypeParameters(fromFieldType->declaredType))) {
                TypeInfo* instantiatedType = instantiateTypeParameter(fromFieldType->declaredType, formalParams, actualParams);
                FieldTypeInfo* toFieldType = newFieldTypeInfo(fromFieldType->baseType.id, entry->key, instantiatedType, fromFieldType->isMutable, fromFieldType->hasInitializer);
            }

            FieldTypeInfo* toFieldType = newFieldTypeInfo(fromFieldType->baseType.id, entry->key, fromFieldType->declaredType, fromFieldType->isMutable, fromFieldType->hasInitializer);
            typeTableSet(to, entry->key, (TypeInfo*)toFieldType);
        }
    }
}

void typeTableFieldsInherit(BehaviorTypeInfo* subclassType, TypeInfo* superclassType) {
    if (superclassType == NULL) return;
    else if (IS_BEHAVIOR_TYPE(superclassType)) {
        typeTableFieldsInheritBehavior(AS_BEHAVIOR_TYPE(superclassType)->fields, subclassType->fields);
    }
    else if (IS_GENERIC_TYPE(superclassType)) {
        GenericTypeInfo* genericType = AS_GENERIC_TYPE(superclassType);
        BehaviorTypeInfo* behaviorType = AS_BEHAVIOR_TYPE(genericType->rawType);
        typeTableFieldsInheritGeneric(behaviorType->fields, subclassType->fields, behaviorType->formalTypeParams, genericType->actualTypeParams);
    }
}

TypeInfo* typeTableMethodLookup(TypeInfo* type, ObjString* key) {
    if (type == NULL) return NULL;
	TypeInfo* baseType = getInnerBaseType(type);
	if (!IS_BEHAVIOR_TYPE(baseType)) return NULL;
    BehaviorTypeInfo* behaviorType = AS_BEHAVIOR_TYPE(baseType);
    TypeInfo* methodType = typeTableGet(behaviorType->methods, key);
    if (methodType != NULL) return methodType;

    if (behaviorType->traitTypes != NULL) {
        for (int i = 0; i < behaviorType->traitTypes->count; i++) {
            BehaviorTypeInfo* traitType = AS_BEHAVIOR_TYPE(getInnerBaseType(behaviorType->traitTypes->elements[i]));
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

GenericTypeInfo* typeTableInsertGeneric(TypeTable* typetab, ObjString* shortName, ObjString* fullName, TypeInfo* rawType) {
    int id = typetab->count + 1;
    GenericTypeInfo* genericType = newGenericTypeInfo(id, shortName, fullName, rawType);
    typeTableSet(typetab, fullName, (TypeInfo*)genericType);
    return genericType;
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
        case TYPE_CATEGORY_FORMAL:
            printf("formal");
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

    if (behavior->formalTypeParams != NULL && behavior->formalTypeParams->count > 0) {
        printf("    formal type parameters: %s", behavior->formalTypeParams->elements[0]->shortName->chars);
        for (int i = 1; i < behavior->formalTypeParams->count; i++) {
            printf(", %s", behavior->formalTypeParams->elements[i]->shortName->chars);
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

static void typeTableOutputCallable(CallableTypeInfo* function) {
    printf("    signature: ");
    if (function->returnType == NULL) printf("dynamic ");
    else if (function->attribute.isVoid) printf("void ");
    else printf("%s ", function->returnType->shortName->chars);

    printf("%s", function->baseType.shortName->chars);
	if (function->formalTypeParams != NULL && function->formalTypeParams->count > 0) {
        printf("<%s", function->formalTypeParams->elements[0]->shortName->chars);
        for (int i = 1; i < function->formalTypeParams->count; i++) {
            printf(", %s", function->formalTypeParams->elements[i]->shortName->chars);
        }
        printf(">");
    }

    printf("(");
    if (function->paramTypes != NULL && function->paramTypes->count > 0) {
        printf("%s", (function->paramTypes->elements[0] != NULL) ? function->paramTypes->elements[0]->shortName->chars : "dynamic");
        for (int i = 1; i < function->paramTypes->count; i++) {
            printf(", %s", (function->paramTypes->elements[i] != NULL) ? function->paramTypes->elements[i]->shortName->chars : "dynamic");
        }
    } 
    printf(")\n");
}

static void typeTableOutputGeneric(GenericTypeInfo* generic) {
    printf("    generic type: %s\n", generic->rawType->shortName->chars);
    if (generic->actualTypeParams != NULL && generic->actualTypeParams->count > 0) {
        printf("    actual type parameters: %s", generic->actualTypeParams->elements[0]->shortName->chars);
        for (int i = 1; i < generic->actualTypeParams->count; i++) {
            printf(", %s", generic->actualTypeParams->elements[i]->shortName->chars);
        }
        printf("\n");
    }
}

static void typeTableOutputAlias(AliasTypeInfo* alias) {
    if (alias->targetType != NULL) {
        printf("    target: %s\n", alias->targetType->shortName->chars);
		if (alias->formalTypeParams != NULL && alias->formalTypeParams->count > 0) {
            printf("    formal type parameters: %s", alias->formalTypeParams->elements[0]->shortName->chars);
            for (int i = 1; i < alias->formalTypeParams->count; i++) {
                printf(", %s", alias->formalTypeParams->elements[i]->shortName->chars);
            }
            printf("\n");
        }
    }
}

static void typeTableOutputEntry(TypeEntry* entry) {
    printf("  %s", entry->value->shortName->chars);
    if (IS_BEHAVIOR_TYPE(entry->value)) printf("(%s)", entry->value->fullName->chars);
    printf("\n    id: %d\n    category: ", entry->value->id);
    typeTableOutputCategory(entry->value->category);

    if (IS_BEHAVIOR_TYPE(entry->value)) typeTableOutputBehavior(AS_BEHAVIOR_TYPE(entry->value));
    else if (IS_CALLABLE_TYPE(entry->value)) typeTableOutputCallable(AS_CALLABLE_TYPE(entry->value));
    else if (IS_GENERIC_TYPE(entry->value)) typeTableOutputGeneric(AS_GENERIC_TYPE(entry->value));
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

static bool isCallableEqualType(CallableTypeInfo* type, CallableTypeInfo* type2) {
    if (!isEqualType(type->returnType, type2->returnType)) return false;
    if (type->paramTypes->count != type2->paramTypes->count) return false;
    for (int i = 0; i < type->paramTypes->count; i++) {
        if (!isEqualType(type->paramTypes->elements[i], type2->paramTypes->elements[i])) return false;
    }

    if (type->formalTypeParams->count != type2->formalTypeParams->count) return false;
	for (int i = 0; i < type->formalTypeParams->count; i++) {
        if (!isEqualType(type->formalTypeParams->elements[i], type2->formalTypeParams->elements[i])) return false;
    }
    return true;
}

static bool isGenericEqualType(GenericTypeInfo* type, GenericTypeInfo* type2) {
    if (!isEqualType(type->rawType, type2->rawType)) return false;
    if (type->actualTypeParams->count != type2->actualTypeParams->count) return false;
    for (int i = 0; i < type->actualTypeParams->count; i++) {
        if (!isEqualType(type->actualTypeParams->elements[i], type2->actualTypeParams->elements[i])) return false;
    }
    return true;
}

bool isEqualType(TypeInfo* type, TypeInfo* type2) {
    if (type == NULL || type2 == NULL) return true;
	if (IS_VOID_TYPE(type) && IS_VOID_TYPE(type2)) return true;
    if (IS_FORMAL_TYPE(type) && IS_FORMAL_TYPE(type2)) return true;
    if (IS_ALIAS_TYPE(type) || IS_ALIAS_TYPE(type2)) return isEqualType(getAliasTargetType(type), getAliasTargetType(type2));

    if (IS_BEHAVIOR_TYPE(type) && IS_BEHAVIOR_TYPE(type2)) return (type->id == type2->id);
    else if (IS_CALLABLE_TYPE(type) && IS_CALLABLE_TYPE(type2)) {
		return isCallableEqualType(AS_CALLABLE_TYPE(type), AS_CALLABLE_TYPE(type2));
    }
	else if (IS_GENERIC_TYPE(type) && IS_GENERIC_TYPE(type2)) {
        return isGenericEqualType(AS_GENERIC_TYPE(type), AS_GENERIC_TYPE(type2));
    }
    else return false;
}

static bool isBehaviorSubtypeOfType(BehaviorTypeInfo* subtype, BehaviorTypeInfo* supertype) {
    TypeInfo* superclassType = subtype->superclassType;
    while (superclassType != NULL) {
        if (superclassType->id == supertype->baseType.id) return true;
        superclassType = AS_BEHAVIOR_TYPE(getInnerBaseType(superclassType))->superclassType;
    }

    if (subtype->traitTypes != NULL && subtype->traitTypes->count > 0) {
        for (int i = 0; i < subtype->traitTypes->count; i++) {
            if (subtype->traitTypes->elements[i]->id == supertype->baseType.id) return true;
        }
    }
    return false;
}

static bool isCallableSubtypeOfType(CallableTypeInfo* type, TypeInfo* type2) {
    if (IS_BEHAVIOR_TYPE(type2)) {
        CallableTypeInfo* callableSupertype = AS_CALLABLE_TYPE(type2);
        if (memcmp(type2->shortName->chars, "Function", 8) == 0) return true;
        else if (memcmp(type2->shortName->chars, "TCallable", 9) == 0) return true;
    }
    else if (IS_CALLABLE_TYPE(type2)) return isCallableEqualType(type, AS_CALLABLE_TYPE(type2));
    return false;
}

static bool isGenericSubtypeOfType(GenericTypeInfo* type, TypeInfo* type2) {
    if (isEqualType(type->rawType, type2)) return true;
    else if (IS_BEHAVIOR_TYPE(type->rawType)) {
        BehaviorTypeInfo* subtype = AS_BEHAVIOR_TYPE(type->rawType);
        if (IS_BEHAVIOR_TYPE(type2)) return isBehaviorSubtypeOfType(subtype, AS_BEHAVIOR_TYPE(type2));
        else if (IS_GENERIC_TYPE(type2)) {
            GenericTypeInfo* genericSupertype = AS_GENERIC_TYPE(type2);
            if (!IS_BEHAVIOR_TYPE(genericSupertype->rawType)) return false;
            else {
                BehaviorTypeInfo* behaviorSupertype = AS_BEHAVIOR_TYPE(genericSupertype->rawType);
                if (!isBehaviorSubtypeOfType(subtype, behaviorSupertype)) return false;
                for (int i = 0; i < type->actualTypeParams->count; i++) {
                    if (!isEqualType(type->actualTypeParams->elements[i], genericSupertype->actualTypeParams->elements[i])) return false;
                }
                return true;
            }
        }
    }
    else if (IS_CALLABLE_TYPE(type->rawType)) {
        CallableTypeInfo* subtype = AS_CALLABLE_TYPE(type->rawType);
        if (IS_GENERIC_TYPE(type2)) {
            GenericTypeInfo* genericSupertype = AS_GENERIC_TYPE(type2);
            if (!IS_CALLABLE_TYPE(genericSupertype->rawType)) return false;
            else {
                CallableTypeInfo* callableSupertype = AS_CALLABLE_TYPE(genericSupertype->rawType);
                if (!isCallableSubtypeOfType(subtype, (TypeInfo*)callableSupertype)) return false;
                for (int i = 0; i < type->actualTypeParams->count; i++) {
                    if (!isEqualType(type->actualTypeParams->elements[i], genericSupertype->actualTypeParams->elements[i])) return false;
                }
                return true;
            }
        }
        else return isCallableSubtypeOfType(subtype, type2);
    }
    return false;
}

bool isSubtypeOfType(TypeInfo* type, TypeInfo* type2) {
    if (isEqualType(type, type2)) return true;
    if (memcmp(type->shortName->chars, "Nil", 3) == 0) return true;
    if (memcmp(type2->shortName->chars, "Object", 3) == 0) return true;
    if (IS_ALIAS_TYPE(type) || IS_ALIAS_TYPE(type2)) return isSubtypeOfType(getAliasTargetType(type), getAliasTargetType(type2));

    if (IS_CALLABLE_TYPE(type)) {
        return isCallableSubtypeOfType(AS_CALLABLE_TYPE(type), type2);
    } 
    if (IS_GENERIC_TYPE(type)) {
        return isGenericSubtypeOfType(AS_GENERIC_TYPE(type), type2);
    }
    if (!IS_BEHAVIOR_TYPE(type) || !IS_BEHAVIOR_TYPE(type2)) {
        return false;
    }
    return isBehaviorSubtypeOfType(AS_BEHAVIOR_TYPE(type), AS_BEHAVIOR_TYPE(type2));
}