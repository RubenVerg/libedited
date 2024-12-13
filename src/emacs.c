/*	$NetBSD: emacs.c,v 1.38 2024/06/29 17:28:07 christos Exp $	*/

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
static char sccsid[] = "@(#)emacs.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: emacs.c,v 1.38 2024/06/29 17:28:07 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * emacs.c: Emacs functions
 */
#include <ctype.h>

#include "edited/el.h"
#include "edited/emacs.h"
#include "edited/fcns.h"

/* edited_em_delete_or_list():
 *	Delete character under cursor or list completions if at end of line
 *	[^D]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_delete_or_list(Edited *el, wint_t c)
{

	if (el->edited_line.cursor == el->edited_line.lastchar) {
					/* if I'm at the end */
		if (el->edited_line.cursor == el->edited_line.buffer) {
					/* and the beginning */
			edited_term_writec(el, c);	/* then do an EOF */
			return CC_EOF;
		} else {
			/*
			 * Here we could list completions, but it is an
			 * error right now
			 */
			edited_term_beep(el);
			return CC_ERROR;
		}
	} else {
		if (el->edited_state.doingarg)
			edited_c_delafter(el, el->edited_state.argument);
		else
			edited_c_delafter1(el);
		if (el->edited_line.cursor > el->edited_line.lastchar)
			el->edited_line.cursor = el->edited_line.lastchar;
				/* bounds check */
		return CC_REFRESH;
	}
}


/* edited_em_delete_next_word():
 *	Cut from cursor to end of current word
 *	[M-d]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_delete_next_word(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *cp, *p, *kp;

	if (el->edited_line.cursor == el->edited_line.lastchar)
		return CC_ERROR;

	cp = edited_c__next_word(el->edited_line.cursor, el->edited_line.lastchar,
	    el->edited_state.argument, edited_ce__isword);

	for (p = el->edited_line.cursor, kp = el->edited_chared.edited_c_kill.buf; p < cp; p++)
				/* save the text */
		*kp++ = *p;
	el->edited_chared.edited_c_kill.last = kp;

	edited_c_delafter(el, (int)(cp - el->edited_line.cursor));	/* delete after dot */
	if (el->edited_line.cursor > el->edited_line.lastchar)
		el->edited_line.cursor = el->edited_line.lastchar;
				/* bounds check */
	return CC_REFRESH;
}


/* edited_em_yank():
 *	Paste cut buffer at cursor position
 *	[^Y]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_yank(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *kp, *cp;

	if (el->edited_chared.edited_c_kill.last == el->edited_chared.edited_c_kill.buf)
		return CC_NORM;

	if (el->edited_line.lastchar +
	    (el->edited_chared.edited_c_kill.last - el->edited_chared.edited_c_kill.buf) >=
	    el->edited_line.limit)
		return CC_ERROR;

	el->edited_chared.edited_c_kill.mark = el->edited_line.cursor;

	/* open the space, */
	edited_c_insert(el,
	    (int)(el->edited_chared.edited_c_kill.last - el->edited_chared.edited_c_kill.buf));
	cp = el->edited_line.cursor;
	/* copy the chars */
	for (kp = el->edited_chared.edited_c_kill.buf; kp < el->edited_chared.edited_c_kill.last; kp++)
		*cp++ = *kp;

	/* if an arg, cursor at beginning else cursor at end */
	if (el->edited_state.argument == 1)
		el->edited_line.cursor = cp;

	return CC_REFRESH;
}


/* edited_em_kill_line():
 *	Cut the entire line and save in cut buffer
 *	[^U]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_kill_line(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *kp, *cp;

	cp = el->edited_line.buffer;
	kp = el->edited_chared.edited_c_kill.buf;
	while (cp < el->edited_line.lastchar)
		*kp++ = *cp++;	/* copy it */
	el->edited_chared.edited_c_kill.last = kp;
				/* zap! -- delete all of it */
	el->edited_line.lastchar = el->edited_line.buffer;
	el->edited_line.cursor = el->edited_line.buffer;
	return CC_REFRESH;
}


/* edited_em_kill_region():
 *	Cut area between mark and cursor and save in cut buffer
 *	[^W]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_kill_region(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *kp, *cp;

	if (!el->edited_chared.edited_c_kill.mark)
		return CC_ERROR;

	if (el->edited_chared.edited_c_kill.mark > el->edited_line.cursor) {
		cp = el->edited_line.cursor;
		kp = el->edited_chared.edited_c_kill.buf;
		while (cp < el->edited_chared.edited_c_kill.mark)
			*kp++ = *cp++;	/* copy it */
		el->edited_chared.edited_c_kill.last = kp;
		edited_c_delafter(el, (int)(cp - el->edited_line.cursor));
	} else {		/* mark is before cursor */
		cp = el->edited_chared.edited_c_kill.mark;
		kp = el->edited_chared.edited_c_kill.buf;
		while (cp < el->edited_line.cursor)
			*kp++ = *cp++;	/* copy it */
		el->edited_chared.edited_c_kill.last = kp;
		edited_c_delbefore(el, (int)(cp - el->edited_chared.edited_c_kill.mark));
		el->edited_line.cursor = el->edited_chared.edited_c_kill.mark;
	}
	return CC_REFRESH;
}


/* edited_em_copy_region():
 *	Copy area between mark and cursor to cut buffer
 *	[M-W]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_copy_region(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *kp, *cp;

	if (!el->edited_chared.edited_c_kill.mark)
		return CC_ERROR;

	if (el->edited_chared.edited_c_kill.mark > el->edited_line.cursor) {
		cp = el->edited_line.cursor;
		kp = el->edited_chared.edited_c_kill.buf;
		while (cp < el->edited_chared.edited_c_kill.mark)
			*kp++ = *cp++;	/* copy it */
		el->edited_chared.edited_c_kill.last = kp;
	} else {
		cp = el->edited_chared.edited_c_kill.mark;
		kp = el->edited_chared.edited_c_kill.buf;
		while (cp < el->edited_line.cursor)
			*kp++ = *cp++;	/* copy it */
		el->edited_chared.edited_c_kill.last = kp;
	}
	return CC_NORM;
}


/* edited_em_gosmacs_transpose():
 *	Exchange the two characters before the cursor
 *	Gosling emacs transpose chars [^T]
 */
libedited_private edited_action_t
edited_em_gosmacs_transpose(Edited *el, wint_t c)
{

	if (el->edited_line.cursor > &el->edited_line.buffer[1]) {
		/* must have at least two chars entered */
		c = el->edited_line.cursor[-2];
		el->edited_line.cursor[-2] = el->edited_line.cursor[-1];
		el->edited_line.cursor[-1] = c;
		return CC_REFRESH;
	} else
		return CC_ERROR;
}


/* edited_em_next_word():
 *	Move next to end of current word
 *	[M-f]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_next_word(Edited *el, wint_t c __attribute__((__unused__)))
{
	if (el->edited_line.cursor == el->edited_line.lastchar)
		return CC_ERROR;

	el->edited_line.cursor = edited_c__next_word(el->edited_line.cursor,
	    el->edited_line.lastchar,
	    el->edited_state.argument,
	    edited_ce__isword);

	if (el->edited_map.type == MAP_VI)
		if (el->edited_chared.edited_c_vcmd.action != NOP) {
			edited_cv_delfini(el);
			return CC_REFRESH;
		}
	return CC_CURSOR;
}


/* edited_em_upper_case():
 *	Uppercase the characters from cursor to end of current word
 *	[M-u]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_upper_case(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *cp, *ep;

	ep = edited_c__next_word(el->edited_line.cursor, el->edited_line.lastchar,
	    el->edited_state.argument, edited_ce__isword);

	for (cp = el->edited_line.cursor; cp < ep; cp++)
		if (iswlower(*cp))
			*cp = towupper(*cp);

	el->edited_line.cursor = ep;
	if (el->edited_line.cursor > el->edited_line.lastchar)
		el->edited_line.cursor = el->edited_line.lastchar;
	return CC_REFRESH;
}


/* edited_em_capitol_case():
 *	Capitalize the characters from cursor to end of current word
 *	[M-c]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_capitol_case(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *cp, *ep;

	ep = edited_c__next_word(el->edited_line.cursor, el->edited_line.lastchar,
	    el->edited_state.argument, edited_ce__isword);

	for (cp = el->edited_line.cursor; cp < ep; cp++) {
		if (iswalpha(*cp)) {
			if (iswlower(*cp))
				*cp = towupper(*cp);
			cp++;
			break;
		}
	}
	for (; cp < ep; cp++)
		if (iswupper(*cp))
			*cp = towlower(*cp);

	el->edited_line.cursor = ep;
	if (el->edited_line.cursor > el->edited_line.lastchar)
		el->edited_line.cursor = el->edited_line.lastchar;
	return CC_REFRESH;
}


/* edited_em_lower_case():
 *	Lowercase the characters from cursor to end of current word
 *	[M-l]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_lower_case(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *cp, *ep;

	ep = edited_c__next_word(el->edited_line.cursor, el->edited_line.lastchar,
	    el->edited_state.argument, edited_ce__isword);

	for (cp = el->edited_line.cursor; cp < ep; cp++)
		if (iswupper(*cp))
			*cp = towlower(*cp);

	el->edited_line.cursor = ep;
	if (el->edited_line.cursor > el->edited_line.lastchar)
		el->edited_line.cursor = el->edited_line.lastchar;
	return CC_REFRESH;
}


/* edited_em_set_mark():
 *	Set the mark at cursor
 *	[^@]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_set_mark(Edited *el, wint_t c __attribute__((__unused__)))
{

	el->edited_chared.edited_c_kill.mark = el->edited_line.cursor;
	return CC_NORM;
}


/* edited_em_exchange_mark():
 *	Exchange the cursor and mark
 *	[^X^X]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_exchange_mark(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *cp;

	cp = el->edited_line.cursor;
	el->edited_line.cursor = el->edited_chared.edited_c_kill.mark;
	el->edited_chared.edited_c_kill.mark = cp;
	return CC_CURSOR;
}


/* edited_em_universal_argument():
 *	Universal argument (argument times 4)
 *	[^U]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_universal_argument(Edited *el, wint_t c __attribute__((__unused__)))
{				/* multiply current argument by 4 */

	if (el->edited_state.argument > 1000000)
		return CC_ERROR;
	el->edited_state.doingarg = 1;
	el->edited_state.argument *= 4;
	return CC_ARGHACK;
}


/* edited_em_meta_next():
 *	Add 8th bit to next character typed
 *	[<ESC>]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_meta_next(Edited *el, wint_t c __attribute__((__unused__)))
{

	el->edited_state.metanext = 1;
	return CC_ARGHACK;
}


/* edited_em_toggle_overwrite():
 *	Switch from insert to overwrite mode or vice versa
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_toggle_overwrite(Edited *el, wint_t c __attribute__((__unused__)))
{

	el->edited_state.inputmode = (el->edited_state.inputmode == MODE_INSERT) ?
	    MODE_REPLACE : MODE_INSERT;
	return CC_NORM;
}


/* edited_em_copy_prev_word():
 *	Copy current word to cursor
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_copy_prev_word(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *cp, *oldc, *dp;

	if (el->edited_line.cursor == el->edited_line.buffer)
		return CC_ERROR;

	/* does a bounds check */
	cp = edited_c__prev_word(el->edited_line.cursor, el->edited_line.buffer,
	    el->edited_state.argument, edited_ce__isword);

	edited_c_insert(el, (int)(el->edited_line.cursor - cp));
	oldc = el->edited_line.cursor;
	for (dp = oldc; cp < oldc && dp < el->edited_line.lastchar; cp++)
		*dp++ = *cp;

	el->edited_line.cursor = dp;/* put cursor at end */

	return CC_REFRESH;
}


/* edited_em_inc_search_next():
 *	Emacs incremental next search
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_inc_search_next(Edited *el, wint_t c __attribute__((__unused__)))
{

	el->edited_search.patlen = 0;
	return edited_ce_inc_search(el, EDITED_ED_SEARCH_NEXT_HISTORY);
}


/* edited_em_inc_search_prev():
 *	Emacs incremental reverse search
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_inc_search_prev(Edited *el, wint_t c __attribute__((__unused__)))
{

	el->edited_search.patlen = 0;
	return edited_ce_inc_search(el, EDITED_ED_SEARCH_PREV_HISTORY);
}


/* edited_em_delete_prev_char():
 *	Delete the character to the left of the cursor
 *	[^?]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_em_delete_prev_char(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_line.cursor <= el->edited_line.buffer)
		return CC_ERROR;

	if (el->edited_state.doingarg)
		edited_c_delbefore(el, el->edited_state.argument);
	else
		edited_c_delbefore1(el);
	el->edited_line.cursor -= el->edited_state.argument;
	if (el->edited_line.cursor < el->edited_line.buffer)
		el->edited_line.cursor = el->edited_line.buffer;
	return CC_REFRESH;
}
