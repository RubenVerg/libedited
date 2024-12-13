/*	$NetBSD: prompt.c,v 1.27 2017/06/27 23:25:13 christos Exp $	*/

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
static char sccsid[] = "@(#)prompt.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: prompt.c,v 1.27 2017/06/27 23:25:13 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * prompt.c: Prompt printing functions
 */
#include <stdio.h>
#include "edited/el.h"

static wchar_t	*edited_prompt_default(Edited *);
static wchar_t	*edited_prompt_default_r(Edited *);

/* edited_prompt_default():
 *	Just a default prompt, in case the user did not provide one
 */
static wchar_t *
/*ARGSUSED*/
edited_prompt_default(Edited *el __attribute__((__unused__)))
{
	static wchar_t a[3] = L"? ";

	return a;
}


/* edited_prompt_default_r():
 *	Just a default rprompt, in case the user did not provide one
 */
static wchar_t *
/*ARGSUSED*/
edited_prompt_default_r(Edited *el __attribute__((__unused__)))
{
	static wchar_t a[1] = L"";

	return a;
}


/* edited_prompt_print():
 *	Print the prompt and update the prompt position.
 */
libedited_private void
edited_prompt_print(Edited *el, int op)
{
	edited_prompt_t *elp;
	wchar_t *p;

	if (op == EL_PROMPT)
		elp = &el->edited_prompt;
	else
		elp = &el->edited_rprompt;

	if (elp->p_wide)
		p = (*elp->p_func)(el);
	else
		p = edited_ct_decode_string((char *)(void *)(*elp->p_func)(el),
		    &el->edited_scratch);

	for (; *p; p++) {
		if (elp->p_ignore == *p) {
			wchar_t *litstart = ++p;
			while (*p && *p != elp->p_ignore)
				p++;
			if (!*p || !p[1]) {
				// XXX: We lose the last literal
				break;
			}
			edited_re_putliteral(el, litstart, p++);
			continue;
		}
		edited_re_putc(el, *p, 1);
	}

	elp->p_pos.v = el->edited_refresh.r_cursor.v;
	elp->p_pos.h = el->edited_refresh.r_cursor.h;
}


/* edited_prompt_init():
 *	Initialize the prompt stuff
 */
libedited_private int
edited_prompt_init(Edited *el)
{

	el->edited_prompt.p_func = edited_prompt_default;
	el->edited_prompt.p_pos.v = 0;
	el->edited_prompt.p_pos.h = 0;
	el->edited_prompt.p_ignore = '\0';
	el->edited_rprompt.p_func = edited_prompt_default_r;
	el->edited_rprompt.p_pos.v = 0;
	el->edited_rprompt.p_pos.h = 0;
	el->edited_rprompt.p_ignore = '\0';
	return 0;
}


/* edited_prompt_end():
 *	Clean up the prompt stuff
 */
libedited_private void
/*ARGSUSED*/
edited_prompt_end(Edited *el __attribute__((__unused__)))
{
}


/* edited_prompt_set():
 *	Install a prompt printing function
 */
libedited_private int
edited_prompt_set(Edited *el, edited_pfunc_t prf, wchar_t c, int op, int wide)
{
	edited_prompt_t *p;

	if (op == EL_PROMPT || op == EL_PROMPT_ESC)
		p = &el->edited_prompt;
	else
		p = &el->edited_rprompt;

	if (prf == NULL) {
		if (op == EL_PROMPT || op == EL_PROMPT_ESC)
			p->p_func = edited_prompt_default;
		else
			p->p_func = edited_prompt_default_r;
	} else {
		p->p_func = prf;
	}

	p->p_ignore = c;

	p->p_pos.v = 0;
	p->p_pos.h = 0;
	p->p_wide = wide;

	return 0;
}


/* edited_prompt_get():
 *	Retrieve the prompt printing function
 */
libedited_private int
edited_prompt_get(Edited *el, edited_pfunc_t *prf, wchar_t *c, int op)
{
	edited_prompt_t *p;

	if (prf == NULL)
		return -1;

	if (op == EL_PROMPT)
		p = &el->edited_prompt;
	else
		p = &el->edited_rprompt;

	if (prf)
		*prf = p->p_func;
	if (c)
		*c = p->p_ignore;

	return 0;
}
