#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <curl/curl.h>

#include "os.h"

#ifdef _WIN32
    // define windows only functions.
    #pragma comment(lib,"WS2_32")

char* dirname(char* path) { 
    if (path == NULL || *path == '\0') return ".";

    char* lastSlash = strpbrk(path, "/\\");
    char* temp = lastSlash;
    while (temp) {
        lastSlash = temp;
        temp = strpbrk(lastSlash + 1, "/\\");
    }

    if (!lastSlash) return ".";
    if (lastSlash == path) {
        *(lastSlash + 1) = '\0';
        return path;
    }

    *lastSlash = '\0';
    return path;
}
#else
    // define non-windows functions.
static void strrev(char str[]) {
    int len = strlen(str);
    int start = 0;
    int end = len - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        end--;
        start++;
    }
}

void _itoa_s(int value, char buffer[], size_t bufsz, int radix) {
    int i = 0;
    bool isNegative = false;

    if (value == 0) {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return;
    }

    if (value < 0 && radix == 10) {
        isNegative = true;
        value = -value;
    }

    while (value != 0) {
        int rem = value % radix;
        buffer[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / radix;
    }

    if (isNegative) buffer[i++] = '-';
    buffer[i] = '\0';
    strrev(buffer);
}
#endif

int mkdir_p(const char* path) {
    char buffer[256];
    char* p = NULL;
    size_t len;

    snprintf(buffer, sizeof(buffer), "%s", path);
    len = strlen(buffer);
    if (buffer[len - 1] == '/') buffer[len - 1] = 0;

    for (p = buffer + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            int result = _mkdir(buffer);
			if (result != 0 && errno != EEXIST) return result;
            *p = '/';
        }
    }
    return _mkdir(buffer);
}

int fopen_p(FILE** file, const char* path, const char* mode) {
    char dirPath[256];

    strcpy_s(dirPath, sizeof(dirPath), path);
    char* lastSlash = strrchr(dirPath, '/');
    if (lastSlash) *lastSlash = '\0';

    struct stat dirStat;
    if (stat(dirPath, &dirStat) == -1) {
        int result = mkdir_p(dirPath);
        if (result != 0) return result;
    }
	return fopen_s(file, path, mode);
}

void runAtStartup() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != NO_ERROR) exit(60);
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    curl_global_init(CURL_GLOBAL_ALL);
}

void runAtExit(void) {
    curl_global_cleanup();

#ifdef _WIN32
    WSACleanup();
#endif
}