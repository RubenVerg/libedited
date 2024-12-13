/*	$NetBSD: terminal.c,v 1.46 2023/02/04 14:34:28 christos Exp $	*/

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
static char sccsid[] = "@(#)term.c	8.2 (Berkeley) 4/30/95";
#else
__RCSID("$NetBSD: terminal.c,v 1.46 2023/02/04 14:34:28 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * terminal.c: Editor/termcap-curses interface
 *	       We have to declare a static variable here, since the
 *	       termcap putchar routine does not take an argument!
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#endif
#ifdef HAVE_CURSES_H
#include <curses.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#endif

/* Solaris's term.h does horrid things. */
#if defined(HAVE_TERM_H) && !defined(__sun) && !defined(HAVE_TERMCAP_H)
#include <term.h>
#endif

#if defined(__sun)
extern int tgetent(char *, const char *);
extern int tgetflag(char *);
extern int tgetnum(char *);
extern int tputs(const char *, int, int (*)(int));
extern char* tgoto(const char*, int, int);
extern char* tgetstr(char*, char**);
#endif

#ifdef _REENTRANT
#include <pthread.h>
#endif

#include "edited/el.h"
#include "edited/fcns.h"

/*
 * IMPORTANT NOTE: these routines are allowed to look at the current screen
 * and the current position assuming that it is correct.  If this is not
 * true, then the update will be WRONG!  This is (should be) a valid
 * assumption...
 */

#define	TC_BUFSIZE	((size_t)2048)

#define	GoodStr(a)	(el->edited_terminal.t_str[a] != NULL && \
			    el->edited_terminal.t_str[a][0] != '\0')
#define	Str(a)		el->edited_terminal.t_str[a]
#define	Val(a)		el->edited_terminal.t_val[a]

static const struct termcapstr {
	const char *name;
	const char *long_name;
} tstr[] = {
#define	T_al	0
	{ "al", "add new blank line" },
#define	T_bl	1
	{ "bl", "audible bell" },
#define	T_cd	2
	{ "cd", "clear to bottom" },
#define	T_ce	3
	{ "ce", "clear to end of line" },
#define	T_ch	4
	{ "ch", "cursor to horiz pos" },
#define	T_cl	5
	{ "cl", "clear screen" },
#define	T_dc	6
	{ "dc", "delete a character" },
#define	T_dl	7
	{ "dl", "delete a line" },
#define	T_dm	8
	{ "dm", "start delete mode" },
#define	T_ed	9
	{ "ed", "end delete mode" },
#define	T_ei	10
	{ "ei", "end insert mode" },
#define	T_fs	11
	{ "fs", "cursor from status line" },
#define	T_ho	12
	{ "ho", "home cursor" },
#define	T_ic	13
	{ "ic", "insert character" },
#define	T_im	14
	{ "im", "start insert mode" },
#define	T_ip	15
	{ "ip", "insert padding" },
#define	T_kd	16
	{ "kd", "sends cursor down" },
#define	T_kl	17
	{ "kl", "sends cursor left" },
#define	T_kr	18
	{ "kr", "sends cursor right" },
#define	T_ku	19
	{ "ku", "sends cursor up" },
#define	T_md	20
	{ "md", "begin bold" },
#define	T_me	21
	{ "me", "end attributes" },
#define	T_nd	22
	{ "nd", "non destructive space" },
#define	T_se	23
	{ "se", "end standout" },
#define	T_so	24
	{ "so", "begin standout" },
#define	T_ts	25
	{ "ts", "cursor to status line" },
#define	T_up	26
	{ "up", "cursor up one" },
#define	T_us	27
	{ "us", "begin underline" },
#define	T_ue	28
	{ "ue", "end underline" },
#define	T_vb	29
	{ "vb", "visible bell" },
#define	T_DC	30
	{ "DC", "delete multiple chars" },
#define	T_DO	31
	{ "DO", "cursor down multiple" },
#define	T_IC	32
	{ "IC", "insert multiple chars" },
#define	T_LE	33
	{ "LE", "cursor left multiple" },
#define	T_RI	34
	{ "RI", "cursor right multiple" },
#define	T_UP	35
	{ "UP", "cursor up multiple" },
#define	T_kh	36
	{ "kh", "send cursor home" },
#define	T_at7	37
	{ "@7", "send cursor end" },
#define	T_kD	38
	{ "kD", "send cursor delete" },
#define	T_str	39
	{ NULL, NULL }
};

static const struct termcapval {
	const char *name;
	const char *long_name;
} tval[] = {
#define	T_am	0
	{ "am", "has automatic margins" },
#define	T_pt	1
	{ "pt", "has physical tabs" },
#define	T_li	2
	{ "li", "Number of lines" },
#define	T_co	3
	{ "co", "Number of columns" },
#define	T_km	4
	{ "km", "Has meta key" },
#define	T_xt	5
	{ "xt", "Tab chars destructive" },
#define	T_xn	6
	{ "xn", "newline ignored at right margin" },
#define	T_MT	7
	{ "MT", "Has meta key" },			/* XXX? */
#define	T_val	8
	{ NULL, NULL, }
};
/* do two or more of the attributes use me */

static void	edited_term_setflags(Edited *);
static int	edited_term_rebuffer_display(Edited *);
static void	edited_term_free_display(Edited *);
static int	edited_term_alloc_display(Edited *);
static void	edited_term_alloc(Edited *, const struct termcapstr *,
    const char *);
static void	edited_term_init_arrow(Edited *);
static void	edited_term_reset_arrow(Edited *);
static int	edited_term_putc(int);
static void	edited_term_tputs(Edited *, const char *, int);

#ifdef _REENTRANT
static pthread_mutex_t edited_term_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
static FILE *edited_term_outfile = NULL;


/* edited_term_setflags():
 *	Set the terminal capability flags
 */
static void
edited_term_setflags(Edited *el)
{
	EL_FLAGS = 0;
	if (el->edited_tty.t_tabs)
		EL_FLAGS |= (Val(T_pt) && !Val(T_xt)) ? TERM_CAN_TAB : 0;

	EL_FLAGS |= (Val(T_km) || Val(T_MT)) ? TERM_HAS_META : 0;
	EL_FLAGS |= GoodStr(T_ce) ? TERM_CAN_CEOL : 0;
	EL_FLAGS |= (GoodStr(T_dc) || GoodStr(T_DC)) ? TERM_CAN_DELETE : 0;
	EL_FLAGS |= (GoodStr(T_im) || GoodStr(T_ic) || GoodStr(T_IC)) ?
	    TERM_CAN_INSERT : 0;
	EL_FLAGS |= (GoodStr(T_up) || GoodStr(T_UP)) ? TERM_CAN_UP : 0;
	EL_FLAGS |= Val(T_am) ? TERM_HAS_AUTO_MARGINS : 0;
	EL_FLAGS |= Val(T_xn) ? TERM_HAS_MAGIC_MARGINS : 0;

	if (GoodStr(T_me) && GoodStr(T_ue))
		EL_FLAGS |= (strcmp(Str(T_me), Str(T_ue)) == 0) ?
		    TERM_CAN_ME : 0;
	else
		EL_FLAGS &= ~TERM_CAN_ME;
	if (GoodStr(T_me) && GoodStr(T_se))
		EL_FLAGS |= (strcmp(Str(T_me), Str(T_se)) == 0) ?
		    TERM_CAN_ME : 0;


#ifdef DEBUG_SCREEN
	if (!EL_CAN_UP) {
		(void) fprintf(el->edited_errfile,
		    "WARNING: Your terminal cannot move up.\n");
		(void) fprintf(el->edited_errfile,
		    "Editing may be odd for long lines.\n");
	}
	if (!EL_CAN_CEOL)
		(void) fprintf(el->edited_errfile, "no clear EOL capability.\n");
	if (!EL_CAN_DELETE)
		(void) fprintf(el->edited_errfile, "no delete char capability.\n");
	if (!EL_CAN_INSERT)
		(void) fprintf(el->edited_errfile, "no insert char capability.\n");
#endif /* DEBUG_SCREEN */
}

/* edited_term_init():
 *	Initialize the terminal stuff
 */
libedited_private int
edited_term_init(Edited *el)
{

	el->edited_terminal.t_buf = edited_calloc(TC_BUFSIZE,
	    sizeof(*el->edited_terminal.t_buf));
	if (el->edited_terminal.t_buf == NULL)
		return -1;
	el->edited_terminal.t_cap = edited_calloc(TC_BUFSIZE,
	    sizeof(*el->edited_terminal.t_cap));
	if (el->edited_terminal.t_cap == NULL)
		goto out;
	el->edited_terminal.t_fkey = edited_calloc(A_K_NKEYS,
	    sizeof(*el->edited_terminal.t_fkey));
	if (el->edited_terminal.t_fkey == NULL)
		goto out;
	el->edited_terminal.t_loc = 0;
	el->edited_terminal.t_str = edited_calloc(T_str,
	    sizeof(*el->edited_terminal.t_str));
	if (el->edited_terminal.t_str == NULL)
		goto out;
	el->edited_terminal.t_val = edited_calloc(T_val,
	    sizeof(*el->edited_terminal.t_val));
	if (el->edited_terminal.t_val == NULL)
		goto out;
	(void) edited_term_set(el, NULL);
	edited_term_init_arrow(el);
	return 0;
out:
	edited_term_end(el);
	return -1;
}

/* edited_term_end():
 *	Clean up the terminal stuff
 */
libedited_private void
edited_term_end(Edited *el)
{

	edited_free(el->edited_terminal.t_buf);
	el->edited_terminal.t_buf = NULL;
	edited_free(el->edited_terminal.t_cap);
	el->edited_terminal.t_cap = NULL;
	el->edited_terminal.t_loc = 0;
	edited_free(el->edited_terminal.t_str);
	el->edited_terminal.t_str = NULL;
	edited_free(el->edited_terminal.t_val);
	el->edited_terminal.t_val = NULL;
	edited_free(el->edited_terminal.t_fkey);
	el->edited_terminal.t_fkey = NULL;
	edited_term_free_display(el);
}


/* edited_term_alloc():
 *	Maintain a string pool for termcap strings
 */
static void
edited_term_alloc(Edited *el, const struct termcapstr *t, const char *cap)
{
	char termbuf[TC_BUFSIZE];
	size_t tlen, clen;
	char **tlist = el->edited_terminal.t_str;
	char **tmp, **str = &tlist[t - tstr];

	(void) memset(termbuf, 0, sizeof(termbuf));
	if (cap == NULL || *cap == '\0') {
		*str = NULL;
		return;
	} else
		clen = strlen(cap);

	tlen = *str == NULL ? 0 : strlen(*str);

	/*
         * New string is shorter; no need to allocate space
         */
	if (clen <= tlen) {
		if (*str)
			(void) strcpy(*str, cap);	/* XXX strcpy is safe */
		return;
	}
	/*
         * New string is longer; see if we have enough space to append
         */
	if (el->edited_terminal.t_loc + 3 < TC_BUFSIZE) {
						/* XXX strcpy is safe */
		(void) strcpy(*str = &el->edited_terminal.t_buf[
		    el->edited_terminal.t_loc], cap);
		el->edited_terminal.t_loc += clen + 1;	/* one for \0 */
		return;
	}
	/*
         * Compact our buffer; no need to check compaction, cause we know it
         * fits...
         */
	tlen = 0;
	for (tmp = tlist; tmp < &tlist[T_str]; tmp++)
		if (*tmp != NULL && **tmp != '\0' && *tmp != *str) {
			char *ptr;

			for (ptr = *tmp; *ptr != '\0'; termbuf[tlen++] = *ptr++)
				continue;
			termbuf[tlen++] = '\0';
		}
	memcpy(el->edited_terminal.t_buf, termbuf, TC_BUFSIZE);
	el->edited_terminal.t_loc = tlen;
	if (el->edited_terminal.t_loc + 3 >= TC_BUFSIZE) {
		(void) fprintf(el->edited_errfile,
		    "Out of termcap string space.\n");
		return;
	}
					/* XXX strcpy is safe */
	(void) strcpy(*str = &el->edited_terminal.t_buf[el->edited_terminal.t_loc],
	    cap);
	el->edited_terminal.t_loc += (size_t)clen + 1;	/* one for \0 */
	return;
}


/* edited_term_rebuffer_display():
 *	Rebuffer the display after the screen changed size
 */
static int
edited_term_rebuffer_display(Edited *el)
{
	coord_t *c = &el->edited_terminal.t_size;

	edited_term_free_display(el);

	c->h = Val(T_co);
	c->v = Val(T_li);

	if (edited_term_alloc_display(el) == -1)
		return -1;
	return 0;
}

static wint_t **
edited_term_alloc_buffer(Edited *el)
{
	wint_t **b;
	coord_t *c = &el->edited_terminal.t_size;
	int i;

	b =  edited_calloc((size_t)(c->v + 1), sizeof(*b));
	if (b == NULL)
		return NULL;
	for (i = 0; i < c->v; i++) {
		b[i] = edited_calloc((size_t)(c->h + 1), sizeof(**b));
		if (b[i] == NULL) {
			while (--i >= 0)
				edited_free(b[i]);
			edited_free(b);
			return NULL;
		}
	}
	b[c->v] = NULL;
	return b;
}

static void
edited_term_free_buffer(wint_t ***bp)
{
	wint_t **b;
	wint_t **bufp;

	if (*bp == NULL)
		return;

	b = *bp;
	*bp = NULL;

	for (bufp = b; *bufp != NULL; bufp++)
		edited_free(*bufp);
	edited_free(b);
}

/* edited_term_alloc_display():
 *	Allocate a new display.
 */
static int
edited_term_alloc_display(Edited *el)
{
	el->edited_display = edited_term_alloc_buffer(el);
	if (el->edited_display == NULL)
		goto done;
	el->edited_vdisplay = edited_term_alloc_buffer(el);
	if (el->edited_vdisplay == NULL)
		goto done;
	return 0;
done:
	edited_term_free_display(el);
	return -1;
}


/* edited_term_free_display():
 *	Free the display buffers
 */
static void
edited_term_free_display(Edited *el)
{
	edited_term_free_buffer(&el->edited_display);
	edited_term_free_buffer(&el->edited_vdisplay);
}


/* edited_term_move_to_line():
 *	move to line <where> (first line == 0)
 *	as efficiently as possible
 */
libedited_private void
edited_term_move_to_line(Edited *el, int where)
{
	int del;

	if (where == el->edited_cursor.v)
		return;

	if (where >= el->edited_terminal.t_size.v) {
#ifdef DEBUG_SCREEN
		(void) fprintf(el->edited_errfile,
		    "%s: where is ridiculous: %d\r\n", __func__, where);
#endif /* DEBUG_SCREEN */
		return;
	}
	if ((del = where - el->edited_cursor.v) > 0) {
		/*
		 * We don't use DO here because some terminals are buggy
		 * if the destination is beyond bottom of the screen.
		 */
		for (; del > 0; del--)
			edited_term__putc(el, '\n');
		/* because the \n will become \r\n */
		el->edited_cursor.h = 0;
	} else {		/* del < 0 */
		if (GoodStr(T_UP) && (-del > 1 || !GoodStr(T_up)))
			edited_term_tputs(el, tgoto(Str(T_UP), -del, -del), -del);
		else {
			if (GoodStr(T_up))
				for (; del < 0; del++)
					edited_term_tputs(el, Str(T_up), 1);
		}
	}
	el->edited_cursor.v = where;/* now where is here */
}


/* edited_term_move_to_char():
 *	Move to the character position specified
 */
libedited_private void
edited_term_move_to_char(Edited *el, int where)
{
	int del, i;

mc_again:
	if (where == el->edited_cursor.h)
		return;

	if (where > el->edited_terminal.t_size.h) {
#ifdef DEBUG_SCREEN
		(void) fprintf(el->edited_errfile,
		    "%s: where is ridiculous: %d\r\n", __func__, where);
#endif /* DEBUG_SCREEN */
		return;
	}
	if (!where) {		/* if where is first column */
		edited_term__putc(el, '\r');	/* do a CR */
		el->edited_cursor.h = 0;
		return;
	}
	del = where - el->edited_cursor.h;

	if ((del < -4 || del > 4) && GoodStr(T_ch))
		/* go there directly */
		edited_term_tputs(el, tgoto(Str(T_ch), where, where), where);
	else {
		if (del > 0) {	/* moving forward */
			if ((del > 4) && GoodStr(T_RI))
				edited_term_tputs(el, tgoto(Str(T_RI), del, del),
				    del);
			else {
					/* if I can do tabs, use them */
				if (EL_CAN_TAB) {
					if ((el->edited_cursor.h & 0370) !=
					    (where & ~0x7)
					    && (el->edited_display[
					    el->edited_cursor.v][where & 0370] !=
					    MB_FILL_CHAR)
					    ) {
						/* if not within tab stop */
						for (i =
						    (el->edited_cursor.h & 0370);
						    i < (where & ~0x7);
						    i += 8)
							edited_term__putc(el,
							    '\t');
							/* then tab over */
						el->edited_cursor.h = where & ~0x7;
					}
				}
				/*
				 * it's usually cheaper to just write the
				 * chars, so we do.
				 */
				/*
				 * NOTE THAT edited_term_overwrite() WILL CHANGE
				 * el->edited_cursor.h!!!
				 */
				edited_term_overwrite(el,
				    (wchar_t *)&el->edited_display[
				    el->edited_cursor.v][el->edited_cursor.h],
				    (size_t)(where - el->edited_cursor.h));

			}
		} else {	/* del < 0 := moving backward */
			if ((-del > 4) && GoodStr(T_LE))
				edited_term_tputs(el, tgoto(Str(T_LE), -del, -del),
				    -del);
			else {	/* can't go directly there */
				/*
				 * if the "cost" is greater than the "cost"
				 * from col 0
				 */
				if (EL_CAN_TAB ?
				    ((unsigned int)-del >
				    (((unsigned int) where >> 3) +
				     (where & 07)))
				    : (-del > where)) {
					edited_term__putc(el, '\r');/* do a CR */
					el->edited_cursor.h = 0;
					goto mc_again;	/* and try again */
				}
				for (i = 0; i < -del; i++)
					edited_term__putc(el, '\b');
			}
		}
	}
	el->edited_cursor.h = where;		/* now where is here */
}


/* edited_term_overwrite():
 *	Overstrike num characters
 *	Assumes MB_FILL_CHARs are present to keep the column count correct
 */
libedited_private void
edited_term_overwrite(Edited *el, const wchar_t *cp, size_t n)
{
	if (n == 0)
		return;

	if (n > (size_t)el->edited_terminal.t_size.h) {
#ifdef DEBUG_SCREEN
		(void) fprintf(el->edited_errfile,
		    "%s: n is ridiculous: %zu\r\n", __func__, n);
#endif /* DEBUG_SCREEN */
		return;
	}

        do {
                /* edited_term__putc() ignores any MB_FILL_CHARs */
                edited_term__putc(el, *cp++);
                el->edited_cursor.h++;
        } while (--n);

	if (el->edited_cursor.h >= el->edited_terminal.t_size.h) {	/* wrap? */
		if (EL_HAS_AUTO_MARGINS) {	/* yes */
			el->edited_cursor.h = 0;
			if (el->edited_cursor.v + 1 < el->edited_terminal.t_size.v)
				el->edited_cursor.v++;
			if (EL_HAS_MAGIC_MARGINS) {
				/* force the wrap to avoid the "magic"
				 * situation */
				wchar_t c;
				if ((c = el->edited_display[el->edited_cursor.v]
				    [el->edited_cursor.h]) != '\0') {
					edited_term_overwrite(el, &c, (size_t)1);
					while (el->edited_display[el->edited_cursor.v]
					    [el->edited_cursor.h] == MB_FILL_CHAR)
						el->edited_cursor.h++;
				} else {
					edited_term__putc(el, ' ');
					el->edited_cursor.h = 1;
				}
			}
		} else		/* no wrap, but cursor stays on screen */
			el->edited_cursor.h = el->edited_terminal.t_size.h - 1;
	}
}

/* edited_term_overwrite_styled():
 */

libedited_private void
edited_term_overwrite_styled(Edited *el, const wchar_t *cp, size_t n, edited_style_t *style)
{
	int i, j, len;
	wchar_t *tmp = malloc(sizeof(wchar_t) * 16);
	for (i = 0; i < n; i++) {
		len = edited_style_to_escape(el, style[i], tmp);
		for (j = 0; j < len; j++)
			edited_term__putc(el, tmp[j]);
		edited_term_overwrite(el, cp + i, 1);
	}
	len = edited_style_to_escape(el, EDITED_STYLE_RESET, tmp);
	for (j = 0; j < len; j++)
		edited_term__putc(el, tmp[j]);
}

/* edited_term_deletechars():
 *	Delete num characters
 */
libedited_private void
edited_term_deletechars(Edited *el, int num)
{
	if (num <= 0)
		return;

	if (!EL_CAN_DELETE) {
#ifdef DEBUG_EDIT
		(void) fprintf(el->edited_errfile, "   ERROR: cannot delete   \n");
#endif /* DEBUG_EDIT */
		return;
	}
	if (num > el->edited_terminal.t_size.h) {
#ifdef DEBUG_SCREEN
		(void) fprintf(el->edited_errfile,
		    "%s: num is ridiculous: %d\r\n", __func__, num);
#endif /* DEBUG_SCREEN */
		return;
	}
	if (GoodStr(T_DC))	/* if I have multiple delete */
		if ((num > 1) || !GoodStr(T_dc)) {	/* if dc would be more
							 * expen. */
			edited_term_tputs(el, tgoto(Str(T_DC), num, num), num);
			return;
		}
	if (GoodStr(T_dm))	/* if I have delete mode */
		edited_term_tputs(el, Str(T_dm), 1);

	if (GoodStr(T_dc))	/* else do one at a time */
		while (num--)
			edited_term_tputs(el, Str(T_dc), 1);

	if (GoodStr(T_ed))	/* if I have delete mode */
		edited_term_tputs(el, Str(T_ed), 1);
}


/* edited_term_insertwrite():
 *	Puts terminal in insert character mode or inserts num
 *	characters in the line
 *      Assumes MB_FILL_CHARs are present to keep column count correct
 */
libedited_private void
edited_term_insertwrite(Edited *el, wchar_t *cp, int num)
{
	if (num <= 0)
		return;
	if (!EL_CAN_INSERT) {
#ifdef DEBUG_EDIT
		(void) fprintf(el->edited_errfile, "   ERROR: cannot insert   \n");
#endif /* DEBUG_EDIT */
		return;
	}
	if (num > el->edited_terminal.t_size.h) {
#ifdef DEBUG_SCREEN
		(void) fprintf(el->edited_errfile,
		    "%s: num is ridiculous: %d\r\n", __func__, num);
#endif /* DEBUG_SCREEN */
		return;
	}
	if (GoodStr(T_IC))	/* if I have multiple insert */
		if ((num > 1) || !GoodStr(T_ic)) {
				/* if ic would be more expensive */
			edited_term_tputs(el, tgoto(Str(T_IC), num, num), num);
			edited_term_overwrite(el, cp, (size_t)num);
				/* this updates edited_cursor.h */
			return;
		}
	if (GoodStr(T_im) && GoodStr(T_ei)) {	/* if I have insert mode */
		edited_term_tputs(el, Str(T_im), 1);

		el->edited_cursor.h += num;
		do
			edited_term__putc(el, *cp++);
		while (--num);

		if (GoodStr(T_ip))	/* have to make num chars insert */
			edited_term_tputs(el, Str(T_ip), 1);

		edited_term_tputs(el, Str(T_ei), 1);
		return;
	}
	do {
		if (GoodStr(T_ic))	/* have to make num chars insert */
			edited_term_tputs(el, Str(T_ic), 1);

		edited_term__putc(el, *cp++);

		el->edited_cursor.h++;

		if (GoodStr(T_ip))	/* have to make num chars insert */
			edited_term_tputs(el, Str(T_ip), 1);
					/* pad the inserted char */

	} while (--num);
}


/* edited_term_insertwrite_styled():
 */

libedited_private void
edited_term_insertwrite_styled(Edited *el, wchar_t *cp, int num, edited_style_t *style)
{
	int i, j, len;
	wchar_t *tmp = malloc(sizeof(wchar_t) * 16);
	for (i = 0; i < num; i++) {
		len = edited_style_to_escape(el, style[i], tmp);
		for (j = 0; j < len; j++)
			edited_term__putc(el, tmp[j]);
		edited_term_insertwrite(el, cp + i, 1);
	}
	len = edited_style_to_escape(el, EDITED_STYLE_RESET, tmp);
	for (j = 0; j < len; j++)
		edited_term__putc(el, tmp[j]);
}


/* edited_term_clear_EOL():
 *	clear to end of line.  There are num characters to clear
 */
libedited_private void
edited_term_clear_EOL(Edited *el, int num)
{
	int i;

	if (EL_CAN_CEOL && GoodStr(T_ce))
		edited_term_tputs(el, Str(T_ce), 1);
	else {
		for (i = 0; i < num; i++)
			edited_term__putc(el, ' ');
		el->edited_cursor.h += num;	/* have written num spaces */
	}
}


/* edited_term_clear_screen():
 *	Clear the screen
 */
libedited_private void
edited_term_clear_screen(Edited *el)
{				/* clear the whole screen and home */

	if (GoodStr(T_cl))
		/* send the clear screen code */
		edited_term_tputs(el, Str(T_cl), Val(T_li));
	else if (GoodStr(T_ho) && GoodStr(T_cd)) {
		edited_term_tputs(el, Str(T_ho), Val(T_li));	/* home */
		/* clear to bottom of screen */
		edited_term_tputs(el, Str(T_cd), Val(T_li));
	} else {
		edited_term__putc(el, '\r');
		edited_term__putc(el, '\n');
	}
}


/* edited_term_beep():
 *	Beep the way the terminal wants us
 */
libedited_private void
edited_term_beep(Edited *el)
{
	if (GoodStr(T_bl))
		/* what termcap says we should use */
		edited_term_tputs(el, Str(T_bl), 1);
	else
		edited_term__putc(el, '\007');	/* an ASCII bell; ^G */
}


libedited_private void
edited_term_get(Edited *el, const char **term)
{
	*term = el->edited_terminal.t_name;
}


/* edited_term_set():
 *	Read in the terminal capabilities from the requested terminal
 */
libedited_private int
edited_term_set(Edited *el, const char *term)
{
	int i;
	char buf[TC_BUFSIZE];
	char *area;
	const struct termcapstr *t;
	sigset_t oset, nset;
	int lins, cols;

	(void) sigemptyset(&nset);
	(void) sigaddset(&nset, SIGWINCH);
	(void) sigprocmask(SIG_BLOCK, &nset, &oset);

	area = buf;


	if (term == NULL)
		term = getenv("TERM");

	if (!term || !term[0])
		term = "dumb";

	if (strcmp(term, "emacs") == 0)
		el->edited_flags |= EDIT_DISABLED;

	(void) memset(el->edited_terminal.t_cap, 0, TC_BUFSIZE);

	i = tgetent(el->edited_terminal.t_cap, term);

	if (i <= 0) {
		if (i == -1)
			(void) fprintf(el->edited_errfile,
			    "Cannot read termcap database;\n");
		else if (i == 0)
			(void) fprintf(el->edited_errfile,
			    "No entry for terminal type \"%s\";\n", term);
		(void) fprintf(el->edited_errfile,
		    "using dumb terminal settings.\n");
		Val(T_co) = 80;	/* do a dumb terminal */
		Val(T_pt) = Val(T_km) = Val(T_li) = 0;
		Val(T_xt) = Val(T_MT);
		for (t = tstr; t->name != NULL; t++)
			edited_term_alloc(el, t, NULL);
	} else {
		/* auto/magic margins */
		Val(T_am) = tgetflag("am");
		Val(T_xn) = tgetflag("xn");
		/* Can we tab */
		Val(T_pt) = tgetflag("pt");
		Val(T_xt) = tgetflag("xt");
		/* do we have a meta? */
		Val(T_km) = tgetflag("km");
		Val(T_MT) = tgetflag("MT");
		/* Get the size */
		Val(T_co) = tgetnum("co");
		Val(T_li) = tgetnum("li");
		for (t = tstr; t->name != NULL; t++) {
			/* XXX: some systems' tgetstr needs non const */
			edited_term_alloc(el, t, tgetstr(strchr(t->name, *t->name),
			    &area));
		}
	}

	if (Val(T_co) < 2)
		Val(T_co) = 80;	/* just in case */
	if (Val(T_li) < 1)
		Val(T_li) = 24;

	el->edited_terminal.t_size.v = Val(T_co);
	el->edited_terminal.t_size.h = Val(T_li);

	edited_term_setflags(el);

				/* get the correct window size */
	(void) edited_term_get_size(el, &lins, &cols);
	if (edited_term_change_size(el, lins, cols) == -1)
		return -1;
	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
	edited_term_bind_arrow(el);
	el->edited_terminal.t_name = term;
	return i <= 0 ? -1 : 0;
}


/* edited_term_get_size():
 *	Return the new window size in lines and cols, and
 *	true if the size was changed.
 */
libedited_private int
edited_term_get_size(Edited *el, int *lins, int *cols)
{

	*cols = Val(T_co);
	*lins = Val(T_li);

#ifdef TIOCGWINSZ
	{
		struct winsize ws;
		if (ioctl(el->edited_infd, TIOCGWINSZ, &ws) != -1) {
			if (ws.ws_col)
				*cols = ws.ws_col;
			if (ws.ws_row)
				*lins = ws.ws_row;
		}
	}
#endif
#ifdef TIOCGSIZE
	{
		struct ttysize ts;
		if (ioctl(el->edited_infd, TIOCGSIZE, &ts) != -1) {
			if (ts.ts_cols)
				*cols = ts.ts_cols;
			if (ts.ts_lines)
				*lins = ts.ts_lines;
		}
	}
#endif
	return Val(T_co) != *cols || Val(T_li) != *lins;
}


/* edited_term_change_size():
 *	Change the size of the terminal
 */
libedited_private int
edited_term_change_size(Edited *el, int lins, int cols)
{
	coord_t cur = el->edited_cursor;
	/*
	 * Just in case
	 */
	Val(T_co) = (cols < 2) ? 80 : cols;
	Val(T_li) = (lins < 1) ? 24 : lins;

	/* re-make display buffers */
	if (edited_term_rebuffer_display(el) == -1)
		return -1;
	edited_re_clear_display(el);
	el->edited_cursor = cur;
	return 0;
}


/* edited_term_init_arrow():
 *	Initialize the arrow key bindings from termcap
 */
static void
edited_term_init_arrow(Edited *el)
{
	funckey_t *arrow = el->edited_terminal.t_fkey;

	arrow[A_K_DN].name = L"down";
	arrow[A_K_DN].key = T_kd;
	arrow[A_K_DN].fun.cmd = EDITED_ED_NEXT_HISTORY;
	arrow[A_K_DN].type = XK_CMD;

	arrow[A_K_UP].name = L"up";
	arrow[A_K_UP].key = T_ku;
	arrow[A_K_UP].fun.cmd = EDITED_ED_PREV_HISTORY;
	arrow[A_K_UP].type = XK_CMD;

	arrow[A_K_LT].name = L"left";
	arrow[A_K_LT].key = T_kl;
	arrow[A_K_LT].fun.cmd = EDITED_ED_PREV_CHAR;
	arrow[A_K_LT].type = XK_CMD;

	arrow[A_K_RT].name = L"right";
	arrow[A_K_RT].key = T_kr;
	arrow[A_K_RT].fun.cmd = EDITED_ED_NEXT_CHAR;
	arrow[A_K_RT].type = XK_CMD;

	arrow[A_K_HO].name = L"home";
	arrow[A_K_HO].key = T_kh;
	arrow[A_K_HO].fun.cmd = EDITED_ED_MOVE_TO_BEG;
	arrow[A_K_HO].type = XK_CMD;

	arrow[A_K_EN].name = L"end";
	arrow[A_K_EN].key = T_at7;
	arrow[A_K_EN].fun.cmd = EDITED_ED_MOVE_TO_END;
	arrow[A_K_EN].type = XK_CMD;

	arrow[A_K_DE].name = L"delete";
	arrow[A_K_DE].key = T_kD;
	arrow[A_K_DE].fun.cmd = EDITED_ED_DELETE_NEXT_CHAR;
	arrow[A_K_DE].type = XK_CMD;
}


/* edited_term_reset_arrow():
 *	Reset arrow key bindings
 */
static void
edited_term_reset_arrow(Edited *el)
{
	funckey_t *arrow = el->edited_terminal.t_fkey;
	static const wchar_t strA[] = L"\033[A";
	static const wchar_t strB[] = L"\033[B";
	static const wchar_t strC[] = L"\033[C";
	static const wchar_t strD[] = L"\033[D";
	static const wchar_t strH[] = L"\033[H";
	static const wchar_t strF[] = L"\033[F";
	static const wchar_t stOA[] = L"\033OA";
	static const wchar_t stOB[] = L"\033OB";
	static const wchar_t stOC[] = L"\033OC";
	static const wchar_t stOD[] = L"\033OD";
	static const wchar_t stOH[] = L"\033OH";
	static const wchar_t stOF[] = L"\033OF";

	edited_km_add(el, strA, &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	edited_km_add(el, strB, &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	edited_km_add(el, strC, &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	edited_km_add(el, strD, &arrow[A_K_LT].fun, arrow[A_K_LT].type);
	edited_km_add(el, strH, &arrow[A_K_HO].fun, arrow[A_K_HO].type);
	edited_km_add(el, strF, &arrow[A_K_EN].fun, arrow[A_K_EN].type);
	edited_km_add(el, stOA, &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	edited_km_add(el, stOB, &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	edited_km_add(el, stOC, &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	edited_km_add(el, stOD, &arrow[A_K_LT].fun, arrow[A_K_LT].type);
	edited_km_add(el, stOH, &arrow[A_K_HO].fun, arrow[A_K_HO].type);
	edited_km_add(el, stOF, &arrow[A_K_EN].fun, arrow[A_K_EN].type);

	if (el->edited_map.type != MAP_VI)
		return;
	edited_km_add(el, &strA[1], &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	edited_km_add(el, &strB[1], &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	edited_km_add(el, &strC[1], &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	edited_km_add(el, &strD[1], &arrow[A_K_LT].fun, arrow[A_K_LT].type);
	edited_km_add(el, &strH[1], &arrow[A_K_HO].fun, arrow[A_K_HO].type);
	edited_km_add(el, &strF[1], &arrow[A_K_EN].fun, arrow[A_K_EN].type);
	edited_km_add(el, &stOA[1], &arrow[A_K_UP].fun, arrow[A_K_UP].type);
	edited_km_add(el, &stOB[1], &arrow[A_K_DN].fun, arrow[A_K_DN].type);
	edited_km_add(el, &stOC[1], &arrow[A_K_RT].fun, arrow[A_K_RT].type);
	edited_km_add(el, &stOD[1], &arrow[A_K_LT].fun, arrow[A_K_LT].type);
	edited_km_add(el, &stOH[1], &arrow[A_K_HO].fun, arrow[A_K_HO].type);
	edited_km_add(el, &stOF[1], &arrow[A_K_EN].fun, arrow[A_K_EN].type);
}


/* edited_term_set_arrow():
 *	Set an arrow key binding
 */
libedited_private int
edited_term_set_arrow(Edited *el, const wchar_t *name, edited_km_value_t *fun,
    int type)
{
	funckey_t *arrow = el->edited_terminal.t_fkey;
	int i;

	for (i = 0; i < A_K_NKEYS; i++)
		if (wcscmp(name, arrow[i].name) == 0) {
			arrow[i].fun = *fun;
			arrow[i].type = type;
			return 0;
		}
	return -1;
}


/* edited_term_clear_arrow():
 *	Clear an arrow key binding
 */
libedited_private int
edited_term_clear_arrow(Edited *el, const wchar_t *name)
{
	funckey_t *arrow = el->edited_terminal.t_fkey;
	int i;

	for (i = 0; i < A_K_NKEYS; i++)
		if (wcscmp(name, arrow[i].name) == 0) {
			arrow[i].type = XK_NOD;
			return 0;
		}
	return -1;
}


/* edited_term_print_arrow():
 *	Print the arrow key bindings
 */
libedited_private void
edited_term_print_arrow(Edited *el, const wchar_t *name)
{
	int i;
	funckey_t *arrow = el->edited_terminal.t_fkey;

	for (i = 0; i < A_K_NKEYS; i++)
		if (*name == '\0' || wcscmp(name, arrow[i].name) == 0)
			if (arrow[i].type != XK_NOD)
				edited_km_kprint(el, arrow[i].name,
				    &arrow[i].fun, arrow[i].type);
}


/* edited_term_bind_arrow():
 *	Bind the arrow keys
 */
libedited_private void
edited_term_bind_arrow(Edited *el)
{
	edited_action_t *map;
	const edited_action_t *dmap;
	int i, j;
	char *p;
	funckey_t *arrow = el->edited_terminal.t_fkey;

	/* Check if the components needed are initialized */
	if (el->edited_terminal.t_buf == NULL || el->edited_map.key == NULL)
		return;

	map = el->edited_map.type == MAP_VI ? el->edited_map.alt : el->edited_map.key;
	dmap = el->edited_map.type == MAP_VI ? el->edited_map.vic : el->edited_map.emacs;

	edited_term_reset_arrow(el);

	for (i = 0; i < A_K_NKEYS; i++) {
		wchar_t wt_str[VISUAL_WIDTH_MAX];
		wchar_t *px;
		size_t n;

		p = el->edited_terminal.t_str[arrow[i].key];
		if (!p || !*p)
			continue;
		for (n = 0; n < VISUAL_WIDTH_MAX && p[n]; ++n)
			wt_str[n] = p[n];
		while (n < VISUAL_WIDTH_MAX)
			wt_str[n++] = '\0';
		px = wt_str;
		j = (unsigned char) *p;
		/*
		 * Assign the arrow keys only if:
		 *
		 * 1. They are multi-character arrow keys and the user
		 *    has not re-assigned the leading character, or
		 *    has re-assigned the leading character to be
		 *	  EDITED_ED_SEQUENCE_LEAD_IN
		 * 2. They are single arrow keys pointing to an
		 *    unassigned key.
		 */
		if (arrow[i].type == XK_NOD)
			edited_km_clear(el, map, px);
		else {
			if (p[1] && (dmap[j] == map[j] ||
				map[j] == EDITED_ED_SEQUENCE_LEAD_IN)) {
				edited_km_add(el, px, &arrow[i].fun,
				    arrow[i].type);
				map[j] = EDITED_ED_SEQUENCE_LEAD_IN;
			} else if (map[j] == EDITED_ED_UNASSIGNED) {
				edited_km_clear(el, map, px);
				if (arrow[i].type == XK_CMD)
					map[j] = arrow[i].fun.cmd;
				else
					edited_km_add(el, px, &arrow[i].fun,
					    arrow[i].type);
			}
		}
	}
}

/* edited_term_putc():
 *	Add a character
 */
static int
edited_term_putc(int c)
{
	if (edited_term_outfile == NULL)
		return -1;
	return fputc(c, edited_term_outfile);
}

static void
edited_term_tputs(Edited *el, const char *cap, int affcnt)
{
#ifdef _REENTRANT
	pthread_mutex_lock(&edited_term_mutex);
#endif
	edited_term_outfile = el->edited_outfile;
	(void)tputs(cap, affcnt, edited_term_putc);
#ifdef _REENTRANT
	pthread_mutex_unlock(&edited_term_mutex);
#endif
}

/* edited_term__putc():
 *	Add a character
 */
libedited_private int
edited_term__putc(Edited *el, wint_t c)
{
	char buf[MB_LEN_MAX +1];
	ssize_t i;
	if (c == MB_FILL_CHAR)
		return 0;
	if (c & EL_LITERAL)
		return fputs(edited_lit_get(el, c), el->edited_outfile);
	i = edited_ct_encode_char(buf, (size_t)MB_LEN_MAX, c);
	if (i <= 0)
		return (int)i;
	buf[i] = '\0';
	return fputs(buf, el->edited_outfile);
}

/* edited_term__flush():
 *	Flush output
 */
libedited_private void
edited_term__flush(Edited *el)
{

	(void) fflush(el->edited_outfile);
}

/* edited_term_writec():
 *	Write the given character out, in a human readable form
 */
libedited_private void
edited_term_writec(Edited *el, wint_t c)
{
	wchar_t visbuf[VISUAL_WIDTH_MAX +1];
	ssize_t vcnt = edited_ct_visual_char(visbuf, VISUAL_WIDTH_MAX, c);
	if (vcnt < 0)
		vcnt = 0;
	visbuf[vcnt] = '\0';
	edited_term_overwrite(el, visbuf, (size_t)vcnt);
	edited_term__flush(el);
}


/* edited_term_telltc():
 *	Print the current termcap characteristics
 */
libedited_private int
/*ARGSUSED*/
edited_term_telltc(Edited *el, int argc __attribute__((__unused__)),
    const wchar_t **argv __attribute__((__unused__)))
{
	const struct termcapstr *t;
	char **ts;

	(void) fprintf(el->edited_outfile, "\n\tYour terminal has the\n");
	(void) fprintf(el->edited_outfile, "\tfollowing characteristics:\n\n");
	(void) fprintf(el->edited_outfile, "\tIt has %d columns and %d lines\n",
	    Val(T_co), Val(T_li));
	(void) fprintf(el->edited_outfile,
	    "\tIt has %s meta key\n", EL_HAS_META ? "a" : "no");
	(void) fprintf(el->edited_outfile,
	    "\tIt can%suse tabs\n", EL_CAN_TAB ? " " : "not ");
	(void) fprintf(el->edited_outfile, "\tIt %s automatic margins\n",
	    EL_HAS_AUTO_MARGINS ? "has" : "does not have");
	if (EL_HAS_AUTO_MARGINS)
		(void) fprintf(el->edited_outfile, "\tIt %s magic margins\n",
		    EL_HAS_MAGIC_MARGINS ? "has" : "does not have");

	for (t = tstr, ts = el->edited_terminal.t_str; t->name != NULL; t++, ts++) {
		const char *ub;
		if (*ts && **ts) {
			ub = edited_ct_encode_string(edited_ct_visual_string(
			    edited_ct_decode_string(*ts, &el->edited_scratch),
			    &el->edited_visual), &el->edited_scratch);
		} else {
			ub = "(empty)";
		}
		(void) fprintf(el->edited_outfile, "\t%25s (%s) == %s\n",
		    t->long_name, t->name, ub);
	}
	(void) fputc('\n', el->edited_outfile);
	return 0;
}


/* edited_term_settc():
 *	Change the current terminal characteristics
 */
libedited_private int
/*ARGSUSED*/
edited_term_settc(Edited *el, int argc __attribute__((__unused__)),
    const wchar_t **argv)
{
	const struct termcapstr *ts;
	const struct termcapval *tv;
	char what[8], how[8];
	long i;
	char *ep;

	if (argv == NULL || argv[1] == NULL || argv[2] == NULL)
		return -1;

	strlcpy(what, edited_ct_encode_string(argv[1], &el->edited_scratch), sizeof(what));
	strlcpy(how,  edited_ct_encode_string(argv[2], &el->edited_scratch), sizeof(how));

	/*
         * Do the strings first
         */
	for (ts = tstr; ts->name != NULL; ts++)
		if (strcmp(ts->name, what) == 0)
			break;

	if (ts->name != NULL) {
		edited_term_alloc(el, ts, how);
		edited_term_setflags(el);
		return 0;
	}
	/*
         * Do the numeric ones second
         */
	for (tv = tval; tv->name != NULL; tv++)
		if (strcmp(tv->name, what) == 0)
			break;

	if (tv->name == NULL) {
		(void) fprintf(el->edited_errfile,
		    "%ls: Bad capability `%s'.\n", argv[0], what);
		return -1;
	}

	if (tv == &tval[T_pt] || tv == &tval[T_km] ||
	    tv == &tval[T_am] || tv == &tval[T_xn]) {
		/*
		 * Booleans
		 */
		if (strcmp(how, "yes") == 0)
			el->edited_terminal.t_val[tv - tval] = 1;
		else if (strcmp(how, "no") == 0)
			el->edited_terminal.t_val[tv - tval] = 0;
		else {
			(void) fprintf(el->edited_errfile,
			    "%ls: Bad value `%s'.\n", argv[0], how);
			return -1;
		}
		edited_term_setflags(el);
		return 0;
	}

	/*
	 * Numerics
	 */
	i = strtol(how, &ep, 10);
	if (*ep != '\0') {
		(void) fprintf(el->edited_errfile,
		    "%ls: Bad value `%s'.\n", argv[0], how);
		return -1;
	}
	el->edited_terminal.t_val[tv - tval] = (int) i;
	i = 0;
	if (tv == &tval[T_co]) {
		el->edited_terminal.t_size.v = Val(T_co);
		i++;
	} else if (tv == &tval[T_li]) {
		el->edited_terminal.t_size.h = Val(T_li);
		i++;
	}
	if (i && edited_term_change_size(el, Val(T_li), Val(T_co)) == -1)
		return -1;
	return 0;
}


/* edited_term_gettc():
 *	Get the current terminal characteristics
 */
libedited_private int
/*ARGSUSED*/
edited_term_gettc(Edited *el, int argc __attribute__((__unused__)), char **argv)
{
	const struct termcapstr *ts;
	const struct termcapval *tv;
	char *what;
	void *how;

	if (argv == NULL || argv[1] == NULL || argv[2] == NULL)
		return -1;

	what = argv[1];
	how = argv[2];

	/*
         * Do the strings first
         */
	for (ts = tstr; ts->name != NULL; ts++)
		if (strcmp(ts->name, what) == 0)
			break;

	if (ts->name != NULL) {
		*(char **)how = el->edited_terminal.t_str[ts - tstr];
		return 0;
	}
	/*
         * Do the numeric ones second
         */
	for (tv = tval; tv->name != NULL; tv++)
		if (strcmp(tv->name, what) == 0)
			break;

	if (tv->name == NULL)
		return -1;

	if (tv == &tval[T_pt] || tv == &tval[T_km] ||
	    tv == &tval[T_am] || tv == &tval[T_xn]) {
		static char yes[] = "yes";
		static char no[] = "no";
		if (el->edited_terminal.t_val[tv - tval])
			*(char **)how = yes;
		else
			*(char **)how = no;
		return 0;
	} else {
		*(int *)how = el->edited_terminal.t_val[tv - tval];
		return 0;
	}
}

/* edited_term_echotc():
 *	Print the termcap string out with variable substitution
 */
libedited_private int
/*ARGSUSED*/
edited_term_echotc(Edited *el, int argc __attribute__((__unused__)),
    const wchar_t **argv)
{
	char *cap, *scap;
	wchar_t *ep;
	int arg_need, arg_cols, arg_rows;
	int verbose = 0, silent = 0;
	char *area;
	static const char fmts[] = "%s\n", fmtd[] = "%d\n";
	const struct termcapstr *t;
	char buf[TC_BUFSIZE];
	long i;

	area = buf;

	if (argv == NULL || argv[1] == NULL)
		return -1;
	argv++;

	if (argv[0][0] == '-') {
		switch (argv[0][1]) {
		case 'v':
			verbose = 1;
			break;
		case 's':
			silent = 1;
			break;
		default:
			/* stderror(ERR_NAME | ERR_TCUSAGE); */
			break;
		}
		argv++;
	}
	if (!*argv || *argv[0] == '\0')
		return 0;
	if (wcscmp(*argv, L"tabs") == 0) {
		(void) fprintf(el->edited_outfile, fmts, EL_CAN_TAB ? "yes" : "no");
		return 0;
	} else if (wcscmp(*argv, L"meta") == 0) {
		(void) fprintf(el->edited_outfile, fmts, Val(T_km) ? "yes" : "no");
		return 0;
	} else if (wcscmp(*argv, L"xn") == 0) {
		(void) fprintf(el->edited_outfile, fmts, EL_HAS_MAGIC_MARGINS ?
		    "yes" : "no");
		return 0;
	} else if (wcscmp(*argv, L"am") == 0) {
		(void) fprintf(el->edited_outfile, fmts, EL_HAS_AUTO_MARGINS ?
		    "yes" : "no");
		return 0;
	} else if (wcscmp(*argv, L"baud") == 0) {
		(void) fprintf(el->edited_outfile, fmtd, (int)el->edited_tty.t_speed);
		return 0;
	} else if (wcscmp(*argv, L"rows") == 0 ||
                   wcscmp(*argv, L"lines") == 0) {
		(void) fprintf(el->edited_outfile, fmtd, Val(T_li));
		return 0;
	} else if (wcscmp(*argv, L"cols") == 0) {
		(void) fprintf(el->edited_outfile, fmtd, Val(T_co));
		return 0;
	}
	/*
         * Try to use our local definition first
         */
	scap = NULL;
	for (t = tstr; t->name != NULL; t++)
		if (strcmp(t->name,
		    edited_ct_encode_string(*argv, &el->edited_scratch)) == 0) {
			scap = el->edited_terminal.t_str[t - tstr];
			break;
		}
	if (t->name == NULL) {
		/* XXX: some systems' tgetstr needs non const */
                scap = tgetstr(edited_ct_encode_string(*argv, &el->edited_scratch), &area);
	}
	if (!scap || scap[0] == '\0') {
		if (!silent)
			(void) fprintf(el->edited_errfile,
			    "echotc: Termcap parameter `%ls' not found.\n",
			    *argv);
		return -1;
	}
	/*
         * Count home many values we need for this capability.
         */
	for (cap = scap, arg_need = 0; *cap; cap++)
		if (*cap == '%')
			switch (*++cap) {
			case 'd':
			case '2':
			case '3':
			case '.':
			case '+':
				arg_need++;
				break;
			case '%':
			case '>':
			case 'i':
			case 'r':
			case 'n':
			case 'B':
			case 'D':
				break;
			default:
				/*
				 * hpux has lot's of them...
				 */
				if (verbose)
					(void) fprintf(el->edited_errfile,
				"echotc: Warning: unknown termcap %% `%c'.\n",
					    *cap);
				/* This is bad, but I won't complain */
				break;
			}

	switch (arg_need) {
	case 0:
		argv++;
		if (*argv && *argv[0]) {
			if (!silent)
				(void) fprintf(el->edited_errfile,
				    "echotc: Warning: Extra argument `%ls'.\n",
				    *argv);
			return -1;
		}
		edited_term_tputs(el, scap, 1);
		break;
	case 1:
		argv++;
		if (!*argv || *argv[0] == '\0') {
			if (!silent)
				(void) fprintf(el->edited_errfile,
				    "echotc: Warning: Missing argument.\n");
			return -1;
		}
		arg_cols = 0;
		i = wcstol(*argv, &ep, 10);
		if (*ep != '\0' || i < 0) {
			if (!silent)
				(void) fprintf(el->edited_errfile,
				    "echotc: Bad value `%ls' for rows.\n",
				    *argv);
			return -1;
		}
		arg_rows = (int) i;
		argv++;
		if (*argv && *argv[0]) {
			if (!silent)
				(void) fprintf(el->edited_errfile,
				    "echotc: Warning: Extra argument `%ls"
				    "'.\n", *argv);
			return -1;
		}
		edited_term_tputs(el, tgoto(scap, arg_cols, arg_rows), 1);
		break;
	default:
		/* This is wrong, but I will ignore it... */
		if (verbose)
			(void) fprintf(el->edited_errfile,
			 "echotc: Warning: Too many required arguments (%d).\n",
			    arg_need);
		/* FALLTHROUGH */
	case 2:
		argv++;
		if (!*argv || *argv[0] == '\0') {
			if (!silent)
				(void) fprintf(el->edited_errfile,
				    "echotc: Warning: Missing argument.\n");
			return -1;
		}
		i = wcstol(*argv, &ep, 10);
		if (*ep != '\0' || i < 0) {
			if (!silent)
				(void) fprintf(el->edited_errfile,
				    "echotc: Bad value `%ls' for cols.\n",
				    *argv);
			return -1;
		}
		arg_cols = (int) i;
		argv++;
		if (!*argv || *argv[0] == '\0') {
			if (!silent)
				(void) fprintf(el->edited_errfile,
				    "echotc: Warning: Missing argument.\n");
			return -1;
		}
		i = wcstol(*argv, &ep, 10);
		if (*ep != '\0' || i < 0) {
			if (!silent)
				(void) fprintf(el->edited_errfile,
				    "echotc: Bad value `%ls' for rows.\n",
				    *argv);
			return -1;
		}
		arg_rows = (int) i;
		if (*ep != '\0') {
			if (!silent)
				(void) fprintf(el->edited_errfile,
				    "echotc: Bad value `%ls'.\n", *argv);
			return -1;
		}
		argv++;
		if (*argv && *argv[0]) {
			if (!silent)
				(void) fprintf(el->edited_errfile,
				    "echotc: Warning: Extra argument `%ls"
				    "'.\n", *argv);
			return -1;
		}
		edited_term_tputs(el, tgoto(scap, arg_cols, arg_rows), arg_rows);
		break;
	}
	return 0;
}
