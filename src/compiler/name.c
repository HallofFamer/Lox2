#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "name.h"
#include "../common/buffer.h"
#include "../vm/object.h"

NameTable* newNameTable() {
    NameTable* table = (NameTable*)malloc(sizeof(NameTable));
    if (table != NULL) {
        table->count = 0;
        table->capacity = 0;
        table->entries = NULL;
    }
    return table;
}

void freeNameTable(NameTable* table) {
    if (table != NULL) {
        if (table->entries != NULL) free(table->entries);
        free(table);
    }
}

static NameEntry* findNameEntry(NameEntry* entries, int capacity, ObjString* shortName) {
    uint32_t index = shortName->hash & (capacity - 1);
    for (;;) {
        NameEntry* entry = &entries[index];
        if (entry->shortName == shortName || entry->shortName == NULL) {
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

static void nameTableAdjustCapacity(NameTable* table, int capacity) {
    int oldCapacity = table->capacity;
    NameEntry* entries = (NameEntry*)malloc(sizeof(NameEntry) * capacity);
    if (entries == NULL) exit(1);

    for (int i = 0; i < capacity; i++) {
        entries[i].shortName = NULL;
        entries[i].fullName = NULL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        NameEntry* entry = &table->entries[i];
        if (entry->shortName == NULL) continue;

        NameEntry* dest = findNameEntry(entries, capacity, entry->shortName);
        dest->shortName = entry->shortName;
        dest->fullName = entry->fullName;
        table->count++;
    }

    free(table->entries);
    table->capacity = capacity;
    table->entries = entries;
}

ObjString* nameTableGet(NameTable* table, ObjString* shortName) {
    if (table->count == 0) return NULL;
    NameEntry* entry = findNameEntry(table->entries, table->capacity, shortName);
    if (entry->shortName == NULL) return NULL;
    return entry->fullName;
}

bool nameTableSet(NameTable* table, ObjString* shortName, ObjString* fullName) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = bufferGrowCapacity(table->capacity);
        nameTableAdjustCapacity(table, capacity);
    }

    NameEntry* entry = findNameEntry(table->entries, table->capacity, shortName);
    if (entry->shortName != NULL) return false;
    table->count++;

    entry->shortName = shortName;
    entry->fullName = fullName;
    return true;
}