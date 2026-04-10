#pragma once
#ifndef clox_marshal_h
#define clox_marshal_h

#include "../common/buffer.h"
#include "../common/common.h"
#include "../vm/vm.h"

typedef enum {
	MARSHAL_TYPE_NIL,
	MARSHAL_TYPE_BOOL,
	MARSHAL_TYPE_INT,
	MARSHAL_TYPE_NUMBER,
	MARSHAL_TYPE_STRING,
	MARSHAL_TYPE_FUNCTION,
	MARSHAL_TYPE_END
} MarshalType;

struct Marshaller {
	VM* vm;
	ObjModule* module;
	ByteArray* bytes;
	int offset;
};

Marshaller* newMarshaller(VM* vm);
void freeMarshaller(Marshaller* marshaller);
void marshalSerializeValue(Marshaller* marshaller, ByteArray* bytes, Value value);
Value marshalDeserializeValue(Marshaller* marshaller);
void marshalDump(Marshaller* marshaller, ObjModule* module);
bool marshalLoad(Marshaller* marshaller, ObjModule* module);

#endif // !clox_marshal_h