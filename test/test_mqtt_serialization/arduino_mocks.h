#pragma once

#ifdef UNIT_TEST
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdint>

// Mock Arduino Serial for native testing
class SerialMock {
public:
    void print(const char* str) { printf("%s", str); }
    void println(const char* str) { printf("%s\n", str); }
};

extern SerialMock Serial;

// Mock Arduino macros
#define F(x) x

// Mock ESP32 functions
extern "C" uint32_t esp_random();

// Mock Arduino string functions (not standard on Linux)
size_t strlcpy(char* dst, const char* src, size_t size);

#endif // UNIT_TEST
