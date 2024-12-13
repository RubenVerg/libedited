/*	$NetBSD: eln.c,v 1.38 2024/05/17 02:59:08 christos Exp $	*/

/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#if !defined(lint) && !defined(SCCSID)
__RCSID("$NetBSD: eln.c,v 1.38 2024/05/17 02:59:08 christos Exp $");
#endif /* not lint && not SCCSID */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "edited/el.h"

int
edited_getc(Edited *el, char *cp)
{
	int num_read;
	wchar_t wc = 0;

	num_read = edited_wgetc(el, &wc);
	*cp = '\0';
	if (num_read <= 0)
		return num_read;
	num_read = wctob(wc);
	if (num_read == EOF) {
		errno = ERANGE;
		return -1;
	} else {
		*cp = (char)num_read;
		return 1;
	}
}


void
edited_push(Edited *el, const char *str)
{
	/* Using multibyte->wide string decoding works fine under single-byte
	 * character sets too, and Does The Right Thing. */
	edited_wpush(el, edited_ct_decode_string(str, &el->edited_lgcyconv));
}


const char *
edited_gets(Edited *el, int *nread)
{
	const wchar_t *tmp;

	tmp = edited_wgets(el, nread);
	if (tmp != NULL) {
	    int i;
	    size_t nwread = 0;

	    for (i = 0; i < *nread; i++)
		nwread += edited_ct_enc_width(tmp[i]);
	    *nread = (int)nwread;
	}
	return edited_ct_encode_string(tmp, &el->edited_lgcyconv);
}


int
edited_parse(Edited *el, int argc, const char *argv[])
{
	int ret;
	const wchar_t **wargv;

	wargv = (void *)edited_ct_decode_argv(argc, argv, &el->edited_lgcyconv);
	if (!wargv)
		return -1;
	ret = edited_wparse(el, argc, wargv);
	edited_free(wargv);

	return ret;
}


int
edited_set(Edited *el, int op, ...)
{
	va_list ap;
	int ret;

	if (!el)
		return -1;
	va_start(ap, op);

	switch (op) {
	case EL_PROMPT:         /* edited_pfunc_t */
	case EL_RPROMPT: {
		edited_pfunc_t p = va_arg(ap, edited_pfunc_t);
		ret = edited_prompt_set(el, p, 0, op, 0);
		break;
	}

	case EL_RESIZE: {
		edited_zfunc_t p = va_arg(ap, edited_zfunc_t);
		void *arg = va_arg(ap, void *);
		ret = ch_resizefun(el, p, arg);
		break;
	}

	case EL_ALIAS_TEXT: {
		edited_afunc_t p = va_arg(ap, edited_afunc_t);
		void *arg = va_arg(ap, void *);
		ret = ch_aliasfun(el, p, arg);
		break;
	}

	case EL_PROMPT_ESC:
	case EL_RPROMPT_ESC: {
		edited_pfunc_t p = va_arg(ap, edited_pfunc_t);
		int c = va_arg(ap, int);

		ret = edited_prompt_set(el, p, c, op, 0);
		break;
	}

	case EL_TERMINAL:       /* const char * */
		ret = edited_wset(el, op, va_arg(ap, char *));
		break;

	case EL_EDITOR:		/* const wchar_t * */
		ret = edited_wset(el, op, edited_ct_decode_string(va_arg(ap, char *),
		    &el->edited_lgcyconv));
		break;

	case EL_SIGNAL:         /* int */
	case EL_EDITMODE:
	case EL_SAFEREAD:
	case EL_UNBUFFERED:
	case EL_PREP_TERM:
		ret = edited_wset(el, op, va_arg(ap, int));
		break;

	case EL_BIND:   /* const char * list -> const wchar_t * list */
	case EL_TELLTC:
	case EL_SETTC:
	case EL_ECHOTC:
	case EL_SETTY: {
		const char *argv[20];
		int i;
		const wchar_t **wargv;
		for (i = 1; i < (int)__arraycount(argv) - 1; ++i)
			if ((argv[i] = va_arg(ap, const char *)) == NULL)
			    break;
		argv[0] = argv[i] = NULL;
		wargv = (void *)edited_ct_decode_argv(i + 1, argv, &el->edited_lgcyconv);
		if (!wargv) {
		    ret = -1;
		    goto out;
		}
		/*
		 * AFAIK we can't portably pass through our new wargv to
		 * edited_wset(), so we have to reimplement the body of
		 * edited_wset() for these ops.
		 */
		switch (op) {
		case EL_BIND:
			wargv[0] = L"bind";
			ret = edited_map_bind(el, i, wargv);
			break;
		case EL_TELLTC:
			wargv[0] = L"telltc";
			ret = edited_term_telltc(el, i, wargv);
			break;
		case EL_SETTC:
			wargv[0] = L"settc";
			ret = edited_term_settc(el, i, wargv);
			break;
		case EL_ECHOTC:
			wargv[0] = L"echotc";
			ret = edited_term_echotc(el, i, wargv);
			break;
		case EL_SETTY:
			wargv[0] = L"setty";
			ret = edited_tty_stty(el, i, wargv);
			break;
		default:
			ret = -1;
		}
		edited_free(wargv);
		break;
	}

	/* XXX: do we need to change edited_func_t too? */
	case EL_ADDFN: {          /* const char *, const char *, edited_func_t */
		const char *args[2];
		edited_func_t func;
		wchar_t **wargv;

		args[0] = va_arg(ap, const char *);
		args[1] = va_arg(ap, const char *);
		func = va_arg(ap, edited_func_t);

		wargv = edited_ct_decode_argv(2, args, &el->edited_lgcyconv);
		if (!wargv) {
		    ret = -1;
		    goto out;
		}
		/* XXX: The two strdup's leak */
		ret = edited_map_addfunc(el, wcsdup(wargv[0]), wcsdup(wargv[1]),
		    func);
		edited_free(wargv);
		break;
	}
	case EL_HIST: {           /* hist_fun_t, const char * */
		hist_fun_t fun = va_arg(ap, hist_fun_t);
		void *ptr = va_arg(ap, void *);
		ret = hist_set(el, fun, ptr);
		el->edited_flags |= NARROW_HISTORY;
		break;
	}

	case EL_GETCFN:         /* edited_rfunc_t */
		ret = edited_wset(el, op, va_arg(ap, edited_rfunc_t));
		break;

	case EL_CLIENTDATA:     /* void * */
		ret = edited_wset(el, op, va_arg(ap, void *));
		break;

	case EL_SETFP: {          /* int, FILE * */
		int what = va_arg(ap, int);
		FILE *fp = va_arg(ap, FILE *);
		ret = edited_wset(el, op, what, fp);
		break;
	}

	case EL_REFRESH:
		edited_re_clear_display(el);
		edited_re_refresh(el);
		edited_term__flush(el);
		ret = 0;
		break;

	case EL_USE_STYLE:
		el->edited_use_style = va_arg(ap, int) != 0;
		break;

	default:
		ret = -1;
		break;
	}

out:
	va_end(ap);
	return ret;
}


int
edited_get(Edited *el, int op, ...)
{
	va_list ap;
	int ret;

	if (!el)
		return -1;

	va_start(ap, op);

	switch (op) {
	case EL_PROMPT:         /* edited_pfunc_t * */
	case EL_RPROMPT: {
		edited_pfunc_t *p = va_arg(ap, edited_pfunc_t *);
		ret = edited_prompt_get(el, p, 0, op);
		break;
	}

	case EL_PROMPT_ESC: /* edited_pfunc_t *, char **/
	case EL_RPROMPT_ESC: {
		edited_pfunc_t *p = va_arg(ap, edited_pfunc_t *);
		char *c = va_arg(ap, char *);
		wchar_t wc = 0;
		ret = edited_prompt_get(el, p, &wc, op);
		*c = (char)wc;
		break;
	}

	case EL_EDITOR: {
		const char **p = va_arg(ap, const char **);
		const wchar_t *pw;
		ret = edited_wget(el, op, &pw);
		*p = edited_ct_encode_string(pw, &el->edited_lgcyconv);
		if (!el->edited_lgcyconv.csize)
			ret = -1;
		break;
	}

	case EL_TERMINAL:       /* const char ** */
		ret = edited_wget(el, op, va_arg(ap, const char **));
		break;

	case EL_SIGNAL:         /* int * */
	case EL_EDITMODE:
	case EL_SAFEREAD:
	case EL_UNBUFFERED:
	case EL_PREP_TERM:
		ret = edited_wget(el, op, va_arg(ap, int *));
		break;

	case EL_GETTC: {
		char *argv[3];
		static char gettc[] = "gettc";
		argv[0] = gettc;
		argv[1] = va_arg(ap, char *);
		argv[2] = va_arg(ap, void *);
		ret = edited_term_gettc(el, 3, argv);
		break;
	}

	case EL_GETCFN:         /* edited_rfunc_t */
		ret = edited_wget(el, op, va_arg(ap, edited_rfunc_t *));
		break;

	case EL_CLIENTDATA:     /* void ** */
		ret = edited_wget(el, op, va_arg(ap, void **));
		break;

	case EL_GETFP: {          /* int, FILE ** */
		int what = va_arg(ap, int);
		FILE **fpp = va_arg(ap, FILE **);
		ret = edited_wget(el, op, what, fpp);
		break;
	}

	case EL_USE_STYLE: {
		int* ren = va_arg(ap, int *);
		*ren = el->edited_use_style;
		break;
	}

	default:
		ret = -1;
		break;
	}

	va_end(ap);
	return ret;
}


const LineInfo *
edited_line(Edited *el)
{
	const LineInfoW *winfo = edited_wline(el);
	LineInfo *info = &el->edited_lgcylinfo;
	size_t offset;
	const wchar_t *p;

	if (el->edited_flags & FROM_ELLINE)
		return info;

	el->edited_flags |= FROM_ELLINE;
	info->buffer   = edited_ct_encode_string(winfo->buffer, &el->edited_lgcyconv);

	offset = 0;
	for (p = winfo->buffer; p < winfo->cursor; p++)
		offset += edited_ct_enc_width(*p);
	info->cursor = info->buffer + offset;

	offset = 0;
	for (p = winfo->buffer; p < winfo->lastchar; p++)
		offset += edited_ct_enc_width(*p);
	info->lastchar = info->buffer + offset;

	if (el->edited_chared.edited_c_resizefun)  
		(*el->edited_chared.edited_c_resizefun)(el, el->edited_chared.edited_c_resizearg);
	el->edited_flags &= ~FROM_ELLINE;

	return info;
}


int
edited_insertstr(Edited *el, const char *str)
{
	return edited_winsertstr(el, edited_ct_decode_string(str, &el->edited_lgcyconv));
}

int
edited_replacestr(Edited *el, const char *str)
{
	return edited_wreplacestr(el, edited_ct_decode_string(str, &el->edited_lgcyconv));
}
