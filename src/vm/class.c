#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "class.h"
#include "memory.h"
#include "vm.h"
#include "../common/os.h"

static ObjString* createBehaviorName(VM* vm, BehaviorType behaviorType, ObjClass* superclass) {
    unsigned long currentTimeStamp = (unsigned long)time(NULL);
    if (behaviorType == BEHAVIOR_TRAIT) return formattedStringPerma(vm, "Trait@%x", currentTimeStamp);
    else return formattedStringPerma(vm, "%s@%x", superclass->name->chars, currentTimeStamp);
}

void initClass(VM* vm, ObjClass* klass, ObjString* name, ObjClass* metaclass, BehaviorType behaviorType) {
    push(vm, OBJ_VAL(klass));
    klass->behaviorID = vm->behaviorCount++;
    klass->behaviorType = behaviorType;
    klass->classType = OBJ_INSTANCE;
    klass->name = name != NULL ? name : emptyString(vm);
    klass->namespace = vm->currentNamespace;
    klass->superclass = NULL;
    klass->isNative = false;
    klass->interceptors = 0;
    klass->defaultShapeID = 0;

    if (!klass->namespace->isRoot) {
        char chars[UINT8_MAX];
        int length = sprintf_s(chars, UINT8_MAX, "%s.%s", klass->namespace->fullName->chars, klass->name->chars);
        klass->fullName = copyStringPerma(vm, chars, length);
    }
    else klass->fullName = klass->name;

    initValueArray(&klass->traits, klass->obj.generation);
    initIDMap(&klass->indexes, klass->obj.generation);
    initValueArray(&klass->fields, klass->obj.generation);
    initTable(&klass->methods, klass->obj.generation);
    initValueArray(&klass->defaultInstanceFields, klass->obj.generation);
    pop(vm);
}

ObjClass* createClass(VM* vm, ObjString* name, ObjClass* metaclass, BehaviorType behaviorType) {
    ObjClass* klass = ALLOCATE_CLASS(metaclass);
    initClass(vm, klass, name, metaclass, behaviorType);
    typeTableInsertBehavior(vm->typetab, TYPE_CATEGORY_CLASS, klass->name, klass->fullName, NULL);
    return klass;
}

void initTrait(VM* vm, ObjClass* trait, ObjString* name) {
    push(vm, OBJ_VAL(trait));
    trait->behaviorType = BEHAVIOR_TRAIT;
    trait->behaviorID = vm->behaviorCount++;
    trait->name = (name != NULL) ? name : createBehaviorName(vm, BEHAVIOR_TRAIT, NULL);
    trait->namespace = vm->currentNamespace;
    trait->superclass = NULL;
    trait->isNative = false;
    trait->interceptors = 0;
    trait->defaultShapeID = 0;

    if (!trait->namespace->isRoot) {
        char chars[UINT8_MAX];
        int length = sprintf_s(chars, UINT8_MAX, "%s.%s", trait->namespace->fullName->chars, trait->name->chars);
        trait->fullName = copyStringPerma(vm, chars, length);
    }
    else trait->fullName = trait->name;

    initValueArray(&trait->traits, trait->obj.generation);
    initIDMap(&trait->indexes, trait->obj.generation);
    initValueArray(&trait->fields, trait->obj.generation);
    initTable(&trait->methods, trait->obj.generation);
    initValueArray(&trait->defaultInstanceFields, trait->obj.generation);
    pop(vm);
}

ObjClass* createTrait(VM* vm, ObjString* name) {
    ObjClass* trait = ALLOCATE_CLASS(vm->traitClass);
    initTrait(vm, trait, name);
    return trait;
}

ObjClass* getObjClass(VM* vm, Value value) {
    if (IS_BOOL(value)) return vm->boolClass;
    else if (IS_NIL(value)) return vm->nilClass;
    else if (IS_INT(value)) return vm->intClass;
    else if (IS_FLOAT(value)) return vm->floatClass;
    else if (IS_OBJ(value)) return AS_OBJ(value)->klass;
    else return NULL;
}

ObjString* getClassNameFromMetaclass(VM* vm, ObjString* metaclassName) {
    return subString(vm, metaclassName, 0, metaclassName->length - 7);
}

ObjString* getMetaclassNameFromClass(VM* vm, ObjString* className) {
    if (strstr(className->chars, " class") != NULL) return copyStringPerma(vm, "Metaclass", 9);
    return concatenateString(vm, className, newStringPerma(vm, "class"), " ");
}

ObjString* getClassFullName(VM* vm, ObjString* shortName, ObjString* currentNamespace) {
    if (currentNamespace == NULL) return shortName;
    return concatenateString(vm, currentNamespace, shortName, ".");
}

bool isObjInstanceOf(VM* vm, Value value, ObjClass* klass) {
    ObjClass* currentClass = getObjClass(vm, value);
    if (currentClass == klass) return true;
    if (isClassExtendingSuperclass(currentClass->superclass, klass)) return true;
    return isClassImplementingTrait(currentClass, klass);
}

bool isClassExtendingSuperclass(ObjClass* klass, ObjClass* superclass) {
    if (klass == superclass) return true;
    if (klass->behaviorType == BEHAVIOR_TRAIT) return false;

    ObjClass* currentClass = klass->superclass;
    while (currentClass != NULL) {
        if (currentClass == superclass) return true;
        currentClass = currentClass->superclass;
    }
    return false;
}

bool isClassImplementingTrait(ObjClass* klass, ObjClass* trait) {
    if (klass->behaviorType == BEHAVIOR_METACLASS || klass->traits.count == 0) return false;
    ValueArray* traits = &klass->traits;

    for (int i = 0; i < traits->count; i++) {
        if (AS_CLASS(traits->values[i]) == trait) return true;
    }
    return false;
}

static void inheritTraits(VM* vm, ObjClass* subclass, ObjClass* superclass) {
    BehaviorTypeInfo* subclassType = AS_BEHAVIOR_TYPE(typeTableGet(vm->typetab, subclass->fullName));
    for (int i = 0; i < superclass->traits.count; i++) {        
        valueArrayWrite(vm, &subclass->traits, superclass->traits.values[i]);
        if (subclass->isNative) {
            ObjClass* trait = AS_CLASS(superclass->traits.values[i]);
            TypeInfo* traitType = typeTableGet(vm->typetab, trait->fullName);
            TypeInfoArrayAdd(subclassType->traitTypes, traitType);
        }
    }
}

static void inheritMethods(VM* vm, ObjClass* subclass, ObjClass* superclass) {
    tableAddAll(vm, &superclass->methods, &subclass->methods);
}

void inheritSuperclass(VM* vm, ObjClass* subclass, ObjClass* superclass) {
    subclass->superclass = superclass;
    subclass->classType = superclass->classType;
    subclass->interceptors = superclass->interceptors;
    subclass->defaultShapeID = superclass->defaultShapeID;

    for (int i = 0; i < superclass->defaultInstanceFields.count; i++) {
        valueArrayWrite(vm, &subclass->defaultInstanceFields, superclass->defaultInstanceFields.values[i]);
    }

    if (subclass->isNative) {
        BehaviorTypeInfo* subclassType = AS_BEHAVIOR_TYPE(typeTableGet(vm->typetab, subclass->fullName));
        TypeInfo* superclassType = typeTableGet(vm->typetab, superclass->fullName);
        subclassType->superclassType = superclassType;
        typeTableAddAll(AS_BEHAVIOR_TYPE(superclassType)->fields, subclassType->fields);
    }

    if (superclass->behaviorType == BEHAVIOR_CLASS) {
        inheritTraits(vm, subclass, superclass);
    }
    inheritMethods(vm, subclass, superclass);
}

void bindSuperclass(VM* vm, ObjClass* subclass, ObjClass* superclass) {
    if (superclass == NULL) {
        runtimeError(vm, "Superclass cannot be NULL for class %s", subclass->name);
        exit(70);
    }
    inheritSuperclass(vm, subclass, superclass);

    if (subclass->name->length == 0) {
        subclass->name = createBehaviorName(vm, BEHAVIOR_CLASS, superclass);
        subclass->obj.klass = superclass->obj.klass;
    }
    else inheritSuperclass(vm, subclass->obj.klass, superclass->obj.klass);
}

static void copyTraitsToTable(VM* vm, ValueArray* traitArray, Table* traitTable) {
    for (int i = 0; i < traitArray->count; i++) {
        ObjClass* trait = AS_CLASS(traitArray->values[i]);
        tableSet(vm, traitTable, trait->name, traitArray->values[i]);
        if (trait->traits.count == 0) continue;

        for (int j = 0; j < trait->traits.count; j++) {
            ObjClass* superTrait = AS_CLASS(trait->traits.values[j]);
            tableSet(vm, traitTable, superTrait->name, trait->traits.values[j]);
        }
    }
}

static void copyTraitsFromTable(VM* vm, ObjClass* klass, Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;
        valueArrayWrite(vm, &klass->traits, entry->value);
    }
}

static void flattenTraits(VM* vm, ObjClass* klass, ValueArray* traits) {
    Table traitTable;
    initTable(&traitTable, klass->obj.generation);

    copyTraitsToTable(vm, traits, &traitTable);
    if (klass->superclass != NULL && klass->superclass->traits.count > 0) {
        copyTraitsToTable(vm, &klass->superclass->traits, &traitTable);
    }

    freeValueArray(vm, traits);
    copyTraitsFromTable(vm, klass, &traitTable);
    freeTable(vm, &traitTable);
}

void implementTraits(VM* vm, ObjClass* klass, ValueArray* traits) {
    if (traits->count == 0) return;
    for (int i = 0; i < traits->count; i++) {
        ObjClass* trait = AS_CLASS(traits->values[i]);
        tableAddAll(vm, &trait->methods, &klass->methods);
    }
    flattenTraits(vm, klass, traits);
}

void bindTrait(VM* vm, ObjClass* klass, ObjClass* trait) {
    tableAddAll(vm, &trait->methods, &klass->methods);
    valueArrayWrite(vm, &klass->traits, OBJ_VAL(trait));
    for (int i = 0; i < trait->traits.count; i++) {
        valueArrayWrite(vm, &klass->traits, trait->traits.values[i]);
    }
}

void bindTraits(VM* vm, int numTraits, ObjClass* klass, ...) {
    va_list args;
    va_start(args, klass);
    for (int i = 0; i < numTraits; i++) {
        Value trait = va_arg(args, Value);
        bindTrait(vm, klass, AS_CLASS(trait));
    }
    flattenTraits(vm, klass, &klass->traits);
}

Value getClassProperty(VM* vm, ObjClass* klass, char* name) {
    int index;
    if (!idMapGet(&klass->indexes, newStringPerma(vm, name), &index)) {
        runtimeError(vm, "Class property %s does not exist for class %s", name, klass->fullName->chars);
        return NIL_VAL;
    }
    return klass->fields.values[index];
}

void setClassProperty(VM* vm, ObjClass* klass, char* name, Value value) {
    PROCESS_WRITE_BARRIER((Obj*)klass, value);
    ObjString* propertyName = newStringPerma(vm, name);
    int index;
    if (!idMapGet(&klass->indexes, propertyName, &index)) {
        index = klass->fields.count;
        valueArrayWrite(vm, &klass->fields, value);
        idMapSet(vm, &klass->indexes, propertyName, index);
    }
    else klass->fields.values[index] = value;
}