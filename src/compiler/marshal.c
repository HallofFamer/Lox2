#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "marshal.h"

void initMarshaler(Marshaler* marshaler, VM* vm) {
	marshaler->vm = vm;
	marshaler->module = NULL;
	marshaler->bytes = NULL;
	marshaler->offset = 0;
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
	free(marshaler->bytes);
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

	marshalSerializeByte(bytes, (uint8_t)function->arity);
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
	for (int i = 0; i < module->varIndexes.count; i++) {
		IDEntry* entry = &module->varIndexes.entries[i];
		if (entry != NULL && entry->key != NULL) {
			marshalSerializeString(bytes, entry->key);
			marshalSerializeInt(bytes, (uint32_t)entry->value);
		}
	}
	marshalSerializeFunction(bytes, function);
}

void marshalDump(Marshaler* marshaler, ObjModule* module) {
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
