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

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <system_error>
#include <memory>
#include "getenv_r.hpp"

static inline void write_latency_debug_info()
{
    const char *fifo_path = getenv_r("CMENU__LATENCY_DEBUG_FIFO__");
    if (!fifo_path || !fifo_path[0]) {
        return;
    }

    // We need to spell out the function pointer type explicitly,
    // otherwise we get a warning about some attributes of fclose()
    // being dropped by decltype(&fclose).
    using FcloseType = int(*)(FILE *);

    std::unique_ptr<FILE, FcloseType> f{
        fopen(fifo_path, "w"),
        &fclose
    };
    if (!f) {
        throw std::system_error(errno, std::generic_category(), "cannot open latency debug info FIFO");
    }

    if (fputc('x', f.get()) < 0 || fflush(f.get()) < 0) {
        throw std::system_error(errno, std::generic_category(), "cannot write to latency debug info FIFO");
    }
}
