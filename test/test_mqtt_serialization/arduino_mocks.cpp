#include "arduino_mocks.h"

#ifdef UNIT_TEST

// Serial mock instance
SerialMock Serial;

// Mock esp_random for native testing
extern "C" uint32_t esp_random() {
    return (uint32_t)rand();
}

// Mock strlcpy (not standard on Linux)
size_t strlcpy(char* dst, const char* src, size_t size) {
    size_t src_len = strlen(src);
    if (size > 0) {
        size_t copy_len = (src_len >= size) ? size - 1 : src_len;
        memcpy(dst, src, copy_len);
        dst[copy_len] = '\0';
    }
    return src_len;
}

#endif // UNIT_TEST
