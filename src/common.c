/*	$NetBSD: common.c,v 1.50 2024/06/30 16:29:42 christos Exp $	*/

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
static char sccsid[] = "@(#)common.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: common.c,v 1.50 2024/06/30 16:29:42 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * common.c: Common Editor functions
 */
#include <ctype.h>
#include <string.h>

#include "edited/el.h"
#include "edited/common.h"
#include "edited/fcns.h"
#include "edited/parse.h"
#include "edited/vi.h"

/* edited_ed_end_of_file():
 *	Indicate end of file
 *	[^D]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_end_of_file(Edited *el, wint_t c __attribute__((__unused__)))
{

	edited_re_goto_bottom(el);
	*el->edited_line.lastchar = '\0';
	return CC_EOF;
}


/* edited_ed_insert():
 *	Add character to the line
 *	Insert a character [bound to all insert keys]
 */
libedited_private edited_action_t
edited_ed_insert(Edited *el, wint_t c)
{
	int count = el->edited_state.argument;

	if (c == '\0')
		return CC_ERROR;

	if (el->edited_line.lastchar + el->edited_state.argument >=
	    el->edited_line.limit) {
		/* end of buffer space, try to allocate more */
		if (!ch_enlargebufs(el, (size_t) count))
			return CC_ERROR;	/* error allocating more */
	}

	if (count == 1) {
		if (el->edited_state.inputmode == MODE_INSERT
		    || el->edited_line.cursor >= el->edited_line.lastchar)
			edited_c_insert(el, 1);

		*el->edited_line.cursor++ = c;
		edited_re_fastaddc(el);		/* fast refresh for one char. */
	} else {
		if (el->edited_state.inputmode != MODE_REPLACE_1)
			edited_c_insert(el, el->edited_state.argument);

		while (count-- && el->edited_line.cursor < el->edited_line.lastchar)
			*el->edited_line.cursor++ = c;
		edited_re_refresh(el);
	}

	if (el->edited_state.inputmode == MODE_REPLACE_1)
		return edited_vi_command_mode(el, 0);

	return CC_NORM;
}


/* edited_ed_delete_prev_word():
 *	Delete from beginning of current word to cursor
 *	[M-^?] [^W]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_delete_prev_word(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *cp, *p, *kp;

	if (el->edited_line.cursor == el->edited_line.buffer)
		return CC_ERROR;

	cp = edited_c__prev_word(el->edited_line.cursor, el->edited_line.buffer,
	    el->edited_state.argument, edited_ce__isword);

	for (p = cp, kp = el->edited_chared.edited_c_kill.buf; p < el->edited_line.cursor; p++)
		*kp++ = *p;
	el->edited_chared.edited_c_kill.last = kp;

	edited_c_delbefore(el, (int)(el->edited_line.cursor - cp));/* delete before dot */
	el->edited_line.cursor = cp;
	if (el->edited_line.cursor < el->edited_line.buffer)
		el->edited_line.cursor = el->edited_line.buffer; /* bounds check */
	return CC_REFRESH;
}


/* edited_ed_delete_next_char():
 *	Delete character under cursor
 *	[^D] [x]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_delete_next_char(Edited *el, wint_t c __attribute__((__unused__)))
{
#ifdef DEBUG_EDIT
#define	EL	el->edited_line
	(void) fprintf(el->edited_errfile,
	    "\nD(b: %p(%ls)  c: %p(%ls) last: %p(%ls) limit: %p(%ls)\n",
	    EL.buffer, EL.buffer, EL.cursor, EL.cursor, EL.lastchar,
	    EL.lastchar, EL.limit, EL.limit);
#endif
	if (el->edited_line.cursor == el->edited_line.lastchar) {
			/* if I'm at the end */
		if (el->edited_map.type == MAP_VI) {
			if (el->edited_line.cursor == el->edited_line.buffer) {
				/* if I'm also at the beginning */
#ifdef KSHVI
				return CC_ERROR;
#else
				/* then do an EOF */
				edited_term_writec(el, c);
				return CC_EOF;
#endif
			} else {
#ifdef KSHVI
				el->edited_line.cursor--;
#else
				return CC_ERROR;
#endif
			}
		} else
				return CC_ERROR;
	}
	edited_c_delafter(el, el->edited_state.argument);	/* delete after dot */
	if (el->edited_map.type == MAP_VI &&
	    el->edited_line.cursor >= el->edited_line.lastchar &&
	    el->edited_line.cursor > el->edited_line.buffer)
			/* bounds check */
		el->edited_line.cursor = el->edited_line.lastchar - 1;
	return CC_REFRESH;
}


/* edited_ed_kill_line():
 *	Cut to the end of line
 *	[^K] [^K]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_kill_line(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *kp, *cp;

	cp = el->edited_line.cursor;
	kp = el->edited_chared.edited_c_kill.buf;
	while (cp < el->edited_line.lastchar)
		*kp++ = *cp++;	/* copy it */
	el->edited_chared.edited_c_kill.last = kp;
			/* zap! -- delete to end */
	el->edited_line.lastchar = el->edited_line.cursor;
	return CC_REFRESH;
}


/* edited_ed_move_to_end():
 *	Move cursor to the end of line
 *	[^E] [^E]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_move_to_end(Edited *el, wint_t c __attribute__((__unused__)))
{

	el->edited_line.cursor = el->edited_line.lastchar;
	if (el->edited_map.type == MAP_VI) {
		if (el->edited_chared.edited_c_vcmd.action != NOP) {
			edited_cv_delfini(el);
			return CC_REFRESH;
		}
#ifdef MOVE
		if (el->edited_line.cursor > el->edited_line.buffer)
			el->edited_line.cursor--;
#endif
	}
	return CC_CURSOR;
}


/* edited_ed_move_to_beg():
 *	Move cursor to the beginning of line
 *	[^A] [^A]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_move_to_beg(Edited *el, wint_t c __attribute__((__unused__)))
{

	el->edited_line.cursor = el->edited_line.buffer;

	if (el->edited_map.type == MAP_VI) {
			/* We want FIRST non space character */
		while (iswspace(*el->edited_line.cursor))
			el->edited_line.cursor++;
		if (el->edited_chared.edited_c_vcmd.action != NOP) {
			edited_cv_delfini(el);
			return CC_REFRESH;
		}
	}
	return CC_CURSOR;
}


/* edited_ed_transpose_chars():
 *	Exchange the character to the left of the cursor with the one under it
 *	[^T] [^T]
 */
libedited_private edited_action_t
edited_ed_transpose_chars(Edited *el, wint_t c)
{

	if (el->edited_line.cursor < el->edited_line.lastchar) {
		if (el->edited_line.lastchar <= &el->edited_line.buffer[1])
			return CC_ERROR;
		else
			el->edited_line.cursor++;
	}
	if (el->edited_line.cursor > &el->edited_line.buffer[1]) {
		/* must have at least two chars entered */
		c = el->edited_line.cursor[-2];
		el->edited_line.cursor[-2] = el->edited_line.cursor[-1];
		el->edited_line.cursor[-1] = c;
		return CC_REFRESH;
	} else
		return CC_ERROR;
}


/* edited_ed_next_char():
 *	Move to the right one character
 *	[^F] [^F]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_next_char(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *lim = el->edited_line.lastchar;

	if (el->edited_line.cursor >= lim ||
	    (el->edited_line.cursor == lim - 1 &&
	    el->edited_map.type == MAP_VI &&
	    el->edited_chared.edited_c_vcmd.action == NOP))
		return CC_ERROR;

	el->edited_line.cursor += el->edited_state.argument;
	if (el->edited_line.cursor > lim)
		el->edited_line.cursor = lim;

	if (el->edited_map.type == MAP_VI)
		if (el->edited_chared.edited_c_vcmd.action != NOP) {
			edited_cv_delfini(el);
			return CC_REFRESH;
		}
	return CC_CURSOR;
}


/* edited_ed_prev_word():
 *	Move to the beginning of the current word
 *	[M-b] [b]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_prev_word(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_line.cursor == el->edited_line.buffer)
		return CC_ERROR;

	el->edited_line.cursor = edited_c__prev_word(el->edited_line.cursor,
	    el->edited_line.buffer,
	    el->edited_state.argument,
	    edited_ce__isword);

	if (el->edited_map.type == MAP_VI)
		if (el->edited_chared.edited_c_vcmd.action != NOP) {
			edited_cv_delfini(el);
			return CC_REFRESH;
		}
	return CC_CURSOR;
}


/* edited_ed_prev_char():
 *	Move to the left one character
 *	[^B] [^B]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_prev_char(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_line.cursor > el->edited_line.buffer) {
		el->edited_line.cursor -= el->edited_state.argument;
		if (el->edited_line.cursor < el->edited_line.buffer)
			el->edited_line.cursor = el->edited_line.buffer;

		if (el->edited_map.type == MAP_VI)
			if (el->edited_chared.edited_c_vcmd.action != NOP) {
				edited_cv_delfini(el);
				return CC_REFRESH;
			}
		return CC_CURSOR;
	} else
		return CC_ERROR;
}


/* edited_ed_quoted_insert():
 *	Add the next character typed verbatim
 *	[^V] [^V]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_quoted_insert(Edited *el, wint_t c __attribute__((__unused__)))
{
	int num;
	wchar_t ch;

	edited_tty_quotemode(el);
	num = edited_wgetc(el, &ch);
	edited_tty_noquotemode(el);
	if (num == 1)
		return edited_ed_insert(el, ch);
	else
		return edited_ed_end_of_file(el, 0);
}


/* edited_ed_digit():
 *	Adds to argument or enters a digit
 */
libedited_private edited_action_t
edited_ed_digit(Edited *el, wint_t c)
{

	if (!iswdigit(c))
		return CC_ERROR;

	if (el->edited_state.doingarg) {
			/* if doing an arg, add this in... */
		if (el->edited_state.lastcmd == EDITED_EM_UNIVERSAL_ARGUMENT)
			el->edited_state.argument = c - '0';
		else {
			if (el->edited_state.argument > 1000000)
				return CC_ERROR;
			el->edited_state.argument =
			    (el->edited_state.argument * 10) + (c - '0');
		}
		return CC_ARGHACK;
	}

	return edited_ed_insert(el, c);
}


/* edited_ed_argument_digit():
 *	Digit that starts argument
 *	For ESC-n
 */
libedited_private edited_action_t
edited_ed_argument_digit(Edited *el, wint_t c)
{

	if (!iswdigit(c))
		return CC_ERROR;

	if (el->edited_state.doingarg) {
		if (el->edited_state.argument > 1000000)
			return CC_ERROR;
		el->edited_state.argument = (el->edited_state.argument * 10) +
		    (c - '0');
	} else {		/* else starting an argument */
		el->edited_state.argument = c - '0';
		el->edited_state.doingarg = 1;
	}
	return CC_ARGHACK;
}


/* edited_ed_unassigned():
 *	Indicates unbound character
 *	Bound to keys that are not assigned
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_unassigned(Edited *el __attribute__((__unused__)),
    wint_t c __attribute__((__unused__)))
{

	return CC_ERROR;
}


/* edited_ed_ignore():
 *	Input characters that have no effect
 *	[^C ^O ^Q ^S ^Z ^\ ^]] [^C ^O ^Q ^S ^\]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_ignore(Edited *el __attribute__((__unused__)),
	      wint_t c __attribute__((__unused__)))
{

	return CC_NORM;
}


/* edited_ed_newline():
 *	Execute command
 *	[^J]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_newline(Edited *el, wint_t c __attribute__((__unused__)))
{

	edited_re_goto_bottom(el);
	*el->edited_line.lastchar++ = '\n';
	*el->edited_line.lastchar = '\0';
	return CC_NEWLINE;
}


/* edited_ed_delete_prev_char():
 *	Delete the character to the left of the cursor
 *	[^?]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_delete_prev_char(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_line.cursor <= el->edited_line.buffer)
		return CC_ERROR;

	edited_c_delbefore(el, el->edited_state.argument);
	el->edited_line.cursor -= el->edited_state.argument;
	if (el->edited_line.cursor < el->edited_line.buffer)
		el->edited_line.cursor = el->edited_line.buffer;
	return CC_REFRESH;
}


/* edited_ed_clear_screen():
 *	Clear screen leaving current line at the top
 *	[^L]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_clear_screen(Edited *el, wint_t c __attribute__((__unused__)))
{

	edited_term_clear_screen(el);	/* clear the whole real screen */
	edited_re_clear_display(el);	/* reset everything */
	return CC_REFRESH;
}


/* edited_ed_redisplay():
 *	Redisplay everything
 *	^R
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_redisplay(Edited *el __attribute__((__unused__)),
	     wint_t c __attribute__((__unused__)))
{

	return CC_REDISPLAY;
}


/* edited_ed_start_over():
 *	Erase current line and start from scratch
 *	[^G]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_start_over(Edited *el, wint_t c __attribute__((__unused__)))
{

	ch_reset(el);
	return CC_REFRESH;
}


/* edited_ed_sequence_lead_in():
 *	First character in a bound sequence
 *	Placeholder for external keys
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_sequence_lead_in(Edited *el __attribute__((__unused__)),
		    wint_t c __attribute__((__unused__)))
{

	return CC_NORM;
}


/* edited_ed_prev_history():
 *	Move to the previous history line
 *	[^P] [k]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_prev_history(Edited *el, wint_t c __attribute__((__unused__)))
{
	char beep = 0;
	int sv_event = el->edited_history.eventno;

	el->edited_chared.edited_c_undo.len = -1;
	*el->edited_line.lastchar = '\0';		/* just in case */

	if (el->edited_history.eventno == 0) {	/* save the current buffer
						 * away */
		(void) wcsncpy(el->edited_history.buf, el->edited_line.buffer,
		    EL_BUFSIZ);
		el->edited_history.last = el->edited_history.buf +
		    (el->edited_line.lastchar - el->edited_line.buffer);
	}
	el->edited_history.eventno += el->edited_state.argument;

	if (hist_get(el) == CC_ERROR) {
		if (el->edited_map.type == MAP_VI) {
			el->edited_history.eventno = sv_event;
		}
		beep = 1;
		/* el->edited_history.eventno was fixed by first call */
		(void) hist_get(el);
	}
	if (beep)
		return CC_REFRESH_BEEP;
	return CC_REFRESH;
}


/* edited_ed_next_history():
 *	Move to the next history line
 *	[^N] [j]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_next_history(Edited *el, wint_t c __attribute__((__unused__)))
{
	edited_action_t beep = CC_REFRESH, rval;

	el->edited_chared.edited_c_undo.len = -1;
	*el->edited_line.lastchar = '\0';	/* just in case */

	el->edited_history.eventno -= el->edited_state.argument;

	if (el->edited_history.eventno < 0) {
		el->edited_history.eventno = 0;
		beep = CC_REFRESH_BEEP;
	}
	rval = hist_get(el);
	if (rval == CC_REFRESH)
		return beep;
	return rval;

}


/* edited_ed_search_prev_history():
 *	Search previous in history for a line matching the current
 *	next search history [M-P] [K]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_search_prev_history(Edited *el, wint_t c __attribute__((__unused__)))
{
	const wchar_t *hp;
	int h;
	int found = 0;

	el->edited_chared.edited_c_vcmd.action = NOP;
	el->edited_chared.edited_c_undo.len = -1;
	*el->edited_line.lastchar = '\0';	/* just in case */
	if (el->edited_history.eventno < 0) {
#ifdef DEBUG_EDIT
		(void) fprintf(el->edited_errfile,
		    "e_prev_search_hist(): eventno < 0;\n");
#endif
		el->edited_history.eventno = 0;
		return CC_ERROR;
	}
	if (el->edited_history.eventno == 0) {
		(void) wcsncpy(el->edited_history.buf, el->edited_line.buffer,
		    EL_BUFSIZ);
		el->edited_history.last = el->edited_history.buf +
		    (el->edited_line.lastchar - el->edited_line.buffer);
	}
	if (el->edited_history.ref == NULL)
		return CC_ERROR;

	hp = HIST_FIRST(el);
	if (hp == NULL)
		return CC_ERROR;

	edited_c_setpat(el);		/* Set search pattern !! */

	for (h = 1; h <= el->edited_history.eventno; h++)
		hp = HIST_NEXT(el);

	while (hp != NULL) {
#ifdef SDEBUG
		(void) fprintf(el->edited_errfile, "Comparing with \"%ls\"\n", hp);
#endif
		if ((wcsncmp(hp, el->edited_line.buffer, (size_t)
			    (el->edited_line.lastchar - el->edited_line.buffer)) ||
			hp[el->edited_line.lastchar - el->edited_line.buffer]) &&
		    edited_c_hmatch(el, hp)) {
			found = 1;
			break;
		}
		h++;
		hp = HIST_NEXT(el);
	}

	if (!found) {
#ifdef SDEBUG
		(void) fprintf(el->edited_errfile, "not found\n");
#endif
		return CC_ERROR;
	}
	el->edited_history.eventno = h;

	return hist_get(el);
}


/* edited_ed_search_next_history():
 *	Search next in history for a line matching the current
 *	[M-N] [J]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_search_next_history(Edited *el, wint_t c __attribute__((__unused__)))
{
	const wchar_t *hp;
	int h;
	int found = 0;

	el->edited_chared.edited_c_vcmd.action = NOP;
	el->edited_chared.edited_c_undo.len = -1;
	*el->edited_line.lastchar = '\0';	/* just in case */

	if (el->edited_history.eventno == 0)
		return CC_ERROR;

	if (el->edited_history.ref == NULL)
		return CC_ERROR;

	hp = HIST_FIRST(el);
	if (hp == NULL)
		return CC_ERROR;

	edited_c_setpat(el);		/* Set search pattern !! */

	for (h = 1; h < el->edited_history.eventno && hp; h++) {
#ifdef SDEBUG
		(void) fprintf(el->edited_errfile, "Comparing with \"%ls\"\n", hp);
#endif
		if ((wcsncmp(hp, el->edited_line.buffer, (size_t)
			    (el->edited_line.lastchar - el->edited_line.buffer)) ||
			hp[el->edited_line.lastchar - el->edited_line.buffer]) &&
		    edited_c_hmatch(el, hp))
			found = h;
		hp = HIST_NEXT(el);
	}

	if (!found) {		/* is it the current history number? */
		if (!edited_c_hmatch(el, el->edited_history.buf)) {
#ifdef SDEBUG
			(void) fprintf(el->edited_errfile, "not found\n");
#endif
			return CC_ERROR;
		}
	}
	el->edited_history.eventno = found;

	return hist_get(el);
}


/* edited_ed_prev_line():
 *	Move up one line
 *	Could be [k] [^p]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_prev_line(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *ptr;
	int nchars = edited_c_hpos(el);

	/*
         * Move to the line requested
         */
	if (*(ptr = el->edited_line.cursor) == '\n')
		ptr--;

	for (; ptr >= el->edited_line.buffer; ptr--)
		if (*ptr == '\n' && --el->edited_state.argument <= 0)
			break;

	if (el->edited_state.argument > 0)
		return CC_ERROR;

	/*
         * Move to the beginning of the line
         */
	for (ptr--; ptr >= el->edited_line.buffer && *ptr != '\n'; ptr--)
		continue;

	/*
         * Move to the character requested
         */
	for (ptr++;
	    nchars-- > 0 && ptr < el->edited_line.lastchar && *ptr != '\n';
	    ptr++)
		continue;

	el->edited_line.cursor = ptr;
	return CC_CURSOR;
}


/* edited_ed_next_line():
 *	Move down one line
 *	Could be [j] [^n]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_next_line(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *ptr;
	int nchars = edited_c_hpos(el);

	/*
         * Move to the line requested
         */
	for (ptr = el->edited_line.cursor; ptr < el->edited_line.lastchar; ptr++)
		if (*ptr == '\n' && --el->edited_state.argument <= 0)
			break;

	if (el->edited_state.argument > 0)
		return CC_ERROR;

	/*
         * Move to the character requested
         */
	for (ptr++;
	    nchars-- > 0 && ptr < el->edited_line.lastchar && *ptr != '\n';
	    ptr++)
		continue;

	el->edited_line.cursor = ptr;
	return CC_CURSOR;
}


/* edited_ed_command():
 *	Editline extended command
 *	[M-X] [:]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_ed_command(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t tmpbuf[EL_BUFSIZ];
	int tmplen;

	tmplen = edited_c_gets(el, tmpbuf, L"\n: ");
	edited_term__putc(el, '\n');

	if (tmplen < 0 || (tmpbuf[tmplen] = 0, edited_parse_line(el, tmpbuf)) == -1)
		edited_term_beep(el);

	el->edited_map.current = el->edited_map.key;
	edited_re_clear_display(el);
	return CC_REFRESH;
}
