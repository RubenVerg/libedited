#include "edited/edited.h"

#include <stdio.h>
#include <wchar.h>

wchar_t *prompt(EditLine *el __attribute__((__unused__))) {
	return L"sample> ";
}

int main(int argc, char **argv) {
	EditLine *el = edited_init("sample", stdin, stdout, stderr);
	edited_wset(el, EL_PROMPT, prompt);
	edited_set(el, EL_EDITOR, "emacs");
	int len;
	const wchar_t *line;
	while ((line = edited_wgets(el, &len)) != NULL) {
		printf("got: %ls\n", line);
	}
	edited_end(el);
}