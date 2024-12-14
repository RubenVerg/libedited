/*	$NetBSD: refresh.c,v 1.59 2024/06/30 17:11:27 christos Exp $	*/

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
static char sccsid[] = "@(#)refresh.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: refresh.c,v 1.59 2024/06/30 17:11:27 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * refresh.c: Lower level screen refreshing functions
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "edited/el.h"

static void	edited_re_nextline(Edited *);
static void	edited_re_addc(Edited *, wint_t);
static void	edited_re_update_line(Edited *, wchar_t *, wchar_t *, int);
static void	edited_re_insert (Edited *, wchar_t *, int, int, wchar_t *, int);
static void	edited_re_delete(Edited *, wchar_t *, int, int, int);
static void	edited_re_fastputc(Edited *, wint_t);
static void	edited_re_clear_eol(Edited *, int, int, int);
static void	edited_re__strncopy(wchar_t *, wchar_t *, size_t);
static void	edited_re__copy_and_pad(wchar_t *, const wchar_t *, size_t);
static void edited_re_addc_styled(Edited *, wint_t, edited_style_t);
static void edited_re_putc_styled(Edited *, wint_t, edited_style_t);

#ifdef DEBUG_REFRESH
static void	edited_re_printstr(EditLine *, const char *, wchar_t *, wchar_t *);
#define	__F el->edited_errfile
#define	ELRE_ASSERT(a, b, c)	do				\
				    if (/*CONSTCOND*/ a) {	\
					(void) fprintf b;	\
					c;			\
				    }				\
				while (/*CONSTCOND*/0)
#define	ELRE_DEBUG(a, b)	ELRE_ASSERT(a,b,;)

/* edited_re_printstr():
 *	Print a string on the debugging pty
 */
static void
edited_re_printstr(EditLine *el, const char *str, wchar_t *f, wchar_t *t)
{

	ELRE_DEBUG(1, (__F, "%s:\"", str));
	while (f < t)
		ELRE_DEBUG(1, (__F, "%c", *f++ & 0177));
	ELRE_DEBUG(1, (__F, "\"\r\n"));
}
#else
#define	ELRE_ASSERT(a, b, c)
#define	ELRE_DEBUG(a, b)
#endif

/* edited_re_nextline():
 *	Move to the next line or scroll
 */
static void
edited_re_nextline(Edited *el)
{
	el->edited_refresh.r_cursor.h = 0;	/* reset it. */

	/*
	 * If we would overflow (input is longer than terminal size),
	 * emulate scroll by dropping first line and shuffling the rest.
	 * We do this via pointer shuffling - it's safe in this case
	 * and we avoid memcpy().
	 */
	if (el->edited_refresh.r_cursor.v + 1 >= el->edited_terminal.t_size.v) {
		int i, lins = el->edited_terminal.t_size.v;
		wint_t *firstline = el->edited_vdisplay[0];

		for(i = 1; i < lins; i++)
			el->edited_vdisplay[i - 1] = el->edited_vdisplay[i];

		firstline[0] = '\0';		/* empty the string */
		el->edited_vdisplay[i - 1] = firstline;
	} else
		el->edited_refresh.r_cursor.v++;

	ELRE_ASSERT(el->edited_refresh.r_cursor.v >= el->edited_terminal.t_size.v,
	    (__F, "\r\nre_putc: overflow! r_cursor.v == %d > %d\r\n",
	    el->edited_refresh.r_cursor.v, el->edited_terminal.t_size.v),
	    abort());
}

/* edited_re_addc():
 *	Draw c, expanding tabs, control chars etc.
 */
static void
edited_re_addc(Edited *el, wint_t c)
{
	switch (edited_ct_chr_class(c)) {
	case CHTYPE_TAB:        /* expand the tab */
		for (;;) {
			edited_re_putc(el, ' ', 1);
			if ((el->edited_refresh.r_cursor.h & 07) == 0)
				break;			/* go until tab stop */
		}
		break;
	case CHTYPE_NL: {
		int oldv = el->edited_refresh.r_cursor.v;
		edited_re_putc(el, '\0', 0);			/* assure end of line */
		if (oldv == el->edited_refresh.r_cursor.v)	/* XXX */
			edited_re_nextline(el);
		break;
	}
	case CHTYPE_PRINT:
		edited_re_putc(el, c, 1);
		break;
	default: {
		wchar_t visbuf[VISUAL_WIDTH_MAX];
		ssize_t i, n =
		    edited_ct_visual_char(visbuf, VISUAL_WIDTH_MAX, c);
		for (i = 0; n-- > 0; ++i)
		    edited_re_putc(el, visbuf[i], 1);
		break;
	}
	}
}

/* edited_re_addc_styled():
 */

libedited_private void
edited_re_addc_styled(Edited *el, wint_t c, edited_style_t style)
{
	switch (edited_ct_chr_class(c)) {
	case CHTYPE_PRINT:
		edited_re_putc_styled(el, c, style);
		break;
	default:
		edited_re_addc(el, c);
		break;
	}
}

/* edited_re_putliteral():
 *	Place the literal string given
 */
libedited_private void
edited_re_putliteral(Edited *el, const wchar_t *begin, const wchar_t *end)
{
	coord_t *cur = &el->edited_refresh.r_cursor;
	wint_t c;
	int sizeh = el->edited_terminal.t_size.h;
	int i, w;

	c = edited_lit_add(el, begin, end, &w);
	if (c == 0 || w <= 0)
		return;
	el->edited_vdisplay[cur->v][cur->h] = c;

	i = w;
	if (i > sizeh - cur->h)		/* avoid overflow */
		i = sizeh - cur->h;
	while (--i > 0)
		el->edited_vdisplay[cur->v][cur->h + i] = MB_FILL_CHAR;

	cur->h += w;
	if (cur->h >= sizeh) {
		/* assure end of line */
		el->edited_vdisplay[cur->v][sizeh] = '\0';
		edited_re_nextline(el);
	}
}

/* edited_re_putc():
 *	Draw the character given
 */
libedited_private void
edited_re_putc(Edited *el, wint_t c, int shift)
{
	coord_t *cur = &el->edited_refresh.r_cursor;
	int i, w = wcwidth(c);
	int sizeh = el->edited_terminal.t_size.h;

	ELRE_DEBUG(1, (__F, "printing %5x '%lc'\r\n", c, c));
	if (w == -1)
		w = 0;

	while (shift && (cur->h + w > sizeh))
	    edited_re_putc(el, ' ', 1);

	el->edited_vdisplay[cur->v][cur->h] = c;
	/* assumes !shift is only used for single-column chars */
	i = w;
	while (--i > 0)
		el->edited_vdisplay[cur->v][cur->h + i] = MB_FILL_CHAR;

	if (!shift)
		return;

	cur->h += w;	/* advance to next place */
	if (cur->h >= sizeh) {
		/* assure end of line */
		el->edited_vdisplay[cur->v][sizeh] = '\0';
		edited_re_nextline(el);
	}
}

/* edited_re_putc_styled():
 */
static void
edited_re_putc_styled(Edited *el, wint_t c, edited_style_t style)
{
	el->edited_vstyle[el->edited_refresh.r_cursor.v][el->edited_refresh.r_cursor.h] = style;
	edited_re_putc(el, c, 1);
}


/* edited_re_refresh():
 *	draws the new virtual screen image from the current input
 *	line, then goes line-by-line changing the real image to the new
 *	virtual image. The routine to re-draw a line can be replaced
 *	easily in hopes of a smarter one being placed there.
 */
libedited_private void
edited_re_refresh(Edited *el)
{
	int i, rhdiff;
	wchar_t *cp, *st;
	coord_t cur;
#ifdef notyet
	size_t termsz;
#endif
	int use_style = el->edited_use_style && el->edited_style_func != NULL;
	edited_style_t *styled_line;

	ELRE_DEBUG(1, (__F, "el->edited_line.buffer = :%ls:\r\n",
	    el->edited_line.buffer));

	edited_lit_clear(el);
	/* reset the Drawing cursor */
	el->edited_refresh.r_cursor.h = 0;
	el->edited_refresh.r_cursor.v = 0;

	edited_term_move_to_char(el, 0);

	/* temporarily draw rprompt to calculate its size */
	edited_prompt_print(el, EL_RPROMPT);

	/* reset the Drawing cursor */
	el->edited_refresh.r_cursor.h = 0;
	el->edited_refresh.r_cursor.v = 0;

	if (el->edited_line.cursor >= el->edited_line.lastchar) {
		if (el->edited_map.current == el->edited_map.alt
		    && el->edited_line.lastchar != el->edited_line.buffer)
			el->edited_line.cursor = el->edited_line.lastchar - 1;
		else
			el->edited_line.cursor = el->edited_line.lastchar;
	}

	cur.h = -1;		/* set flag in case I'm not set */
	cur.v = 0;

	edited_prompt_print(el, EL_PROMPT);

	/* draw the current input buffer */
#if notyet
	termsz = el->edited_terminal.t_size.h * el->edited_terminal.t_size.v;
	if (el->edited_line.lastchar - el->edited_line.buffer > termsz) {
		/*
		 * If line is longer than terminal, process only part
		 * of line which would influence display.
		 */
		size_t rem = (el->edited_line.lastchar-el->edited_line.buffer)%termsz;

		st = el->edited_line.lastchar - rem
			- (termsz - (((rem / el->edited_terminal.t_size.v) - 1)
					* el->edited_terminal.t_size.v));
	} else
#endif
	st = el->edited_line.buffer;

	if (use_style) {
		el->edited_vstyle = malloc(sizeof(edited_style_t *) * el->edited_terminal.t_size.v);
		for (i = 0; i < el->edited_terminal.t_size.v; i++) {
			el->edited_vstyle[i] = malloc(sizeof(edited_style_t) * el->edited_terminal.t_size.h);
			memset(el->edited_vstyle[i], 0, sizeof(edited_style_t) * el->edited_terminal.t_size.h);
		}
		styled_line = malloc(sizeof(edited_style_t) * (el->edited_line.lastchar - el->edited_line.buffer));
		el->edited_style_func(el, el->edited_line.lastchar - el->edited_line.buffer, st, styled_line);
	}

	for (cp = st; cp < el->edited_line.lastchar; cp++, styled_line++) {
		if (cp == el->edited_line.cursor) {
                        int w = wcwidth(*cp);
			/* save for later */
			cur.h = el->edited_refresh.r_cursor.h;
			cur.v = el->edited_refresh.r_cursor.v;
                        /* handle being at a linebroken doublewidth char */
                        if (w > 1 && el->edited_refresh.r_cursor.h + w >
			    el->edited_terminal.t_size.h) {
				cur.h = 0;
				cur.v++;
                        }
		}
		if (use_style) edited_re_addc_styled(el, *cp, *styled_line);
		else edited_re_addc(el, *cp);
	}

	if (cur.h == -1) {	/* if I haven't been set yet, I'm at the end */
		cur.h = el->edited_refresh.r_cursor.h;
		cur.v = el->edited_refresh.r_cursor.v;
	}
	rhdiff = el->edited_terminal.t_size.h - el->edited_refresh.r_cursor.h -
	    el->edited_rprompt.p_pos.h;
	if (el->edited_rprompt.p_pos.h && !el->edited_rprompt.p_pos.v &&
	    !el->edited_refresh.r_cursor.v && rhdiff > 1) {
		/*
		 * have a right-hand side prompt that will fit
		 * on the end of the first line with at least
		 * one character gap to the input buffer.
		 */
		while (--rhdiff > 0)	/* pad out with spaces */
			edited_re_putc(el, ' ', 1);
		edited_prompt_print(el, EL_RPROMPT);
	} else {
		el->edited_rprompt.p_pos.h = 0;	/* flag "not using rprompt" */
		el->edited_rprompt.p_pos.v = 0;
	}

	edited_re_putc(el, '\0', 0);	/* make line ended with NUL, no cursor shift */

	el->edited_refresh.r_newcv = el->edited_refresh.r_cursor.v;

	ELRE_DEBUG(1, (__F,
		"term.h=%d vcur.h=%d vcur.v=%d vdisplay[0]=\r\n:%80.80s:\r\n",
		el->edited_terminal.t_size.h, el->edited_refresh.r_cursor.h,
		el->edited_refresh.r_cursor.v, edited_ct_encode_string(el->edited_vdisplay[0],
		&el->edited_scratch)));

	ELRE_DEBUG(1, (__F, "updating %d lines.\r\n", el->edited_refresh.r_newcv));
	for (i = 0; i <= el->edited_refresh.r_newcv; i++) {
		/* NOTE THAT edited_re_update_line MAY CHANGE edited_display[i] */
		edited_re_update_line(el, (wchar_t *)el->edited_display[i],
		    (wchar_t *)el->edited_vdisplay[i], i);

		/*
		 * Copy the new line to be the current one, and pad out with
		 * spaces to the full width of the terminal so that if we try
		 * moving the cursor by writing the character that is at the
		 * end of the screen line, it won't be a NUL or some old
		 * leftover stuff.
		 */
		edited_re__copy_and_pad((wchar_t *)el->edited_display[i],
		    (wchar_t *)el->edited_vdisplay[i],
		    (size_t) el->edited_terminal.t_size.h);
	}
	ELRE_DEBUG(1, (__F,
	"\r\nel->edited_refresh.r_cursor.v=%d,el->edited_refresh.r_oldcv=%d i=%d\r\n",
	    el->edited_refresh.r_cursor.v, el->edited_refresh.r_oldcv, i));

	if (el->edited_refresh.r_oldcv > el->edited_refresh.r_newcv)
		for (; i <= el->edited_refresh.r_oldcv; i++) {
			edited_term_move_to_line(el, i);
			edited_term_move_to_char(el, 0);
                        /* This wcslen should be safe even with MB_FILL_CHARs */
			edited_term_clear_EOL(el,
			    (int) wcslen((const wchar_t *)el->edited_display[i]));
#ifdef DEBUG_REFRESH
			edited_term_overwrite(el, L"C\b", 2);
#endif /* DEBUG_REFRESH */
			el->edited_display[i][0] = '\0';
		}

	el->edited_refresh.r_oldcv = el->edited_refresh.r_newcv; /* set for next time */
	ELRE_DEBUG(1, (__F,
	    "\r\ncursor.h = %d, cursor.v = %d, cur.h = %d, cur.v = %d\r\n",
	    el->edited_refresh.r_cursor.h, el->edited_refresh.r_cursor.v,
	    cur.h, cur.v));
	edited_term_move_to_line(el, cur.v);	/* go to where the cursor is */
	edited_term_move_to_char(el, cur.h);

	if (use_style) {
		for (i = 0; i < el->edited_terminal.t_size.v; i++)
			free(el->edited_vstyle[i]);
		free(el->edited_vstyle);
	}
}


/* edited_re_goto_bottom():
 *	 used to go to last used screen line
 */
libedited_private void
edited_re_goto_bottom(Edited *el)
{

	edited_term_move_to_line(el, el->edited_refresh.r_oldcv);
	edited_term__putc(el, '\n');
	edited_re_clear_display(el);
	edited_term__flush(el);
}


/* edited_re_insert():
 *	insert num characters of s into d (in front of the character)
 *	at dat, maximum length of d is dlen
 */
static void
/*ARGSUSED*/
edited_re_insert(Edited *el __attribute__((__unused__)),
    wchar_t *d, int dat, int dlen, wchar_t *s, int num)
{
	wchar_t *a, *b;

	if (num <= 0)
		return;
	if (num > dlen - dat)
		num = dlen - dat;

	ELRE_DEBUG(1,
	    (__F, "edited_re_insert() starting: %d at %d max %d, d == \"%s\"\n",
	    num, dat, dlen, edited_ct_encode_string(d, &el->edited_scratch)));
	ELRE_DEBUG(1, (__F, "s == \"%s\"\n", edited_ct_encode_string(s,
	    &el->edited_scratch)));

	/* open up the space for num chars */
	if (num > 0) {
		b = d + dlen - 1;
		a = b - num;
		while (a >= &d[dat])
			*b-- = *a--;
		d[dlen] = '\0';	/* just in case */
	}

	ELRE_DEBUG(1, (__F,
		"edited_re_insert() after insert: %d at %d max %d, d == \"%s\"\n",
		num, dat, dlen, edited_ct_encode_string(d, &el->edited_scratch)));
	ELRE_DEBUG(1, (__F, "s == \"%s\"\n", edited_ct_encode_string(s,
		&el->edited_scratch)));

	/* copy the characters */
	for (a = d + dat; (a < d + dlen) && (num > 0); num--)
		*a++ = *s++;

#ifdef notyet
        /* edited_ct_encode_string() uses a static buffer, so we can't conveniently
         * encode both d & s here */
	ELRE_DEBUG(1,
	    (__F, "edited_re_insert() after copy: %d at %d max %d, %s == \"%s\"\n",
	    num, dat, dlen, d, s));
	ELRE_DEBUG(1, (__F, "s == \"%s\"\n", s));
#endif
}


/* edited_re_delete():
 *	delete num characters d at dat, maximum length of d is dlen
 */
static void
/*ARGSUSED*/
edited_re_delete(Edited *el __attribute__((__unused__)),
    wchar_t *d, int dat, int dlen, int num)
{
	wchar_t *a, *b;

	if (num <= 0)
		return;
	if (dat + num >= dlen) {
		d[dat] = '\0';
		return;
	}
	ELRE_DEBUG(1,
	    (__F, "edited_re_delete() starting: %d at %d max %d, d == \"%s\"\n",
	    num, dat, dlen, edited_ct_encode_string(d, &el->edited_scratch)));

	/* open up the space for num chars */
	if (num > 0) {
		b = d + dat;
		a = b + num;
		while (a < &d[dlen])
			*b++ = *a++;
		d[dlen] = '\0';	/* just in case */
	}
	ELRE_DEBUG(1,
	    (__F, "edited_re_delete() after delete: %d at %d max %d, d == \"%s\"\n",
	    num, dat, dlen, edited_ct_encode_string(d, &el->edited_scratch)));
}


/* edited_re__strncopy():
 *	Like strncpy without padding.
 */
static void
edited_re__strncopy(wchar_t *a, wchar_t *b, size_t n)
{

	while (n-- && *b)
		*a++ = *b++;
}

/* edited_re_clear_eol():
 *	Find the number of characters we need to clear till the end of line
 *	in order to make sure that we have cleared the previous contents of
 *	the line. fx and sx is the number of characters inserted or deleted
 *	in the first or second diff, diff is the difference between the
 *	number of characters between the new and old line.
 */
static void
edited_re_clear_eol(Edited *el, int fx, int sx, int diff)
{

	ELRE_DEBUG(1, (__F, "edited_re_clear_eol sx %d, fx %d, diff %d\n",
	    sx, fx, diff));

	if (fx < 0)
		fx = -fx;
	if (sx < 0)
		sx = -sx;
	if (fx > diff)
		diff = fx;
	if (sx > diff)
		diff = sx;

	ELRE_DEBUG(1, (__F, "edited_re_clear_eol %d\n", diff));
	edited_term_clear_EOL(el, diff);
}

/*****************************************************************
    edited_re_update_line() is based on finding the middle difference of each line
    on the screen; vis:

			     /old first difference
	/beginning of line   |              /old last same       /old EOL
	v		     v              v                    v
old:	eddie> Oh, my little gruntle-buggy is to me, as lurgid as
new:	eddie> Oh, my little buggy says to me, as lurgid as
	^		     ^        ^			   ^
	\beginning of line   |        \new last same	   \new end of line
			     \new first difference

    all are character pointers for the sake of speed.  Special cases for
    no differences, as well as for end of line additions must be handled.
**************************************************************** */

/* Minimum at which doing an insert it "worth it".  This should be about
 * half the "cost" of going into insert mode, inserting a character, and
 * going back out.  This should really be calculated from the termcap
 * data...  For the moment, a good number for ANSI terminals.
 */
#define	MIN_END_KEEP	4

static void
edited_re_update_line(Edited *el, wchar_t *old, wchar_t *new, int i)
{
	wchar_t *o, *n, *p, c;
	wchar_t *ofd, *ols, *oe, *nfd, *nls, *ne;
	wchar_t *osb, *ose, *nsb, *nse;
	int fx, sx;
	size_t len;

	if (el->edited_use_style) {
		edited_term_move_to_char(el, 0);
		edited_term_clear_EOL(el, el->edited_terminal.t_size.h - 1);
#define _le_min(a, b) ((a) < (b) ? (a) : (b))
		edited_term_overwrite_styled(el, new, _le_min(wcslen(new), el->edited_terminal.t_size.h - 1), el->edited_vstyle[el->edited_refresh.r_cursor.v]);
#undef _le_min
		return;
	}

	/*
         * find first diff
         */
	for (o = old, n = new; *o && (*o == *n); o++, n++)
		continue;
	ofd = o;
	nfd = n;

	/*
         * Find the end of both old and new
         */
	while (*o)
		o++;
	/*
         * Remove any trailing blanks off of the end, being careful not to
         * back up past the beginning.
         */
	while (ofd < o) {
		if (o[-1] != ' ')
			break;
		o--;
	}
	oe = o;
	*oe = '\0';

	while (*n)
		n++;

	/* remove blanks from end of new */
	while (nfd < n) {
		if (n[-1] != ' ')
			break;
		n--;
	}
	ne = n;
	*ne = '\0';

	/*
         * if no diff, continue to next line of redraw
         */
	if (*ofd == '\0' && *nfd == '\0') {
		ELRE_DEBUG(1, (__F, "no difference.\r\n"));
		return;
	}
	/*
         * find last same pointer
         */
	while ((o > ofd) && (n > nfd) && (*--o == *--n))
		continue;
	ols = ++o;
	nls = ++n;

	/*
         * find same beginning and same end
         */
	osb = ols;
	nsb = nls;
	ose = ols;
	nse = nls;

	/*
         * case 1: insert: scan from nfd to nls looking for *ofd
         */
	if (*ofd) {
		for (c = *ofd, n = nfd; n < nls; n++) {
			if (c == *n) {
				for (o = ofd, p = n;
				    p < nls && o < ols && *o == *p;
				    o++, p++)
					continue;
				/*
				 * if the new match is longer and it's worth
				 * keeping, then we take it
				 */
				if (((nse - nsb) < (p - n)) &&
				    (2 * (p - n) > n - nfd)) {
					nsb = n;
					nse = p;
					osb = ofd;
					ose = o;
				}
			}
		}
	}
	/*
         * case 2: delete: scan from ofd to ols looking for *nfd
         */
	if (*nfd) {
		for (c = *nfd, o = ofd; o < ols; o++) {
			if (c == *o) {
				for (n = nfd, p = o;
				    p < ols && n < nls && *p == *n;
				    p++, n++)
					continue;
				/*
				 * if the new match is longer and it's worth
				 * keeping, then we take it
				 */
				if (((ose - osb) < (p - o)) &&
				    (2 * (p - o) > o - ofd)) {
					nsb = nfd;
					nse = n;
					osb = o;
					ose = p;
				}
			}
		}
	}
	/*
         * Pragmatics I: If old trailing whitespace or not enough characters to
         * save to be worth it, then don't save the last same info.
         */
	if ((oe - ols) < MIN_END_KEEP) {
		ols = oe;
		nls = ne;
	}
	/*
         * Pragmatics II: if the terminal isn't smart enough, make the data
         * dumber so the smart update doesn't try anything fancy
         */

	/*
         * fx is the number of characters we need to insert/delete: in the
         * beginning to bring the two same begins together
         */
	fx = (int)((nsb - nfd) - (osb - ofd));
	/*
         * sx is the number of characters we need to insert/delete: in the
         * end to bring the two same last parts together
         */
	sx = (int)((nls - nse) - (ols - ose));

	if (!EL_CAN_INSERT) {
		if (fx > 0) {
			osb = ols;
			ose = ols;
			nsb = nls;
			nse = nls;
		}
		if (sx > 0) {
			ols = oe;
			nls = ne;
		}
		if ((ols - ofd) < (nls - nfd)) {
			ols = oe;
			nls = ne;
		}
	}
	if (!EL_CAN_DELETE) {
		if (fx < 0) {
			osb = ols;
			ose = ols;
			nsb = nls;
			nse = nls;
		}
		if (sx < 0) {
			ols = oe;
			nls = ne;
		}
		if ((ols - ofd) > (nls - nfd)) {
			ols = oe;
			nls = ne;
		}
	}
	/*
         * Pragmatics III: make sure the middle shifted pointers are correct if
         * they don't point to anything (we may have moved ols or nls).
         */
	/* if the change isn't worth it, don't bother */
	/* was: if (osb == ose) */
	if ((ose - osb) < MIN_END_KEEP) {
		osb = ols;
		ose = ols;
		nsb = nls;
		nse = nls;
	}
	/*
         * Now that we are done with pragmatics we recompute fx, sx
         */
	fx = (int)((nsb - nfd) - (osb - ofd));
	sx = (int)((nls - nse) - (ols - ose));

	ELRE_DEBUG(1, (__F, "fx %d, sx %d\n", fx, sx));
	ELRE_DEBUG(1, (__F, "ofd %td, osb %td, ose %td, ols %td, oe %td\n",
		ofd - old, osb - old, ose - old, ols - old, oe - old));
	ELRE_DEBUG(1, (__F, "nfd %td, nsb %td, nse %td, nls %td, ne %td\n",
		nfd - new, nsb - new, nse - new, nls - new, ne - new));
	ELRE_DEBUG(1, (__F,
		"xxx-xxx:\"00000000001111111111222222222233333333334\"\r\n"));
	ELRE_DEBUG(1, (__F,
		"xxx-xxx:\"01234567890123456789012345678901234567890\"\r\n"));
#ifdef DEBUG_REFRESH
	edited_re_printstr(el, "old- oe", old, oe);
	edited_re_printstr(el, "new- ne", new, ne);
	edited_re_printstr(el, "old-ofd", old, ofd);
	edited_re_printstr(el, "new-nfd", new, nfd);
	edited_re_printstr(el, "ofd-osb", ofd, osb);
	edited_re_printstr(el, "nfd-nsb", nfd, nsb);
	edited_re_printstr(el, "osb-ose", osb, ose);
	edited_re_printstr(el, "nsb-nse", nsb, nse);
	edited_re_printstr(el, "ose-ols", ose, ols);
	edited_re_printstr(el, "nse-nls", nse, nls);
	edited_re_printstr(el, "ols- oe", ols, oe);
	edited_re_printstr(el, "nls- ne", nls, ne);
#endif /* DEBUG_REFRESH */

	/*
         * edited_cursor.v to this line i MUST be in this routine so that if we
         * don't have to change the line, we don't move to it. edited_cursor.h to
         * first diff char
         */
	edited_term_move_to_line(el, i);

	/*
         * at this point we have something like this:
         *
         * /old                  /ofd    /osb               /ose    /ols     /oe
         * v.....................v       v..................v       v........v
         * eddie> Oh, my fredded gruntle-buggy is to me, as foo var lurgid as
         * eddie> Oh, my fredded quiux buggy is to me, as gruntle-lurgid as
         * ^.....................^     ^..................^       ^........^
         * \new                  \nfd  \nsb               \nse     \nls    \ne
         *
         * fx is the difference in length between the chars between nfd and
         * nsb, and the chars between ofd and osb, and is thus the number of
         * characters to delete if < 0 (new is shorter than old, as above),
         * or insert (new is longer than short).
         *
         * sx is the same for the second differences.
         */

	/*
         * if we have a net insert on the first difference, AND inserting the
         * net amount ((nsb-nfd) - (osb-ofd)) won't push the last useful
         * character (which is ne if nls != ne, otherwise is nse) off the edge
	 * of the screen (el->edited_terminal.t_size.h) else we do the deletes first
	 * so that we keep everything we need to.
         */

	/*
         * if the last same is the same like the end, there is no last same
         * part, otherwise we want to keep the last same part set p to the
         * last useful old character
         */
	p = (ols != oe) ? oe : ose;

	/*
         * if (There is a diffence in the beginning) && (we need to insert
         *   characters) && (the number of characters to insert is less than
         *   the term width)
	 *	We need to do an insert!
	 * else if (we need to delete characters)
	 *	We need to delete characters!
	 * else
	 *	No insert or delete
         */
	if ((nsb != nfd) && fx > 0 &&
	    ((p - old) + fx <= el->edited_terminal.t_size.h)) {
		ELRE_DEBUG(1,
		    (__F, "first diff insert at %td...\r\n", nfd - new));
		/*
		 * Move to the first char to insert, where the first diff is.
		 */
		edited_term_move_to_char(el, (int)(nfd - new));
		/*
		 * Check if we have stuff to keep at end
		 */
		if (nsb != ne) {
			ELRE_DEBUG(1, (__F, "with stuff to keep at end\r\n"));
			/*
		         * insert fx chars of new starting at nfd
		         */
			if (fx > 0) {
				ELRE_DEBUG(!EL_CAN_INSERT, (__F,
				"ERROR: cannot insert in early first diff\n"));
				edited_term_insertwrite(el, nfd, fx);
				edited_re_insert(el, old, (int)(ofd - old),
				    el->edited_terminal.t_size.h, nfd, fx);
			}
			/*
		         * write (nsb-nfd) - fx chars of new starting at
		         * (nfd + fx)
			 */
			len = (size_t) ((nsb - nfd) - fx);
			edited_term_overwrite(el, (nfd + fx), len);
			edited_re__strncopy(ofd + fx, nfd + fx, len);
		} else {
			ELRE_DEBUG(1, (__F, "without anything to save\r\n"));
			len = (size_t)(nsb - nfd);
			edited_term_overwrite(el, nfd, len);
			edited_re__strncopy(ofd, nfd, len);
			/*
		         * Done
		         */
			return;
		}
	} else if (fx < 0) {
		ELRE_DEBUG(1,
		    (__F, "first diff delete at %td...\r\n", ofd - old));
		/*
		 * move to the first char to delete where the first diff is
		 */
		edited_term_move_to_char(el, (int)(ofd - old));
		/*
		 * Check if we have stuff to save
		 */
		if (osb != oe) {
			ELRE_DEBUG(1, (__F, "with stuff to save at end\r\n"));
			/*
		         * fx is less than zero *always* here but we check
		         * for code symmetry
		         */
			if (fx < 0) {
				ELRE_DEBUG(!EL_CAN_DELETE, (__F,
				    "ERROR: cannot delete in first diff\n"));
				edited_term_deletechars(el, -fx);
				edited_re_delete(el, old, (int)(ofd - old),
				    el->edited_terminal.t_size.h, -fx);
			}
			/*
		         * write (nsb-nfd) chars of new starting at nfd
		         */
			len = (size_t) (nsb - nfd);
			edited_term_overwrite(el, nfd, len);
			edited_re__strncopy(ofd, nfd, len);

		} else {
			ELRE_DEBUG(1, (__F,
			    "but with nothing left to save\r\n"));
			/*
		         * write (nsb-nfd) chars of new starting at nfd
		         */
			edited_term_overwrite(el, nfd, (size_t)(nsb - nfd));
			edited_re_clear_eol(el, fx, sx,
			    (int)((oe - old) - (ne - new)));
			/*
		         * Done
		         */
			return;
		}
	} else
		fx = 0;

	if (sx < 0 && (ose - old) + fx < el->edited_terminal.t_size.h) {
		ELRE_DEBUG(1, (__F,
		    "second diff delete at %td...\r\n", (ose - old) + fx));
		/*
		 * Check if we have stuff to delete
		 */
		/*
		 * fx is the number of characters inserted (+) or deleted (-)
		 */

		edited_term_move_to_char(el, (int)((ose - old) + fx));
		/*
		 * Check if we have stuff to save
		 */
		if (ols != oe) {
			ELRE_DEBUG(1, (__F, "with stuff to save at end\r\n"));
			/*
		         * Again a duplicate test.
		         */
			if (sx < 0) {
				ELRE_DEBUG(!EL_CAN_DELETE, (__F,
				    "ERROR: cannot delete in second diff\n"));
				edited_term_deletechars(el, -sx);
			}
			/*
		         * write (nls-nse) chars of new starting at nse
		         */
			edited_term_overwrite(el, nse, (size_t)(nls - nse));
		} else {
			ELRE_DEBUG(1, (__F,
			    "but with nothing left to save\r\n"));
			edited_term_overwrite(el, nse, (size_t)(nls - nse));
			edited_re_clear_eol(el, fx, sx,
			    (int)((oe - old) - (ne - new)));
		}
	}
	/*
         * if we have a first insert AND WE HAVEN'T ALREADY DONE IT...
         */
	if ((nsb != nfd) && (osb - ofd) <= (nsb - nfd) && (fx == 0)) {
		ELRE_DEBUG(1, (__F, "late first diff insert at %td...\r\n",
		    nfd - new));

		edited_term_move_to_char(el, (int)(nfd - new));
		/*
		 * Check if we have stuff to keep at the end
		 */
		if (nsb != ne) {
			ELRE_DEBUG(1, (__F, "with stuff to keep at end\r\n"));
			/*
		         * We have to recalculate fx here because we set it
		         * to zero above as a flag saying that we hadn't done
		         * an early first insert.
		         */
			fx = (int)((nsb - nfd) - (osb - ofd));
			if (fx > 0) {
				/*
				 * insert fx chars of new starting at nfd
				 */
				ELRE_DEBUG(!EL_CAN_INSERT, (__F,
				 "ERROR: cannot insert in late first diff\n"));
				edited_term_insertwrite(el, nfd, fx);
				edited_re_insert(el, old, (int)(ofd - old),
				    el->edited_terminal.t_size.h, nfd, fx);
			}
			/*
		         * write (nsb-nfd) - fx chars of new starting at
		         * (nfd + fx)
			 */
			len = (size_t) ((nsb - nfd) - fx);
			edited_term_overwrite(el, (nfd + fx), len);
			edited_re__strncopy(ofd + fx, nfd + fx, len);
		} else {
			ELRE_DEBUG(1, (__F, "without anything to save\r\n"));
			len = (size_t) (nsb - nfd);
			edited_term_overwrite(el, nfd, len);
			edited_re__strncopy(ofd, nfd, len);
		}
	}
	/*
         * line is now NEW up to nse
         */
	if (sx >= 0) {
		ELRE_DEBUG(1, (__F,
		    "second diff insert at %d...\r\n", (int)(nse - new)));
		edited_term_move_to_char(el, (int)(nse - new));
		if (ols != oe) {
			ELRE_DEBUG(1, (__F, "with stuff to keep at end\r\n"));
			if (sx > 0) {
				/* insert sx chars of new starting at nse */
				ELRE_DEBUG(!EL_CAN_INSERT, (__F,
				    "ERROR: cannot insert in second diff\n"));
				edited_term_insertwrite(el, nse, sx);
			}
			/*
		         * write (nls-nse) - sx chars of new starting at
			 * (nse + sx)
		         */
			edited_term_overwrite(el, (nse + sx), (size_t)((nls - nse) - sx));
		} else {
			ELRE_DEBUG(1, (__F, "without anything to save\r\n"));
			edited_term_overwrite(el, nse, (size_t)(nls - nse));

			/*
	                 * No need to do a clear-to-end here because we were
	                 * doing a second insert, so we will have over
	                 * written all of the old string.
		         */
		}
	}
	ELRE_DEBUG(1, (__F, "done.\r\n"));
}


/* edited_re__copy_and_pad():
 *	Copy string and pad with spaces
 */
static void
edited_re__copy_and_pad(wchar_t *dst, const wchar_t *src, size_t width)
{
	size_t i;

	for (i = 0; i < width; i++) {
		if (*src == '\0')
			break;
		*dst++ = *src++;
	}

	for (; i < width; i++)
		*dst++ = ' ';

	*dst = '\0';
}


/* edited_re_refresh_cursor():
 *	Move to the new cursor position
 */
libedited_private void
edited_re_refresh_cursor(Edited *el)
{
	wchar_t *cp;
	int h, v, th, w;

	if (el->edited_use_style) {
		edited_re_refresh(el);
		return;
	}

	if (el->edited_line.cursor >= el->edited_line.lastchar) {
		if (el->edited_map.current == el->edited_map.alt
		    && el->edited_line.lastchar != el->edited_line.buffer)
			el->edited_line.cursor = el->edited_line.lastchar - 1;
		else
			el->edited_line.cursor = el->edited_line.lastchar;
	}

	/* first we must find where the cursor is... */
	h = el->edited_prompt.p_pos.h;
	v = el->edited_prompt.p_pos.v;
	th = el->edited_terminal.t_size.h;	/* optimize for speed */

	/* do input buffer to el->edited_line.cursor */
	for (cp = el->edited_line.buffer; cp < el->edited_line.cursor; cp++) {
                switch (edited_ct_chr_class(*cp)) {
		case CHTYPE_NL:  /* handle newline in data part too */
			h = 0;
			v++;
			break;
		case CHTYPE_TAB: /* if a tab, to next tab stop */
			while (++h & 07)
				continue;
			break;
		default:
			w = wcwidth(*cp);
			if (w > 1 && h + w > th) { /* won't fit on line */
				h = 0;
				v++;
			}
			h += edited_ct_visual_width(*cp);
			break;
                }

		if (h >= th) {	/* check, extra long tabs picked up here also */
			h -= th;
			v++;
		}
	}
        /* if we have a next character, and it's a doublewidth one, we need to
         * check whether we need to linebreak for it to fit */
        if (cp < el->edited_line.lastchar && (w = wcwidth(*cp)) > 1)
                if (h + w > th) {
                    h = 0;
                    v++;
                }

	/* now go there */
	edited_term_move_to_line(el, v);
	edited_term_move_to_char(el, h);
	edited_term__flush(el);
}


/* edited_re_fastputc():
 *	Add a character fast.
 */
static void
edited_re_fastputc(Edited *el, wint_t c)
{
	wint_t *lastline;
	int w;

	w = wcwidth(c);
	while (w > 1 && el->edited_cursor.h + w > el->edited_terminal.t_size.h)
	    edited_re_fastputc(el, ' ');

	edited_term__putc(el, c);
	el->edited_display[el->edited_cursor.v][el->edited_cursor.h++] = c;
	while (--w > 0)
		el->edited_display[el->edited_cursor.v][el->edited_cursor.h++]
			= MB_FILL_CHAR;

	if (el->edited_cursor.h >= el->edited_terminal.t_size.h) {
		/* if we must overflow */
		el->edited_cursor.h = 0;

		/*
		 * If we would overflow (input is longer than terminal size),
		 * emulate scroll by dropping first line and shuffling the rest.
		 * We do this via pointer shuffling - it's safe in this case
		 * and we avoid memcpy().
		 */
		if (el->edited_cursor.v + 1 >= el->edited_terminal.t_size.v) {
			int i, lins = el->edited_terminal.t_size.v;

			lastline = el->edited_display[0];
			for(i = 1; i < lins; i++)
				el->edited_display[i - 1] = el->edited_display[i];

			el->edited_display[i - 1] = lastline;
		} else {
			el->edited_cursor.v++;
			lastline = el->edited_display[++el->edited_refresh.r_oldcv];
		}
		edited_re__copy_and_pad((wchar_t *)lastline, L"",
		    (size_t)el->edited_terminal.t_size.h);

		if (EL_HAS_AUTO_MARGINS) {
			if (EL_HAS_MAGIC_MARGINS) {
				edited_term__putc(el, ' ');
				edited_term__putc(el, '\b');
			}
		} else {
			edited_term__putc(el, '\r');
			edited_term__putc(el, '\n');
		}
	}
}


/* edited_re_fastaddc():
 *	we added just one char, handle it fast.
 *	Assumes that screen cursor == real cursor
 */
libedited_private void
edited_re_fastaddc(Edited *el)
{
	wchar_t c;
	int rhdiff;

	if (el->edited_use_style) {
		edited_re_refresh(el);
		return;
	}
	if (el->edited_line.cursor == el->edited_line.buffer) {
		edited_re_refresh(el);
		return;
	}
	c = el->edited_line.cursor[-1];

	if (c == '\t' || el->edited_line.cursor != el->edited_line.lastchar) {
		edited_re_refresh(el);	/* too hard to handle */
		return;
	}
	rhdiff = el->edited_terminal.t_size.h - el->edited_cursor.h -
	    el->edited_rprompt.p_pos.h;
	if (el->edited_rprompt.p_pos.h && rhdiff < 3) {
		edited_re_refresh(el);	/* clear out rprompt if less than 1 char gap */
		return;
	}			/* else (only do at end of line, no TAB) */
	switch (edited_ct_chr_class(c)) {
	case CHTYPE_TAB: /* already handled, should never happen here */
		break;
	case CHTYPE_NL:
	case CHTYPE_PRINT:
		edited_re_fastputc(el, c);
		break;
	case CHTYPE_ASCIICTL:
	case CHTYPE_NONPRINT: {
		wchar_t visbuf[VISUAL_WIDTH_MAX];
		ssize_t i, n =
		    edited_ct_visual_char(visbuf, VISUAL_WIDTH_MAX, c);
		for (i = 0; n-- > 0; ++i)
			edited_re_fastputc(el, visbuf[i]);
		break;
	}
	}
	edited_term__flush(el);
}


/* edited_re_clear_display():
 *	clear the screen buffers so that new new prompt starts fresh.
 */
libedited_private void
edited_re_clear_display(Edited *el)
{
	int i;

	el->edited_cursor.v = 0;
	el->edited_cursor.h = 0;
	for (i = 0; i < el->edited_terminal.t_size.v; i++)
		el->edited_display[i][0] = '\0';
	el->edited_refresh.r_oldcv = 0;
}


/* edited_re_clear_lines():
 *	Make sure all lines are *really* blank
 */
libedited_private void
edited_re_clear_lines(Edited *el)
{

	if (EL_CAN_CEOL) {
		int i;
		for (i = el->edited_refresh.r_oldcv; i >= 0; i--) {
			/* for each line on the screen */
			edited_term_move_to_line(el, i);
			edited_term_move_to_char(el, 0);
			edited_term_clear_EOL(el, el->edited_terminal.t_size.h);
		}
	} else {
		edited_term_move_to_line(el, el->edited_refresh.r_oldcv);
					/* go to last line */
		edited_term__putc(el, '\r');	/* go to BOL */
		edited_term__putc(el, '\n');	/* go to new line */
	}
}
