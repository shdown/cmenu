#pragma once

#include <wchar.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    wchar_t *s;
    size_t n;
    size_t truncated_n;
    uint32_t target_width;
} TruncatedText;

void truncate_text_to_width(TruncatedText *t, uint32_t width);
