/*	$NetBSD: vi.c,v 1.64 2021/08/28 17:17:47 christos Exp $	*/

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
static char sccsid[] = "@(#)vi.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: vi.c,v 1.64 2021/08/28 17:17:47 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * vi.c: Vi mode commands.
 */
#include <sys/wait.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "edited/el.h"
#include "edited/common.h"
#include "edited/emacs.h"
#include "edited/fcns.h"
#include "edited/vi.h"

static edited_action_t	edited_cv_action(Edited *, wint_t);
static edited_action_t	edited_cv_paste(Edited *, wint_t);

/* edited_cv_action():
 *	Handle vi actions.
 */
static edited_action_t
edited_cv_action(Edited *el, wint_t c)
{

	if (el->edited_chared.edited_c_vcmd.action != NOP) {
		/* 'cc', 'dd' and (possibly) friends */
		if (c != (wint_t)el->edited_chared.edited_c_vcmd.action)
			return CC_ERROR;

		if (!(c & YANK))
			edited_cv_undo(el);
		edited_cv_yank(el, el->edited_line.buffer,
		    (int)(el->edited_line.lastchar - el->edited_line.buffer));
		el->edited_chared.edited_c_vcmd.action = NOP;
		el->edited_chared.edited_c_vcmd.pos = 0;
		if (!(c & YANK)) {
			el->edited_line.lastchar = el->edited_line.buffer;
			el->edited_line.cursor = el->edited_line.buffer;
		}
		if (c & INSERT)
			el->edited_map.current = el->edited_map.key;

		return CC_REFRESH;
	}
	el->edited_chared.edited_c_vcmd.pos = el->edited_line.cursor;
	el->edited_chared.edited_c_vcmd.action = c;
	return CC_ARGHACK;
}

/* edited_cv_paste():
 *	Paste previous deletion before or after the cursor
 */
static edited_action_t
edited_cv_paste(Edited *el, wint_t c)
{
	edited_c_kill_t *k = &el->edited_chared.edited_c_kill;
	size_t len = (size_t)(k->last - k->buf);

	if (k->buf == NULL || len == 0)
		return CC_ERROR;
#ifdef DEBUG_PASTE
	(void) fprintf(el->edited_errfile, "Paste: \"%.*ls\"\n", (int)len,
	    k->buf);
#endif

	edited_cv_undo(el);

	if (!c && el->edited_line.cursor < el->edited_line.lastchar)
		el->edited_line.cursor++;

	edited_c_insert(el, (int)len);
	if (el->edited_line.cursor + len > el->edited_line.lastchar)
		return CC_ERROR;
	(void) memcpy(el->edited_line.cursor, k->buf, len *
	    sizeof(*el->edited_line.cursor));

	return CC_REFRESH;
}


/* edited_vi_paste_next():
 *	Vi paste previous deletion to the right of the cursor
 *	[p]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_paste_next(Edited *el, wint_t c __attribute__((__unused__)))
{

	return edited_cv_paste(el, 0);
}


/* edited_vi_paste_prev():
 *	Vi paste previous deletion to the left of the cursor
 *	[P]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_paste_prev(Edited *el, wint_t c __attribute__((__unused__)))
{

	return edited_cv_paste(el, 1);
}


/* edited_vi_prev_big_word():
 *	Vi move to the previous space delimited word
 *	[B]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_prev_big_word(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_line.cursor == el->edited_line.buffer)
		return CC_ERROR;

	el->edited_line.cursor = edited_cv_prev_word(el->edited_line.cursor,
	    el->edited_line.buffer,
	    el->edited_state.argument,
	    edited_cv__isWord);

	if (el->edited_chared.edited_c_vcmd.action != NOP) {
		edited_cv_delfini(el);
		return CC_REFRESH;
	}
	return CC_CURSOR;
}


/* edited_vi_prev_word():
 *	Vi move to the previous word
 *	[b]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_prev_word(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_line.cursor == el->edited_line.buffer)
		return CC_ERROR;

	el->edited_line.cursor = edited_cv_prev_word(el->edited_line.cursor,
	    el->edited_line.buffer,
	    el->edited_state.argument,
	    edited_cv__isword);

	if (el->edited_chared.edited_c_vcmd.action != NOP) {
		edited_cv_delfini(el);
		return CC_REFRESH;
	}
	return CC_CURSOR;
}


/* edited_vi_next_big_word():
 *	Vi move to the next space delimited word
 *	[W]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_next_big_word(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_line.cursor >= el->edited_line.lastchar - 1)
		return CC_ERROR;

	el->edited_line.cursor = edited_cv_next_word(el, el->edited_line.cursor,
	    el->edited_line.lastchar, el->edited_state.argument, edited_cv__isWord);

	if (el->edited_map.type == MAP_VI)
		if (el->edited_chared.edited_c_vcmd.action != NOP) {
			edited_cv_delfini(el);
			return CC_REFRESH;
		}
	return CC_CURSOR;
}


/* edited_vi_next_word():
 *	Vi move to the next word
 *	[w]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_next_word(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_line.cursor >= el->edited_line.lastchar - 1)
		return CC_ERROR;

	el->edited_line.cursor = edited_cv_next_word(el, el->edited_line.cursor,
	    el->edited_line.lastchar, el->edited_state.argument, edited_cv__isword);

	if (el->edited_map.type == MAP_VI)
		if (el->edited_chared.edited_c_vcmd.action != NOP) {
			edited_cv_delfini(el);
			return CC_REFRESH;
		}
	return CC_CURSOR;
}


/* edited_vi_change_case():
 *	Vi change case of character under the cursor and advance one character
 *	[~]
 */
libedited_private edited_action_t
edited_vi_change_case(Edited *el, wint_t c)
{
	int i;

	if (el->edited_line.cursor >= el->edited_line.lastchar)
		return CC_ERROR;
	edited_cv_undo(el);
	for (i = 0; i < el->edited_state.argument; i++) {

		c = *el->edited_line.cursor;
		if (iswupper(c))
			*el->edited_line.cursor = towlower(c);
		else if (iswlower(c))
			*el->edited_line.cursor = towupper(c);

		if (++el->edited_line.cursor >= el->edited_line.lastchar) {
			el->edited_line.cursor--;
			edited_re_fastaddc(el);
			break;
		}
		edited_re_fastaddc(el);
	}
	return CC_NORM;
}


/* edited_vi_change_meta():
 *	Vi change prefix command
 *	[c]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_change_meta(Edited *el, wint_t c __attribute__((__unused__)))
{

	/*
         * Delete with insert == change: first we delete and then we leave in
         * insert mode.
         */
	return edited_cv_action(el, DELETE | INSERT);
}


/* edited_vi_insert_at_bol():
 *	Vi enter insert mode at the beginning of line
 *	[I]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_insert_at_bol(Edited *el, wint_t c __attribute__((__unused__)))
{

	el->edited_line.cursor = el->edited_line.buffer;
	edited_cv_undo(el);
	el->edited_map.current = el->edited_map.key;
	return CC_CURSOR;
}


/* edited_vi_replace_char():
 *	Vi replace character under the cursor with the next character typed
 *	[r]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_replace_char(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_line.cursor >= el->edited_line.lastchar)
		return CC_ERROR;

	el->edited_map.current = el->edited_map.key;
	el->edited_state.inputmode = MODE_REPLACE_1;
	edited_cv_undo(el);
	return CC_ARGHACK;
}


/* edited_vi_replace_mode():
 *	Vi enter replace mode
 *	[R]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_replace_mode(Edited *el, wint_t c __attribute__((__unused__)))
{

	el->edited_map.current = el->edited_map.key;
	el->edited_state.inputmode = MODE_REPLACE;
	edited_cv_undo(el);
	return CC_NORM;
}


/* edited_vi_substitute_char():
 *	Vi replace character under the cursor and enter insert mode
 *	[s]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_substitute_char(Edited *el, wint_t c __attribute__((__unused__)))
{

	edited_c_delafter(el, el->edited_state.argument);
	el->edited_map.current = el->edited_map.key;
	return CC_REFRESH;
}


/* edited_vi_substitute_line():
 *	Vi substitute entire line
 *	[S]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_substitute_line(Edited *el, wint_t c __attribute__((__unused__)))
{

	edited_cv_undo(el);
	edited_cv_yank(el, el->edited_line.buffer,
	    (int)(el->edited_line.lastchar - el->edited_line.buffer));
	(void) edited_em_kill_line(el, 0);
	el->edited_map.current = el->edited_map.key;
	return CC_REFRESH;
}


/* edited_vi_change_to_eol():
 *	Vi change to end of line
 *	[C]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_change_to_eol(Edited *el, wint_t c __attribute__((__unused__)))
{

	edited_cv_undo(el);
	edited_cv_yank(el, el->edited_line.cursor,
	    (int)(el->edited_line.lastchar - el->edited_line.cursor));
	(void) edited_ed_kill_line(el, 0);
	el->edited_map.current = el->edited_map.key;
	return CC_REFRESH;
}


/* edited_vi_insert():
 *	Vi enter insert mode
 *	[i]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_insert(Edited *el, wint_t c __attribute__((__unused__)))
{

	el->edited_map.current = el->edited_map.key;
	edited_cv_undo(el);
	return CC_NORM;
}


/* edited_vi_add():
 *	Vi enter insert mode after the cursor
 *	[a]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_add(Edited *el, wint_t c __attribute__((__unused__)))
{
	int ret;

	el->edited_map.current = el->edited_map.key;
	if (el->edited_line.cursor < el->edited_line.lastchar) {
		el->edited_line.cursor++;
		if (el->edited_line.cursor > el->edited_line.lastchar)
			el->edited_line.cursor = el->edited_line.lastchar;
		ret = CC_CURSOR;
	} else
		ret = CC_NORM;

	edited_cv_undo(el);

	return (edited_action_t)ret;
}


/* edited_vi_add_at_eol():
 *	Vi enter insert mode at end of line
 *	[A]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_add_at_eol(Edited *el, wint_t c __attribute__((__unused__)))
{

	el->edited_map.current = el->edited_map.key;
	el->edited_line.cursor = el->edited_line.lastchar;
	edited_cv_undo(el);
	return CC_CURSOR;
}


/* edited_vi_delete_meta():
 *	Vi delete prefix command
 *	[d]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_delete_meta(Edited *el, wint_t c __attribute__((__unused__)))
{

	return edited_cv_action(el, DELETE);
}


/* edited_vi_end_big_word():
 *	Vi move to the end of the current space delimited word
 *	[E]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_end_big_word(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_line.cursor == el->edited_line.lastchar)
		return CC_ERROR;

	el->edited_line.cursor = edited_cv__endword(el->edited_line.cursor,
	    el->edited_line.lastchar, el->edited_state.argument, edited_cv__isWord);

	if (el->edited_chared.edited_c_vcmd.action != NOP) {
		el->edited_line.cursor++;
		edited_cv_delfini(el);
		return CC_REFRESH;
	}
	return CC_CURSOR;
}


/* edited_vi_end_word():
 *	Vi move to the end of the current word
 *	[e]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_end_word(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_line.cursor == el->edited_line.lastchar)
		return CC_ERROR;

	el->edited_line.cursor = edited_cv__endword(el->edited_line.cursor,
	    el->edited_line.lastchar, el->edited_state.argument, edited_cv__isword);

	if (el->edited_chared.edited_c_vcmd.action != NOP) {
		el->edited_line.cursor++;
		edited_cv_delfini(el);
		return CC_REFRESH;
	}
	return CC_CURSOR;
}


/* edited_vi_undo():
 *	Vi undo last change
 *	[u]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_undo(Edited *el, wint_t c __attribute__((__unused__)))
{
	edited_c_undo_t un = el->edited_chared.edited_c_undo;

	if (un.len == -1)
		return CC_ERROR;

	/* switch line buffer and undo buffer */
	el->edited_chared.edited_c_undo.buf = el->edited_line.buffer;
	el->edited_chared.edited_c_undo.len = el->edited_line.lastchar - el->edited_line.buffer;
	el->edited_chared.edited_c_undo.cursor =
	    (int)(el->edited_line.cursor - el->edited_line.buffer);
	el->edited_line.limit = un.buf + (el->edited_line.limit - el->edited_line.buffer);
	el->edited_line.buffer = un.buf;
	el->edited_line.cursor = un.buf + un.cursor;
	el->edited_line.lastchar = un.buf + un.len;

	return CC_REFRESH;
}


/* edited_vi_command_mode():
 *	Vi enter command mode (use alternative key bindings)
 *	[<ESC>]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_command_mode(Edited *el, wint_t c __attribute__((__unused__)))
{

	/* [Esc] cancels pending action */
	el->edited_chared.edited_c_vcmd.action = NOP;
	el->edited_chared.edited_c_vcmd.pos = 0;

	el->edited_state.doingarg = 0;

	el->edited_state.inputmode = MODE_INSERT;
	el->edited_map.current = el->edited_map.alt;
#ifdef MOVE
	if (el->edited_line.cursor > el->edited_line.buffer)
		el->edited_line.cursor--;
#endif
	return CC_CURSOR;
}


/* edited_vi_zero():
 *	Vi move to the beginning of line
 *	[0]
 */
libedited_private edited_action_t
edited_vi_zero(Edited *el, wint_t c)
{

	if (el->edited_state.doingarg)
		return edited_ed_argument_digit(el, c);

	el->edited_line.cursor = el->edited_line.buffer;
	if (el->edited_chared.edited_c_vcmd.action != NOP) {
		edited_cv_delfini(el);
		return CC_REFRESH;
	}
	return CC_CURSOR;
}


/* edited_vi_delete_prev_char():
 *	Vi move to previous character (backspace)
 *	[^H] in insert mode only
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_delete_prev_char(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_line.cursor <= el->edited_line.buffer)
		return CC_ERROR;

	edited_c_delbefore1(el);
	el->edited_line.cursor--;
	return CC_REFRESH;
}


/* edited_vi_list_or_eof():
 *	Vi list choices for completion or indicate end of file if empty line
 *	[^D]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_list_or_eof(Edited *el, wint_t c)
{

	if (el->edited_line.cursor == el->edited_line.lastchar) {
		if (el->edited_line.cursor == el->edited_line.buffer) {
			edited_term_writec(el, c);	/* then do a EOF */
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
#ifdef notyet
		edited_re_goto_bottom(el);
		*el->edited_line.lastchar = '\0';	/* just in case */
		return CC_LIST_CHOICES;
#else
		/*
		 * Just complain for now.
		 */
		edited_term_beep(el);
		return CC_ERROR;
#endif
	}
}


/* edited_vi_kill_line_prev():
 *	Vi cut from beginning of line to cursor
 *	[^U]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_kill_line_prev(Edited *el, wint_t c __attribute__((__unused__)))
{
	wchar_t *kp, *cp;

	cp = el->edited_line.buffer;
	kp = el->edited_chared.edited_c_kill.buf;
	while (cp < el->edited_line.cursor)
		*kp++ = *cp++;	/* copy it */
	el->edited_chared.edited_c_kill.last = kp;
	edited_c_delbefore(el, (int)(el->edited_line.cursor - el->edited_line.buffer));
	el->edited_line.cursor = el->edited_line.buffer;	/* zap! */
	return CC_REFRESH;
}


/* edited_vi_search_prev():
 *	Vi search history previous
 *	[?]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_search_prev(Edited *el, wint_t c __attribute__((__unused__)))
{

	return edited_cv_search(el, EDITED_ED_SEARCH_PREV_HISTORY);
}


/* edited_vi_search_next():
 *	Vi search history next
 *	[/]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_search_next(Edited *el, wint_t c __attribute__((__unused__)))
{

	return edited_cv_search(el, EDITED_ED_SEARCH_NEXT_HISTORY);
}


/* edited_vi_repeat_search_next():
 *	Vi repeat current search in the same search direction
 *	[n]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_repeat_search_next(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_search.patlen == 0)
		return CC_ERROR;
	else
		return edited_cv_repeat_srch(el, el->edited_search.patdir);
}


/* edited_vi_repeat_search_prev():
 *	Vi repeat current search in the opposite search direction
 *	[N]
 */
/*ARGSUSED*/
libedited_private edited_action_t
edited_vi_repeat_search_prev(Edited *el, wint_t c __attribute__((__unused__)))
{

	if (el->edited_search.patlen == 0)
		return CC_ERROR;
	else
		return (edited_cv_repeat_srch(el,
		    el->edited_search.patdir == EDITED_ED_SEARCH_PREV_HISTORY ?
		    EDITED_ED_SEARCH_NEXT_HISTORY : EDITED_ED_SEARCH_PREV_HISTORY));
}


/* edited_vi_next_char():
 *	Vi move to the character specified next
 *	[f]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_next_char(Edited *el, wint_t c __attribute__((__unused__)))
{
	return edited_cv_csearch(el, CHAR_FWD, -1, el->edited_state.argument, 0);
}


/* edited_vi_prev_char():
 *	Vi move to the character specified previous
 *	[F]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_prev_char(Edited *el, wint_t c __attribute__((__unused__)))
{
	return edited_cv_csearch(el, CHAR_BACK, -1, el->edited_state.argument, 0);
}


/* edited_vi_to_next_char():
 *	Vi move up to the character specified next
 *	[t]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_to_next_char(Edited *el, wint_t c __attribute__((__unused__)))
{
	return edited_cv_csearch(el, CHAR_FWD, -1, el->edited_state.argument, 1);
}


/* edited_vi_to_prev_char():
 *	Vi move up to the character specified previous
 *	[T]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_to_prev_char(Edited *el, wint_t c __attribute__((__unused__)))
{
	return edited_cv_csearch(el, CHAR_BACK, -1, el->edited_state.argument, 1);
}


/* edited_vi_repeat_next_char():
 *	Vi repeat current character search in the same search direction
 *	[;]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_repeat_next_char(Edited *el, wint_t c __attribute__((__unused__)))
{

	return edited_cv_csearch(el, el->edited_search.chadir, el->edited_search.chacha,
		el->edited_state.argument, el->edited_search.chatflg);
}


/* edited_vi_repeat_prev_char():
 *	Vi repeat current character search in the opposite search direction
 *	[,]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_repeat_prev_char(Edited *el, wint_t c __attribute__((__unused__)))
{
	edited_action_t r;
	int dir = el->edited_search.chadir;

	r = edited_cv_csearch(el, -dir, el->edited_search.chacha,
		el->edited_state.argument, el->edited_search.chatflg);
	el->edited_search.chadir = dir;
	return r;
}


/* edited_vi_match():
 *	Vi go to matching () {} or []
 *	[%]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_match(Edited *el, wint_t c __attribute__((__unused__)))
{
	const wchar_t match_chars[] = L"()[]{}";
	wchar_t *cp;
	size_t delta, i, count;
	wchar_t o_ch, edited_c_ch;

	*el->edited_line.lastchar = '\0';		/* just in case */

	i = wcscspn(el->edited_line.cursor, match_chars);
	o_ch = el->edited_line.cursor[i];
	if (o_ch == 0)
		return CC_ERROR;
	delta = (size_t)(wcschr(match_chars, o_ch) - match_chars);
	edited_c_ch = match_chars[delta ^ 1];
	count = 1;
	delta = 1 - (delta & 1) * 2;

	for (cp = &el->edited_line.cursor[i]; count; ) {
		cp += delta;
		if (cp < el->edited_line.buffer || cp >= el->edited_line.lastchar)
			return CC_ERROR;
		if (*cp == o_ch)
			count++;
		else if (*cp == edited_c_ch)
			count--;
	}

	el->edited_line.cursor = cp;

	if (el->edited_chared.edited_c_vcmd.action != NOP) {
		/* NB posix says char under cursor should NOT be deleted
		   for -ve delta - this is different to netbsd vi. */
		if (delta > 0)
			el->edited_line.cursor++;
		edited_cv_delfini(el);
		return CC_REFRESH;
	}
	return CC_CURSOR;
}

/* edited_vi_undo_line():
 *	Vi undo all changes to line
 *	[U]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_undo_line(Edited *el, wint_t c __attribute__((__unused__)))
{

	edited_cv_undo(el);
	return hist_get(el);
}

/* edited_vi_to_column():
 *	Vi go to specified column
 *	[|]
 * NB netbsd vi goes to screen column 'n', posix says nth character
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_to_column(Edited *el, wint_t c __attribute__((__unused__)))
{

	el->edited_line.cursor = el->edited_line.buffer;
	el->edited_state.argument--;
	return edited_ed_next_char(el, 0);
}

/* edited_vi_yank_end():
 *	Vi yank to end of line
 *	[Y]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_yank_end(Edited *el, wint_t c __attribute__((__unused__)))
{

	edited_cv_yank(el, el->edited_line.cursor,
	    (int)(el->edited_line.lastchar - el->edited_line.cursor));
	return CC_REFRESH;
}

/* edited_vi_yank():
 *	Vi yank
 *	[y]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_yank(Edited *el, wint_t c __attribute__((__unused__)))
{

	return edited_cv_action(el, YANK);
}

/* edited_vi_comment_out():
 *	Vi comment out current command
 *	[#]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_comment_out(Edited *el, wint_t c __attribute__((__unused__)))
{

	el->edited_line.cursor = el->edited_line.buffer;
	edited_c_insert(el, 1);
	*el->edited_line.cursor = '#';
	edited_re_refresh(el);
	return edited_ed_newline(el, 0);
}

/* edited_vi_alias():
 *	Vi include shell alias
 *	[@]
 * NB: posix implies that we should enter insert mode, however
 * this is against historical precedent...
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_alias(Edited *el, wint_t c __attribute__((__unused__)))
{
	char alias_name[3];
	const char *alias_text;

	if (el->edited_chared.edited_c_aliasfun == NULL)
		return CC_ERROR;

	alias_name[0] = '_';
	alias_name[2] = 0;
	if (edited_getc(el, &alias_name[1]) != 1)
		return CC_ERROR;

	alias_text = (*el->edited_chared.edited_c_aliasfun)(el->edited_chared.edited_c_aliasarg,
	    alias_name);
	if (alias_text != NULL)
		edited_wpush(el, edited_ct_decode_string(alias_text, &el->edited_scratch));
	return CC_NORM;
}

/* edited_vi_to_history_line():
 *	Vi go to specified history file line.
 *	[G]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_to_history_line(Edited *el, wint_t c __attribute__((__unused__)))
{
	int sv_event_no = el->edited_history.eventno;
	edited_action_t rval;


	if (el->edited_history.eventno == 0) {
		 (void) wcsncpy(el->edited_history.buf, el->edited_line.buffer,
		     EL_BUFSIZ);
		 el->edited_history.last = el->edited_history.buf +
			 (el->edited_line.lastchar - el->edited_line.buffer);
	}

	/* Lack of a 'count' means oldest, not 1 */
	if (!el->edited_state.doingarg) {
		el->edited_history.eventno = 0x7fffffff;
		hist_get(el);
	} else {
		/* This is brain dead, all the rest of this code counts
		 * upwards going into the past.  Here we need count in the
		 * other direction (to match the output of fc -l).
		 * I could change the world, but this seems to suffice.
		 */
		el->edited_history.eventno = 1;
		if (hist_get(el) == CC_ERROR)
			return CC_ERROR;
		el->edited_history.eventno = 1 + el->edited_history.ev.num
					- el->edited_state.argument;
		if (el->edited_history.eventno < 0) {
			el->edited_history.eventno = sv_event_no;
			return CC_ERROR;
		}
	}
	rval = hist_get(el);
	if (rval == CC_ERROR)
		el->edited_history.eventno = sv_event_no;
	return rval;
}

/* edited_vi_histedit():
 *	Vi edit history line with vi
 *	[v]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_histedit(Edited *el, wint_t c __attribute__((__unused__)))
{
	int fd;
	pid_t pid;
	ssize_t st;
	int status;
	char tempfile[] = "/tmp/histedit.XXXXXXXXXX";
	char *cp = NULL;
	size_t len;
	wchar_t *line = NULL;
	const char *editor;

	if (el->edited_state.doingarg) {
		if (edited_vi_to_history_line(el, 0) == CC_ERROR)
			return CC_ERROR;
	}

	if ((editor = getenv("EDITOR")) == NULL)
		editor = "vi";
	fd = mkstemp(tempfile);
	if (fd < 0)
		return CC_ERROR;
	len = (size_t)(el->edited_line.lastchar - el->edited_line.buffer);
#define TMP_BUFSIZ (EL_BUFSIZ * MB_LEN_MAX)
	cp = edited_calloc(TMP_BUFSIZ, sizeof(*cp));
	if (cp == NULL)
		goto error;
	line = edited_calloc(len + 1, sizeof(*line));
	if (line == NULL)
		goto error;
	wcsncpy(line, el->edited_line.buffer, len);
	line[len] = '\0';
	wcstombs(cp, line, TMP_BUFSIZ - 1);
	cp[TMP_BUFSIZ - 1] = '\0';
	len = strlen(cp);
	write(fd, cp, len);
	write(fd, "\n", (size_t)1);
	pid = fork();
	switch (pid) {
	case -1:
		goto error;
	case 0:
		close(fd);
		execlp(editor, editor, tempfile, (char *)NULL);
		exit(0);
		/*NOTREACHED*/
	default:
		while (waitpid(pid, &status, 0) != pid)
			continue;
		lseek(fd, (off_t)0, SEEK_SET);
		st = read(fd, cp, TMP_BUFSIZ - 1);
		if (st > 0) {
			cp[st] = '\0';
			len = (size_t)(el->edited_line.limit - el->edited_line.buffer);
			len = mbstowcs(el->edited_line.buffer, cp, len);
			if (len > 0 && el->edited_line.buffer[len - 1] == '\n')
				--len;
		}
		else
			len = 0;
                el->edited_line.cursor = el->edited_line.buffer;
                el->edited_line.lastchar = el->edited_line.buffer + len;
		edited_free(cp);
                edited_free(line);
		break;
	}

	close(fd);
	unlink(tempfile);
	/* return CC_REFRESH; */
	return edited_ed_newline(el, 0);
error:
	edited_free(line);
	edited_free(cp);
	close(fd);
	unlink(tempfile);
	return CC_ERROR;
}

/* edited_vi_history_word():
 *	Vi append word from previous input line
 *	[_]
 * Who knows where this one came from!
 * '_' in vi means 'entire current line', so 'cc' is a synonym for 'edited_c_'
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_history_word(Edited *el, wint_t c __attribute__((__unused__)))
{
	const wchar_t *wp = HIST_FIRST(el);
	const wchar_t *wep, *wsp;
	int len;
	wchar_t *cp;
	const wchar_t *lim;

	if (wp == NULL)
		return CC_ERROR;

	wep = wsp = NULL;
	do {
		while (iswspace(*wp))
			wp++;
		if (*wp == 0)
			break;
		wsp = wp;
		while (*wp && !iswspace(*wp))
			wp++;
		wep = wp;
	} while ((!el->edited_state.doingarg || --el->edited_state.argument > 0)
	    && *wp != 0);

	if (wsp == NULL || (el->edited_state.doingarg && el->edited_state.argument != 0))
		return CC_ERROR;

	edited_cv_undo(el);
	len = (int)(wep - wsp);
	if (el->edited_line.cursor < el->edited_line.lastchar)
		el->edited_line.cursor++;
	edited_c_insert(el, len + 1);
	cp = el->edited_line.cursor;
	lim = el->edited_line.limit;
	if (cp < lim)
		*cp++ = ' ';
	while (wsp < wep && cp < lim)
		*cp++ = *wsp++;
	el->edited_line.cursor = cp;

	el->edited_map.current = el->edited_map.key;
	return CC_REFRESH;
}

/* edited_vi_redo():
 *	Vi redo last non-motion command
 *	[.]
 */
libedited_private edited_action_t
/*ARGSUSED*/
edited_vi_redo(Edited *el, wint_t c __attribute__((__unused__)))
{
	edited_c_redo_t *r = &el->edited_chared.edited_c_redo;

	if (!el->edited_state.doingarg && r->count) {
		el->edited_state.doingarg = 1;
		el->edited_state.argument = r->count;
	}

	el->edited_chared.edited_c_vcmd.pos = el->edited_line.cursor;
	el->edited_chared.edited_c_vcmd.action = r->action;
	if (r->pos != r->buf) {
		if (r->pos + 1 > r->lim)
			/* sanity */
			r->pos = r->lim - 1;
		r->pos[0] = 0;
		edited_wpush(el, r->buf);
	}

	el->edited_state.thiscmd = r->cmd;
	el->edited_state.thisch = r->ch;
	return (*el->edited_map.func[r->cmd])(el, r->ch);
}
