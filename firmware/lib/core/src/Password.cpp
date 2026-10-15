#include "Password.h"

#include <string.h>

bool generatePassword(char* out, size_t outSize, size_t length, RandomFn randomFn) {
    if (out == nullptr || randomFn == nullptr || length < kMinWpaPasswordLength || outSize <= length) {
        return false;
    }
    const size_t alphabetSize = strlen(kPasswordAlphabet);
    for (size_t i = 0; i < length; i++) {
        out[i] = kPasswordAlphabet[randomFn() % alphabetSize];
    }
    out[length] = '\0';
    return true;
}
