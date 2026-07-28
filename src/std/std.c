#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "collection.h"
#include "io.h"
#include "lang.h"
#include "net.h"
#include "util.h"
#include "std.h"
#include "../vm/namespace.h"
#include "../vm/native.h"
#include "../vm/vm.h"

static ObjNamespace* defineRootNamespace(VM* vm) {
    ObjString* name = newStringPerma(vm, "");
    ObjNamespace* rootNamespace = newNamespace(vm, name, NULL);
    rootNamespace->isRoot = true;

    push(vm, OBJ_VAL(rootNamespace));
    tableSet(vm, &vm->namespaces, name, OBJ_VAL(rootNamespace));
    pop(vm);
    return rootNamespace;
}

static ObjClass* defineSpecialClass(VM* vm, const char* name, BehaviorType behavior) {
    ObjString* className = newStringPerma(vm, name);
    push(vm, OBJ_VAL(className));
    ObjClass* nativeClass = createClass(vm, className, NULL, behavior);
    nativeClass->isNative = true;
    push(vm, OBJ_VAL(nativeClass));

    tableSet(vm, &vm->classes, nativeClass->fullName, OBJ_VAL(nativeClass));
    tableSet(vm, &vm->rootNamespace->values, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop(vm);
    pop(vm);
    typeTableInsertBehavior(vm->typetab, TYPE_CATEGORY_CLASS, className, nativeClass->fullName, NULL);
    return nativeClass;
}

static void defineCollectionTypes(VM* vm) {
    ObjNamespace* collectionNamespace = defineNativeNamespace(vm, "collection", vm->stdNamespace);
    vm->currentNamespace = collectionNamespace;

    TypeInfo* elementType = declareNativeTypeParameter(vm, "E");
    ObjClass* iterableTrait = getNativeClass(vm, "clox.std.lang.TIterable");
    ObjClass* collectionClass = defineNativeGenericClass(vm, "Collection", 1, elementType);
    ObjClass* listClass = defineNativeGenericClass(vm, "List", 1, elementType);

    vm->arrayClass = defineNativeGenericClass(vm, "Array", 1, elementType);
    ObjClass* arrayIteratorClass = defineNativeGenericClass(vm, "ArrayIterator", 1, elementType);
    ObjClass* linkedListClass = defineNativeGenericClass(vm, "LinkedList", 1, elementType);
    ObjClass* linkedListIteratorClass = defineNativeGenericClass(vm, "LinkedListIterator", 1, elementType);
    vm->nodeClass = defineNativeGenericClass(vm, "Node", 1, elementType);

    TypeInfo* keyType = declareNativeTypeParameter(vm, "K");
    TypeInfo* valueType = declareNativeTypeParameter(vm, "V");
    vm->dictionaryClass = defineNativeGenericClass(vm, "Dictionary", 2, keyType, valueType);
    ObjClass* dictionaryIteratorClass = defineNativeGenericClass(vm, "DictionaryIterator", 1, valueType);
    vm->entryClass = defineNativeGenericClass(vm, "Entry", 2, keyType, valueType);

    ObjClass* setClass = defineNativeGenericClass(vm, "Set", 1, elementType);
    ObjClass* setIteratorClass = defineNativeGenericClass(vm, "SetIterator", 1, elementType);
    vm->rangeClass = defineNativeClass(vm, "Range");
    ObjClass* rangeIteratorClass = defineNativeClass(vm, "RangeIterator");
    
    ObjClass* stackClass = defineNativeGenericClass(vm, "Stack", 1, elementType);
    ObjClass* stackIteratorClass = defineNativeGenericClass(vm, "StackIterator", 1, elementType);
    ObjClass* queueClass = defineNativeGenericClass(vm, "Queue", 1, elementType);
    ObjClass* queueIteratorClass = defineNativeGenericClass(vm, "QueueIterator", 1, elementType);
    vm->currentNamespace = vm->rootNamespace;
}

static void defineIOTypes(VM* vm) {
    ObjNamespace* ioNamespace = defineNativeNamespace(vm, "io", vm->stdNamespace);
    vm->currentNamespace = ioNamespace;

    vm->fileClass = defineNativeClass(vm, "File");
    ObjClass* closableTrait = defineNativeTrait(vm, "TClosable");
    ObjClass* ioStreamClass = defineNativeClass(vm, "IOStream");
    ObjClass* readStreamClass = defineNativeClass(vm, "ReadStream");
    ObjClass* writeStreamClass = defineNativeClass(vm, "WriteStream");

    ObjClass* binaryReadStreamClass = defineNativeClass(vm, "BinaryReadStream");
    ObjClass* binaryWriteStreamClass = defineNativeClass(vm, "BinaryWriteStream");
    ObjClass* fileReadStreamClass = defineNativeClass(vm, "FileReadStream");
    ObjClass* fileWriteStreamClass = defineNativeClass(vm, "FileWriteStream");
    vm->currentNamespace = vm->rootNamespace;
}

static void defineLangTypes(VM* vm) {
    vm->rootNamespace = defineRootNamespace(vm);
    vm->cloxNamespace = defineNativeNamespace(vm, "clox", vm->rootNamespace);
    vm->stdNamespace = defineNativeNamespace(vm, "std", vm->cloxNamespace);
    vm->langNamespace = defineNativeNamespace(vm, "lang", vm->stdNamespace);
    vm->currentNamespace = vm->langNamespace;

    vm->objectClass = defineSpecialClass(vm, "Object", BEHAVIOR_CLASS);
    ObjClass* behaviorClass = defineSpecialClass(vm, "Behavior", BEHAVIOR_CLASS);
    vm->classClass = defineSpecialClass(vm, "Class", BEHAVIOR_CLASS);
    vm->metaclassClass = defineSpecialClass(vm, "Metaclass", BEHAVIOR_METACLASS);
    ObjClass* objectMetaclass = defineSpecialClass(vm, "Object class", BEHAVIOR_METACLASS);
    ObjClass* behaviorMetaclass = defineSpecialClass(vm, "Behavior class", BEHAVIOR_METACLASS);
    ObjClass* classMetaclass = defineSpecialClass(vm, "Class class", BEHAVIOR_METACLASS);
    ObjClass* metaclassMetaclass = defineSpecialClass(vm, "Metaclass class", BEHAVIOR_METACLASS);

    vm->methodClass = defineNativeClass(vm, "Method");
    vm->namespaceClass = defineNativeClass(vm, "Namespace");
    vm->traitClass = defineNativeClass(vm, "Trait");
    vm->typeClass = defineNativeClass(vm, "Type");

    insertGlobalSymbolTable(vm, "clox", "Namespace");
    insertGlobalSymbolTable(vm, "Object", "Object class");
    insertGlobalSymbolTable(vm, "Behavior", "Behavior class");
    insertGlobalSymbolTable(vm, "Class", "Class class");
    insertGlobalSymbolTable(vm, "Metaclass", "Metaclass class");
    insertGlobalSymbolTable(vm, "Method", "Method class");
    insertGlobalSymbolTable(vm, "Namespace", "Namespace class");
    insertGlobalSymbolTable(vm, "Trait", "Trait class");
    insertGlobalSymbolTable(vm, "Type", "Type class");

    vm->nilClass = defineNativeClass(vm, "Nil");
    vm->boolClass = defineNativeClass(vm, "Bool");
    ObjClass* comparableTrait = defineNativeTrait(vm, "TComparable");
    vm->numberClass = defineNativeClass(vm, "Number");
    vm->intClass = defineNativeClass(vm, "Int");
    vm->floatClass = defineNativeClass(vm, "Float");

    TypeInfo* elementType = declareNativeTypeParameter(vm, "E");
    ObjClass* iterableTrait = defineNativeGenericTrait(vm, "TIterable", 1, elementType);
    ObjClass* iteratorTrait = defineNativeGenericTrait(vm, "TIterator", 1, elementType);
    vm->iteratorClass = defineNativeGenericClass(vm, "Iterator", 1, elementType);
    vm->stringClass = defineNativeClass(vm, "String");
    ObjClass* stringIteratorClass = defineNativeGenericClass(vm, "StringIterator", 1, elementType);

    ObjClass* callableTrait = defineNativeTrait(vm, "TCallable");
    vm->functionClass = defineNativeClass(vm, "Function");
    vm->boundMethodClass = defineNativeClass(vm, "BoundMethod");
    vm->generatorClass = defineNativeClass(vm, "Generator");
    vm->exceptionClass = defineNativeClass(vm, "Exception");

    insertGlobalSymbolTable(vm, "Nil", "Nil class");
	insertGlobalSymbolTable(vm, "Bool", "Bool class");
    insertGlobalSymbolTable(vm, "TComparable", "Trait");
    insertGlobalSymbolTable(vm, "Number", "Number class");
	insertGlobalSymbolTable(vm, "Int", "Int class");
    insertGlobalSymbolTable(vm, "Float", "Float class");
	insertGlobalSymbolTable(vm, "TIterable", "Trait");
	insertGlobalSymbolTable(vm, "TIterator", "Trait");
	insertGlobalSymbolTable(vm, "Iterator", "Iterator class");
    insertGlobalSymbolTable(vm, "String", "String class");
	insertGlobalSymbolTable(vm, "StringIterator", "StringIterator class");
	insertGlobalSymbolTable(vm, "TCallable", "Trait");
    insertGlobalSymbolTable(vm, "Function", "Function class");
    insertGlobalSymbolTable(vm, "BoundMethod", "BoundMethod class");
	insertGlobalSymbolTable(vm, "Generator", "Generator class");
    insertGlobalSymbolTable(vm, "Exception", "Exception class");
    vm->currentNamespace = vm->rootNamespace;
}

static void defineNetTypes(VM* vm) {
    ObjNamespace* netNamespace = defineNativeNamespace(vm, "net", vm->stdNamespace);
    vm->currentNamespace = netNamespace;

    ObjClass* urlClass = defineNativeClass(vm, "URL");
    ObjClass* domainClass = defineNativeClass(vm, "Domain");
    ObjClass* ipAddressClass = defineNativeClass(vm, "IPAddress");
    ObjClass* socketAddressClass = defineNativeClass(vm, "SocketAddress");
    ObjClass* socketClass = defineNativeClass(vm, "Socket");
    ObjClass* socketClientClass = defineNativeClass(vm, "SocketClient");
    ObjClass* socketServerClass = defineNativeClass(vm, "SocketServer");

    ObjClass* httpRequestClass = defineNativeClass(vm, "HTTPRequest");
    ObjClass* httpResponseClass = defineNativeClass(vm, "HTTPResponse");
    ObjClass* httpClientClass = defineNativeClass(vm, "HTTPClient");
    vm->currentNamespace = vm->rootNamespace;
}

static void defineUtilTypes(VM* vm) {
    ObjNamespace* utilNamespace = defineNativeNamespace(vm, "util", vm->stdNamespace);
    vm->currentNamespace = utilNamespace;

    TypeInfo* placeholderType = declareNativeTypeParameter(vm, "T");
    ObjClass* comparableTrait = getNativeClass(vm, "clox.std.lang.TComparable");
    ObjClass* dateClass = defineNativeClass(vm, "Date");
    ObjClass* dateTimeClass = defineNativeClass(vm, "DateTime");
    ObjClass* durationClass = defineNativeClass(vm, "Duration");
    vm->promiseClass = defineNativeGenericClass(vm, "Promise", 1, placeholderType);
    ObjClass* randomClass = defineNativeClass(vm, "Random");
    ObjClass* regexClass = defineNativeClass(vm, "Regex");
    vm->timerClass = defineNativeClass(vm, "Timer");
    ObjClass* uuidClass = defineNativeClass(vm, "UUID");

    vm->currentNamespace = vm->rootNamespace;
}

void registerStdPackages(VM* vm) {
    defineLangTypes(vm);
	defineCollectionTypes(vm);
	defineUtilTypes(vm);
    defineIOTypes(vm);
	defineNetTypes(vm);

    registerLangPackage(vm);
    registerCollectionPackage(vm);
    registerUtilPackage(vm);
    registerIOPackage(vm);
    registerNetPackage(vm);
    registerNativeFunctions(vm);
}