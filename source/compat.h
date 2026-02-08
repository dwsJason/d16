#ifndef D16_COMPAT_H
#define D16_COMPAT_H

// Cross-platform compatibility shims

#ifndef _MSC_VER
// Provide fopen_s / errno_t for non-MSVC compilers
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

typedef int errno_t;

static inline errno_t fopen_s(FILE** pFile, const char* filename, const char* mode)
{
    *pFile = fopen(filename, mode);
    if (*pFile == NULL)
        return errno;
    return 0;
}

// sscanf_s is MSVC-specific; for the format specifiers used in this project
// (%f, %d, %s with no buffer-size args), sscanf is equivalent
#define sscanf_s sscanf

#endif // _MSC_VER

#endif // D16_COMPAT_H
