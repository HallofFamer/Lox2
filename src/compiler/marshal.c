#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "chunk.h"
#include "marshal.h"
#include "../vm/debug.h"

static void initMarshaler(Marshaler* marshaler, VM* vm) {
	marshaler->vm = vm;
	marshaler->module = NULL;
	marshaler->bytes = NULL;
	marshaler->offset = 0;
}

Marshaler* newMarshaler(VM* vm) {
	Marshaler* marshaler = (Marshaler*)malloc(sizeof(Marshaler));
	ABORT_IFNULL(marshaler, "Failed to allocate memory for Marshaler.\n");
	initMarshaler(marshaler, vm);
	return marshaler;
}

void freeMarshaler(Marshaler* marshaler) {
	if (marshaler->bytes != NULL) {
		ByteArrayFree(marshaler->bytes);
		free(marshaler->bytes);
	}
	free(marshaler);
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

static void marshalInitBytes(Marshaler* marshaler, int fileSize) {
	ByteArrayInit(marshaler->bytes);
	marshaler->bytes->count = fileSize;
	marshaler->bytes->capacity = marshalNextCapacity(fileSize);
	marshaler->bytes->elements = (uint8_t*)malloc(marshaler->bytes->capacity);
	ABORT_IFNULL(marshaler->bytes->elements, "Failed to allocate memory for byte streams to perform marshal deserialization.\n");
}

static void marshalCleanup(Marshaler* marshaler) {
	ByteArrayFree(marshaler->bytes);
	initMarshaler(marshaler, marshaler->vm);
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

static void marshalSerializeFunction(ByteArray* bytes, ObjFunction* function) {
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
		marshalSerializeValue(bytes, function->chunk.constants.values[i]);
	}

	marshalSerializeByte(bytes, (uint8_t)function->chunk.identifiers.count);
	for (int i = 0; i < function->chunk.identifiers.count; i++) {
		marshalSerializeString(bytes, AS_STRING(function->chunk.identifiers.values[i]));
	}

	marshalSerializeInt(bytes, (uint32_t)function->chunk.count);
	for (int i = 0; i < function->chunk.count; i++) {
		marshalSerializeByte(bytes, function->chunk.code[i]);
	}
}

static void marshalSerializeValue(ByteArray* bytes, Value value) {
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
		marshalSerializeFunction(bytes, AS_FUNCTION(value));
	}
	else {
		fprintf(stderr, "Unsupported value type for serialization.\n");
		exit(1);
	}
}

static void marshalSerializeModule(ByteArray* bytes, ObjModule* module) {
	ObjFunction* function = module->closure->function;
	marshalSerializeString(bytes, module->path);

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
	marshalSerializeFunction(bytes, function);
}

void marshalDump(Marshaler* marshaler, ObjModule* module) {
	if (!marshaler->vm->config.marshalEnabled) return;
	char fileName[UINT8_COUNT];
	sprintf_s(fileName, UINT8_COUNT, "%s%s", module->path->chars, "o");
	FILE* file;
	fopen_s(&file, fileName, "wb");
	ABORT_IFNULL(file, "Failed to open file \"%s\" for marshal serialization.\n", fileName);

	marshaler->module = module;
	marshaler->bytes = (ByteArray*)malloc(sizeof(ByteArray));
	ByteArrayInit(marshaler->bytes);
	ABORT_IFNULL(marshaler->bytes, "Failed to allocate memory for byte streams to perform marshal serialization.\n");
	marshalSerializeModule(marshaler->bytes, marshaler->module);

	fwrite(marshaler->bytes->elements, sizeof(uint8_t), marshaler->bytes->count, file);
	fclose(file);
	marshalCleanup(marshaler);
}

static uint8_t marshalDeserializeByte(Marshaler* marshaler) {
	return marshaler->bytes->elements[marshaler->offset++];
}

static uint16_t marshalDeserializeShort(Marshaler* marshaler) {
	uint16_t value = (marshalDeserializeByte(marshaler) << 8) | marshalDeserializeByte(marshaler);
	return value;
}

static uint32_t marshalDeserializeInt(Marshaler* marshaler) {
	uint32_t value = (marshalDeserializeByte(marshaler) << 24) | (marshalDeserializeByte(marshaler) << 16) |
		(marshalDeserializeByte(marshaler) << 8) | marshalDeserializeByte(marshaler);
	return value;
}

static double marshalDeserializeDouble(Marshaler* marshaler) {
	uint64_t numBits = ((uint64_t)marshalDeserializeByte(marshaler) << 56) | ((uint64_t)marshalDeserializeByte(marshaler) << 48) |
		((uint64_t)marshalDeserializeByte(marshaler) << 40) | ((uint64_t)marshalDeserializeByte(marshaler) << 32) |
		((uint64_t)marshalDeserializeByte(marshaler) << 24) | ((uint64_t)marshalDeserializeByte(marshaler) << 16) |
		((uint64_t)marshalDeserializeByte(marshaler) << 8) | (uint64_t)marshalDeserializeByte(marshaler);
	return AS_NUMBER(numBits);
}

static ObjString* marshalDeserializeString(Marshaler* marshaler) {
	uint32_t length = marshalDeserializeInt(marshaler);
	char* chars = (char*)malloc((size_t)length + 1);
	ABORT_IFNULL(chars, "Failed to allocate memory for deserializing string.\n");

	for (uint32_t i = 0; i < length; i++) {
		chars[i] = (char)marshalDeserializeByte(marshaler);
	}

	chars[length] = '\0';
	ObjString* string = takeString(marshaler->vm, chars, length);
	return string;
}

static ObjFunction* marshalDeserializeFunction(Marshaler* marshaler) {
	bool isNamed = marshalDeserializeByte(marshaler) == 1;
	ObjString* name = isNamed ? marshalDeserializeString(marshaler) : NULL;
	ObjFunction* function = newFunction(marshaler->vm, name, false);
	push(marshaler->vm, OBJ_VAL(function));

	function->arity = (int16_t)marshalDeserializeShort(marshaler);
	function->typeParamCount = (int8_t)marshalDeserializeByte(marshaler);
	function->upvalueCount = (int8_t)marshalDeserializeByte(marshaler);
	function->isAsync = (marshalDeserializeByte(marshaler) == 1);
	function->isGenerator = (marshalDeserializeByte(marshaler) == 1);

	uint8_t constantCount = marshalDeserializeByte(marshaler);
	for (uint8_t i = 0; i < constantCount; i++) {
		Value constant = marshalDeserializeValue(marshaler);
		addConstant(marshaler->vm, &function->chunk, constant);
	}

	uint8_t identifierCount = marshalDeserializeByte(marshaler);
	for (uint8_t i = 0; i < identifierCount; i++) {
		ObjString* identifier = marshalDeserializeString(marshaler);
		addIdentifier(marshaler->vm, &function->chunk, OBJ_VAL(identifier));
	}

	uint32_t codeCount = marshalDeserializeInt(marshaler);
	for (uint32_t i = 0; i < codeCount; i++) {
		uint8_t code = marshalDeserializeByte(marshaler);
		writeChunk(marshaler->vm, &function->chunk, code, 0);
	}

	if (marshaler->vm->config.debugCode) {
	    disassembleChunk(&function->chunk, function->name != NULL ? function->name->chars : "<script>");
	}
	pop(marshaler->vm);
	return function;
}

static Value marshalDeserializeValue(Marshaler* marshaler) {
	uint8_t type = marshalDeserializeByte(marshaler);
	switch (type) {
	    case MARSHAL_TYPE_NIL:
		    return NIL_VAL;
	    case MARSHAL_TYPE_BOOL:
		    return BOOL_VAL(marshalDeserializeByte(marshaler) == 1);
	    case MARSHAL_TYPE_INT:
	     	return INT_VAL((int32_t)marshalDeserializeInt(marshaler));
	    case MARSHAL_TYPE_NUMBER:
		    return NUMBER_VAL(marshalDeserializeDouble(marshaler));
	    case MARSHAL_TYPE_STRING:
		    return OBJ_VAL(marshalDeserializeString(marshaler));
	    case MARSHAL_TYPE_FUNCTION:
		    return OBJ_VAL(marshalDeserializeFunction(marshaler));
	    default:
		    fprintf(stderr, "Unsupported value type for deserialization.\n");
	     	exit(1);
	}
}

static void marshalDeserializeModule(Marshaler* marshaler) {
	ObjString* path = marshalDeserializeString(marshaler);
	if (marshaler->module->path != path) {
		fprintf(stderr, "Module path mismatch during marshal deserialization. Expected: %s, but got: %s.\n", marshaler->module->path->chars, path->chars);
		exit(1);
	}

	int numValIndexes = marshalDeserializeInt(marshaler);
	for (int i = 0; i < numValIndexes; i++) {
		ObjString* name = marshalDeserializeString(marshaler);
		uint32_t index = marshalDeserializeInt(marshaler);
		idMapSet(marshaler->vm, &marshaler->module->valIndexes, name, index);
		valueArrayWrite(marshaler->vm, &marshaler->module->valFields, NIL_VAL);
	}

	int numVarIndexes = marshalDeserializeInt(marshaler);
	for (int i = 0; i < numVarIndexes; i++) {
		ObjString* name = marshalDeserializeString(marshaler);
		uint32_t index = marshalDeserializeInt(marshaler);
		idMapSet(marshaler->vm, &marshaler->module->varIndexes, name, index);
		valueArrayWrite(marshaler->vm, &marshaler->module->varFields, NIL_VAL);
	}

	ObjFunction* function = marshalDeserializeFunction(marshaler);
	ABORT_IFNULL(function, "Failed to deserialize program for file \"%s\".\n", marshaler->module->path->chars);
	push(marshaler->vm, OBJ_VAL(function));
	marshaler->module->closure = newClosure(marshaler->vm, function);
	pop(marshaler->vm);
}

static bool marshalSourceFileModified(const char* sourceFilePath, const char* compiledFilePath) {
	struct stat sourceFileStat, compiledFileStat;
	if (stat(sourceFilePath, &sourceFileStat) != 0 || stat(compiledFilePath, &compiledFileStat)) {	
		return false;
	}
	return difftime(sourceFileStat.st_mtime, compiledFileStat.st_mtime) > 0;
}

bool marshalLoad(Marshaler* marshaler, ObjModule* module) {
	if (!marshaler->vm->config.marshalEnabled) return false;
	char fileName[UINT8_COUNT];
	sprintf_s(fileName, UINT8_COUNT, "%s%s", module->path->chars, "o");
	if (marshalSourceFileModified(module->path->chars, fileName)) return false;
	FILE* file;
    fopen_s(&file, fileName, "rb");
	if (file == NULL) return false;

	marshaler->module = module;
	marshaler->bytes = (ByteArray*)malloc(sizeof(ByteArray));
	ABORT_IFNULL(marshaler->bytes, "Failed to allocate memory for byte streams to perform marshal deserialization.\n");

	size_t fileSize = marshalFileSize(file);
	ABORT_IFTRUE(fileSize == 0, "File \"%s\" is empty, cannot perform marshal deserialization.\n", fileName);
	marshalInitBytes(marshaler, (int)fileSize);
	size_t bytesRead = fread(marshaler->bytes->elements, sizeof(uint8_t), fileSize, file);
	ABORT_IFTRUE(bytesRead < fileSize, "Failed to read file \"%s\" for marshal deserialization.\n", fileName);
	marshalDeserializeModule(marshaler);
	
	fclose(file);
	marshalCleanup(marshaler);
	return true;
}
