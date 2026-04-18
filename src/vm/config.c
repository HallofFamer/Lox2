#include <stdlib.h>

#include "config.h"
#include "vm.h"
#include "../inc/ini.h"

#define HAS_CONFIG_SECTION(s) strcmp(section, (s)) == 0
#define HAS_CONFIG_NAME(n) strcmp(name, (n)) == 0

static char* getIniFilePath() {
    char* initPath = "lox2.ini";
    struct stat initStat;

    if (stat(initPath, &initStat) == -1) {
        char defaultPath[UINT8_COUNT];
        char* defaultDir = getenv("LOX2_HOME");

        if (defaultDir != NULL) {
            sprintf_s(defaultPath, UINT8_COUNT, "%s/lox2.ini", defaultDir);
            return (stat(defaultPath, &initStat) == -1) ? NULL : defaultPath;
        }
        else return NULL;
    }
    return initPath;
}

static int parseBasicSection(void* data, const char* name, const char* value) {
    Configuration* config = (Configuration*)data;
    if (HAS_CONFIG_NAME("version")) {
        config->version = _strdup(value);
    }
    else if (HAS_CONFIG_NAME("script")) {
        config->script = _strdup(value);
    }
    else if (HAS_CONFIG_NAME("path")) {
        config->path = _strdup(value);
    }
    else if (HAS_CONFIG_NAME("timezone")) {
        config->timezone = _strdup(value);
    }
    else {
        return 0;
    }
    return 1;
}

static int parseDebugSection(void* data, const char* name, const char* value) {
    Configuration* config = (Configuration*)data;
    if (HAS_CONFIG_NAME("debugToken")) {
        config->debugToken = (bool)atoi(value);
    }
    else if (HAS_CONFIG_NAME("debugAst")) {
        config->debugAst = (bool)atoi(value);
    }
    else if (HAS_CONFIG_NAME("debugSymtab")) {
        config->debugSymtab = (bool)atoi(value);
    }
    else if (HAS_CONFIG_NAME("debugTypetab")) {
        config->debugTypetab = (bool)atoi(value);
    }
    else if (HAS_CONFIG_NAME("debugCode")) {
        config->debugCode = (bool)atoi(value);
    }
    else {
        return 0;
    }
    return 1;
}

static int parseGCSection(void* data, const char* name, const char* value) {
    Configuration* config = (Configuration*)data;
    if (HAS_CONFIG_NAME("gcType")) {
        config->gcType = _strdup(value);
    }
    else if (HAS_CONFIG_NAME("gcTotalHeapSize")) {
        config->gcTotalHeapSize = (size_t)atol(value);
    }
    else if (HAS_CONFIG_NAME("gcEdenHeapSize")) {
        config->gcEdenHeapSize = (size_t)atol(value);
    }
    else if (HAS_CONFIG_NAME("gcYoungHeapSize")) {
        config->gcYoungHeapSize = (size_t)atol(value);
    }
    else if (HAS_CONFIG_NAME("gcOldHeapSize")) {
        config->gcOldHeapSize = (size_t)atol(value);
    }
    else {
        return 0;
    }
    return 1;
}

static int parseMarshalSection(void* data, const char* name, const char* value) {
    Configuration* config = (Configuration*)data;
    if (HAS_CONFIG_NAME("marshalEnabled")) {
        config->marshalEnabled = (bool)atoi(value);
    }
    else if (HAS_CONFIG_NAME("marshalFileWatch")) {
        config->marshalFileWatch = (bool)atoi(value);
    }
    else if (HAS_CONFIG_NAME("marshalLineInfo")) {
        config->marshalLineInfo = (bool)atoi(value);
    }
    else if (HAS_CONFIG_NAME("marshalOutputPath")) {
        config->marshalOutputPath = _strdup(value);
    }
    else {
        return 0;
    }
    return 1;
}

static int parseFlagSection(void* data, const char* name, const char* value) {
    Configuration* config = (Configuration*)data;
    if (HAS_CONFIG_NAME("flagUnusedImport")) {
        config->flagUnusedImport = (uint8_t)atoi(value);
    }
    else if (HAS_CONFIG_NAME("flagUnusedVariable")) {
        config->flagUnusedVariable = (uint8_t)atoi(value);
    }
    else if (HAS_CONFIG_NAME("flagMutableVariable")) {
        config->flagMutableVariable = (uint8_t)atoi(value);
    }
    else if (HAS_CONFIG_NAME("flagUndefinedType")) {
        config->flagUndefinedType = (uint8_t)atoi(value);
    }
    else {
        return 0;
    }
    return 1;
}

static int parseConfiguration(void* data, const char* section, const char* name, const char* value) {
    Configuration* config = (Configuration*)data;
	if (HAS_CONFIG_SECTION("basic")) {
        return parseBasicSection(data, name, value);
    }
	else if (HAS_CONFIG_SECTION("debug")) {
        return parseDebugSection(data, name, value);
    }
    else if (HAS_CONFIG_SECTION("flag")) {
        return parseFlagSection(data, name, value);
	}
    else if (HAS_CONFIG_SECTION("gc")) {
        return parseGCSection(data, name, value);
    }
    else if (HAS_CONFIG_SECTION("marshal")) {
        return parseMarshalSection(data, name, value);
    }
    else {
        return 0;
	}
}

void initConfiguration(VM* vm) {
    char* initPath = getIniFilePath();
	ABORT_IFNULL(initPath, "Error: Could not find \"lox2.ini\" configuration file in the current directory or in lox2 home directory.\n");
    Configuration config;
    int iniParsed = ini_parse(initPath, parseConfiguration, &config);
    ABORT_IFTRUE(iniParsed < 0, "Error parsing \"lox2.ini\" configuration file...\n");
    vm->config = config;
}

void freeConfiguration(Configuration* config) {
    if (config->version != NULL) free((void*)config->version);
    if (config->script != NULL) free((void*)config->script);
    if (config->path != NULL) free((void*)config->path);
    if (config->timezone != NULL) free((void*)config->timezone);
    if (config->gcType != NULL) free((void*)config->gcType);
    if (config->marshalOutputPath != NULL) free((void*)config->marshalOutputPath);
}

#undef HAS_CONFIG_SECTION
#undef HAS_CONFIG_NAME