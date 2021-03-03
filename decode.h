#pragma once

#include <wchar.h>

// Returns NULL on encoding error.
wchar_t *decode_copy(const char *s);
