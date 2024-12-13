/*	$NetBSD: chared.c,v 1.64 2024/06/29 14:13:14 christos Exp $	*/

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
static char sccsid[] = "@(#)chared.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: chared.c,v 1.64 2024/06/29 14:13:14 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * chared.c: Character editor utilities
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "edited/el.h"
#include "edited/common.h"
#include "edited/fcns.h"

/* value to leave unused in line buffer */
#define	EL_LEAVE	2

/* edited_cv_undo():
 *	Handle state for the vi undo command
 */
libedited_private void
edited_cv_undo(Edited *el)
{
	edited_c_undo_t *vu = &el->edited_chared.edited_c_undo;
	edited_c_redo_t *r = &el->edited_chared.edited_c_redo;
	size_t size;

	/* Save entire line for undo */
	size = (size_t)(el->edited_line.lastchar - el->edited_line.buffer);
	vu->len = (ssize_t)size;
	vu->cursor = (int)(el->edited_line.cursor - el->edited_line.buffer);
	(void)memcpy(vu->buf, el->edited_line.buffer, size * sizeof(*vu->buf));

	/* save command info for redo */
	r->count = el->edited_state.doingarg ? el->edited_state.argument : 0;
	r->action = el->edited_chared.edited_c_vcmd.action;
	r->pos = r->buf;
	r->cmd = el->edited_state.thiscmd;
	r->ch = el->edited_state.thisch;
}

/* edited_cv_yank():
 *	Save yank/delete data for paste
 */
libedited_private void
edited_cv_yank(Edited *el, const wchar_t *ptr, int size)
{
	edited_c_kill_t *k = &el->edited_chared.edited_c_kill;

	(void)memcpy(k->buf, ptr, (size_t)size * sizeof(*k->buf));
	k->last = k->buf + size;
}


/* edited_c_insert():
 *	Insert num characters
 */
libedited_private void
edited_c_insert(Edited *el, int num)
{
	wchar_t *cp;

	if (el->edited_line.lastchar + num >= el->edited_line.limit) {
		if (!ch_enlargebufs(el, (size_t)num))
			return;		/* can't go past end of buffer */
	}

	if (el->edited_line.cursor < el->edited_line.lastchar) {
		/* if I must move chars */
		for (cp = el->edited_line.lastchar; cp >= el->edited_line.cursor; cp--)
			cp[num] = *cp;
	}
	el->edited_line.lastchar += num;
}


/* edited_c_delafter():
 *	Delete num characters after the cursor
 */
libedited_private void
edited_c_delafter(Edited *el, int num)
{

	if (el->edited_line.cursor + num > el->edited_line.lastchar)
		num = (int)(el->edited_line.lastchar - el->edited_line.cursor);

	if (el->edited_map.current != el->edited_map.emacs) {
		edited_cv_undo(el);
		edited_cv_yank(el, el->edited_line.cursor, num);
	}

	if (num > 0) {
		wchar_t *cp;

		for (cp = el->edited_line.cursor; cp <= el->edited_line.lastchar; cp++)
			*cp = cp[num];

		el->edited_line.lastchar -= num;
	}
}


/* edited_c_delafter1():
 *	Delete the character after the cursor, do not yank
 */
libedited_private void
edited_c_delafter1(Edited *el)
{
	wchar_t *cp;

	for (cp = el->edited_line.cursor; cp <= el->edited_line.lastchar; cp++)
		*cp = cp[1];

	el->edited_line.lastchar--;
}


/* edited_c_delbefore():
 *	Delete num characters before the cursor
 */
libedited_private void
edited_c_delbefore(Edited *el, int num)
{

	if (el->edited_line.cursor - num < el->edited_line.buffer)
		num = (int)(el->edited_line.cursor - el->edited_line.buffer);

	if (el->edited_map.current != el->edited_map.emacs) {
		edited_cv_undo(el);
		edited_cv_yank(el, el->edited_line.cursor - num, num);
	}

	if (num > 0) {
		wchar_t *cp;

		for (cp = el->edited_line.cursor - num;
		    &cp[num] <= el->edited_line.lastchar;
		    cp++)
			*cp = cp[num];

		el->edited_line.lastchar -= num;
	}
}


/* edited_c_delbefore1():
 *	Delete the character before the cursor, do not yank
 */
libedited_private void
edited_c_delbefore1(Edited *el)
{
	wchar_t *cp;

	for (cp = el->edited_line.cursor - 1; cp <= el->edited_line.lastchar; cp++)
		*cp = cp[1];

	el->edited_line.lastchar--;
}


/* edited_ce__isword():
 *	Return if p is part of a word according to emacs
 */
libedited_private int
edited_ce__isword(wint_t p)
{
	return iswalnum(p) || wcschr(L"*?_-.[]~=", p) != NULL;
}


/* edited_cv__isword():
 *	Return if p is part of a word according to vi
 */
libedited_private int
edited_cv__isword(wint_t p)
{
	if (iswalnum(p) || p == L'_')
		return 1;
	if (iswgraph(p))
		return 2;
	return 0;
}


/* edited_cv__isWord():
 *	Return if p is part of a big word according to vi
 */
libedited_private int
edited_cv__isWord(wint_t p)
{
	return !iswspace(p);
}


/* edited_c__prev_word():
 *	Find the previous word
 */
libedited_private wchar_t *
edited_c__prev_word(wchar_t *p, wchar_t *low, int n, int (*wtest)(wint_t))
{
	p--;

	while (n--) {
		while ((p >= low) && !(*wtest)(*p))
			p--;
		while ((p >= low) && (*wtest)(*p))
			p--;
	}

	/* cp now points to one character before the word */
	p++;
	if (p < low)
		p = low;
	/* cp now points where we want it */
	return p;
}


/* edited_c__next_word():
 *	Find the next word
 */
libedited_private wchar_t *
edited_c__next_word(wchar_t *p, wchar_t *high, int n, int (*wtest)(wint_t))
{
	while (n--) {
		while ((p < high) && !(*wtest)(*p))
			p++;
		while ((p < high) && (*wtest)(*p))
			p++;
	}
	if (p > high)
		p = high;
	/* p now points where we want it */
	return p;
}

/* edited_cv_next_word():
 *	Find the next word vi style
 */
libedited_private wchar_t *
edited_cv_next_word(Edited *el, wchar_t *p, wchar_t *high, int n,
    int (*wtest)(wint_t))
{
	int test;

	while (n--) {
		test = (*wtest)(*p);
		while ((p < high) && (*wtest)(*p) == test)
			p++;
		/*
		 * vi historically deletes with cw only the word preserving the
		 * trailing whitespace! This is not what 'w' does..
		 */
		if (n || el->edited_chared.edited_c_vcmd.action != (DELETE|INSERT))
			while ((p < high) && iswspace(*p))
				p++;
	}

	/* p now points where we want it */
	if (p > high)
		return high;
	else
		return p;
}


/* edited_cv_prev_word():
 *	Find the previous word vi style
 */
libedited_private wchar_t *
edited_cv_prev_word(wchar_t *p, wchar_t *low, int n, int (*wtest)(wint_t))
{
	int test;

	p--;
	while (n--) {
		while ((p > low) && iswspace(*p))
			p--;
		test = (*wtest)(*p);
		while ((p >= low) && (*wtest)(*p) == test)
			p--;
		if (p < low)
			return low;
	}
	p++;

	/* p now points where we want it */
	if (p < low)
		return low;
	else
		return p;
}


/* edited_cv_delfini():
 *	Finish vi delete action
 */
libedited_private void
edited_cv_delfini(Edited *el)
{
	int size;
	int action = el->edited_chared.edited_c_vcmd.action;

	if (action & INSERT)
		el->edited_map.current = el->edited_map.key;

	if (el->edited_chared.edited_c_vcmd.pos == 0)
		/* sanity */
		return;

	size = (int)(el->edited_line.cursor - el->edited_chared.edited_c_vcmd.pos);
	if (size == 0)
		size = 1;
	el->edited_line.cursor = el->edited_chared.edited_c_vcmd.pos;
	if (action & YANK) {
		if (size > 0)
			edited_cv_yank(el, el->edited_line.cursor, size);
		else
			edited_cv_yank(el, el->edited_line.cursor + size, -size);
	} else {
		if (size > 0) {
			edited_c_delafter(el, size);
			edited_re_refresh_cursor(el);
		} else  {
			edited_c_delbefore(el, -size);
			el->edited_line.cursor += size;
		}
	}
	el->edited_chared.edited_c_vcmd.action = NOP;
}


/* edited_cv__endword():
 *	Go to the end of this word according to vi
 */
libedited_private wchar_t *
edited_cv__endword(wchar_t *p, wchar_t *high, int n, int (*wtest)(wint_t))
{
	int test;

	p++;

	while (n--) {
		while ((p < high) && iswspace(*p))
			p++;

		test = (*wtest)(*p);
		while ((p < high) && (*wtest)(*p) == test)
			p++;
	}
	p--;
	return p;
}

/* ch_init():
 *	Initialize the character editor
 */
libedited_private int
ch_init(Edited *el)
{
	el->edited_line.buffer		= edited_calloc(EL_BUFSIZ,
	    sizeof(*el->edited_line.buffer));
	if (el->edited_line.buffer == NULL)
		return -1;

	el->edited_line.cursor		= el->edited_line.buffer;
	el->edited_line.lastchar		= el->edited_line.buffer;
	el->edited_line.limit		= &el->edited_line.buffer[EL_BUFSIZ - EL_LEAVE];

	el->edited_chared.edited_c_undo.buf	= edited_calloc(EL_BUFSIZ,
	    sizeof(*el->edited_chared.edited_c_undo.buf));
	if (el->edited_chared.edited_c_undo.buf == NULL)
		return -1;
	el->edited_chared.edited_c_undo.len	= -1;
	el->edited_chared.edited_c_undo.cursor	= 0;
	el->edited_chared.edited_c_redo.buf	= edited_calloc(EL_BUFSIZ,
	    sizeof(*el->edited_chared.edited_c_redo.buf));
	if (el->edited_chared.edited_c_redo.buf == NULL)
		goto out;
	el->edited_chared.edited_c_redo.pos	= el->edited_chared.edited_c_redo.buf;
	el->edited_chared.edited_c_redo.lim	= el->edited_chared.edited_c_redo.buf + EL_BUFSIZ;
	el->edited_chared.edited_c_redo.cmd	= EDITED_ED_UNASSIGNED;

	el->edited_chared.edited_c_vcmd.action	= NOP;
	el->edited_chared.edited_c_vcmd.pos	= el->edited_line.buffer;

	el->edited_chared.edited_c_kill.buf	= edited_calloc(EL_BUFSIZ,
	    sizeof(*el->edited_chared.edited_c_kill.buf));
	if (el->edited_chared.edited_c_kill.buf == NULL)
		goto out;
	el->edited_chared.edited_c_kill.mark	= el->edited_line.buffer;
	el->edited_chared.edited_c_kill.last	= el->edited_chared.edited_c_kill.buf;
	el->edited_chared.edited_c_resizefun	= NULL;
	el->edited_chared.edited_c_resizearg	= NULL;
	el->edited_chared.edited_c_aliasfun	= NULL;
	el->edited_chared.edited_c_aliasarg	= NULL;

	el->edited_map.current		= el->edited_map.key;

	el->edited_state.inputmode		= MODE_INSERT; /* XXX: save a default */
	el->edited_state.doingarg		= 0;
	el->edited_state.metanext		= 0;
	el->edited_state.argument		= 1;
	el->edited_state.lastcmd		= EDITED_ED_UNASSIGNED;

	return 0;
out:
	ch_end(el);
	return -1;
}

/* ch_reset():
 *	Reset the character editor
 */
libedited_private void
ch_reset(Edited *el)
{
	el->edited_line.cursor		= el->edited_line.buffer;
	el->edited_line.lastchar		= el->edited_line.buffer;

	el->edited_chared.edited_c_undo.len	= -1;
	el->edited_chared.edited_c_undo.cursor	= 0;

	el->edited_chared.edited_c_vcmd.action	= NOP;
	el->edited_chared.edited_c_vcmd.pos	= el->edited_line.buffer;

	el->edited_chared.edited_c_kill.mark	= el->edited_line.buffer;

	el->edited_map.current		= el->edited_map.key;

	el->edited_state.inputmode		= MODE_INSERT; /* XXX: save a default */
	el->edited_state.doingarg		= 0;
	el->edited_state.metanext		= 0;
	el->edited_state.argument		= 1;
	el->edited_state.lastcmd		= EDITED_ED_UNASSIGNED;

	el->edited_history.eventno		= 0;
}

/* ch_enlargebufs():
 *	Enlarge line buffer to be able to hold twice as much characters.
 *	Returns 1 if successful, 0 if not.
 */
libedited_private int
ch_enlargebufs(Edited *el, size_t addlen)
{
	size_t sz, newsz;
	wchar_t *newbuffer, *oldbuf, *oldkbuf;

	sz = (size_t)(el->edited_line.limit - el->edited_line.buffer + EL_LEAVE);
	newsz = sz * 2;
	/*
	 * If newly required length is longer than current buffer, we need
	 * to make the buffer big enough to hold both old and new stuff.
	 */
	if (addlen > sz) {
		while(newsz - sz < addlen)
			newsz *= 2;
	}

	/*
	 * Reallocate line buffer.
	 */
	newbuffer = edited_realloc(el->edited_line.buffer, newsz * sizeof(*newbuffer));
	if (!newbuffer)
		return 0;

	/* zero the newly added memory, leave old data in */
	(void) memset(&newbuffer[sz], 0, (newsz - sz) * sizeof(*newbuffer));

	oldbuf = el->edited_line.buffer;

	el->edited_line.buffer = newbuffer;
	el->edited_line.cursor = newbuffer + (el->edited_line.cursor - oldbuf);
	el->edited_line.lastchar = newbuffer + (el->edited_line.lastchar - oldbuf);
	/* don't set new size until all buffers are enlarged */
	el->edited_line.limit  = &newbuffer[sz - EL_LEAVE];

	/*
	 * Reallocate kill buffer.
	 */
	newbuffer = edited_realloc(el->edited_chared.edited_c_kill.buf, newsz *
	    sizeof(*newbuffer));
	if (!newbuffer)
		return 0;

	/* zero the newly added memory, leave old data in */
	(void) memset(&newbuffer[sz], 0, (newsz - sz) * sizeof(*newbuffer));

	oldkbuf = el->edited_chared.edited_c_kill.buf;

	el->edited_chared.edited_c_kill.buf = newbuffer;
	el->edited_chared.edited_c_kill.last = newbuffer +
					(el->edited_chared.edited_c_kill.last - oldkbuf);
	el->edited_chared.edited_c_kill.mark = el->edited_line.buffer +
					(el->edited_chared.edited_c_kill.mark - oldbuf);

	/*
	 * Reallocate undo buffer.
	 */
	newbuffer = edited_realloc(el->edited_chared.edited_c_undo.buf,
	    newsz * sizeof(*newbuffer));
	if (!newbuffer)
		return 0;

	/* zero the newly added memory, leave old data in */
	(void) memset(&newbuffer[sz], 0, (newsz - sz) * sizeof(*newbuffer));
	el->edited_chared.edited_c_undo.buf = newbuffer;

	newbuffer = edited_realloc(el->edited_chared.edited_c_redo.buf,
	    newsz * sizeof(*newbuffer));
	if (!newbuffer)
		return 0;
	el->edited_chared.edited_c_redo.pos = newbuffer +
			(el->edited_chared.edited_c_redo.pos - el->edited_chared.edited_c_redo.buf);
	el->edited_chared.edited_c_redo.lim = newbuffer +
			(el->edited_chared.edited_c_redo.lim - el->edited_chared.edited_c_redo.buf);
	el->edited_chared.edited_c_redo.buf = newbuffer;

	if (!hist_enlargebuf(el, sz, newsz))
		return 0;

	/* Safe to set enlarged buffer size */
	el->edited_line.limit  = &el->edited_line.buffer[newsz - EL_LEAVE];
	if (el->edited_chared.edited_c_resizefun)
		(*el->edited_chared.edited_c_resizefun)(el, el->edited_chared.edited_c_resizearg);
	return 1;
}

/* ch_end():
 *	Free the data structures used by the editor
 */
libedited_private void
ch_end(Edited *el)
{
	edited_free(el->edited_line.buffer);
	el->edited_line.buffer = NULL;
	el->edited_line.limit = NULL;
	edited_free(el->edited_chared.edited_c_undo.buf);
	el->edited_chared.edited_c_undo.buf = NULL;
	edited_free(el->edited_chared.edited_c_redo.buf);
	el->edited_chared.edited_c_redo.buf = NULL;
	el->edited_chared.edited_c_redo.pos = NULL;
	el->edited_chared.edited_c_redo.lim = NULL;
	el->edited_chared.edited_c_redo.cmd = EDITED_ED_UNASSIGNED;
	edited_free(el->edited_chared.edited_c_kill.buf);
	el->edited_chared.edited_c_kill.buf = NULL;
	ch_reset(el);
}


/* edited_insertstr():
 *	Insert string at cursor
 */
int
edited_winsertstr(Edited *el, const wchar_t *s)
{
	size_t len;

	if (s == NULL || (len = wcslen(s)) == 0)
		return -1;
	if (el->edited_line.lastchar + len >= el->edited_line.limit) {
		if (!ch_enlargebufs(el, len))
			return -1;
	}

	edited_c_insert(el, (int)len);
	while (*s)
		*el->edited_line.cursor++ = *s++;
	return 0;
}


/* edited_deletestr():
 *	Delete num characters before the cursor
 */
void
edited_deletestr(Edited *el, int n)
{
	if (n <= 0)
		return;

	if (el->edited_line.cursor < &el->edited_line.buffer[n])
		return;

	edited_c_delbefore(el, n);		/* delete before dot */
	el->edited_line.cursor -= n;
	if (el->edited_line.cursor < el->edited_line.buffer)
		el->edited_line.cursor = el->edited_line.buffer;
}

/* edited_deletestr1():
 *	Delete characters between start and end
 */
int
edited_deletestr1(Edited *el, int start, int end)
{
	size_t line_length, len;
	wchar_t *p1, *p2;

	if (end <= start)
		return 0;

	line_length = (size_t)(el->edited_line.lastchar - el->edited_line.buffer);

	if (start >= (int)line_length || end >= (int)line_length)
		return 0;

	len = (size_t)(end - start);
	if (len > line_length - (size_t)end)
		len = line_length - (size_t)end;

	p1 = el->edited_line.buffer + start;
	p2 = el->edited_line.buffer + end;
	for (size_t i = 0; i < len; i++) {
		*p1++ = *p2++;
		el->edited_line.lastchar--;
	}

	if (el->edited_line.cursor < el->edited_line.buffer)
		el->edited_line.cursor = el->edited_line.buffer;

	return end - start;
}

/* edited_wreplacestr():
 *	Replace the contents of the line with the provided string
 */
int
edited_wreplacestr(Edited *el, const wchar_t *s)
{
	size_t len;
	wchar_t * p;

	if (s == NULL || (len = wcslen(s)) == 0)
		return -1;

	if (el->edited_line.buffer + len >= el->edited_line.limit) {
		if (!ch_enlargebufs(el, len))
			return -1;
	}

	p = el->edited_line.buffer;
	for (size_t i = 0; i < len; i++)
		*p++ = *s++;

	el->edited_line.buffer[len] = '\0';
	el->edited_line.lastchar = el->edited_line.buffer + len;
	if (el->edited_line.cursor > el->edited_line.lastchar)
		el->edited_line.cursor = el->edited_line.lastchar;

	return 0;
}

/* edited_cursor():
 *	Move the cursor to the left or the right of the current position
 */
int
edited_cursor(Edited *el, int n)
{
	if (n == 0)
		goto out;

	el->edited_line.cursor += n;

	if (el->edited_line.cursor < el->edited_line.buffer)
		el->edited_line.cursor = el->edited_line.buffer;
	if (el->edited_line.cursor > el->edited_line.lastchar)
		el->edited_line.cursor = el->edited_line.lastchar;
out:
	return (int)(el->edited_line.cursor - el->edited_line.buffer);
}

/* edited_c_gets():
 *	Get a string
 */
libedited_private int
edited_c_gets(Edited *el, wchar_t *buf, const wchar_t *prompt)
{
	ssize_t len;
	wchar_t *cp = el->edited_line.buffer, ch;

	if (prompt) {
		len = (ssize_t)wcslen(prompt);
		(void)memcpy(cp, prompt, (size_t)len * sizeof(*cp));
		cp += len;
	}
	len = 0;

	for (;;) {
		el->edited_line.cursor = cp;
		*cp = ' ';
		el->edited_line.lastchar = cp + 1;
		edited_re_refresh(el);

		if (edited_wgetc(el, &ch) != 1) {
			edited_ed_end_of_file(el, 0);
			len = -1;
			break;
		}

		switch (ch) {

		case L'\b':	/* Delete and backspace */
		case 0177:
			if (len == 0) {
				len = -1;
				break;
			}
			len--;
			cp--;
			continue;

		case 0033:	/* ESC */
		case L'\r':	/* Newline */
		case L'\n':
			buf[len] = ch;
			break;

		default:
			if (len >= (ssize_t)(EL_BUFSIZ - 16))
				edited_term_beep(el);
			else {
				buf[len++] = ch;
				*cp++ = ch;
			}
			continue;
		}
		break;
	}

	el->edited_line.buffer[0] = '\0';
	el->edited_line.lastchar = el->edited_line.buffer;
	el->edited_line.cursor = el->edited_line.buffer;
	return (int)len;
}


/* edited_c_hpos():
 *	Return the current horizontal position of the cursor
 */
libedited_private int
edited_c_hpos(Edited *el)
{
	wchar_t *ptr;

	/*
	 * Find how many characters till the beginning of this line.
	 */
	if (el->edited_line.cursor == el->edited_line.buffer)
		return 0;
	else {
		for (ptr = el->edited_line.cursor - 1;
		     ptr >= el->edited_line.buffer && *ptr != '\n';
		     ptr--)
			continue;
		return (int)(el->edited_line.cursor - ptr - 1);
	}
}

libedited_private int
ch_resizefun(Edited *el, edited_zfunc_t f, void *a)
{
	el->edited_chared.edited_c_resizefun = f;
	el->edited_chared.edited_c_resizearg = a;
	return 0;
}

libedited_private int
ch_aliasfun(Edited *el, edited_afunc_t f, void *a)
{
	el->edited_chared.edited_c_aliasfun = f;
	el->edited_chared.edited_c_aliasarg = a;
	return 0;
}
