/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <https://unlicense.org>
 */

#include "my_assert.hpp"

#include <stdio.h>
#include <string.h>
#include <exception>

void my_panic_with_errnum_impl_(
        const char *msg,
        int errnum,
        const char *file,
        int line,
        const char *func
    ) noexcept
{
    fprintf(
        stderr,
        "FATAL: MY_PANIC_WITH_ERRNUM(): %s: %s (at %s:%d in %s).\nAborting.\n",
        msg, strerror(errnum), file, line, func
    );
    fflush(stderr);
    std::terminate();
}

void my_assert_failed_(
        const char *expr,
        const char *file,
        int line,
        const char *func
    ) noexcept
{
    fprintf(
        stderr,
        "FATAL: MY_ASSERT(%s) at %s:%d in %s failed.\nAborting.\n",
        expr, file, line, func
    );
    fflush(stderr);
    std::terminate();
}
