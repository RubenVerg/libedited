#include "edited/style.h"
#include "edited/sys.h"

#include <string.h>

int edited_style_to_escape(Edited *el __attribute__((__unused__)), edited_style_t color, wchar_t *buf) {
	wchar_t *c = buf;

	memset(buf, 0, sizeof(buf));
	*c++ = L'\e';
	*c++ = L'[';
	if (color.reset || color.color == 0) {
		*c++ = L'0';
		*c++ = L'm';
	} else {
		if (color.foreground & 0x8) {
			*c++ = L'3';
			*c++ = L'0' + (color.foreground & 0x7);
			*c++ = L';';
		} else if (color.foreground == EDITED_COLOR_DEFAULT) {
			*c++ = L'3';
			*c++ = L'9';
			*c++ = L';';
		}
		if (color.background != 0) {
			*c++ = L'4';
			*c++ = L'0' + (color.background & 0x7);
			*c++ = L';';
		} else if (color.background == EDITED_COLOR_DEFAULT) {
			*c++ = L'4';
			*c++ = L'9';
			*c++ = L';';
		}
		if (color.bold) {
			*c++ = L'1';
			*c++ = L';';
		}
		if (color.italic) {
			*c++ = L'3';
			*c++ = L';';
		}
		if (color.underline) {
			*c++ = L'4';
			*c++ = L';';
		}
		if (color.strikethrough) {
			*c++ = L'9';
			*c++ = L';';
		}
		// override last ;
		*(c - 1) = L'm';
	}

	return (int)(c - buf);
}