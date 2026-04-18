#pragma once
#ifndef clox_config_h
#define clox_config_h

#include "../common/common.h"

typedef struct {
    const char* version;
    const char* script;
    const char* path;
    const char* timezone;

    bool debugToken;
    bool debugAst;
    bool debugSymtab;
    bool debugTypetab;
    bool debugCode;

    uint8_t flagUnusedImport;
    uint8_t flagUnusedVariable;
    uint8_t flagMutableVariable;
    uint8_t flagUndefinedType;

    const char* gcType;
    size_t gcTotalHeapSize;
    size_t gcEdenHeapSize;
    size_t gcYoungHeapSize;
    size_t gcOldHeapSize;

    bool marshalEnabled;
    bool marshalFileWatch;
    bool marshalLineInfo;
    const char* marshalOutputPath;
} Configuration;

void initConfiguration(VM* vm);
void freeConfiguration(Configuration* config);

#endif // !clox_config_h