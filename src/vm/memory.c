#include <stdlib.h>

#include "hash.h"
#include "memory.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#pragma warning(disable : 33010)

void* reallocate(VM* vm, void* pointer, size_t oldSize, size_t newSize, GCGenerationType generation) {
    GCGeneration* currentHeap = GET_GC_GENERATION(generation);
    currentHeap->bytesAllocated += newSize - oldSize;
    if (newSize > oldSize && generation < GC_GENERATION_TYPE_PERMANENT) {
#ifdef DEBUG_STRESS_GC
        collectGarbage(vm, generation);
#endif

        if (currentHeap->bytesAllocated > currentHeap->heapSize) {
            collectGarbage(vm, generation);
        }
    }

    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

static void initGCRememberedSet(GCRememberedSet* rememberedSet, GCGenerationType generation) {
    rememberedSet->capacity = 0;
    rememberedSet->count = 0;
    rememberedSet->generation = generation;
    rememberedSet->entries = NULL;
}

static void freeGCRememeberedSet(VM* vm, GCRememberedSet* remSet) {
    FREE_ARRAY(GCRememberedEntry, remSet->entries, remSet->capacity, remSet->generation);
    initGCRememberedSet(remSet, remSet->generation);
}

static void initGCGenerations(GC* gc, size_t heapSizes[]) {
    for (int i = 0; i < GC_GENERATION_TYPE_COUNT; i++) {
        gc->generations[i] = (GCGeneration*)malloc(sizeof(GCGeneration));
        if (gc->generations[i] != NULL) {
            gc->generations[i]->bytesAllocated = 0;
            gc->generations[i]->heapSize = heapSizes[i]; 
            gc->generations[i]->objects = NULL;
            gc->generations[i]->type = i;
            initGCRememberedSet(&gc->generations[i]->remSet, i);
        }
        else {
            fprintf(stderr, "Not enough memory to allocate heaps for garbage collector.");
            exit(74);
        }
    }
}

static void freeGCGenerations(VM* vm) {
    for (int i = 0; i < GC_GENERATION_TYPE_COUNT; i++) {
        freeGCRememeberedSet(vm, &vm->gc->generations[i]->remSet);
        free(vm->gc->generations[i]);
    }
}

GC* newGC(VM* vm) {
    GC* gc = (GC*)malloc(sizeof(GC));
    if (gc != NULL) {
        size_t heapSizes[] = { vm->config.gcEdenHeapSize, vm->config.gcYoungHeapSize, vm->config.gcOldHeapSize, vm->config.gcTotalHeapSize - vm->config.gcEdenHeapSize - vm->config.gcYoungHeapSize - vm->config.gcOldHeapSize };
        initGCGenerations(gc, heapSizes);
        gc->grayCapacity = 0;
        gc->grayCount = 0;
        gc->grayStack = NULL;
        return gc;
    }

    fprintf(stderr, "Not enough memory to allocate for garbage collector.");
    exit(74);
}

void freeGC(VM* vm) {
    freeGCGenerations(vm);
    free(vm->gc);
}

static GCRememberedEntry* findRememberedSetEntry(GCRememberedEntry* entries, int capacity, Obj* object) {
    uint32_t hash = hashObject(object);
    uint32_t index = (uint32_t)hash & ((uint32_t)capacity - 1);
    for (;;) {
        GCRememberedEntry* entry = &entries[index];
        if (entry->object == NULL || entry->object == object) {
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

static bool rememberedSetGetObject(GCRememberedSet* rememberedSet, Obj* object) {
    if (rememberedSet->count == 0) return false;
    GCRememberedEntry* entry = findRememberedSetEntry(rememberedSet->entries, rememberedSet->capacity, object);
    if (entry->object == NULL) return false;
    return true;
}

static void rememberedSetAdjustCapacity(VM* vm, GCRememberedSet* remSet, int capacity) {
    GCRememberedEntry* entries = ALLOCATE(GCRememberedEntry, capacity, remSet->generation);
    for (int i = 0; i < capacity; i++) {
        entries[i].object = NULL;
    }

    remSet->count = 0;
    for (int i = 0; i < remSet->capacity; i++) {
        GCRememberedEntry* entry = &remSet->entries[i];
        if (entry->object == NULL) continue;
        GCRememberedEntry* dest = findRememberedSetEntry(entries, capacity, entry->object);
        dest->object = entry->object;
        remSet->count++;
    }

    FREE_ARRAY(GCRememberedEntry, remSet->entries, remSet->capacity, remSet->generation);
    remSet->entries = entries;
    remSet->capacity = capacity;
}

static bool rememberedSetPutObject(VM* vm, GCRememberedSet* remSet, Obj* object) {
    ENSURE_OBJECT_ID(object);
    if (remSet->count + 1 > remSet->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(remSet->capacity);
        rememberedSetAdjustCapacity(vm, remSet, capacity);
    }

    GCRememberedEntry* entry = findRememberedSetEntry(remSet->entries, remSet->capacity, object);
    bool isNewObject = entry->object == NULL;
    if (isNewObject) {
#ifdef DEBUG_LOG_GC
        printf("%p added to remembered set ", (void*)object);
        printValue(OBJ_VAL(object));
        printf("\n");
#endif
        remSet->count++;
    }

    entry->object = object;
    return isNewObject;
}

void addToRememberedSet(VM* vm, Obj* object, GCGenerationType generation) {
    GCRememberedSet* remSet = &vm->gc->generations[generation]->remSet;
    rememberedSetPutObject(vm, remSet, object);
}

void markObject(VM* vm, Obj* object, GCGenerationType generation) {
    if (object == NULL || object->generation > generation || object->isMarked) return;

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    object->isMarked = true;
    if (vm->gc->grayCapacity < vm->gc->grayCount + 1) {
        vm->gc->grayCapacity = GROW_CAPACITY(vm->gc->grayCapacity);
        Obj** grayStack = (Obj**)realloc(vm->gc->grayStack, sizeof(Obj*) * vm->gc->grayCapacity);

        if (grayStack == NULL) {
            fprintf(stderr, "Not enough memory to allocate for GC gray stack.");
            exit(74);
        }
        vm->gc->grayStack = grayStack;
    }
    vm->gc->grayStack[vm->gc->grayCount++] = object;
}

void markValue(VM* vm, Value value, GCGenerationType generation) {
    if (IS_OBJ(value)) {
        markObject(vm, AS_OBJ(value), generation);
    }
}

static void markArray(VM* vm, ValueArray* array, GCGenerationType generation) {
    for (int i = 0; i < array->count; i++) {
        markValue(vm, array->values[i], generation);
    }
}

static void markGlobals(VM* vm, GCGenerationType generation) {
    markIDMap(vm, &vm->currentModule->valIndexes, generation);
    markArray(vm, &vm->currentModule->valFields, generation);
    markIDMap(vm, &vm->currentModule->varIndexes, generation);
    markArray(vm, &vm->currentModule->varFields, generation);
}

void markRememberedSet(VM* vm, GCGenerationType generation) {
    GCRememberedSet* remSet = &vm->gc->generations[generation]->remSet;
    for (int i = 0; i < remSet->capacity; i++) {
        GCRememberedEntry* entry = &remSet->entries[i];
        markObject(vm, entry->object, GC_GENERATION_TYPE_PERMANENT);
    }
}

static size_t sizeOfObject(Obj* object) {
    switch (object->type) {
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            return sizeof(ObjArray) + sizeof(Value) * array->elements.capacity;
        }
        case OBJ_BOUND_METHOD: 
            return sizeof(ObjBoundMethod);
        case OBJ_CLASS: {
            ObjClass* _class = (ObjClass*)object;
            return sizeof(ObjClass) + sizeof(Value) * _class->traits.capacity + sizeof(Value) * _class->fields.capacity
                + sizeof(Entry) * _class->methods.capacity + sizeof(IDEntry) * _class->indexes.capacity + sizeof(Value) * _class->defaultInstanceFields.capacity; 
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            return sizeof(ObjClosure) + sizeof(ObjUpvalue) * closure->upvalueCount;
        }
        case OBJ_DICTIONARY: {
            ObjDictionary* dictionary = (ObjDictionary*)object;
            return sizeof(ObjDictionary) + sizeof(ObjEntry) * dictionary->capacity;
        }
        case OBJ_ENTRY: 
            return sizeof(ObjEntry);
        case OBJ_EXCEPTION:
            return sizeof(ObjException);
        case OBJ_FILE:
            return sizeof(ObjFile) + sizeof(uv_fs_t) * 4;
        case OBJ_FRAME: {
            return sizeof(ObjFrame);
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            return sizeof(ObjFunction) + sizeof(Chunk) + sizeof(uint8_t) * function->chunk.capacity + sizeof(int) * function->chunk.capacity
                + sizeof(InlineCache) * function->chunk.identifiers.capacity + sizeof(Value) * function->chunk.constants.capacity
                + sizeof(Value) * function->chunk.identifiers.capacity;
        }
        case OBJ_GENERATOR: 
            return sizeof(ObjGenerator);
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            return sizeof(ObjInstance) + sizeof(Value) * instance->fields.capacity;
        }
        case OBJ_METHOD: 
            return sizeof(ObjMethod);
        case OBJ_MODULE: {
            ObjModule* module = (ObjModule*)object;
            return sizeof(ObjModule) + sizeof(Value) * module->valFields.capacity + sizeof(IDEntry) * module->valIndexes.capacity
                + sizeof(Value) * module->varFields.capacity + sizeof(IDEntry) * module->varIndexes.capacity;
        }
        case OBJ_NAMESPACE: {
            ObjNamespace* _namespace = (ObjNamespace*)object;
            return sizeof(ObjNamespace) + sizeof(Value) * _namespace->values.capacity;
        }
        case OBJ_NATIVE_FUNCTION:
            return sizeof(ObjNativeFunction);
        case OBJ_NATIVE_METHOD:
            return sizeof(ObjNativeMethod);
        case OBJ_NODE:
            return sizeof(ObjNode);
        case OBJ_PROMISE: {
            ObjPromise* promise = (ObjPromise*)object;
            return sizeof(ObjPromise) + sizeof(Value) * promise->handlers.capacity;
        }
        case OBJ_RANGE:
            return sizeof(ObjRange);
        case OBJ_RECORD: {
            ObjRecord* record = (ObjRecord*)object;
            return sizeof(ObjRecord) + (record->sizeFunction == NULL ? 0 : record->sizeFunction(record->data));
        }
        case OBJ_TIMER: {
            ObjTimer* timer = (ObjTimer*)object;
            return sizeof(ObjTimer) + sizeof(uv_timer_t) + sizeof(timer->timer->data);
        }
        case OBJ_UPVALUE:
            return sizeof(ObjUpvalue);
        case OBJ_VALUE_INSTANCE: {
            ObjValueInstance* instance = (ObjValueInstance*)object;
            return sizeof(ObjValueInstance) + sizeof(Value) * instance->fields.capacity;
        }
        default: 
            return sizeof(Obj);
    }
}

static void blackenObject(VM* vm, Obj* object, GCGenerationType generation) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            markArray(vm, &array->elements, generation);
            break;
        }
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* boundMethod = (ObjBoundMethod*)object;
            markValue(vm, boundMethod->receiver, generation);
            markValue(vm, boundMethod->method, generation);
            break;
        }
        case OBJ_CLASS: {
            ObjClass* _class = (ObjClass*)object;
            markObject(vm, (Obj*)_class->name, generation);
            markObject(vm, (Obj*)_class->fullName, generation);
            markObject(vm, (Obj*)_class->superclass, generation);
            markObject(vm, (Obj*)_class->obj.klass, generation);
            markObject(vm, (Obj*)_class->namespace, generation);
            markArray(vm, &_class->traits, generation);
            markIDMap(vm, &_class->indexes, generation);
            markArray(vm, &_class->fields, generation);
            markTable(vm, &_class->methods, generation);
            markArray(vm, &_class->defaultInstanceFields, generation);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            markObject(vm, (Obj*)closure->function, generation);
            markObject(vm, (Obj*)closure->module, generation);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markObject(vm, (Obj*)closure->upvalues[i], generation);
            }
            break;
        }
        case OBJ_DICTIONARY: {
            ObjDictionary* dict = (ObjDictionary*)object;
            for (int i = 0; i < dict->capacity; i++) {
                ObjEntry* entry = &dict->entries[i];
                markValue(vm, entry->key, generation);
                markObject(vm, (Obj*)entry, generation);
            }
            break;
        }
        case OBJ_ENTRY: {
            ObjEntry* entry = (ObjEntry*)object;
            markValue(vm, entry->key, generation);
            markValue(vm, entry->value, generation);
            break;
        }
        case OBJ_EXCEPTION: { 
            ObjException* exception = (ObjException*)object;
            markObject(vm, (Obj*)exception->message, generation);
            markObject(vm, (Obj*)exception->stacktrace, generation);
            break;
        }
        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)object;
            markObject(vm, (Obj*)file->name, generation);
            markObject(vm, (Obj*)file->mode, generation);
            break;
        }
        case OBJ_FRAME: {
            ObjFrame* frame = (ObjFrame*)object;
            markObject(vm, (Obj*)frame->closure, generation);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            markObject(vm, (Obj*)function->name, generation);
            markArray(vm, &function->chunk.constants, generation);
            markArray(vm, &function->chunk.identifiers, generation);
            break;
        }
        case OBJ_GENERATOR: {
            ObjGenerator* generator = (ObjGenerator*)object;
            markObject(vm, (Obj*)generator->frame, generation);
            markObject(vm, (Obj*)generator->outer, generation);
            markObject(vm, (Obj*)generator->inner, generation);
            markValue(vm, generator->value, generation);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            markObject(vm, (Obj*)object->klass, generation);
            markArray(vm, &instance->fields, generation);
            break;
        }
        case OBJ_METHOD: {
            ObjMethod* method = (ObjMethod*)object;
            markObject(vm, (Obj*)method->behavior, generation);
            markObject(vm, (Obj*)method->closure, generation);
            break;
        }
        case OBJ_MODULE: {
            ObjModule* module = (ObjModule*)object;
            markObject(vm, (Obj*)module->path, generation);
            if (module->closure != NULL) markObject(vm, (Obj*)module->closure, generation);
            markIDMap(vm, &module->valIndexes, generation);
            markArray(vm, &module->valFields, generation);
            markIDMap(vm, &module->varIndexes, generation);
            markArray(vm, &module->varFields, generation);
            break;
        }
        case OBJ_NAMESPACE: {
            ObjNamespace* _namespace = (ObjNamespace*)object;
            markObject(vm, (Obj*)_namespace->shortName, generation);
            markObject(vm, (Obj*)_namespace->fullName, generation);
            markObject(vm, (Obj*)_namespace->enclosing, generation);
            markTable(vm, &_namespace->values, generation);
            break;
        }
        case OBJ_NATIVE_FUNCTION: {
            ObjNativeFunction* nativeFunction = (ObjNativeFunction*)object;
            markObject(vm, (Obj*)nativeFunction->name, generation);
            break;
        }
        case OBJ_NATIVE_METHOD: {
            ObjNativeMethod* nativeMethod = (ObjNativeMethod*)object;
            markObject(vm, (Obj*)nativeMethod->klass, generation);
            markObject(vm, (Obj*)nativeMethod->name, generation);
            break;
        }
        case OBJ_NODE: {
            ObjNode* node = (ObjNode*)object;
            markValue(vm, node->element, generation);
            markObject(vm, (Obj*)node->prev, generation);
            markObject(vm, (Obj*)node->next, generation);
            break;
        }
        case OBJ_PROMISE: {
            ObjPromise* promise = (ObjPromise*)object;
            markValue(vm, promise->value, generation);
            markObject(vm, (Obj*)promise->captures, generation);
            markObject(vm, (Obj*)promise->exception, generation);
            markValue(vm, promise->executor, generation);
            markArray(vm, &promise->handlers, generation);
            break;
        }
        case OBJ_RECORD: {
            ObjRecord* record = (ObjRecord*)object;
            if (record->markFunction) {
                record->markFunction(record->data, generation);
            }
            break;
        }
        case OBJ_TIMER: { 
            ObjTimer* timer = (ObjTimer*)object;
            if (timer->timer != NULL && timer->timer->data != NULL) {
                TimerData* data = (TimerData*)timer->timer->data;
                markValue(vm, data->receiver, generation);
                markObject(vm, (Obj*)data->closure, generation);
            }
            break;
        }
        case OBJ_UPVALUE:
            markValue(vm, ((ObjUpvalue*)object)->closed, generation);
            break;
        case OBJ_VALUE_INSTANCE: { 
            ObjValueInstance* instance = (ObjValueInstance*)object;
            markObject(vm, (Obj*)object->klass, generation);
            markArray(vm, &instance->fields, generation);
            break;
        }
        default:
            break;
    }
}

static void freeObject(VM* vm, Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d at generation %d\n", (void*)object, object->type, object->generation);
#endif

    switch (object->type) {
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            freeValueArray(vm, &array->elements);
            FREE(ObjArray, object, object->generation);
            break;
        }
        case OBJ_BOUND_METHOD: 
            FREE(ObjBoundMethod, object, object->generation);
            break;       
        case OBJ_CLASS: {
            ObjClass* _class = (ObjClass*)object;
            freeValueArray(vm, &_class->traits);
            freeIDMap(vm, &_class->indexes);
            freeValueArray(vm, &_class->fields);
            freeTable(vm, &_class->methods);
            freeValueArray(vm, &_class->defaultInstanceFields);
            FREE(ObjClass, object, object->generation);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount, closure->obj.generation);
            FREE(ObjClosure, object, object->generation);
            break;
        }
        case OBJ_DICTIONARY: {
            ObjDictionary* dict = (ObjDictionary*)object;
            FREE_ARRAY(ObjEntry, dict->entries, dict->capacity, dict->obj.generation);
            FREE(ObjDictionary, object, object->generation);
            break;
        }
        case OBJ_ENTRY: {
            FREE(ObjEntry, object, object->generation);
            break;
        }
        case OBJ_EXCEPTION: { 
            FREE(ObjException, object, object->generation);
            break;
        }
        case OBJ_FILE: {
            ObjFile* file = (ObjFile*)object;
            if (file->fsStat != NULL) {
                uv_fs_req_cleanup(file->fsStat);
                free(file->fsStat);
            }
            if (file->fsOpen != NULL) {
                uv_fs_req_cleanup(file->fsOpen);
                free(file->fsOpen);
            }
            if (file->fsRead != NULL) {
                uv_fs_req_cleanup(file->fsRead);
                free(file->fsRead);
            }
            if (file->fsWrite != NULL) {
                uv_fs_req_cleanup(file->fsWrite);
                free(file->fsWrite);
            }
            FREE(ObjFile, object, object->generation);
            break;
        }
        case OBJ_FRAME: {
            FREE(ObjFrame, object, object->generation);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            freeChunk(vm, &function->chunk);
            FREE(ObjFunction, object, object->generation);
            break;
        }
        case OBJ_GENERATOR: {
            FREE(ObjGenerator, object, object->generation);
            break; 
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            freeValueArray(vm, &instance->fields);
            FREE(ObjInstance, object, object->generation);
            break;
        }
        case OBJ_METHOD: {
            FREE(ObjMethod, object, object->generation);
            break;
        }
        case OBJ_MODULE: {
            ObjModule* module = (ObjModule*)object;
            freeIDMap(vm, &module->valIndexes);
            freeValueArray(vm, &module->valFields);
            freeIDMap(vm, &module->varIndexes);
            freeValueArray(vm, &module->varFields);
            FREE(ObjModule, object, object->generation);
            break;
        }             
        case OBJ_NAMESPACE: { 
            ObjNamespace* _namespace = (ObjNamespace*)object;
            freeTable(vm, &_namespace->values);
            FREE(ObjNamespace, object, object->generation);
            break;
        }
        case OBJ_NATIVE_FUNCTION:
            FREE(ObjNativeFunction, object, object->generation);
            break;
        case OBJ_NATIVE_METHOD:
            FREE(ObjNativeMethod, object, object->generation);
            break;
        case OBJ_NODE: {
            FREE(ObjNode, object, object->generation);
            break;
        }
        case OBJ_PROMISE: {
            ObjPromise* promise = (ObjPromise*)object;
            freeValueArray(vm, &promise->handlers);
            FREE(ObjPromise, object, object->generation);
            break;
        }
        case OBJ_RANGE: {
            FREE(ObjRange, object, object->generation);
            break;
        }
        case OBJ_RECORD: {
            ObjRecord* record = (ObjRecord*)object;
            if (record->freeFunction) record->freeFunction(record->data);
            else if(record->shouldFree) free(record->data);
            FREE(ObjRecord, object, object->generation);
            break;
        }
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            reallocate(vm, object, sizeof(ObjString) + string->length + 1, 0, object->generation);
            break;
        } 
        case OBJ_TIMER: { 
            ObjTimer* timer = (ObjTimer*)object;
            FREE(ObjTimer, object, object->generation);
            break;
        }
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object, object->generation);
            break;
        case OBJ_VALUE_INSTANCE: { 
            ObjValueInstance* instance = (ObjValueInstance*)object;
            freeValueArray(vm, &instance->fields);
            FREE(ObjValueInstance, object, object->generation);
            break;
        }
    }
}

static void promoteObject(VM* vm, Obj* object, GCGenerationType generation) {
    GCGeneration* currentHeap = GET_GC_GENERATION(generation);
    GCGeneration* nextHeap = GET_GC_GENERATION(generation + 1);
    object->generation++;
    object->next = nextHeap->objects;
    nextHeap->objects = object;

    switch (object->type) {
        case OBJ_ARRAY: {
            ObjArray* array = (ObjArray*)object;
            array->elements.generation++;
            break;
        }
        case OBJ_CLASS: {
            ObjClass* _class = (ObjClass*)object;
            _class->traits.generation++;
            _class->indexes.generation++;
            _class->fields.generation++;
            _class->methods.generation++;
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            function->chunk.generation++;
            function->chunk.constants.generation++;
            function->chunk.identifiers.generation++;
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            instance->fields.generation++;
            break;
        }
        case OBJ_MODULE: {
            ObjModule* module = (ObjModule*)object;
            module->valIndexes.generation++;
            module->valFields.generation++;
            module->varIndexes.generation++;
            module->varFields.generation++;
            break;
        }
        case OBJ_NAMESPACE: {
            ObjNamespace* _namespace = (ObjNamespace*)object;
            _namespace->values.generation++;
            break;
        }
        case OBJ_VALUE_INSTANCE: {
            ObjValueInstance* valueInstance = (ObjValueInstance*)object;
            valueInstance->fields.generation++;
            break;
        }
        default:
            break;
    }

    size_t size = sizeOfObject(object);
    currentHeap->bytesAllocated -= size;
    nextHeap->bytesAllocated += size;
}

static void markRoots(VM* vm, GCGenerationType generation) {
    for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
        markValue(vm, *slot, generation);
    }

    for (int i = 0; i < vm->frameCount; i++) {
        markObject(vm, (Obj*)vm->frames[i].closure, generation);
    }

    for (ObjUpvalue* upvalue = vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject(vm, (Obj*)upvalue, generation);
    }

    for (ObjGenerator* generator = vm->runningGenerator; generator != NULL; generator = generator->outer) {
        markObject(vm, (Obj*)generator, generation);
    }

    markGlobals(vm, generation);
    markRememberedSet(vm, generation);
}

static void traceReferences(VM* vm, GCGenerationType generation) {
    while (vm->gc->grayCount > 0) {
        Obj* object = vm->gc->grayStack[--vm->gc->grayCount];
        blackenObject(vm, object, generation);
    }
}

static void sweep(VM* vm, GCGenerationType generation) {
    GCGeneration* currentHeap = GET_GC_GENERATION(generation);
    GCGeneration* nextHeap = (generation >= GC_GENERATION_TYPE_PERMANENT) ? NULL : GET_GC_GENERATION(generation + 1);
    if (nextHeap == NULL) return;
    Obj* object = currentHeap->objects;

    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            Obj* reached = object;
            object = object->next;
            promoteObject(vm, reached, generation);
        } 
        else {
            Obj* unreached = object;
            object = object->next;
            freeObject(vm, unreached);
        }
        currentHeap->objects = object;
    }
}

static void processRememberedSet(VM* vm, GCGenerationType generation) {
    if (generation >= GC_GENERATION_TYPE_OLD) return;
    GCRememberedSet* currentRemSet = &GET_GC_GENERATION(generation)->remSet;
    GCRememberedSet* nextRemSet = &GET_GC_GENERATION(generation + 1)->remSet;

    for (int i = 0; i < currentRemSet->capacity; i++) {
        GCRememberedEntry* entry = &currentRemSet->entries[i];
        if (entry->object != NULL) {
            entry->object->isMarked = false;
            if (entry->object->generation > generation + 1) {
                rememberedSetPutObject(vm, nextRemSet, entry->object);
            }
        }
    }
    freeGCRememeberedSet(vm, currentRemSet);
}

void collectGarbage(VM* vm, GCGenerationType generation) {
    if (generation > 0) collectGarbage(vm, generation - 1);
    GCGeneration* currentHeap = GET_GC_GENERATION(generation);

#ifdef DEBUG_LOG_GC
    printf("-- gc begin for generation %d\n", generation);
    GCGeneration* nextHeap = (generation >= GC_GENERATION_TYPE_PERMANENT) ? NULL : GET_GC_GENERATION(generation + 1);
    size_t currentBefore = currentHeap->bytesAllocated;
    size_t nextBefore = (nextHeap == NULL) ? 0 : nextHeap->bytesAllocated;
#endif

    markRoots(vm, generation);
    traceReferences(vm, generation);
    tableRemoveWhite(&vm->strings);
    sweep(vm, generation);
    processRememberedSet(vm, generation);

#ifdef DEBUG_LOG_GC
    printf("-- gc end for generation %d\n", generation);
    size_t nextBytesAllocated = (nextHeap == NULL) ? 0 : nextHeap->bytesAllocated;
    size_t nextPromoted = nextBytesAllocated - nextBefore;
    size_t currentFreed = currentBefore - nextPromoted - currentHeap->bytesAllocated;
    printf("   collected %zu bytes, promoted %zu bytes\n", currentFreed, nextPromoted);
    printf("   current heap uses %zu bytes, heap size %zu bytes\n", currentHeap->bytesAllocated, currentHeap->heapSize);

    if (nextHeap != NULL) {
        printf("   next heap uses %zu bytes, heap size %zu bytes\n", nextHeap->bytesAllocated, nextHeap->heapSize);
    }
#endif
}

void freeObjects(VM* vm) {
    for (int i = 0; i < GC_GENERATION_TYPE_COUNT; i++) {
        Obj* object = GET_GC_GENERATION(i)->objects;
        while (object != NULL) {
            Obj* next = object->next;
            freeObject(vm, object);
            object = next;
        }
    }
    free(vm->gc->grayStack);
}