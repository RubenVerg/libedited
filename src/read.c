/*	$NetBSD: read.c,v 1.108 2022/10/30 19:11:31 christos Exp $	*/

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
static char sccsid[] = "@(#)read.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: read.c,v 1.108 2022/10/30 19:11:31 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * read.c: Terminal read functions
 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "edited/el.h"
#include "edited/fcns.h"
#include "edited/read.h"

#define	EL_MAXMACRO	10

struct macros {
	wchar_t	**macro;
	int	  level;
	int	  offset;
};

struct edited_read_t {
	struct macros	 macros;
	edited_rfunc_t	 edited_read_char;	/* Function to read a character. */
	int		 edited_read_errno;
};

static int	edited_read__fixio(int, int);
static int	edited_read_char(Edited *, wchar_t *);
static int	edited_read_getcmd(Edited *, edited_action_t *, wchar_t *);
static void	edited_read_clearmacros(struct macros *);
static void	edited_read_pop(struct macros *);
static const wchar_t *noedit_wgets(Edited *, int *);

/* edited_read_init():
 *	Initialize the read stuff
 */
libedited_private int
edited_read_init(Edited *el)
{
	struct macros *ma;

	if ((el->edited_read = edited_malloc(sizeof(*el->edited_read))) == NULL)
		return -1;

	ma = &el->edited_read->macros;
	if ((ma->macro = edited_calloc(EL_MAXMACRO, sizeof(*ma->macro))) == NULL)
		goto out;
	ma->level = -1;
	ma->offset = 0;

	/* builtin edited_read_char */
	el->edited_read->edited_read_char = edited_read_char;
	return 0;
out:
	edited_read_end(el);
	return -1;
}

/* edited_read_end():
 *	Free the data structures used by the read stuff.
 */
libedited_private void
edited_read_end(Edited *el)
{

	edited_read_clearmacros(&el->edited_read->macros);
	edited_free(el->edited_read->macros.macro);
	el->edited_read->macros.macro = NULL;
	edited_free(el->edited_read);
	el->edited_read = NULL;
}

/* edited_read_setfn():
 *	Set the read char function to the one provided.
 *	If it is set to EL_BUILTIN_GETCFN, then reset to the builtin one.
 */
libedited_private int
edited_read_setfn(struct edited_read_t *edited_read, edited_rfunc_t rc)
{
	edited_read->edited_read_char = (rc == EL_BUILTIN_GETCFN) ? edited_read_char : rc;
	return 0;
}


/* edited_read_getfn():
 *	return the current read char function, or EL_BUILTIN_GETCFN
 *	if it is the default one
 */
libedited_private edited_rfunc_t
edited_read_getfn(struct edited_read_t *edited_read)
{
       return edited_read->edited_read_char == edited_read_char ?
	    EL_BUILTIN_GETCFN : edited_read->edited_read_char;
}


/* edited_read__fixio():
 *	Try to recover from a read error
 */
/* ARGSUSED */
static int
edited_read__fixio(int fd __attribute__((__unused__)), int e)
{

	switch (e) {
	case -1:		/* Make sure that the code is reachable */

#ifdef EWOULDBLOCK
	case EWOULDBLOCK:
#ifndef TRY_AGAIN
#define TRY_AGAIN
#endif
#endif /* EWOULDBLOCK */

#if defined(POSIX) && defined(EAGAIN)
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
	case EAGAIN:
#ifndef TRY_AGAIN
#define TRY_AGAIN
#endif
#endif /* EWOULDBLOCK && EWOULDBLOCK != EAGAIN */
#endif /* POSIX && EAGAIN */

		e = 0;
#ifdef TRY_AGAIN
#if defined(F_SETFL) && defined(O_NDELAY)
		if ((e = fcntl(fd, F_GETFL, 0)) == -1)
			return -1;

		if (fcntl(fd, F_SETFL, e & ~O_NDELAY) == -1)
			return -1;
		else
			e = 1;
#endif /* F_SETFL && O_NDELAY */

#ifdef FIONBIO
		{
			int zero = 0;

			if (ioctl(fd, FIONBIO, &zero) == -1)
				return -1;
			else
				e = 1;
		}
#endif /* FIONBIO */

#endif /* TRY_AGAIN */
		return e ? 0 : -1;

	case EINTR:
		return 0;

	default:
		return -1;
	}
}


/* edited_push():
 *	Push a macro
 */
void
edited_wpush(Edited *el, const wchar_t *str)
{
	struct macros *ma = &el->edited_read->macros;

	if (str != NULL && ma->level + 1 < EL_MAXMACRO) {
		ma->level++;
		if ((ma->macro[ma->level] = wcsdup(str)) != NULL)
			return;
		ma->level--;
	}
	edited_term_beep(el);
	edited_term__flush(el);
}


/* edited_read_getcmd():
 *	Get next command from the input stream,
 *	return 0 on success or -1 on EOF or error.
 *	Character values > 255 are not looked up in the map, but inserted.
 */
static int
edited_read_getcmd(Edited *el, edited_action_t *cmdnum, wchar_t *ch)
{
	static const wchar_t meta = (wchar_t)0x80;
	edited_action_t cmd;

	do {
		if (edited_wgetc(el, ch) != 1)
			return -1;

#ifdef	KANJI
		if ((*ch & meta)) {
			el->edited_state.metanext = 0;
			cmd = CcViMap[' '];
			break;
		} else
#endif /* KANJI */

		if (el->edited_state.metanext) {
			el->edited_state.metanext = 0;
			*ch |= meta;
		}
		if (*ch >= N_KEYS)
			cmd = EDITED_ED_INSERT;
		else
			cmd = el->edited_map.current[(unsigned char) *ch];
		if (cmd == EDITED_ED_SEQUENCE_LEAD_IN) {
			edited_km_value_t val;
			switch (edited_km_get(el, ch, &val)) {
			case XK_CMD:
				cmd = val.cmd;
				break;
			case XK_STR:
				edited_wpush(el, val.str);
				break;
			case XK_NOD:
				return -1;
			default:
				EL_ABORT((el->edited_errfile, "Bad XK_ type \n"));
				break;
			}
		}
	} while (cmd == EDITED_ED_SEQUENCE_LEAD_IN);
	*cmdnum = cmd;
	return 0;
}

/* edited_read_char():
 *	Read a character from the tty.
 */
static int
edited_read_char(Edited *el, wchar_t *cp)
{
	ssize_t num_read;
	int tried = (el->edited_flags & FIXIO) == 0;
	char cbuf[MB_LEN_MAX];
	size_t cbp = 0;
	int save_errno = errno;

 again:
	el->edited_signal->edited_sig_no = 0;
	while ((num_read = read(el->edited_infd, cbuf + cbp, (size_t)1)) == -1) {
		int e = errno;
		switch (el->edited_signal->edited_sig_no) {
		case SIGCONT:
			edited_wset(el, EL_REFRESH);
			/*FALLTHROUGH*/
		case SIGWINCH:
			edited_sig_set(el);
			goto again;
		default:
			break;
		}
		if (!tried && edited_read__fixio(el->edited_infd, e) == 0) {
			errno = save_errno;
			tried = 1;
		} else {
			errno = e;
			*cp = L'\0';
			return -1;
		}
	}

	/* Test for EOF */
	if (num_read == 0) {
		*cp = L'\0';
		return 0;
	}

	for (;;) {
		mbstate_t mbs;

		++cbp;
		/* This only works because UTF8 is stateless. */
		memset(&mbs, 0, sizeof(mbs));
		switch (mbrtowc(cp, cbuf, cbp, &mbs)) {
		case (size_t)-1:
			if (cbp > 1) {
				/*
				 * Invalid sequence, discard all bytes
				 * except the last one.
				 */
				cbuf[0] = cbuf[cbp - 1];
				cbp = 0;
				break;
			} else {
				/* Invalid byte, discard it. */
				cbp = 0;
				goto again;
			}
		case (size_t)-2:
			if (cbp >= MB_LEN_MAX) {
				errno = EILSEQ;
				*cp = L'\0';
				return -1;
			}
			/* Incomplete sequence, read another byte. */
			goto again;
		default:
			/* Valid character, process it. */
			return 1;
		}
	}
}

/* edited_read_pop():
 *	Pop a macro from the stack
 */
static void
edited_read_pop(struct macros *ma)
{
	int i;

	edited_free(ma->macro[0]);
	for (i = 0; i < ma->level; i++)
		ma->macro[i] = ma->macro[i + 1];
	ma->level--;
	ma->offset = 0;
}

static void
edited_read_clearmacros(struct macros *ma)
{
	while (ma->level >= 0)
		edited_free(ma->macro[ma->level--]);
	ma->offset = 0;
}

/* edited_wgetc():
 *	Read a wide character
 */
int
edited_wgetc(Edited *el, wchar_t *cp)
{
	struct macros *ma = &el->edited_read->macros;
	int num_read;

	edited_term__flush(el);
	for (;;) {
		if (ma->level < 0)
			break;

		if (ma->macro[0][ma->offset] == '\0') {
			edited_read_pop(ma);
			continue;
		}

		*cp = ma->macro[0][ma->offset++];

		if (ma->macro[0][ma->offset] == '\0') {
			/* Needed for QuoteMode On */
			edited_read_pop(ma);
		}

		return 1;
	}

	if (edited_tty_rawmode(el) < 0)/* make sure the tty is set up correctly */
		return 0;

	num_read = (*el->edited_read->edited_read_char)(el, cp);

	/*
	 * Remember the original reason of a read failure
	 * such that edited_wgets() can restore it after doing
	 * various cleanup operation that might change errno.
	 */
	if (num_read < 0)
		el->edited_read->edited_read_errno = errno;

	return num_read;
}

libedited_private void
edited_read_prepare(Edited *el)
{
	if (el->edited_flags & HANDLE_SIGNALS)
		edited_sig_set(el);
	if (el->edited_flags & NO_TTY)
		return;
	if ((el->edited_flags & (UNBUFFERED|EDIT_DISABLED)) == UNBUFFERED)
		edited_tty_rawmode(el);

	/* This is relatively cheap, and things go terribly wrong if
	   we have the wrong size. */
	edited_resize(el);
	edited_re_clear_display(el);	/* reset the display stuff */
	ch_reset(el);
	edited_re_refresh(el);		/* print the prompt */

	if (el->edited_flags & UNBUFFERED)
		edited_term__flush(el);
}

libedited_private void
edited_read_finish(Edited *el)
{
	if ((el->edited_flags & UNBUFFERED) == 0)
		(void) edited_tty_cookedmode(el);
	if (el->edited_flags & HANDLE_SIGNALS)
		edited_sig_clr(el);
}

static const wchar_t *
noedit_wgets(Edited *el, int *nread)
{
	edited_line_t	*lp = &el->edited_line;
	int		 num;

	while ((num = (*el->edited_read->edited_read_char)(el, lp->lastchar)) == 1) {
		if (lp->lastchar + 1 >= lp->limit &&
		    !ch_enlargebufs(el, (size_t)2))
			break;
		lp->lastchar++;
		if (el->edited_flags & UNBUFFERED ||
		    lp->lastchar[-1] == '\r' ||
		    lp->lastchar[-1] == '\n')
			break;
	}
	if (num == -1 && errno == EINTR)
		lp->lastchar = lp->buffer;
	lp->cursor = lp->lastchar;
	*lp->lastchar = '\0';
	*nread = (int)(lp->lastchar - lp->buffer);
	return *nread ? lp->buffer : NULL;
}

const wchar_t *
edited_wgets(Edited *el, int *nread)
{
	int retval;
	edited_action_t cmdnum = 0;
	int num;		/* how many chars we have read at NL */
	wchar_t ch;
	int nrb;

	if (nread == NULL)
		nread = &nrb;
	*nread = 0;
	el->edited_read->edited_read_errno = 0;

	if (el->edited_flags & NO_TTY) {
		el->edited_line.lastchar = el->edited_line.buffer;
		return noedit_wgets(el, nread);
	}

#ifdef FIONREAD
	if (el->edited_tty.t_mode == EX_IO && el->edited_read->macros.level < 0) {
		int chrs = 0;

		(void) ioctl(el->edited_infd, FIONREAD, &chrs);
		if (chrs == 0) {
			if (edited_tty_rawmode(el) < 0) {
				errno = 0;
				*nread = 0;
				return NULL;
			}
		}
	}
#endif /* FIONREAD */

	if ((el->edited_flags & UNBUFFERED) == 0)
		edited_read_prepare(el);

	if (el->edited_flags & EDIT_DISABLED) {
		if ((el->edited_flags & UNBUFFERED) == 0)
			el->edited_line.lastchar = el->edited_line.buffer;
		edited_term__flush(el);
		return noedit_wgets(el, nread);
	}

	for (num = -1; num == -1;) {  /* while still editing this line */
		/* if EOF or error */
		if (edited_read_getcmd(el, &cmdnum, &ch) == -1)
			break;
		if ((size_t)cmdnum >= el->edited_map.nfunc) /* BUG CHECK command */
			continue;	/* try again */
		/* now do the real command */
		/* vi redo needs these way down the levels... */
		el->edited_state.thiscmd = cmdnum;
		el->edited_state.thisch = ch;
		if (el->edited_map.type == MAP_VI &&
		    el->edited_map.current == el->edited_map.key &&
		    el->edited_chared.edited_c_redo.pos < el->edited_chared.edited_c_redo.lim) {
			if (cmdnum == EDITED_VI_DELETE_PREV_CHAR &&
			    el->edited_chared.edited_c_redo.pos != el->edited_chared.edited_c_redo.buf
			    && iswprint(el->edited_chared.edited_c_redo.pos[-1]))
				el->edited_chared.edited_c_redo.pos--;
			else
				*el->edited_chared.edited_c_redo.pos++ = ch;
		}
		retval = (*el->edited_map.func[cmdnum]) (el, ch);

		/* save the last command here */
		el->edited_state.lastcmd = cmdnum;

		/* use any return value */
		switch (retval) {
		case CC_CURSOR:
			edited_re_refresh_cursor(el);
			break;

		case CC_REDISPLAY:
			edited_re_clear_lines(el);
			edited_re_clear_display(el);
			/* FALLTHROUGH */

		case CC_REFRESH:
			edited_re_refresh(el);
			break;

		case CC_REFRESH_BEEP:
			edited_re_refresh(el);
			edited_term_beep(el);
			break;

		case CC_NORM:	/* normal char */
			break;

		case CC_ARGHACK:	/* Suggested by Rich Salz */
			/* <rsalz@pineapple.bbn.com> */
			continue;	/* keep going... */

		case CC_EOF:	/* end of file typed */
			if ((el->edited_flags & UNBUFFERED) == 0)
				num = 0;
			else if (num == -1) {
				*el->edited_line.lastchar++ = CONTROL('d');
				el->edited_line.cursor = el->edited_line.lastchar;
				num = 1;
			}
			break;

		case CC_NEWLINE:	/* normal end of line */
			num = (int)(el->edited_line.lastchar - el->edited_line.buffer);
			break;

		case CC_FATAL:	/* fatal error, reset to known state */
			/* put (real) cursor in a known place */
			edited_re_clear_display(el);	/* reset the display stuff */
			ch_reset(el);	/* reset the input pointers */
			edited_read_clearmacros(&el->edited_read->macros);
			edited_re_refresh(el); /* print the prompt again */
			break;

		case CC_ERROR:
		default:	/* functions we don't know about */
			edited_term_beep(el);
			edited_term__flush(el);
			break;
		}
		el->edited_state.argument = 1;
		el->edited_state.doingarg = 0;
		el->edited_chared.edited_c_vcmd.action = NOP;
		if (el->edited_flags & UNBUFFERED)
			break;
	}

	edited_term__flush(el);		/* flush any buffered output */
	/* make sure the tty is set up correctly */
	if ((el->edited_flags & UNBUFFERED) == 0) {
		edited_read_finish(el);
		*nread = num != -1 ? num : 0;
	} else
		*nread = (int)(el->edited_line.lastchar - el->edited_line.buffer);

	if (*nread == 0) {
		if (num == -1) {
			*nread = -1;
			if (el->edited_read->edited_read_errno)
				errno = el->edited_read->edited_read_errno;
		}
		return NULL;
	} else
		return el->edited_line.buffer;
}
