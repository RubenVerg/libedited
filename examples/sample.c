#include "histedit.h"

#include <stdio.h>
#include <wchar.h>

wchar_t *prompt(EditLine *el __attribute__((__unused__))) {
	return L"sample> ";
}

int main(int argc, char **argv) {
	EditLine *el = el_init("sample", stdin, stdout, stderr);
	el_wset(el, EL_PROMPT, prompt);
	el_set(el, EL_EDITOR, "emacs");
	int len;
	const wchar_t *line;
	while ((line = el_wgets(el, &len)) != NULL) {
		printf("got: %ls\n", line);
	}
	el_end(el);
}