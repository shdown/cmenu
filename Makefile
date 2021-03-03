PKGCONFIG := pkg-config
PKGCONFIG_LIBS := ncursesw

EXTERNAL_CFLAGS := $(shell $(PKGCONFIG) --cflags $(PKGCONFIG_LIBS))
EXTERNAL_LIBS := $(shell $(PKGCONFIG) --libs $(PKGCONFIG_LIBS))

MY_CFLAGS := -D_POSIX_C_SOURCE=200809L -Wall -Wextra -O2

SOURCES := bio.c cmenu.c common.c decode.c parse_uint.c print_uint.c style.c truncated_text.c
HEADERS := bio.h common.h decode.h parse_uint.h print_uint.h style.h truncated_text.h

cmenu: $(SOURCES) $(HEADERS)
	$(CC) $(EXTERNAL_CFLAGS) $(MY_CFLAGS) $(SOURCES) -o cmenu $(EXTERNAL_LIBS)

clean:
	$(RM) cmenu

.PHONY: clean
