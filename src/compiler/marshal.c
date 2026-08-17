#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "chunk.h"
#include "marshal.h"
#include "../common/os.h"
#include "../vm/debug.h"

static void initMarshaller(Marshaller* marshaller, VM* vm) {
	marshaller->vm = vm;	
	marshaller->module = NULL;
	marshaller->bytes = NULL;
	marshaller->offset = 0;
}

Marshaller* newMarshaller(VM* vm) {
	Marshaller* marshaller = (Marshaller*)malloc(sizeof(Marshaller));
	ABORT_IFNULL(marshaller, "Failed to allocate memory for Marshaller.\n");
	initMarshaller(marshaller, vm);
	return marshaller;
}

void freeMarshaller(Marshaller* marshaller) {
	if (marshaller->bytes != NULL) {
		ByteArrayFree(marshaller->bytes);
		free(marshaller->bytes);
	}
	free(marshaller);
}

static size_t marshalFileSize(FILE* file) {
	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);
	return fileSize;
}

static int marshalNextCapacity(int size) {
	if (size < 8) return 8;
	size--;
	size |= size >> 1;
	size |= size >> 2;
	size |= size >> 4;
	size |= size >> 8;
	size |= size >> 16;
	return ++size;
}

static void marshalInitBytes(Marshaller* marshaller, int fileSize) {
	ByteArrayInit(marshaller->bytes);
	marshaller->bytes->count = fileSize;
	marshaller->bytes->capacity = marshalNextCapacity(fileSize);
	marshaller->bytes->elements = (uint8_t*)malloc(marshaller->bytes->capacity);
	ABORT_IFNULL(marshaller->bytes->elements, "Failed to allocate memory for byte streams to perform marshal deserialization.\n");
}

static void marshalCleanup(Marshaller* marshaller) {
	ByteArrayFree(marshaller->bytes);
	initMarshaller(marshaller, marshaller->vm);
}

static void marshalSerializeByte(ByteArray* bytes, uint8_t value) {
	ByteArrayAdd(bytes, value);
}

static void marshalSerializeShort(ByteArray* bytes, uint16_t value) {
	ByteArrayAdd(bytes, (value >> 8) & 0xFF);
	ByteArrayAdd(bytes, value & 0xFF);
}

static void marshalSerializeInt(ByteArray* bytes, uint32_t value) {
	ByteArrayAdd(bytes, (value >> 24) & 0xFF);
	ByteArrayAdd(bytes, (value >> 16) & 0xFF);
	ByteArrayAdd(bytes, (value >> 8) & 0xFF);
	ByteArrayAdd(bytes, value & 0xFF);
}

static void marshalSerializeDouble(ByteArray* bytes, double value) {
	Value numBits = NUMBER_VAL(value);
	ByteArrayAdd(bytes, (numBits >> 56) & 0xFF);
	ByteArrayAdd(bytes, (numBits >> 48) & 0xFF);
	ByteArrayAdd(bytes, (numBits >> 40) & 0xFF);
	ByteArrayAdd(bytes, (numBits >> 32) & 0xFF);
	ByteArrayAdd(bytes, (numBits >> 24) & 0xFF);
	ByteArrayAdd(bytes, (numBits >> 16) & 0xFF);
	ByteArrayAdd(bytes, (numBits >> 8) & 0xFF);
	ByteArrayAdd(bytes, numBits & 0xFF);
}

static void marshalSerializeString(ByteArray* bytes, ObjString* string) {
	marshalSerializeInt(bytes, (uint32_t)string->length);
	for (size_t i = 0; i < string->length; i++) {	
		marshalSerializeByte(bytes, (uint8_t)string->chars[i]);
	}
}

static void marshalSerializeFunction(Marshaller* marshaller, ByteArray* bytes, ObjFunction* function) {
	bool isNamed = function->name != NULL;
	marshalSerializeByte(bytes, isNamed ? 1 : 0);
	if (isNamed) marshalSerializeString(bytes, function->name);

	marshalSerializeShort(bytes, (uint16_t)function->arity);
	marshalSerializeByte(bytes, (uint8_t)function->typeParamCount);
	marshalSerializeByte(bytes, (uint8_t)function->upvalueCount);
	marshalSerializeByte(bytes, (uint8_t)function->isAsync);
	marshalSerializeByte(bytes, (uint8_t)function->isGenerator);

	marshalSerializeByte(bytes, (uint8_t)function->chunk.constants.count);
	for (int i = 0; i < function->chunk.constants.count; i++) {
		marshalSerializeValue(marshaller, bytes, function->chunk.constants.values[i]);
	}

	marshalSerializeByte(bytes, (uint8_t)function->chunk.identifiers.count);
	for (int i = 0; i < function->chunk.identifiers.count; i++) {
		marshalSerializeString(bytes, AS_STRING(function->chunk.identifiers.values[i]));
	}

	marshalSerializeInt(bytes, (uint32_t)function->chunk.count);
	for (int i = 0; i < function->chunk.count; i++) {
		marshalSerializeByte(bytes, function->chunk.code[i]);
	}

	if (marshaller->vm->config.marshalLineInfo) {
		for (int i = 0; i < function->chunk.count; i++) {
			marshalSerializeShort(bytes, (uint16_t)function->chunk.lines[i]);
		}
	}
}

void marshalSerializeValue(Marshaller* marshaller, ByteArray* bytes, Value value) {
	if (IS_NIL(value)) {
		marshalSerializeByte(bytes, MARSHAL_TYPE_NIL);
	}
	else if (IS_BOOL(value)) {
		marshalSerializeByte(bytes, MARSHAL_TYPE_BOOL);
		marshalSerializeByte(bytes, AS_BOOL(value) ? 1 : 0);
	}
	else if (IS_INT(value)) {
		marshalSerializeByte(bytes, MARSHAL_TYPE_INT);
		marshalSerializeInt(bytes, (uint32_t)AS_INT(value));
	}
	else if (IS_NUMBER(value)) {
		marshalSerializeByte(bytes, MARSHAL_TYPE_NUMBER);
		marshalSerializeDouble(bytes, AS_NUMBER(value));
	}
	else if (IS_STRING(value)) {
		marshalSerializeByte(bytes, MARSHAL_TYPE_STRING);
		marshalSerializeString(bytes, AS_STRING(value));
	}
	else if (IS_FUNCTION(value)) {
		marshalSerializeByte(bytes, MARSHAL_TYPE_FUNCTION);
		marshalSerializeFunction(marshaller, bytes, AS_FUNCTION(value));
	}
	else {
		fprintf(stderr, "Unsupported value type for serialization.\n");
		exit(1);
	}
}

static void marshalSerializeTraits(ByteArray* bytes, TypeInfoArray* traits) {
	marshalSerializeInt(bytes, (uint32_t)traits->count);
	for (int i = 0; i < traits->count; i++) {
		TypeInfo* traitType = traits->elements[i];
		marshalSerializeString(bytes, traitType->fullName);
	}
}

static void marshalSerializeFormalTypeParams(ByteArray* bytes, TypeInfoArray* formalTypeParams) {
	marshalSerializeInt(bytes, (uint32_t)formalTypeParams->count);
	for (int i = 0; i < formalTypeParams->count; i++) {
		TypeInfo* typeParam = formalTypeParams->elements[i];
		marshalSerializeString(bytes, typeParam->fullName);
	}
}

static void marshalSerializeFields(Marshaller* marshaller, ByteArray* bytes, TypeTable* fields) {
	marshalSerializeInt(bytes, (uint32_t)fields->count);
	for (int i = 0; i < fields->capacity; i++) {
		TypeEntry* entry = &fields->entries[i];
		if (entry != NULL && entry->key != NULL) {
			FieldTypeInfo* fieldType = AS_FIELD_TYPE(entry->value);
			marshalSerializeString(bytes, entry->key);
			marshalSerializeByte(bytes, fieldType->isMutable ? 1 : 0);
			marshalSerializeByte(bytes, fieldType->hasInitializer ? 1 : 0);
			marshalSerializeByte(bytes, (uint8_t)fieldType->index);
			marshalSerializeString(bytes, fieldType->declaredType != NULL ? fieldType->declaredType->fullName : newStringPerma(marshaller->vm, "dynamic"));
		}
	}
}

static void marshalSerializeMethod(Marshaller* marshaller, ByteArray* bytes, MethodTypeInfo* method) {
	marshalSerializeString(bytes, method->baseType.shortName);
	marshalSerializeByte(bytes, method->isAsync ? 1 : 0);
	marshalSerializeByte(bytes, method->isClass ? 1 : 0);
	marshalSerializeByte(bytes, method->isInitializer ? 1 : 0);
	marshalSerializeString(bytes, method->declaredType->baseType.fullName);
}

static void marshalSerializeMethods(Marshaller* marshaller, ByteArray* bytes, TypeTable* methods) {
	marshalSerializeInt(bytes, (uint32_t)methods->count);
	for (int i = 0; i < methods->capacity; i++) {
		TypeEntry* entry = &methods->entries[i];
		if (entry != NULL && entry->key != NULL) {
			MethodTypeInfo* methodType = AS_METHOD_TYPE(entry->value);
			marshalSerializeMethod(marshaller, bytes, methodType);
		}
	}
}

static void marshalSerializeTypeInfo(Marshaller* marshaller, ByteArray* bytes, TypeInfo* type) {
	marshalSerializeByte(bytes, (uint8_t)type->category);
	marshalSerializeInt(bytes, type->hash);
	marshalSerializeString(bytes, type->shortName);
	marshalSerializeString(bytes, type->fullName);

	if (IS_BEHAVIOR_TYPE(type)) {
		BehaviorTypeInfo* behaviorType = AS_BEHAVIOR_TYPE(type);
		marshalSerializeByte(bytes, (behaviorType->isReified ? 1 : 0));
		marshalSerializeString(bytes, behaviorType->superclassType->fullName);
		marshalSerializeTraits(bytes, behaviorType->traitTypes);
		marshalSerializeFormalTypeParams(bytes, behaviorType->formalTypeParams);
		marshalSerializeFields(marshaller, bytes, behaviorType->fields);
		marshalSerializeMethods(marshaller, bytes, behaviorType->methods);
	}
	else if (IS_CALLABLE_TYPE(type)) {
		CallableTypeInfo* callableType = AS_CALLABLE_TYPE(type);
		marshalSerializeByte(bytes, (uint8_t)callableType->attribute.isGeneric);
		marshalSerializeByte(bytes, (uint8_t)callableType->attribute.isInitializer);
		marshalSerializeByte(bytes, (uint8_t)callableType->attribute.isLambda);
		marshalSerializeByte(bytes, (uint8_t)callableType->attribute.isReified);
		marshalSerializeByte(bytes, (uint8_t)callableType->attribute.isVariadic);
		marshalSerializeByte(bytes, (uint8_t)callableType->attribute.isVoid);
		marshalSerializeString(bytes, callableType->returnType != NULL ? callableType->returnType->fullName : newStringPerma(marshaller->vm, "dynamic"));
		
		marshalSerializeByte(bytes, (uint8_t)callableType->paramTypes->count);	
		for (int i = 0; i < callableType->paramTypes->count; i++) {
			TypeInfo* paramType = callableType->paramTypes->elements[i];
			marshalSerializeString(bytes, paramType != NULL ? paramType->fullName : newStringPerma(marshaller->vm, "dynamic"));
		}
		marshalSerializeFormalTypeParams(bytes, callableType->formalTypeParams);
	}
	else if (IS_GENERIC_TYPE(type)) {
		GenericTypeInfo* genericType = AS_GENERIC_TYPE(type);
		marshalSerializeByte(bytes, (uint8_t)genericType->isFullyInstantiated);
		marshalSerializeString(bytes, genericType->rawType->fullName);
		marshalSerializeByte(bytes, (uint8_t)genericType->actualTypeParams->count);
		
		for (int i = 0; i < genericType->actualTypeParams->count; i++) {
			TypeInfo* actualType = genericType->actualTypeParams->elements[i];
			marshalSerializeString(bytes, actualType != NULL ? actualType->fullName : newStringPerma(marshaller->vm, "dynamic"));
		}
	}
	else if (IS_ALIAS_TYPE(type)) {
		AliasTypeInfo* aliasType = AS_ALIAS_TYPE(type);
		marshalSerializeString(bytes, aliasType->targetType->fullName);
		marshalSerializeFormalTypeParams(bytes, aliasType->formalTypeParams);
	}
}

static void marshalSerializeTypeTable(Marshaller* marshaller, ByteArray* bytes, TypeTable* typeTab) {
	marshalSerializeInt(bytes, (uint32_t)typeTab->count);
	for (int i = 0; i < typeTab->capacity; i++) {
		TypeEntry* entry = &typeTab->entries[i];
		if (entry != NULL && entry->key != NULL) {
			marshalSerializeString(bytes, entry->key);
			marshalSerializeTypeInfo(marshaller, bytes, entry->value);
		}
	}
}

static void marshalSerializeModule(Marshaller* marshaller, ByteArray* bytes, ObjModule* module) {
	ObjFunction* function = module->closure->function;
	marshalSerializeString(bytes, newString(marshaller->vm, marshaller->vm->config.version));
	marshalSerializeString(bytes, module->path);
	marshalSerializeByte(bytes, (uint8_t)marshaller->vm->config.marshalLineInfo);

	marshalSerializeInt(bytes, (uint32_t)module->valIndexes.count);
	for (int i = 0; i < module->valIndexes.capacity; i++) {
		IDEntry* entry = &module->valIndexes.entries[i];
		if (entry != NULL && entry->key != NULL) {
			marshalSerializeString(bytes, entry->key);
			marshalSerializeInt(bytes, (uint32_t)entry->value);
		}
	}

	marshalSerializeInt(bytes, (uint32_t)module->varIndexes.count);
	for (int i = 0; i < module->varIndexes.capacity; i++) {
		IDEntry* entry = &module->varIndexes.entries[i];
		if (entry != NULL && entry->key != NULL) {
			marshalSerializeString(bytes, entry->key);
			marshalSerializeInt(bytes, (uint32_t)entry->value);
		}
	}

	marshalSerializeInt(bytes, (uint32_t)module->dependencies.count);
	for (int i = 0; i < module->dependencies.count; i++) {
		Value entry = module->dependencies.values[i];
		marshalSerializeString(bytes, AS_STRING(entry));		
	}

	marshalSerializeFunction(marshaller, bytes, function);
}

void marshalDump(Marshaller* marshaller, ObjModule* module) {
	if (!marshaller->vm->config.marshalEnabled || module->path->length == 0) return;
	char fileName[UINT8_COUNT];
	sprintf_s(fileName, UINT8_COUNT, "%s%s%s", marshaller->vm->config.marshalOutputPath, module->path->chars, "o");
	FILE* file;
	fopen_p(&file, fileName, "wb");
	ABORT_IFNULL(file, "Failed to open file \"%s\" for marshal serialization.\n", fileName);

	marshaller->module = module;
	marshaller->bytes = (ByteArray*)malloc(sizeof(ByteArray));
	ByteArrayInit(marshaller->bytes);
	ABORT_IFNULL(marshaller->bytes, "Failed to allocate memory for byte streams to perform marshal serialization.\n");
	marshalSerializeModule(marshaller, marshaller->bytes, marshaller->module);

	size_t bytesWritten = fwrite(marshaller->bytes->elements, sizeof(uint8_t), marshaller->bytes->count, file);
	ABORT_IFTRUE(bytesWritten < marshaller->bytes->count, "Failed to write to file \"%s\" for marshal serialization.\n", fileName);
	fclose(file);
	marshalCleanup(marshaller);
}

static uint8_t marshalDeserializeByte(Marshaller* marshaller) {
	return marshaller->bytes->elements[marshaller->offset++];
}

static uint16_t marshalDeserializeShort(Marshaller* marshaller) {
	uint16_t value = (marshalDeserializeByte(marshaller) << 8) | marshalDeserializeByte(marshaller);
	return value;
}

static uint32_t marshalDeserializeInt(Marshaller* marshaller) {
	uint32_t value = (marshalDeserializeByte(marshaller) << 24) | (marshalDeserializeByte(marshaller) << 16) |
		(marshalDeserializeByte(marshaller) << 8) | marshalDeserializeByte(marshaller);
	return value;
}

static double marshalDeserializeDouble(Marshaller* marshaller) {
	uint64_t value = ((uint64_t)marshalDeserializeByte(marshaller) << 56) | ((uint64_t)marshalDeserializeByte(marshaller) << 48) |
		((uint64_t)marshalDeserializeByte(marshaller) << 40) | ((uint64_t)marshalDeserializeByte(marshaller) << 32) |
		((uint64_t)marshalDeserializeByte(marshaller) << 24) | ((uint64_t)marshalDeserializeByte(marshaller) << 16) |
		((uint64_t)marshalDeserializeByte(marshaller) << 8) | (uint64_t)marshalDeserializeByte(marshaller);
	return AS_NUMBER(value);
}

static ObjString* marshalDeserializeString(Marshaller* marshaller) {
	uint32_t length = marshalDeserializeInt(marshaller);
	char* chars = (char*)malloc((size_t)length + 1);
	ABORT_IFNULL(chars, "Failed to allocate memory for deserializing string.\n");

	for (uint32_t i = 0; i < length; i++) {
		chars[i] = (char)marshalDeserializeByte(marshaller);
	}

	chars[length] = '\0';
	ObjString* string = takeStringPerma(marshaller->vm, chars, length);
	return string;
}

static ObjFunction* marshalDeserializeFunction(Marshaller* marshaller) {
	bool isNamed = marshalDeserializeByte(marshaller) == 1;
	ObjString* name = isNamed ? marshalDeserializeString(marshaller) : NULL;
	ObjFunction* function = newFunction(marshaller->vm, name, false);
	push(marshaller->vm, OBJ_VAL(function));

	function->arity = (int16_t)marshalDeserializeShort(marshaller);
	function->typeParamCount = (int8_t)marshalDeserializeByte(marshaller);
	function->upvalueCount = (int8_t)marshalDeserializeByte(marshaller);
	function->isAsync = (marshalDeserializeByte(marshaller) == 1);
	function->isGenerator = (marshalDeserializeByte(marshaller) == 1);

	uint8_t constantCount = marshalDeserializeByte(marshaller);
	for (uint8_t i = 0; i < constantCount; i++) {
		Value constant = marshalDeserializeValue(marshaller);
		addConstant(marshaller->vm, &function->chunk, constant);
	}

	uint8_t identifierCount = marshalDeserializeByte(marshaller);
	for (uint8_t i = 0; i < identifierCount; i++) {
		ObjString* identifier = marshalDeserializeString(marshaller);
		addIdentifier(marshaller->vm, &function->chunk, OBJ_VAL(identifier));
	}

	uint32_t codeCount = marshalDeserializeInt(marshaller);
	for (uint32_t i = 0; i < codeCount; i++) {
		uint8_t code = marshalDeserializeByte(marshaller);
		writeChunk(marshaller->vm, &function->chunk, code, 0);
	}

	if (marshaller->vm->config.marshalLineInfo) {
		for (uint32_t i = 0; i < codeCount; i++) {
			uint16_t line = marshalDeserializeShort(marshaller);
			function->chunk.lines[i] = (int)line;
		}
	}

	if (marshaller->vm->config.debugCode) {
	    disassembleChunk(&function->chunk, function->name != NULL ? function->name->chars : "<script>");
	}
	pop(marshaller->vm);
	return function;
}

Value marshalDeserializeValue(Marshaller* marshaller) {
	uint8_t type = marshalDeserializeByte(marshaller);
	switch (type) {
	    case MARSHAL_TYPE_NIL:
		    return NIL_VAL;
	    case MARSHAL_TYPE_BOOL:
		    return BOOL_VAL(marshalDeserializeByte(marshaller) == 1);
	    case MARSHAL_TYPE_INT:
	     	return INT_VAL((int32_t)marshalDeserializeInt(marshaller));
	    case MARSHAL_TYPE_NUMBER:
		    return NUMBER_VAL(marshalDeserializeDouble(marshaller));
	    case MARSHAL_TYPE_STRING:
		    return OBJ_VAL(marshalDeserializeString(marshaller));
	    case MARSHAL_TYPE_FUNCTION:
		    return OBJ_VAL(marshalDeserializeFunction(marshaller));
	    default:
		    fprintf(stderr, "Unsupported value type for deserialization.\n");
	     	exit(1);
	}
}

static TypeInfo* marshalDeserializeBaseType(Marshaller* marshaller) {
	ObjString* typeName = marshalDeserializeString(marshaller);
	if (strcmp(typeName->chars, "dynamic") == 0) return NULL;
	TypeInfo* typeInfo = typeTableGet(marshaller->vm->typetab, typeName);

	if (typeInfo == NULL) {
		fprintf(stderr, "Failed to find type \"%s\" during marshal deserialization.\n", typeName->chars);
		exit(1);
	}
	return typeInfo;
}

static void marshalDeserializeTraits(Marshaller* marshaller, BehaviorTypeInfo* behaviorType) {
	int traitCount = marshalDeserializeInt(marshaller);
	for (int i = 0; i < traitCount; i++) {
		TypeInfo* traitType = marshalDeserializeBaseType(marshaller);
		TypeInfoArrayAdd(behaviorType->traitTypes, traitType);
	}
}

static void marshalDeserializeFormalTypeParams(Marshaller* marshaller, TypeInfoArray* formalTypeParams) {
	int formalTypeParamCount = marshalDeserializeInt(marshaller);
	for (int i = 0; i < formalTypeParamCount; i++) {
		TypeInfo* placeholderType = marshalDeserializeBaseType(marshaller);
		TypeInfoArrayAdd(formalTypeParams, placeholderType);
	}
}

static void marshalDeserializeFields(Marshaller* marshaller, BehaviorTypeInfo* behaviorType) {
	int fieldCount = marshalDeserializeInt(marshaller);
	for (int i = 0; i < fieldCount; i++) {
		ObjString* fieldName = marshalDeserializeString(marshaller);
		bool isMutable = marshalDeserializeByte(marshaller) == 1;
		bool hasInitializer = marshalDeserializeByte(marshaller) == 1;
		uint8_t index = marshalDeserializeByte(marshaller);
		TypeInfo* declaredType = marshalDeserializeBaseType(marshaller);
		typeTableInsertField(behaviorType->fields, fieldName, declaredType, isMutable, hasInitializer);
	}
}

static void marshalDeserializeMethod(Marshaller* marshaller, TypeTable* methods) {
	ObjString* methodName = marshalDeserializeString(marshaller);
	bool isAsync = marshalDeserializeByte(marshaller) == 1;
	bool isClass = marshalDeserializeByte(marshaller) == 1;
	bool isInitializer = marshalDeserializeByte(marshaller) == 1;
	TypeInfo* declaredType = marshalDeserializeBaseType(marshaller);
	MethodTypeInfo* methodType = typeTableInsertMethod(methods, methodName, (CallableTypeInfo*)declaredType, isAsync, isClass, isInitializer);
}

static void marshalDeserializeMethods(Marshaller* marshaller, BehaviorTypeInfo* behaviorType) {
	int methodCount = marshalDeserializeInt(marshaller);
	for (int i = 0; i < methodCount; i++) {
		marshalDeserializeMethod(marshaller, behaviorType->methods);
	}
}

static TypeInfo* marshalDeserializeTypeInfo(Marshaller* marshaller) {
	uint32_t id = marshaller->vm->typetab->count + 1;
	TypeCategory category = (TypeCategory)marshalDeserializeByte(marshaller);
	uint32_t hash = marshalDeserializeInt(marshaller);
	ObjString* shortName = marshalDeserializeString(marshaller);
	ObjString* fullName = marshalDeserializeString(marshaller);

	if (category == TYPE_CATEGORY_CLASS || category == TYPE_CATEGORY_METACLASS || category == TYPE_CATEGORY_TRAIT) {
		bool isReified = marshalDeserializeByte(marshaller) == 1;
		TypeInfo* superclassType = marshalDeserializeBaseType(marshaller);
		BehaviorTypeInfo* behaviorType = newBehaviorTypeInfo(id, category, shortName, fullName, superclassType);
		behaviorType->isReified = isReified;

		marshalDeserializeTraits(marshaller, behaviorType);
		marshalDeserializeFormalTypeParams(marshaller, behaviorType->formalTypeParams);
		marshalDeserializeFields(marshaller, behaviorType);
		marshalDeserializeMethods(marshaller, behaviorType);
		return (TypeInfo*)behaviorType;
	}
	else if (category == TYPE_CATEGORY_FUNCTION) {
		bool isGeneric = marshalDeserializeByte(marshaller) == 1;
		bool isInitializer = marshalDeserializeByte(marshaller) == 1;
		bool isLambda = marshalDeserializeByte(marshaller) == 1;
		bool isReified = marshalDeserializeByte(marshaller) == 1;
		bool isVariadic = marshalDeserializeByte(marshaller) == 1;
		bool isVoid = marshalDeserializeByte(marshaller) == 1;

		TypeInfo* returnType = marshalDeserializeBaseType(marshaller);
		CallableTypeInfo* callableType = newCallableTypeInfo(id, category, shortName, returnType);
		callableType->baseType.fullName = fullName;
		callableType->attribute.isInitializer = isInitializer;
		callableType->attribute.isLambda = isLambda;
		callableType->attribute.isReified = isReified;
		callableType->attribute.isVariadic = isVariadic;
		callableType->attribute.isVoid = isVoid;

		uint8_t paramCount = marshalDeserializeByte(marshaller);
		for (int i = 0; i < paramCount; i++) {
			TypeInfo* paramType = marshalDeserializeBaseType(marshaller);
			TypeInfoArrayAdd(callableType->paramTypes, paramType);
		}

		marshalDeserializeFormalTypeParams(marshaller, callableType->formalTypeParams);
		return (TypeInfo*)callableType;
	}
	else if (category == TYPE_CATEGORY_GENERIC) {
		bool isFullyInstantiated = marshalDeserializeByte(marshaller) == 1;
		TypeInfo* rawType = marshalDeserializeBaseType(marshaller);
		GenericTypeInfo* genericType = newGenericTypeInfo(id, shortName, fullName, rawType);
		genericType->isFullyInstantiated = isFullyInstantiated;

		uint8_t actualTypeParamCount = marshalDeserializeByte(marshaller);
		for (int i = 0; i < actualTypeParamCount; i++) {
			TypeInfo* actualType = marshalDeserializeBaseType(marshaller);
			TypeInfoArrayAdd(genericType->actualTypeParams, actualType);
		}
		return (TypeInfo*)genericType;
	}
	else if (category == TYPE_CATEGORY_ALIAS) {
		TypeInfo* targetType = marshalDeserializeBaseType(marshaller);
		AliasTypeInfo* aliasType = newAliasTypeInfo(id, shortName, fullName, targetType);
		marshalDeserializeFormalTypeParams(marshaller, aliasType->formalTypeParams);
		return (TypeInfo*)aliasType;
	}
	else if (category == TYPE_CATEGORY_PLACEHOLDER) {
		return newPlaceholderTypeInfo(id, shortName);
	}
	else {
		fprintf(stderr, "Unsupported type category for deserialization.\n");
		exit(1);
	}
}

static void marshalDeserializeTypeTable(Marshaller* marshaller) {
	int typeCount = marshalDeserializeInt(marshaller);
	for (int i = 0; i < typeCount; i++) {
		ObjString* typeName = marshalDeserializeString(marshaller);
		TypeInfo* typeInfo = marshalDeserializeTypeInfo(marshaller);
		typeTableSet(marshaller->vm->typetab, typeName, typeInfo);
		typeTableSet(marshaller->module->typeTab, typeName, typeInfo);
	}
}

static void marshalDeserializeModule(Marshaller* marshaller) {
	ObjString* version = marshalDeserializeString(marshaller);
	if (strcmp(version->chars, marshaller->vm->config.version) != 0) {
		fprintf(stderr, "Version mismatch during marshal deserialization. Expected: v%s, but got: v%s.\n", marshaller->vm->config.version, version->chars);
		exit(1);
	}

	ObjString* path = marshalDeserializeString(marshaller);
	if (path != marshaller->module->path) {
		fprintf(stderr, "Module path mismatch during marshal deserialization. Expected: %s, but got: %s.\n", marshaller->module->path->chars, path->chars);
		exit(1);
	}

	int hasLineInfo = marshalDeserializeByte(marshaller);
	if (hasLineInfo != marshaller->vm->config.marshalLineInfo) {
		fprintf(stderr, "Marshal line info configuration mismatch during deserialization. Expected: %s, but got: %s.\n", 
			marshaller->vm->config.marshalLineInfo ? "1(enabled)" : "0(disabled)", hasLineInfo ? "1(enabled)" : "0(disabled)");
		exit(1);
	}

	int numValIndexes = marshalDeserializeInt(marshaller);
	for (int i = 0; i < numValIndexes; i++) {
		ObjString* name = marshalDeserializeString(marshaller);
		uint32_t index = marshalDeserializeInt(marshaller);
		idMapSet(marshaller->vm, &marshaller->module->valIndexes, name, index);
		valueArrayWrite(marshaller->vm, &marshaller->module->valFields, NIL_VAL);
	}

	int numVarIndexes = marshalDeserializeInt(marshaller);
	for (int i = 0; i < numVarIndexes; i++) {
		ObjString* name = marshalDeserializeString(marshaller);
		uint32_t index = marshalDeserializeInt(marshaller);
		idMapSet(marshaller->vm, &marshaller->module->varIndexes, name, index);
		valueArrayWrite(marshaller->vm, &marshaller->module->varFields, NIL_VAL);
	}

	int numDependencies = marshalDeserializeInt(marshaller);
	for (int i = 0; i < numDependencies; i++) {
		ObjString* dependency = marshalDeserializeString(marshaller);
		valueArrayWrite(marshaller->vm, &marshaller->module->dependencies, OBJ_VAL(dependency));
	}

	ObjFunction* function = marshalDeserializeFunction(marshaller);
	ABORT_IFNULL(function, "Failed to deserialize program for file \"%s\".\n", marshaller->module->path->chars);
	push(marshaller->vm, OBJ_VAL(function));
	marshaller->module->closure = newClosure(marshaller->vm, function);
	pop(marshaller->vm);
}

static bool marshalSourceFileModified(const char* sourceFilePath, const char* compiledFilePath) {
	struct stat sourceFileStat, compiledFileStat;
	if (stat(sourceFilePath, &sourceFileStat) == -1 || stat(compiledFilePath, &compiledFileStat) == -1) {	
		return false;
	}
	return difftime(sourceFileStat.st_mtime, compiledFileStat.st_mtime) > 0;
}

bool marshalLoad(Marshaller* marshaller, ObjModule* module) {
	if (!marshaller->vm->config.marshalEnabled || module->path->length == 0) return false;
	char fileName[UINT8_COUNT];
	sprintf_s(fileName, UINT8_COUNT, "%s%s%s", marshaller->vm->config.marshalOutputPath, module->path->chars, "o");
	if (marshaller->vm->config.marshalFileWatch && marshalSourceFileModified(module->path->chars, fileName)) {
		return false;
	}

	FILE* file;
    fopen_s(&file, fileName, "rb");
	if (file == NULL) return false;
	marshaller->module = module;
	marshaller->bytes = (ByteArray*)malloc(sizeof(ByteArray));
	ABORT_IFNULL(marshaller->bytes, "Failed to allocate memory for byte streams to perform marshal deserialization.\n");

	size_t fileSize = marshalFileSize(file);
	ABORT_IFTRUE(fileSize == 0, "File \"%s\" is empty, cannot perform marshal deserialization.\n", fileName);
	marshalInitBytes(marshaller, (int)fileSize);

	size_t bytesRead = fread(marshaller->bytes->elements, sizeof(uint8_t), fileSize, file);
	ABORT_IFTRUE(bytesRead < fileSize, "Failed to read file \"%s\" for marshal deserialization.\n", fileName);
	marshalDeserializeModule(marshaller);	
	
	fclose(file);
	marshalCleanup(marshaller);
	return true;
}
