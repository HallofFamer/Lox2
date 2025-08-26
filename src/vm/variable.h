#pragma once
#ifndef clox_variable_h
#define clox_variable_h

#include "../compiler/chunk.h"

bool matchVariableName(ObjString* sourceString, const char* targetChars, int targetLength);
bool loadGlobal(VM* vm, Chunk* chunk, uint8_t byte, Value* value);
bool hasInstanceVariable(VM* vm, Obj* object, Chunk* chunk, uint8_t byte);
bool getGenericInstanceVariableByName(VM* vm, Obj* object, ObjString* name);
bool getInstanceVariable(VM* vm, Value receiver, Chunk* chunk, uint8_t byte);
bool setGenericInstanceVariableByName(VM* vm, Obj* object, ObjString* name, Value value);
bool setInstanceVariable(VM* vm, Value receiver, Chunk* chunk, uint8_t byte, Value value);
int getOffsetForGenericObject(Obj* object);

#endif // !clox_variable_h