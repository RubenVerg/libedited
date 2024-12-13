#include "edited/edited.h"
#include "edited/style.h"

#include <locale.h>
#include <stdio.h>
#include <wchar.h>

wchar_t *prompt(Edited *el __attribute__((__unused__))) {
	return L"sample> ";
}

void style(Edited *el, int len, const wchar_t *line, edited_style_t *style) {
	int in_string = 0;
	for (int i = 0; i < len; i++, line++) {
		if (in_string) {
			*style++ = (edited_style_t){ .foreground = EDITED_COLOR_BLUE };
			if (*line == '"') in_string = 0;
		} else if (*line == '"') {
			*style++ = (edited_style_t){ .foreground = EDITED_COLOR_BLUE };
			in_string = 1;
		} else if ('0' <= *line && *line <= '9') {
			*style++ = (edited_style_t){ .foreground = EDITED_COLOR_RED };
		} else if (wcschr(L"+-*/%&|^<>=", *line) != NULL) {
			*style++ = (edited_style_t){ .foreground = EDITED_COLOR_GREEN };
		} else if ('a' <= *line && *line <= 'z' || 'A' <= *line && *line <= 'Z' || *line == '_') {
			*style++ = (edited_style_t){ .foreground = EDITED_COLOR_YELLOW };
		} else {
			*style++ = (edited_style_t){ .foreground = EDITED_COLOR_DEFAULT };
		}
	}
}

int main(int argc, char **argv) {
	setlocale(LC_CTYPE, "");
	Edited *el = edited_init("sample", stdin, stdout, stderr);
	edited_wset(el, EL_PROMPT, prompt);
	edited_set(el, EL_EDITOR, "emacs");
	edited_set(el, EL_USE_STYLE, 1);
	edited_wset(el, EL_STYLE_FUNC, style);
	int len;
	const wchar_t *line;
	while ((line = edited_wgets(el, &len)) != NULL) {
		printf("got: %ls\n", line);
	}
	edited_end(el);
}