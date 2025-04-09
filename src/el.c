/*	$NetBSD: el.c,v 1.101 2022/10/30 19:11:31 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#if !defined(lint) && !defined(SCCSID)
#if 0
static char sccsid[] = "@(#)el.c	8.2 (Berkeley) 1/3/94";
#else
__RCSID("$NetBSD: el.c,v 1.101 2022/10/30 19:11:31 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

/*
 * el.c: EditLine interface functions
 */
#include <sys/types.h>
#include <sys/param.h>
#include <ctype.h>
#include <langinfo.h>
#include <locale.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "edited/el.h"
#include "edited/parse.h"
#include "edited/read.h"

#ifndef HAVE_SECURE_GETENV
#	ifdef HAVE___SECURE_GETENV
#		define secure_getenv __secure_getenv
#		define HAVE_SECURE_GETENV 1
#	else
#		ifdef HAVE_ISSETUGID
#			include <unistd.h>
#		else
#			undef issetugid
#			define issetugid() 1
#		endif
#	endif
#endif

char *edited_secure_getenv(char const *name)
{
	if (issetugid())
		return 0;
	return getenv(name);
}

/* edited_init():
 *	Initialize editline and set default parameters.
 */
Edited *
edited_init(const char *prog, FILE *fin, FILE *fout, FILE *ferr)
{
    return edited_init_fd(prog, fin, fout, ferr, fileno(fin), fileno(fout),
	fileno(ferr));
}

libedited_private Edited *
edited_init_internal(const char *prog, FILE *fin, FILE *fout, FILE *ferr,
    int fdin, int fdout, int fderr, int flags)
{
	Edited *el = edited_calloc(1, sizeof(*el));

	if (el == NULL)
		return NULL;

	el->edited_infile = fin;
	el->edited_outfile = fout;
	el->edited_errfile = ferr;

	el->edited_infd = fdin;
	el->edited_outfd = fdout;
	el->edited_errfd = fderr;

	el->edited_prog = wcsdup(edited_ct_decode_string(prog, &el->edited_scratch));
	if (el->edited_prog == NULL) {
		edited_free(el);
		return NULL;
	}

	/*
         * Initialize all the modules. Order is important!!!
         */
	el->edited_flags = flags;

	if (edited_term_init(el) == -1) {
		edited_free(el->edited_prog);
		edited_free(el);
		return NULL;
	}
	(void) edited_km_init(el);
	(void) edited_map_init(el);
	if (edited_tty_init(el) == -1)
		el->edited_flags |= NO_TTY;
	(void) ch_init(el);
	(void) search_init(el);
	(void) hist_init(el);
	(void) edited_prompt_init(el);
	(void) edited_sig_init(el);
	(void) edited_lit_init(el);
	if (edited_read_init(el) == -1) {
		edited_end(el);
		return NULL;
	}
	el->edited_use_style = 0;
	el->edited_style_func = NULL;
	return el;
}

Edited *
edited_init_fd(const char *prog, FILE *fin, FILE *fout, FILE *ferr,
    int fdin, int fdout, int fderr)
{
	return edited_init_internal(prog, fin, fout, ferr, fdin, fdout, fderr, 0);
}

/* edited_end():
 *	Clean up.
 */
void
edited_end(Edited *el)
{

	if (el == NULL)
		return;

	edited_reset(el);

	edited_term_end(el);
	edited_km_end(el);
	edited_map_end(el);
	if (!(el->edited_flags & NO_TTY))
		edited_tty_end(el, TCSAFLUSH);
	ch_end(el);
	edited_read_end(el);
	search_end(el);
	hist_end(el);
	edited_prompt_end(el);
	edited_sig_end(el);
	edited_lit_end(el);

	edited_free(el->edited_prog);
	edited_free(el->edited_visual.cbuff);
	edited_free(el->edited_visual.wbuff);
	edited_free(el->edited_scratch.cbuff);
	edited_free(el->edited_scratch.wbuff);
	edited_free(el->edited_lgcyconv.cbuff);
	edited_free(el->edited_lgcyconv.wbuff);
	edited_free(el);
}


/* edited_reset():
 *	Reset the tty and the parser
 */
void
edited_reset(Edited *el)
{

	edited_tty_cookedmode(el);
	ch_reset(el);		/* XXX: Do we want that? */
}


/* edited_set():
 *	set the editline parameters
 */
int
edited_wset(Edited *el, int op, ...)
{
	va_list ap;
	int rv = 0;

	if (el == NULL)
		return -1;
	va_start(ap, op);

	switch (op) {
	case EL_PROMPT:
	case EL_RPROMPT: {
		edited_pfunc_t p = va_arg(ap, edited_pfunc_t);

		rv = edited_prompt_set(el, p, 0, op, 1);
		break;
	}

	case EL_RESIZE: {
		edited_zfunc_t p = va_arg(ap, edited_zfunc_t);
		void *arg = va_arg(ap, void *);
		rv = ch_resizefun(el, p, arg);
		break;
	}

	case EL_ALIAS_TEXT: {
		edited_afunc_t p = va_arg(ap, edited_afunc_t);
		void *arg = va_arg(ap, void *);
		rv = ch_aliasfun(el, p, arg);
		break;
	}

	case EL_PROMPT_ESC:
	case EL_RPROMPT_ESC: {
		edited_pfunc_t p = va_arg(ap, edited_pfunc_t);
		int c = va_arg(ap, int);

		rv = edited_prompt_set(el, p, (wchar_t)c, op, 1);
		break;
	}

	case EL_TERMINAL:
		rv = edited_term_set(el, va_arg(ap, char *));
		break;

	case EL_EDITOR:
		rv = edited_map_set_editor(el, va_arg(ap, wchar_t *));
		break;

	case EL_SIGNAL:
		if (va_arg(ap, int))
			el->edited_flags |= HANDLE_SIGNALS;
		else
			el->edited_flags &= ~HANDLE_SIGNALS;
		break;

	case EL_BIND:
	case EL_TELLTC:
	case EL_SETTC:
	case EL_ECHOTC:
	case EL_SETTY:
	{
		const wchar_t *argv[20];
		int i;

		for (i = 1; i < (int)__arraycount(argv); i++)
			if ((argv[i] = va_arg(ap, wchar_t *)) == NULL)
				break;

		switch (op) {
		case EL_BIND:
			argv[0] = L"bind";
			rv = edited_map_bind(el, i, argv);
			break;

		case EL_TELLTC:
			argv[0] = L"telltc";
			rv = edited_term_telltc(el, i, argv);
			break;

		case EL_SETTC:
			argv[0] = L"settc";
			rv = edited_term_settc(el, i, argv);
			break;

		case EL_ECHOTC:
			argv[0] = L"echotc";
			rv = edited_term_echotc(el, i, argv);
			break;

		case EL_SETTY:
			argv[0] = L"setty";
			rv = edited_tty_stty(el, i, argv);
			break;

		default:
			rv = -1;
			EL_ABORT((el->edited_errfile, "Bad op %d\n", op));
			break;
		}
		break;
	}

	case EL_ADDFN:
	{
		wchar_t *name = va_arg(ap, wchar_t *);
		wchar_t *help = va_arg(ap, wchar_t *);
		edited_func_t func = va_arg(ap, edited_func_t);

		rv = edited_map_addfunc(el, name, help, func);
		break;
	}

	case EL_HIST:
	{
		hist_fun_t func = va_arg(ap, hist_fun_t);
		void *ptr = va_arg(ap, void *);

		rv = hist_set(el, func, ptr);
		if (MB_CUR_MAX == 1)
			el->edited_flags &= ~NARROW_HISTORY;
		break;
	}

	case EL_SAFEREAD:
		if (va_arg(ap, int))
			el->edited_flags |= FIXIO;
		else
			el->edited_flags &= ~FIXIO;
		rv = 0;
		break;

	case EL_EDITMODE:
		if (va_arg(ap, int))
			el->edited_flags &= ~EDIT_DISABLED;
		else
			el->edited_flags |= EDIT_DISABLED;
		rv = 0;
		break;

	case EL_GETCFN:
	{
		edited_rfunc_t rc = va_arg(ap, edited_rfunc_t);
		rv = edited_read_setfn(el->edited_read, rc);
		break;
	}

	case EL_CLIENTDATA:
		el->edited_data = va_arg(ap, void *);
		break;

	case EL_UNBUFFERED:
		rv = va_arg(ap, int);
		if (rv && !(el->edited_flags & UNBUFFERED)) {
			el->edited_flags |= UNBUFFERED;
			edited_read_prepare(el);
		} else if (!rv && (el->edited_flags & UNBUFFERED)) {
			el->edited_flags &= ~UNBUFFERED;
			edited_read_finish(el);
		}
		rv = 0;
		break;

	case EL_PREP_TERM:
		rv = va_arg(ap, int);
		if (rv)
			(void) edited_tty_rawmode(el);
		else
			(void) edited_tty_cookedmode(el);
		rv = 0;
		break;

	case EL_SETFP:
	{
		FILE *fp;
		int what;

		what = va_arg(ap, int);
		fp = va_arg(ap, FILE *);

		rv = 0;
		switch (what) {
		case 0:
			el->edited_infile = fp;
			el->edited_infd = fileno(fp);
			break;
		case 1:
			el->edited_outfile = fp;
			el->edited_outfd = fileno(fp);
			break;
		case 2:
			el->edited_errfile = fp;
			el->edited_errfd = fileno(fp);
			break;
		default:
			rv = -1;
			break;
		}
		break;
	}

	case EL_REFRESH:
		edited_re_clear_display(el);
		edited_re_refresh(el);
		edited_term__flush(el);
		break;

	case EL_USE_STYLE:
		el->edited_use_style = va_arg(ap, int) != 0;
		break;

	case EL_STYLE_FUNC:
		el->edited_style_func = va_arg(ap, edited_stylefunc_t);
		break;

	default:
		rv = -1;
		break;
	}

	va_end(ap);
	return rv;
}


/* edited_get():
 *	retrieve the editline parameters
 */
int
edited_wget(Edited *el, int op, ...)
{
	va_list ap;
	int rv;

	if (el == NULL)
		return -1;

	va_start(ap, op);

	switch (op) {
	case EL_PROMPT:
	case EL_RPROMPT: {
		edited_pfunc_t *p = va_arg(ap, edited_pfunc_t *);
		rv = edited_prompt_get(el, p, 0, op);
		break;
	}
	case EL_PROMPT_ESC:
	case EL_RPROMPT_ESC: {
		edited_pfunc_t *p = va_arg(ap, edited_pfunc_t *);
		wchar_t *c = va_arg(ap, wchar_t *);

		rv = edited_prompt_get(el, p, c, op);
		break;
	}

	case EL_EDITOR:
		rv = edited_map_get_editor(el, va_arg(ap, const wchar_t **));
		break;

	case EL_SIGNAL:
		*va_arg(ap, int *) = (el->edited_flags & HANDLE_SIGNALS);
		rv = 0;
		break;

	case EL_EDITMODE:
		*va_arg(ap, int *) = !(el->edited_flags & EDIT_DISABLED);
		rv = 0;
		break;

	case EL_SAFEREAD:
		*va_arg(ap, int *) = (el->edited_flags & FIXIO);
		rv = 0;
		break;

	case EL_TERMINAL:
		edited_term_get(el, va_arg(ap, const char **));
		rv = 0;
		break;

	case EL_GETTC:
	{
		static char name[] = "gettc";
		char *argv[3];
		argv[0] = name;
		argv[1] = va_arg(ap, char *);
		argv[2] = va_arg(ap, void *);
		rv = edited_term_gettc(el, 3, argv);
		break;
	}

	case EL_GETCFN:
		*va_arg(ap, edited_rfunc_t *) = edited_read_getfn(el->edited_read);
		rv = 0;
		break;

	case EL_CLIENTDATA:
		*va_arg(ap, void **) = el->edited_data;
		rv = 0;
		break;

	case EL_UNBUFFERED:
		*va_arg(ap, int *) = (el->edited_flags & UNBUFFERED) != 0;
		rv = 0;
		break;

	case EL_GETFP:
	{
		int what;
		FILE **fpp;

		what = va_arg(ap, int);
		fpp = va_arg(ap, FILE **);
		rv = 0;
		switch (what) {
		case 0:
			*fpp = el->edited_infile;
			break;
		case 1:
			*fpp = el->edited_outfile;
			break;
		case 2:
			*fpp = el->edited_errfile;
			break;
		default:
			rv = -1;
			break;
		}
		break;
	}
	case EL_USE_STYLE: {
		int* ren = va_arg(ap, int *);
		*ren = el->edited_use_style;
		break;
	}
	case EL_STYLE_FUNC: {
		edited_stylefunc_t *fp = va_arg(ap, edited_stylefunc_t *);
		*fp = el->edited_style_func;
		break;
	}
	default:
		rv = -1;
		break;
	}
	va_end(ap);

	return rv;
}


/* edited_line():
 *	Return editing info
 */
const LineInfoW *
edited_wline(Edited *el)
{

	return (const LineInfoW *)(void *)&el->edited_line;
}


/* edited_source():
 *	Source a file
 */
int
edited_source(Edited *el, const char *fname)
{
	FILE *fp;
	size_t len;
	ssize_t slen;
	char *ptr;
	char *path = NULL;
	const wchar_t *dptr;
	int error = 0;

	fp = NULL;
	if (fname == NULL) {

		/* secure_getenv is guaranteed to be defined and do the right thing here */
		/* because of the defines above which take into account issetugid, */
		/* secure_getenv and __secure_getenv availability. */
		if ((fname = edited_secure_getenv("EDITRC")) == NULL) {
			static const char elpath[] = "/.editrc";
			size_t plen = sizeof(elpath);

			if ((ptr = edited_secure_getenv("HOME")) == NULL)
				return -1;
			plen += strlen(ptr);
			if ((path = edited_calloc(plen, sizeof(*path))) == NULL)
				return -1;
			(void)snprintf(path, plen, "%s%s", ptr,
				elpath + (*ptr == '\0'));
			fname = path;
		}

	}
	if (fname[0] == '\0')
		return -1;

	if (fp == NULL)
		fp = fopen(fname, "r");
	if (fp == NULL) {
		edited_free(path);
		return -1;
	}

	ptr = NULL;
	len = 0;
	while ((slen = getline(&ptr, &len, fp)) != -1) {
		if (*ptr == '\n')
			continue;	/* Empty line. */
		if (slen > 0 && ptr[--slen] == '\n')
			ptr[slen] = '\0';

		dptr = edited_ct_decode_string(ptr, &el->edited_scratch);
		if (!dptr)
			continue;
		/* loop until first non-space char or EOL */
		while (*dptr != '\0' && iswspace(*dptr))
			dptr++;
		if (*dptr == '#')
			continue;   /* ignore, this is a comment line */
		if ((error = edited_parse_line(el, dptr)) == -1)
			break;
	}
	free(ptr);

	edited_free(path);
	(void) fclose(fp);
	return error;
}


/* edited_resize():
 *	Called from program when terminal is resized
 */
void
edited_resize(Edited *el)
{
	int lins, cols;
	sigset_t oset, nset;

	(void) sigemptyset(&nset);
	(void) sigaddset(&nset, SIGWINCH);
	(void) sigprocmask(SIG_BLOCK, &nset, &oset);

	/* get the correct window size */
	if (edited_term_get_size(el, &lins, &cols))
		edited_term_change_size(el, lins, cols);

	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
}


/* edited_beep():
 *	Called from the program to beep
 */
void
edited_beep(Edited *el)
{

	edited_term_beep(el);
}


/* edited_editmode()
 *	Set the state of EDIT_DISABLED from the `edit' command.
 */
libedited_private int
/*ARGSUSED*/
edited_editmode(Edited *el, int argc, const wchar_t **argv)
{
	const wchar_t *how;

	if (argv == NULL || argc != 2 || argv[1] == NULL)
		return -1;

	how = argv[1];
	if (wcscmp(how, L"on") == 0) {
		el->edited_flags &= ~EDIT_DISABLED;
		edited_tty_rawmode(el);
	} else if (wcscmp(how, L"off") == 0) {
		edited_tty_cookedmode(el);
		el->edited_flags |= EDIT_DISABLED;
	}
	else {
		(void) fprintf(el->edited_errfile, "edit: Bad value `%ls'.\n",
		    how);
		return -1;
	}
	return 0;
}
