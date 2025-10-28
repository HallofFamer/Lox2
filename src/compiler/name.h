#pragma once
#ifndef clox_name_h
#define clox_name_h

#include "../vm/value.h"

typedef struct {
    ObjString* key;
    ObjString* value;
} NameEntry;

typedef struct {
    int count;
    int capacity;
    NameEntry* entries;
} NameTable;

NameTable* newNameTable();
void freeNameTable(NameTable* table);
ObjString* nameTableGet(NameTable* table, ObjString* key);
bool nameTableSet(NameTable* table, ObjString* key, ObjString* value);

#endif // !clox_name_h