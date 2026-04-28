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
	uint64_t numBits = ((uint64_t)marshalDeserializeByte(marshaller) << 56) | ((uint64_t)marshalDeserializeByte(marshaller) << 48) |
		((uint64_t)marshalDeserializeByte(marshaller) << 40) | ((uint64_t)marshalDeserializeByte(marshaller) << 32) |
		((uint64_t)marshalDeserializeByte(marshaller) << 24) | ((uint64_t)marshalDeserializeByte(marshaller) << 16) |
		((uint64_t)marshalDeserializeByte(marshaller) << 8) | (uint64_t)marshalDeserializeByte(marshaller);
	return AS_NUMBER(numBits);
}

static ObjString* marshalDeserializeString(Marshaller* marshaller) {
	uint32_t length = marshalDeserializeInt(marshaller);
	char* chars = (char*)malloc((size_t)length + 1);
	ABORT_IFNULL(chars, "Failed to allocate memory for deserializing string.\n");

	for (uint32_t i = 0; i < length; i++) {
		chars[i] = (char)marshalDeserializeByte(marshaller);
	}

	chars[length] = '\0';
	ObjString* string = takeString(marshaller->vm, chars, length);
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
