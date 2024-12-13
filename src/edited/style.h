#ifndef _h_edited_style
#define _h_edited_style

#include <stddef.h>
#include <stdint.h>

#include "edited/edited.h"

#define EDITED_STYLE_RESET ((edited_style_t){ .reset = 1 })
#define EDITED_COLOR_BLACK 8
#define EDITED_COLOR_RED 9
#define EDITED_COLOR_GREEN 10
#define EDITED_COLOR_YELLOW 11
#define EDITED_COLOR_BLUE 12
#define EDITED_COLOR_MAGENTA 13
#define EDITED_COLOR_CYAN 14
#define EDITED_COLOR_WHITE 15
#define EDITED_COLOR_DEFAULT 1
#define EDITED_COLOR_UNSET 0

union edited_style_t {
	uint16_t color;
	struct {
		unsigned foreground : 4;
		unsigned background : 4;
		unsigned reset : 1;
		unsigned bold : 1;
		unsigned italic : 1;
		unsigned underline : 1;
		unsigned strikethrough : 1;
	};
};

typedef union edited_style_t edited_style_t;

typedef void (*edited_stylefunc_t)(Edited *, int, const wchar_t *, edited_style_t *);

int edited_style_to_escape(Edited *el, edited_style_t color, wchar_t *buf);

#endif