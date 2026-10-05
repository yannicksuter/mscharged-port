#include "NL/nlPrint.h"
#include <cstdarg>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <vector>

// Host byte-string formatting. Wii 16-bit wide-string formatting is separate.
int nlSNPrintf(char* buffer, unsigned long size, const char* format, ...)
{
    // MSL's __StringWrite consumes aliased %s input before terminating the
    // output. The original nlFont::Load depends on that behavior. A separate
    // native output buffer avoids libc clearing the game's input first.
    std::vector<char> output(size);
    va_list args; va_start(args, format);
    const int result = vsnprintf(size ? output.data() : nullptr, size, format, args);
    va_end(args);
    if (size)
    {
        const auto count = result >= 0
            ? std::min<std::size_t>(size, static_cast<std::size_t>(result) + 1)
            : static_cast<std::size_t>(std::find(output.begin(), output.end(), '\0') - output.begin()) + 1;
        std::memcpy(buffer, output.data(), std::min<std::size_t>(count, size));
    }
    return result;
}
int nlPrintf(const char* format, ...)
{
    va_list args; va_start(args, format);
    const int result = vfprintf(stderr, format, args);
    va_end(args);
    return result;
}
