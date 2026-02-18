#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "memory.h"
#include "native.h"
#include "string.h"

LOX_FUNCTION(assert) {
    ASSERT_ARG_COUNT("assert(expression, message)", 2);
    ASSERT_ARG_TYPE("assert(expression, message)", 1, String);
    if (isFalsey(args[0])) THROW_EXCEPTION(clox.std.lang.AssertionException, AS_CSTRING(args[1]));
    RETURN_NIL;
}

LOX_FUNCTION(clock) {
    ASSERT_ARG_COUNT("clock()", 0);
    RETURN_NUMBER((double)clock() / CLOCKS_PER_SEC);
}

LOX_FUNCTION(error){
    ASSERT_ARG_COUNT("error(message)", 1);
    ASSERT_ARG_TYPE("error(message)", 0, String);
    runtimeError(vm, AS_CSTRING(args[0]));
    exit(70);
    RETURN_NIL;
}

LOX_FUNCTION(gc) {
    ASSERT_ARG_COUNT("gc(generation)", 1);
    ASSERT_ARG_TYPE("gc(generation)", 0, Int);
    int generation = AS_INT(args[0]);
    ASSERT_INDEX_WITHIN_BOUNDS("gc(generation)", generation, 0, 2, 0);
    collectGarbage(vm, generation);
    RETURN_NIL;
}

LOX_FUNCTION(print){
    ASSERT_ARG_COUNT("print(message)", 1);
    printValue(args[0]);
    RETURN_NIL;
}

LOX_FUNCTION(println){
    ASSERT_ARG_COUNT("println(message)", 1);
    printValue(args[0]);
    printf("\n");
    RETURN_NIL;
}

LOX_FUNCTION(read) {
    ASSERT_ARG_COUNT("read()", 0);
    uint64_t inputSize = 128;
    char* line = malloc(inputSize);
    ABORT_IFNULL(line, "Not enough memory to read line.");

    int c = EOF;
    uint64_t i = 0;
    while ((c = getchar()) != '\n' && c != EOF) {
        line[i++] = (char)c;
        if (inputSize == i + 1) {
            inputSize = GROW_CAPACITY(inputSize);
            char* newLine = realloc(line, inputSize);
            if (newLine == NULL) exit(1);
            line = newLine;
        }
    }

    line[i] = '\0';
    ObjString* input = newString(vm, line);
    free(line);
    RETURN_OBJ(input);
}

ObjClass* defineNativeClass(VM* vm, const char* name) {
    ObjString* className = newStringPerma(vm, name);
    ObjClass* nativeClass = newClass(vm, className, OBJ_INSTANCE);
    nativeClass->isNative = true;
    nativeClass->obj.klass->isNative = true;
    push(vm, OBJ_VAL(nativeClass));

    tableSet(vm, &vm->classes, nativeClass->fullName, OBJ_VAL(nativeClass));
    tableSet(vm, &vm->currentNamespace->values, className, OBJ_VAL(nativeClass)); 
    pop(vm);
    return nativeClass;
}

ObjClass* defineNativeGenericClass(VM* vm, const char* name, int numParams, ...) {
	ObjClass* _class = defineNativeClass(vm, name);
	BehaviorTypeInfo* classType = AS_BEHAVIOR_TYPE(typeTableGet(vm->typetab, _class->fullName));
    va_list args;
    va_start(args, numParams);

    for (int i = 0; i < numParams; i++) {
        TypeInfo* formalType = va_arg(args, TypeInfo*);
        TypeInfoArrayAdd(classType->formalTypeParams, formalType);
    }
    va_end(args);
	return _class;
}

void defineNativeFunction(VM* vm, const char* name, int arity, bool isAsync, TypeInfo* returnType, NativeFunction function, ...) {
    ObjString* functionName = newStringPerma(vm, name);
    ObjNativeFunction* nativeFunction = newNativeFunction(vm, functionName, arity, isAsync, function);
    push(vm, OBJ_VAL(nativeFunction));
    tableSet(vm, &vm->rootNamespace->values, functionName, OBJ_VAL(nativeFunction));
    pop(vm);

    SymbolItem* item = insertGlobalSymbolTable(vm, name, NULL);
    CallableTypeInfo* functionType = newCallableTypeInfo(-1, TYPE_CATEGORY_FUNCTION, functionName, returnType);
    functionType->attribute.isAsync = isAsync;
    functionType->attribute.isVoid = (returnType->category == TYPE_CATEGORY_VOID);

    va_list args;
    va_start(args, function);
    for (int i = 0; i < arity; i++) {
        TypeInfo* paramType = va_arg(args, TypeInfo*);
        TypeInfoArrayAdd(functionType->paramTypes, paramType);
    }
    
    char* functionTypeName = createCallableTypeName(functionType);
    item->type = (TypeInfo*)functionType;
    item->type->fullName = item->type->shortName = takeStringPerma(vm, functionTypeName, (int)strlen(functionTypeName));
    TypeInfoArrayAdd(vm->tempTypes, item->type);
    va_end(args);
}

void defineNativeField(VM* vm, ObjClass* klass, const char* name, TypeInfo* type, bool isMutable, Value defaultValue) {
    ObjString* fieldName = newStringPerma(vm, name);
    BehaviorTypeInfo* behaviorType = AS_BEHAVIOR_TYPE(typeTableGet(vm->typetab, klass->fullName));
    FieldTypeInfo* fieldType = typeTableInsertField(behaviorType->fields, fieldName, type, isMutable, !IS_NIL(defaultValue));
    if (fieldType->declaredType != NULL && IS_CALLABLE_TYPE(fieldType->declaredType)) {
        TypeInfoArrayAdd(vm->tempTypes, fieldType->declaredType);
    }

    if (klass->classType == OBJ_INSTANCE || klass->classType == OBJ_CLASS) {
        klass->defaultShapeID = createShapeFromParent(vm, klass->defaultShapeID, fieldName);
    }
    valueArrayWrite(vm, &klass->defaultInstanceFields, defaultValue);

    if (klass->behaviorType == BEHAVIOR_METACLASS) {
        Value value;
        tableGet(&vm->classes, subString(vm, klass->fullName, 0, klass->fullName->length - 7), &value);
        klass = AS_CLASS(value);
        PROCESS_WRITE_BARRIER((Obj*)klass, defaultValue);
        valueArrayWrite(vm, &klass->fields, defaultValue);
        idMapSet(vm, &klass->indexes, fieldName, klass->fields.count - 1);
    }
}

void defineNativeMethod(VM* vm, ObjClass* klass, const char* name, int arity, bool isAsync, NativeMethod method, ...) {
    ObjString* methodName = newStringPerma(vm, name);
    ObjNativeMethod* nativeMethod = newNativeMethod(vm, klass, methodName, arity, isAsync, method);
    push(vm, OBJ_VAL(nativeMethod));
    tableSet(vm, &klass->methods, methodName, OBJ_VAL(nativeMethod));
    pop(vm);

    va_list args;
    va_start(args, method);
    BehaviorTypeInfo* behaviorType = AS_BEHAVIOR_TYPE(typeTableGet(vm->typetab, klass->fullName));
    TypeInfo* returnType = va_arg(args, TypeInfo*);
    if (IS_CALLABLE_TYPE(returnType)) TypeInfoArrayAdd(vm->tempTypes, returnType);
    
    bool isClass = (klass->behaviorType == BEHAVIOR_METACLASS);
    bool isInitializer = (strcmp(name, "__init__") == 0);
    MethodTypeInfo* methodType = newMethodTypeInfo(behaviorType->methods->count + 1, methodName, returnType, isClass, isInitializer);
    methodType->declaredType->attribute.isAsync = isAsync;
    methodType->declaredType->attribute.isVoid = (returnType->category == TYPE_CATEGORY_VOID);

    if (arity < 0) {
        methodType->declaredType->attribute.isVariadic = true;
        TypeInfo* paramType = va_arg(args, TypeInfo*);
        TypeInfoArrayAdd(methodType->declaredType->paramTypes, paramType);
        if (IS_CALLABLE_TYPE(paramType)) TypeInfoArrayAdd(vm->tempTypes, paramType);
    }
    else {
        for (int i = 0; i < arity; i++) {
            TypeInfo* paramType = va_arg(args, TypeInfo*);
            TypeInfoArrayAdd(methodType->declaredType->paramTypes, paramType);
            if (IS_CALLABLE_TYPE(paramType)) TypeInfoArrayAdd(vm->tempTypes, paramType);
        }
    }

    typeTableSet(behaviorType->methods, methodName, (TypeInfo*)methodType);
    if (methodType->declaredType != NULL) {
        TypeInfoArrayAdd(vm->tempTypes, (TypeInfo*)methodType->declaredType);
    }
    va_end(args);
}

void defineNativeInterceptor(VM* vm, ObjClass* klass, InterceptorType type, int arity, NativeMethod method, ...) {
    switch (type) {
        case INTERCEPTOR_INIT:
            defineNativeMethod(vm, klass, "__init__", arity, false, method, RETURN_TYPE(Object));
            break;
        case INTERCEPTOR_BEFORE_GET:
            defineNativeMethod(vm, klass, "__beforeGet__", 1, false, method, RETURN_TYPE(Object));
            break;
        case INTERCEPTOR_AFTER_GET:
            defineNativeMethod(vm, klass, "__afterGet__", 2, false, method, RETURN_TYPE(Object));
            break;
        case INTERCEPTOR_BEFORE_SET:
            defineNativeMethod(vm, klass, "__beforeSet__", 2, false, method, RETURN_TYPE(Object));
            break;
        case INTERCEPTOR_AFTER_SET: 
            defineNativeMethod(vm, klass, "__afterSet__", 2, false, method, RETURN_TYPE(Object));
            break;
        case INTERCEPTOR_ON_INVOKE:
            defineNativeMethod(vm, klass, "__onInvoke__", 2, false, method, RETURN_TYPE(Object));
            break;
        case INTERCEPTOR_ON_RETURN:
            defineNativeMethod(vm, klass, "__onReturn__", 2, false, method, RETURN_TYPE(Object));
            break;
        case INTERCEPTOR_ON_THROW:
            defineNativeMethod(vm, klass, "__onThrow__", 2, false, method, RETURN_TYPE(Object));
            break;
        case INTERCEPTOR_ON_YIELD:
            defineNativeMethod(vm, klass, "__onYield__", 2, false, method, RETURN_TYPE(Object));
            break;
        case INTERCEPTOR_ON_AWAIT:
            defineNativeMethod(vm, klass, "__onAwait__", 2, false, method, RETURN_TYPE(Object));
            break;
        case INTERCEPTOR_UNDEFINED_GET:
            defineNativeMethod(vm, klass, "__undefinedGet__", 1, false, method, RETURN_TYPE(Object));
            break;
        case INTERCEPTOR_UNDEFINED_INVOKE:
            defineNativeMethod(vm, klass, "__undefinedInvoke__", 2, false, method, RETURN_TYPE(Object));
            break;
        default: 
            runtimeError(vm, "Unknown interceptor type %d.", type);
            exit(70);
    }
    SET_CLASS_INTERCEPTOR(klass, type);
}

ObjClass* defineNativeTrait(VM* vm, const char* name) {
    ObjString* traitName = newStringPerma(vm, name);
    ObjClass* nativeTrait = createTrait(vm, traitName);
    nativeTrait->isNative = true;
    push(vm, OBJ_VAL(nativeTrait));

    tableSet(vm, &vm->classes, nativeTrait->fullName, OBJ_VAL(nativeTrait));
    tableSet(vm, &vm->currentNamespace->values, traitName, OBJ_VAL(nativeTrait));
    pop(vm);
    typeTableInsertBehavior(vm->typetab, TYPE_CATEGORY_TRAIT, traitName, nativeTrait->fullName, NULL);
    return nativeTrait;
}

ObjNamespace* defineNativeNamespace(VM* vm, const char* name, ObjNamespace* enclosing) {
    ObjString* shortName = newStringPerma(vm, name);
    push(vm, OBJ_VAL(shortName));
    ObjNamespace* nativeNamespace = newNamespace(vm, shortName, enclosing);
    push(vm, OBJ_VAL(nativeNamespace));

    tableSet(vm, &vm->namespaces, nativeNamespace->fullName, OBJ_VAL(nativeNamespace));
    tableSet(vm, &enclosing->values, nativeNamespace->shortName, OBJ_VAL(nativeNamespace));
    pop(vm);
    pop(vm);
    return nativeNamespace;
}

ObjClass* defineNativeException(VM* vm, const char* name, ObjClass* superClass) {
    ObjClass* exceptionClass = defineNativeClass(vm, name);
    bindSuperclass(vm, exceptionClass, superClass);
    return exceptionClass;
}

TypeInfo* defineCallableTypeInfoWithName(VM* vm, TypeCategory category, ObjString* shortName, TypeInfo* returnType, int numParams, ...) {
    CallableTypeInfo* callableType = newCallableTypeInfo(-1, category, shortName, returnType);
    if (numParams > 0) {
        va_list args;
        va_start(args, numParams);
        for (int i = 0; i < numParams; i++) {
            TypeInfo* paramType = va_arg(args, TypeInfo*);
            TypeInfoArrayAdd(callableType->paramTypes, paramType);
            if (isTempType(paramType)) TypeInfoArrayAdd(vm->tempTypes, paramType);
        }
        va_end(args);
    }

    char* name = createCallableTypeName(callableType);
    callableType->baseType.shortName = takeStringPerma(vm, name, (int)strlen(name));
    callableType->baseType.fullName = callableType->baseType.shortName;
    return (TypeInfo*)callableType;
}

TypeInfo* defineGenericTypeInfoWithName(VM* vm, ObjString* shortName, TypeInfo* rawType, int numParams, ...) {
    GenericTypeInfo* genericType = newGenericTypeInfo(-1, shortName, shortName, rawType);
    va_list args;
    va_start(args, numParams);

    for (int i = 0; i < numParams; i++) {
        TypeInfo* paramType = va_arg(args, TypeInfo*);
        TypeInfoArrayAdd(genericType->actualTypeParams, paramType);
        if (isTempType(paramType)) TypeInfoArrayAdd(vm->tempTypes, paramType);
    }
    va_end(args);

    char* name = createGenericTypeName(genericType);
    genericType->baseType.shortName = takeStringPerma(vm, name, (int)strlen(name));
    genericType->baseType.fullName = genericType->baseType.shortName;
    return (TypeInfo*)genericType;
}

TypeInfo* declareNativeTypeParameter(VM* vm, const char* shortName) {
	ObjString* name = newStringPerma(vm, shortName);
	TypeInfo* formalType = typeTableGet(vm->typetab, name);
    if (formalType == NULL) {
        formalType = newFormalTypeInfo(-1, name);
        typeTableSet(vm->typetab, name, formalType);
    }
	return formalType;
}

ObjClass* getNativeClass(VM* vm, const char* fullName) {
    Value klass;
    tableGet(&vm->classes, newStringPerma(vm, fullName), &klass);
    if (!IS_CLASS(klass)) {
        runtimeError(vm, "Class '%s' is undefined.", fullName);
        exit(70);
    }
    return AS_CLASS(klass);
}

TypeInfo* getNativeType(VM* vm, const char* name) {
    if (name == NULL || strcmp(name, "dynamic") == 0) return NULL;
    ObjString* shortName = newStringPerma(vm, name);
    ObjString* fullName = NULL;
    TypeInfo* type = typeTableGet(vm->typetab, shortName);

    if (type == NULL) {
        fullName = concatenateString(vm, vm->currentNamespace->fullName, shortName, ".");
        type = typeTableGet(vm->typetab, fullName);

        if (type == NULL) {
            fullName = concatenateString(vm, vm->langNamespace->fullName, shortName, ".");
            type = typeTableGet(vm->typetab, fullName);
            
            if (type == NULL) {
                runtimeError(vm, "Type '%s' is undefined.", name);
                exit(70);
            }
        }
    }
    return type;
}

ObjNativeFunction* getNativeFunction(VM* vm, const char* name) {
    Value function;
    tableGet(&vm->rootNamespace->values, newStringPerma(vm, name), &function);
    if (!IS_NATIVE_FUNCTION(function)) {
        runtimeError(vm, "Native function '%s' is undefined.", name);
        exit(70);
    }
    return AS_NATIVE_FUNCTION(function);
}

ObjNativeMethod* getNativeMethod(VM* vm, ObjClass* klass, const char* name) {
    Value method;
    tableGet(&klass->methods, newStringPerma(vm, name), &method);
    if (!IS_NATIVE_METHOD(method)) {
        runtimeError(vm, "Native method '%s::%s' is undefined.", klass->name->chars, name);
        exit(70);
    }
    return AS_NATIVE_METHOD(method);
}

ObjNamespace* getNativeNamespace(VM* vm, const char* name) {
    Value namespace;
    tableGet(&vm->namespaces, newStringPerma(vm, name), &namespace);
    if (!IS_NAMESPACE(namespace)) {
        runtimeError(vm, "Namespace '%s' is undefined.", name);
        exit(70);
    }
    return AS_NAMESPACE(namespace);
}

SymbolItem* insertGlobalSymbolTable(VM* vm, const char* symbolName, const char* typeName) {
    TypeInfo* type = (typeName != NULL) ? getNativeType(vm, typeName) : NULL;
    ObjString* symbol = newStringPerma(vm, symbolName);
    SymbolItem* item = newSymbolItemWithType(syntheticToken(symbolName), SYMBOL_CATEGORY_GLOBAL, SYMBOL_STATE_ACCESSED, false, type);
    symbolTableSet(vm->symtab, symbol, item);
    return item;
}

void loadSourceFile(VM* vm, const char* filePath) {
    char* source = readFile(filePath);
    interpret(vm, source);
    free(source);
}

void registerNativeFunctions(VM* vm){
    DEF_FUNCTION(assert, RETURN_TYPE(void), 2, PARAM_TYPE(Object), PARAM_TYPE(String));
    DEF_FUNCTION(clock, RETURN_TYPE(Number), 0);
    DEF_FUNCTION(error, RETURN_TYPE(void), 1, PARAM_TYPE(String));
    DEF_FUNCTION(gc, RETURN_TYPE(void), 1, PARAM_TYPE(Int));
    DEF_FUNCTION(print, RETURN_TYPE(void), 1, PARAM_TYPE(Object));
    DEF_FUNCTION(println, RETURN_TYPE(void), 1, PARAM_TYPE(Object));
    DEF_FUNCTION(read, RETURN_TYPE(String), 0);
}