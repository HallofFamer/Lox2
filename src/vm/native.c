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

void defineNativeFunction(VM* vm, const char* name, int arity, bool isAsync, NativeFunction function, ...) {
    ObjString* functionName = newStringPerma(vm, name);
    ObjNativeFunction* nativeFunction = newNativeFunction(vm, functionName, arity, isAsync, function);
    push(vm, OBJ_VAL(nativeFunction));
    tableSet(vm, &vm->rootNamespace->values, functionName, OBJ_VAL(nativeFunction));
    pop(vm);

    SymbolItem* item = insertGlobalSymbolTable(vm, name, "Function");
    va_list args;
    va_start(args, function);
    TypeInfo* returnType = va_arg(args, TypeInfo*);
    CallableTypeInfo* functionType = newCallableTypeInfo(vm->typetab->count + 1, TYPE_CATEGORY_FUNCTION, functionName, returnType);
    functionType->attribute.isAsync = isAsync;
    functionType->attribute.isVoid = (returnType->category == TYPE_CATEGORY_VOID);

    for (int i = 0; i < arity; i++) {
        TypeInfo* paramType = va_arg(args, TypeInfo*);
        TypeInfoArrayAdd(functionType->paramTypes, paramType);
    }
    typeTableSet(vm->typetab, functionName, (TypeInfo*)functionType);
    va_end(args);
}

void defineNativeField(VM* vm, ObjClass* klass, const char* name, TypeInfo* type, bool isMutable, Value defaultValue) {
    ObjString* fieldName = newStringPerma(vm, name);
    BehaviorTypeInfo* behaviorType = AS_BEHAVIOR_TYPE(typeTableGet(vm->typetab, klass->fullName));
    typeTableInsertField(behaviorType->fields, fieldName, type, isMutable, !IS_NIL(defaultValue));

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
    CallableTypeInfo* methodType = newCallableTypeInfo(behaviorType->methods->count + 1, TYPE_CATEGORY_METHOD, methodName, returnType);
    methodType->attribute.isAsync = isAsync;
    methodType->attribute.isVoid = (returnType->category == TYPE_CATEGORY_VOID);

    if (arity < 0) {
        methodType->attribute.isVariadic = true;
        TypeInfo* paramType = va_arg(args, TypeInfo*);
        TypeInfoArrayAdd(methodType->paramTypes, paramType);
    }
    else {
        for (int i = 0; i < arity; i++) {
            TypeInfo* paramType = va_arg(args, TypeInfo*);
            TypeInfoArrayAdd(methodType->paramTypes, paramType);
        }
    }

    typeTableSet(behaviorType->methods, methodName, (TypeInfo*)methodType);
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

ObjClass* getNativeClass(VM* vm, const char* fullName) {
    Value klass;
    tableGet(&vm->classes, newStringPerma(vm, fullName), &klass);
    if (!IS_CLASS(klass)) {
        runtimeError(vm, "Class %s is undefined.", fullName);
        exit(70);
    }
    return AS_CLASS(klass);
}

TypeInfo* getNativeType(VM* vm, const char* name) {
    if (name == NULL) return NULL;
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
                runtimeError(vm, "Type %s is undefined.", name);
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
    TypeInfo* type = getNativeType(vm, typeName);
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
    DEF_FUNCTION(assert, 2, RETURN_TYPE(void), PARAM_TYPE(Object), PARAM_TYPE(String));
    DEF_FUNCTION(clock, 0, RETURN_TYPE(Number));
    DEF_FUNCTION(error, 1, RETURN_TYPE(void), PARAM_TYPE(String));
    DEF_FUNCTION(gc, 1, RETURN_TYPE(void), PARAM_TYPE(Int));
    DEF_FUNCTION(print, 1, RETURN_TYPE(void), PARAM_TYPE(Object));
    DEF_FUNCTION(println, 1, RETURN_TYPE(void), PARAM_TYPE(Object));
    DEF_FUNCTION(read, 0, RETURN_TYPE(String));
}