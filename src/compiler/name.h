#pragma once
#ifndef clox_name_h
#define clox_name_h

#include "../vm/value.h"

typedef struct {
    ObjString* shortName;
    ObjString* fullName;
} NameEntry;

typedef struct {
    int count;
    int capacity;
    NameEntry* entries;
} NameTable;

NameTable* newNameTable();
void freeNameTable(NameTable* table);
ObjString* nameTableGet(NameTable* table, ObjString* shortName);
bool nameTableSet(NameTable* table, ObjString* shortName, ObjString* fullName);

#endif // !clox_name_h