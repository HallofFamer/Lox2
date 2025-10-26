#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "../common/os.h"

Obj* allocateObject(VM* vm, size_t size, ObjCategory category, ObjClass* klass, GCGenerationType generation) {
    Obj* object = (Obj*)reallocate(vm, NULL, 0, size, generation);
    object->category = category;
    object->klass = klass;
    object->isMarked = false;
    object->generation = generation;
    object->objectID = 0;
    object->shapeID = getDefaultShapeIDForObject(object);

    GCGeneration* currentHeap = GET_GC_GENERATION(generation);
    object->next = currentHeap->objects;
    currentHeap->objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d at generation %d\n", (void*)object, size, category, generation);
#endif

    return object;
}

Value emptyObject(VM* vm, ObjClass* klass) {
    switch (klass->classType) {
        case OBJ_ARRAY: return OBJ_VAL(newArray(vm));
        case OBJ_BOUND_METHOD: return OBJ_VAL(newBoundMethod(vm, NIL_VAL, NIL_VAL));
        case OBJ_CLASS: return OBJ_VAL(ALLOCATE_CLASS(klass));
        case OBJ_CLOSURE: return OBJ_VAL(ALLOCATE_CLOSURE(klass, GC_GENERATION_TYPE_EDEN));
        case OBJ_DICTIONARY: return OBJ_VAL(newDictionary(vm));
        case OBJ_ENTRY: return OBJ_VAL(newEntry(vm, NIL_VAL, NIL_VAL));
        case OBJ_EXCEPTION: return OBJ_VAL(newException(vm, emptyString(vm), klass));
        case OBJ_FILE: return OBJ_VAL(newFile(vm, NULL));
        case OBJ_GENERATOR: return OBJ_VAL(newGenerator(vm, NULL, NULL));
        case OBJ_INSTANCE: return OBJ_VAL(newInstance(vm, klass));
        case OBJ_ITERATOR: return OBJ_VAL(newIterator(vm, NIL_VAL, klass));
        case OBJ_METHOD: return OBJ_VAL(newMethod(vm, NULL, NULL));
        case OBJ_NAMESPACE: return OBJ_VAL(ALLOCATE_NAMESPACE(klass));
        case OBJ_NODE: return OBJ_VAL(newNode(vm, NIL_VAL, NULL, NULL));
        case OBJ_PROMISE: return OBJ_VAL(newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL));
        case OBJ_RANGE: return OBJ_VAL(newRange(vm, 0, 1));
        case OBJ_RECORD: return OBJ_VAL(newRecord(vm, NULL));
        case OBJ_STRING: return OBJ_VAL(ALLOCATE_STRING(0, klass));
        case OBJ_TIMER: return OBJ_VAL(newTimer(vm, NULL, 0, 0));
        case OBJ_TYPE: return OBJ_VAL(newType(vm, emptyString(vm), NULL));
        case OBJ_VALUE_INSTANCE: return OBJ_VAL(newValueInstance(vm, NIL_VAL, klass));
        default: return NIL_VAL;
    }
}

ObjArray* newArray(VM* vm) {
    ObjArray* array = ALLOCATE_OBJ(ObjArray, OBJ_ARRAY, vm->arrayClass);
    initValueArray(&array->elements, array->obj.generation);
    return array;
}

ObjBoundMethod* newBoundMethod(VM* vm, Value receiver, Value method) {
    ObjBoundMethod* boundMethod = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD, vm->boundMethodClass);
    boundMethod->receiver = receiver;
    boundMethod->method = method;
    boundMethod->isNative = IS_NATIVE_METHOD(method);
    return boundMethod;
}

ObjClass* newClass(VM* vm, ObjString* name, ObjCategory classType) {
    if (vm->behaviorCount == INT32_MAX) {
        runtimeError(vm, "Cannot have more than %d classes/traits.", INT32_MAX);
        exit(70);
    }

    ObjString* metaclassName = formattedStringPerma(vm, "%s class", name->chars);
    ObjClass* metaclass = createClass(vm, metaclassName, vm->metaclassClass, BEHAVIOR_METACLASS);
    metaclass->classType = OBJ_CLASS;
    push(vm, OBJ_VAL(metaclass));

    ObjClass* klass = createClass(vm, name, metaclass, BEHAVIOR_CLASS);
    klass->classType = classType;
    klass->obj.shapeID = metaclass->defaultShapeID;

    for (int i = 0; i < metaclass->defaultInstanceFields.count; i++) {
        idMapSet(vm, &klass->indexes, name, i);
        valueArrayWrite(vm, &klass->fields, metaclass->defaultInstanceFields.values[i]);
    }

    pop(vm);
    return klass;
}

void initClosure(VM* vm, ObjClosure* closure, ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount, closure->obj.generation);
    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    closure->function = function;
    closure->module = vm->currentModule;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
}

ObjClosure* newClosure(VM* vm, ObjFunction* function) {
    ObjClosure* closure = ALLOCATE_CLOSURE(vm->functionClass, function->obj.generation);
    push(vm, OBJ_VAL(closure));
    initClosure(vm, closure, function);
    pop(vm);
    return closure;
}

ObjDictionary* newDictionary(VM* vm) {
    ObjDictionary* dict = ALLOCATE_OBJ(ObjDictionary, OBJ_DICTIONARY, vm->dictionaryClass);
    dict->count = 0;
    dict->capacity = 0;
    dict->entries = NULL;
    return dict;
}

ObjEntry* newEntry(VM* vm, Value key, Value value) {
    ObjEntry* entry = ALLOCATE_OBJ(ObjEntry, OBJ_ENTRY, vm->entryClass);
    entry->key = key;
    entry->value = value;
    return entry;
}

ObjException* newException(VM* vm, ObjString* message, ObjClass* klass) {
    ObjException* exception = ALLOCATE_OBJ(ObjException, OBJ_EXCEPTION, klass);
    push(vm, OBJ_VAL(exception));
    exception->message = message;
    exception->stacktrace = newArray(vm);
    pop(vm);
    return exception;
}

ObjFile* newFile(VM* vm, ObjString* name) {
    ObjFile* file = ALLOCATE_OBJ(ObjFile, OBJ_FILE, vm->fileClass);
    file->name = name;
    file->mode = emptyString(vm);
    file->isOpen = false;
    file->offset = 0;

    file->fsStat = NULL;
    file->fsOpen = NULL;
    file->fsRead = NULL;
    file->fsWrite = NULL;
    return file;
}

ObjFrame* newFrame(VM* vm, CallFrame* callFrame) {
    ObjFrame* frame = ALLOCATE_OBJ(ObjFrame, OBJ_FRAME, NULL);
    frame->closure = callFrame->closure;
    frame->ip = callFrame->ip;
    frame->slotCount = callFrame->closure->function->arity + 1; 
    frame->handlerCount = callFrame->handlerCount;

    for (int i = 0; i < frame->slotCount; i++) {
        frame->slots[i] = peek(vm, callFrame->closure->function->arity - i);
    }

    for (int i = 0; i < frame->handlerCount; i++) {
        frame->handlerStack[i] = callFrame->handlerStack[i];
    }
    return frame;
}

ObjFunction* newFunction(VM* vm, ObjString* name, bool isAsync) {
    bool isAnonymous = (name != NULL) && (name->length == 0 || name->chars[0] == '{' || strcmp(name->chars, "fun") == 0);
    GCGenerationType generation = isAnonymous ? GC_GENERATION_TYPE_EDEN : GC_GENERATION_TYPE_PERMANENT;
    ObjFunction* function = ALLOCATE_OBJ_GEN(ObjFunction, OBJ_FUNCTION, NULL, generation);

    function->arity = 0;
    function->upvalueCount = 0;
    function->isGenerator = false;
    function->isAsync = isAsync;
    function->name = name;
    initChunk(&function->chunk, function->obj.generation);
    return function;
}

ObjGenerator* newGenerator(VM* vm, ObjFrame* frame, ObjGenerator* outer) {
    ObjGenerator* generator = ALLOCATE_OBJ(ObjGenerator, OBJ_GENERATOR, vm->generatorClass);
    generator->frame = frame;
    generator->outer = outer;
    generator->inner = NULL;
    generator->state = GENERATOR_START;
    generator->value = NIL_VAL;
    return generator;
}

ObjInstance* newInstance(VM* vm, ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE, klass);
    initValueArray(&instance->fields, instance->obj.generation);
    instance->obj.shapeID = klass->defaultShapeID;

    for (int i = 0; i < klass->defaultInstanceFields.count; i++) {
        valueArrayWrite(vm, &instance->fields, klass->defaultInstanceFields.values[i]);
    }
    return instance;
}

ObjIterator* newIterator(VM* vm, Value iterable, ObjClass* klass) {
    ObjIterator* iterator = ALLOCATE_OBJ(ObjIterator, OBJ_ITERATOR, klass);
    iterator->iterable = iterable;
    iterator->position = -1;
    iterator->value = NIL_VAL;
    return iterator;
}

ObjMethod* newMethod(VM* vm, ObjClass* behavior, ObjClosure* closure) {
    ObjMethod* method = ALLOCATE_OBJ_GEN(ObjMethod, OBJ_METHOD, vm->methodClass, GC_GENERATION_TYPE_PERMANENT);
    method->behavior = behavior;
    method->closure = closure;
    return method;
}

ObjModule* newModule(VM* vm, ObjString* path) {
    ObjModule* module = ALLOCATE_OBJ_GEN(ObjModule, OBJ_MODULE, NULL, GC_GENERATION_TYPE_PERMANENT);
    module->path = path;
    module->closure = NULL;
    module->isNative = false;

    initIDMap(&module->valIndexes, module->obj.generation);
    initValueArray(&module->valFields, module->obj.generation);
    initIDMap(&module->varIndexes, module->obj.generation);
    initValueArray(&module->varFields, module->obj.generation);

    for (int i = 0; i < vm->langNamespace->values.capacity; i++) {
        Entry* entry = &vm->langNamespace->values.entries[i];
        if (entry->key != NULL) {
            idMapSet(vm, &module->valIndexes, entry->key, module->valFields.count);
            valueArrayWrite(vm, &module->valFields, entry->value);
        }
    }
    
    tableSet(vm, &vm->modules, path, NIL_VAL);
    return module;
}

void initNamespace(VM* vm, ObjNamespace* namespace, ObjString* shortName, ObjNamespace* enclosing) {
    push(vm, OBJ_VAL(namespace));
    namespace->shortName = shortName;
    namespace->enclosing = enclosing;
    namespace->isRoot = false;

    if (namespace->enclosing != NULL && !namespace->enclosing->isRoot) {
        char chars[UINT8_MAX];
        int length = sprintf_s(chars, UINT8_MAX, "%s.%s", namespace->enclosing->fullName->chars, shortName->chars);
        namespace->fullName = copyStringPerma(vm, chars, length);
    }
    else namespace->fullName = namespace->shortName;

    initTable(&namespace->values, namespace->obj.generation);
    pop(vm);
}

ObjNamespace* newNamespace(VM* vm, ObjString* shortName, ObjNamespace* enclosing) {
    ObjNamespace* namespace = ALLOCATE_NAMESPACE(vm->namespaceClass);
    initNamespace(vm, namespace, shortName, enclosing);
    return namespace;
}

ObjNativeFunction* newNativeFunction(VM* vm, ObjString* name, int arity, bool isAsync, NativeFunction function) {
    ObjNativeFunction* nativeFunction = ALLOCATE_OBJ_GEN(ObjNativeFunction, OBJ_NATIVE_FUNCTION, vm->functionClass, GC_GENERATION_TYPE_PERMANENT);
    nativeFunction->name = name;
    nativeFunction->arity = arity;
    nativeFunction->isAsync = isAsync;
    nativeFunction->function = function;
    return nativeFunction;
}

ObjNativeMethod* newNativeMethod(VM* vm, ObjClass* klass, ObjString* name, int arity, bool isAsync, NativeMethod method) {
    ObjNativeMethod* nativeMethod = ALLOCATE_OBJ_GEN(ObjNativeMethod, OBJ_NATIVE_METHOD, vm->methodClass, GC_GENERATION_TYPE_PERMANENT);
    nativeMethod->klass = klass;
    nativeMethod->name = name;
    nativeMethod->arity = arity;
    nativeMethod->isAsync = isAsync;
    nativeMethod->method = method;
    return nativeMethod;
}

ObjNode* newNode(VM* vm, Value element, ObjNode* prev, ObjNode* next) {
    ObjNode* node = ALLOCATE_OBJ(ObjNode, OBJ_NODE, vm->nodeClass);
    node->element = element;
    node->prev = prev;
    node->next = next;
    return node;
}

ObjPromise* newPromise(VM* vm, PromiseState state, Value value, Value executor){
    ObjPromise* promise = ALLOCATE_OBJ(ObjPromise, OBJ_PROMISE, vm->promiseClass);
    promise->id = ++vm->promiseCount;
    promise->state = state;
    promise->value = value;

    if (state == PROMISE_REJECTED && IS_EXCEPTION(value)) {
        promise->exception = AS_EXCEPTION(value);
    }
    else promise->exception = NULL;

    promise->executor = executor;
    promise->onCatch = NIL_VAL;
    promise->onFinally = NIL_VAL;
    push(vm, OBJ_VAL(promise));

    promise->captures = newDictionary(vm);
    initValueArray(&promise->handlers, promise->obj.generation);
    pop(vm);
    return promise;
}

ObjRange* newRange(VM* vm, int from, int to) {
    ObjRange* range = ALLOCATE_OBJ(ObjRange, OBJ_RANGE, vm->rangeClass);
    range->from = from;
    range->to = to;
    return range;
}

ObjRecord* newRecord(VM* vm, void* data) {
    ObjRecord* record = ALLOCATE_OBJ(ObjRecord, OBJ_RECORD, NULL);
    record->data = data;
    record->sizeFunction = NULL;
    record->markFunction = NULL;
    record->freeFunction = NULL;
    record->shouldFree = true;
    return record;
}

ObjTimer* newTimer(VM* vm, ObjClosure* closure, int delay, int interval) {
    ObjTimer* timer = ALLOCATE_OBJ(ObjTimer, OBJ_TIMER, vm->timerClass);
    TimerData* data = ALLOCATE_STRUCT(TimerData);
    timer->timer = ALLOCATE_STRUCT(uv_timer_t);

    if (timer->timer != NULL) {
        timer->timer->data = timerData(vm, closure, delay, interval);
        if (timer->timer->data != NULL) {
            timer->id = 0;
            timer->isRunning = false;
            return timer;
        }
    }

    throwNativeException(vm, "clox.std.lang.OutOfMemoryException", "Not enough memory to allocate timer object.");
    return NULL;
}

static Value createTypeValue(VM* vm, TypeInfo* type) {
    if (type == NULL) return NIL_VAL;
    ObjString* name = type->shortName;
    if (IS_ALIAS_TYPE(type)) type = AS_ALIAS_TYPE(type)->targetType;
    return OBJ_VAL(newType(vm, name, type));
}

ObjType* newType(VM* vm, ObjString* name, TypeInfo* typeInfo) {
    TypeInfo* targetType = IS_ALIAS_TYPE(typeInfo) ? AS_ALIAS_TYPE(typeInfo)->targetType : typeInfo;
    ObjType* type = ALLOCATE_OBJ_GEN(ObjType, OBJ_TYPE, vm->typeClass, GC_GENERATION_TYPE_PERMANENT);
    type->name = name;
    type->category = targetType->category;
    type->isAlias = IS_ALIAS_TYPE(typeInfo);
    type->behavior = getClassFromTypeInfo(vm, targetType);
    initValueArray(&type->parameters, GC_GENERATION_TYPE_PERMANENT);

    if (targetType != NULL && IS_CALLABLE_TYPE(targetType)) {
        CallableTypeInfo* callableType = AS_CALLABLE_TYPE(targetType);
        valueArrayWrite(vm, &type->parameters, createTypeValue(vm, callableType->returnType));
        for (int i = 0; i < callableType->paramTypes->count; i++) {
            TypeInfo* paramType = callableType->paramTypes->elements[i];
            valueArrayWrite(vm, &type->parameters, createTypeValue(vm, paramType));
        }
    }
    else if (targetType != NULL && IS_GENERIC_TYPE(targetType)) {
        GenericTypeInfo* genericType = AS_GENERIC_TYPE(targetType);
        for (int i = 0; i < genericType->parameters->count; i++) {
            TypeInfo* paramType = genericType->parameters->elements[i];
            valueArrayWrite(vm, &type->parameters, createTypeValue(vm, paramType));
        }
    }

    tableSet(vm, &vm->types, name, OBJ_VAL(type));
    return type;
}

ObjUpvalue* newUpvalue(VM* vm, Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE, NULL);
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

ObjValueInstance* newValueInstance(VM* vm, Value value, ObjClass* klass) {
    ObjValueInstance* valueInstance = ALLOCATE_OBJ(ObjValueInstance, OBJ_VALUE_INSTANCE, klass);
    valueInstance->value = value;
    initValueArray(&valueInstance->fields, valueInstance->obj.generation);
    return valueInstance;
}

Value getObjField(VM* vm, ObjInstance* object, char* name) {
    IDMap* idMap = getShapeIndexes(vm, object->obj.shapeID);
    int index;
    idMapGet(idMap, newStringPerma(vm, name), &index);
    return object->fields.values[index];
}

Value getObjFieldByIndex(VM* vm, ObjInstance* object, int index) {
    if (index >= object->fields.count) {
        runtimeError(vm, "Invalid index %d for object %s field.", index, object->obj.klass->fullName->chars);
        exit(70);
    }
    return object->fields.values[index];
}

void setObjField(VM* vm, ObjInstance* object, char* name, Value value) {
    PROCESS_WRITE_BARRIER((Obj*)object, value);
    IDMap* idMap = getShapeIndexes(vm, object->obj.shapeID);
    ObjString* key = newStringPerma(vm, name);
    int index;
    push(vm, OBJ_VAL(key));

    if (idMapGet(idMap, key, &index)) object->fields.values[index] = value;
    else {
        transitionShapeForObject(vm, &object->obj, key);
        valueArrayWrite(vm, &object->fields, value);
    }
    pop(vm);
}

void setObjFieldByIndex(VM* vm, ObjInstance* object, int index, Value value) {
    PROCESS_WRITE_BARRIER((Obj*)object, value);
    if (index < object->fields.count) {
        runtimeError(vm, "Invalid index %d for object %s field.", index, object->obj.klass->fullName->chars);
        exit(70);
    }
    object->fields.values[index] = value;
}

void copyObjField(VM* vm, ObjInstance* fromObject, ObjInstance* toObject, char* name) {
    Value value = getObjField(vm, fromObject, name);
    setObjField(vm, toObject, name, value);
}

void copyObjFields(VM* vm, ObjInstance* fromObject, ObjInstance* toObject) {
    toObject->obj.shapeID = fromObject->obj.shapeID;
    for (int i = 0; i < fromObject->fields.count; i++) {
        PROCESS_WRITE_BARRIER((Obj*)toObject, fromObject->fields.values[i]);
        valueArrayWrite(vm, &toObject->fields, fromObject->fields.values[i]);
    }
}

Value getObjMethod(VM* vm, Value object, char* name) {
    ObjClass* klass = getObjClass(vm, object);
    Value method;
    if (!tableGet(&klass->methods, newStringPerma(vm, name), &method)) {
        runtimeError(vm, "Method %s::%s does not exist for class %s.", klass->name->chars, name, klass->name->chars);
        exit(70);
    }
    return method;
}

static void printArray(ObjArray* array) {
    printf("[");
    for (int i = 0; i < array->elements.count; i++) {
        printValue(array->elements.values[i]);
        if (i < array->elements.count - 1) printf(", ");
    }
    printf("]");
}

static void printClass(ObjClass* klass) {
    switch (klass->behaviorType) {
        case BEHAVIOR_METACLASS:
            printf("<metaclass %s>", klass->name->chars);
            break;
        case BEHAVIOR_TRAIT:
            printf("<trait %s>", klass->name->chars);
            break;
        default:
            printf("<class %s>", klass->name->chars);
    }
}

static void printDictionary(ObjDictionary* dictionary) {
    printf("[");
    int startIndex = 0;
    for (int i = 0; i < dictionary->capacity; i++) {
        ObjEntry* entry = &dictionary->entries[i];
        if (IS_UNDEFINED(entry->key)) continue;
        printValue(entry->key);
        printf(": ");
        printValue(entry->value);
        startIndex = i + 1;
        break;
    }

    for (int i = startIndex; i < dictionary->capacity; i++) {
        ObjEntry* entry = &dictionary->entries[i];
        if (IS_UNDEFINED(entry->key)) continue;
        printf(", ");
        printValue(entry->key);
        printf(": ");
        printValue(entry->value);
    }
    printf("]");
}

static void printFunction(ObjFunction* function) {
    if (function->name == NULL) printf("<script>");
    else if (function->name->length == 0 || function->name->chars[0] == '{') printf("<function>");
    else printf("<function %s>", function->name->chars);
}

static void printType(ObjType* type) {
    printf("<type %s: ", type->name->chars);
    if (IS_BEHAVIOR_TYPE(type)) printf("%s", type->behavior->name->chars);
    else if (IS_CALLABLE_TYPE(type)) {
        printf("%s fun(", AS_TYPE(type->parameters.values[0])->name->chars);
        for (int i = 1; i < type->parameters.count; i++) {
            if (i > 1) printf(", ");
            printf("%s", AS_TYPE(type->parameters.values[i])->name->chars);
        }
        printf(")");
    }
    else if (IS_GENERIC_TYPE(type)) {
        printf("%s<", type->name->chars);
        for (int i = 0; i < type->parameters.count; i++) {
            if (i > 0) printf(", ");
            printf("%s", AS_TYPE(type->parameters.values[i])->name->chars);
        }
        printf(">");
        return;
    }
    else if (IS_VOID_TYPE(type)) printf("void");
    else printf("dynamic");
    printf(">");
}

void printObject(Value value) {
    switch (OBJ_CATEGORY(value)) {
        case OBJ_ARRAY:
            printArray(AS_ARRAY(value));
            break;
        case OBJ_BOUND_METHOD: { 
            ObjBoundMethod* boundMethod = AS_BOUND_METHOD(value);
            if (IS_NATIVE_METHOD(boundMethod->method)) printf("<bound method %s::%s>", AS_OBJ(boundMethod->receiver)->klass->name->chars, AS_NATIVE_METHOD(boundMethod->method)->name->chars);
            else printf("<bound method %s::%s>", AS_OBJ(boundMethod->receiver)->klass->name->chars, AS_CLOSURE(boundMethod->method)->function->name->chars);
            break;
        }
        case OBJ_CLASS:
            printClass(AS_CLASS(value));
            break;
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
        case OBJ_DICTIONARY:
            printDictionary(AS_DICTIONARY(value));
            break;
        case OBJ_ENTRY: {
            ObjEntry* entry = AS_ENTRY(value);
            printf("<entry(");
            printValue(entry->key);
            printf(": ");
            printValue(entry->value);
            printf(")>");
            break;
        }
        case OBJ_EXCEPTION:
            printf("<exception: %s>", AS_EXCEPTION(value)->obj.klass->fullName->chars);
            break;
        case OBJ_FILE:
            printf("<file \"%s\">", AS_FILE(value)->name->chars);
            break;
        case OBJ_FRAME: 
            printf("<frame: %s>", AS_FRAME(value)->closure->function->name->chars);
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_GENERATOR: { 
            ObjFunction* function = AS_GENERATOR(value)->frame->closure->function;
            printf("<generator %s>", (function->name == NULL) ? "script" : function->name->chars);
            break;
        }
        case OBJ_INSTANCE:
            printf("<object %s>", AS_OBJ(value)->klass->name->chars);
            break;
        case OBJ_ITERATOR: {
            ObjIterator* iterator = AS_ITERATOR(value);
            printf("<iterator %s>", iterator->obj.klass->name->chars);
            break;
        }
        case OBJ_METHOD: { 
            ObjMethod* method = AS_METHOD(value);
            printf("<method %s::%s>", method->behavior->name->chars, method->closure->function->name->chars);
            break;
        }
        case OBJ_MODULE:
            printf("<module %s>", AS_MODULE(value)->path->chars);
            break;
        case OBJ_NAMESPACE:
            printf("<namespace %s>", AS_NAMESPACE(value)->fullName->chars);
            break;
        case OBJ_NATIVE_FUNCTION:
            printf("<native function %s>", AS_NATIVE_FUNCTION(value)->name->chars);
            break;
        case OBJ_NATIVE_METHOD: { 
            ObjNativeMethod* nativeMethod = AS_NATIVE_METHOD(value);
            printf("<native method %s::%s>", nativeMethod->klass->name->chars, nativeMethod->name->chars);
            break;
        }
        case OBJ_NODE: {
            ObjNode* node = AS_NODE(value);
            printf("<node(");
            printValue(node->element);
            printf(")>");
            break;
        }
        case OBJ_PROMISE:
            printf("<promise: %d>", AS_PROMISE(value)->id);
            break;
        case OBJ_RANGE: { 
            ObjRange* range = AS_RANGE(value);
            printf("<%d..%d>", range->from, range->to);
            break;
        }
        case OBJ_RECORD:
            printf("<record>");
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_TIMER: 
            printf("<timer: %d>", AS_TIMER(value)->id);
            break;
        case OBJ_TYPE: {
            printType(AS_TYPE(value));
            break;
        }
        case OBJ_UPVALUE:
            printf("<upvalue>");
            break;
        case OBJ_VALUE_INSTANCE: 
            printValue(AS_VALUE_INSTANCE(value)->value);
            break;
        default:
            printf("<unknown>");
  }
}