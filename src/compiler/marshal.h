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

struct Marshaler {
	VM* vm;
	ObjModule* module;
	ByteArray* bytes;
	int offset;
};

void initMarshaler(Marshaler* marshaler, VM* vm);
void marshalSerializeValue(ByteArray* bytes, Value value);
Value marshalDeserializeValue(Marshaler* marshaler);
void marshalDump(Marshaler* marshaler, ObjModule* module);
bool marshalLoad(Marshaler* marshaler, ObjModule* module);

#endif // !clox_marshal_h