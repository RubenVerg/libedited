/*	$NetBSD: search.c,v 1.52 2024/06/30 16:26:30 christos Exp $	*/

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
static char sccsid[] = "@(#)search.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: search.c,v 1.52 2024/06/30 16:26:30 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * search.c: History and character search functions
 */
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#if defined(REGEX)
#include <regex.h>
#elif defined(REGEXP)
#include <regexp.h>
#endif

#include "edited/el.h"
#include "edited/common.h"
#include "edited/fcns.h"

/*
 * Adjust cursor in vi mode to include the character under it
 */
#define	EL_CURSOR(el) \
    ((el)->edited_line.cursor + (((el)->edited_map.type == MAP_VI) && \
			    ((el)->edited_map.current == (el)->edited_map.alt)))

/* search_init():
 *	Initialize the search stuff
 */
libedited_private int
search_init(Edited *el)
{

	el->edited_search.patbuf = edited_calloc(EL_BUFSIZ,
	    sizeof(*el->edited_search.patbuf));
	if (el->edited_search.patbuf == NULL)
		return -1;
	el->edited_search.patbuf[0] = L'\0';
	el->edited_search.patlen = 0;
	el->edited_search.patdir = -1;
	el->edited_search.chacha = L'\0';
	el->edited_search.chadir = CHAR_FWD;
	el->edited_search.chatflg = 0;
	return 0;
}


/* search_end():
 *	Initialize the search stuff
 */
libedited_private void
search_end(Edited *el)
{

	edited_free(el->edited_search.patbuf);
	el->edited_search.patbuf = NULL;
}


#ifdef REGEXP
/* regerror():
 *	Handle regular expression errors
 */
void
/*ARGSUSED*/
regerror(const char *msg)
{
}
#endif


/* edited_match():
 *	Return if string matches pattern
 */
libedited_private int
edited_match(const wchar_t *str, const wchar_t *pat)
{
	static edited_ct_buffer_t conv;
#if defined (REGEX)
	regex_t re;
	int rv;
#elif defined (REGEXP)
	regexp *rp;
	int rv;
#else
	extern char	*edited_re_comp(const char *);
	extern int	 edited_re_exec(const char *);
#endif

	if (wcsstr(str, pat) != 0)
		return 1;

#if defined(REGEX)
	if (regcomp(&re, edited_ct_encode_string(pat, &conv), 0) == 0) {
		rv = regexec(&re, edited_ct_encode_string(str, &conv), (size_t)0, NULL,
		    0) == 0;
		regfree(&re);
	} else {
		rv = 0;
	}
	return rv;
#elif defined(REGEXP)
	if ((re = regcomp(edited_ct_encode_string(pat, &conv))) != NULL) {
		rv = regexec(re, edited_ct_encode_string(str, &conv));
		edited_free(re);
	} else {
		rv = 0;
	}
	return rv;
#else
	if (edited_re_comp(edited_ct_encode_string(pat, &conv)) != NULL)
		return 0;
	else
		return edited_re_exec(edited_ct_encode_string(str, &conv)) == 1;
#endif
}


/* edited_c_hmatch():
 *	 return True if the pattern matches the prefix
 */
libedited_private int
edited_c_hmatch(Edited *el, const wchar_t *str)
{
#ifdef SDEBUG
	(void) fprintf(el->edited_errfile, "match `%ls' with `%ls'\n",
	    el->edited_search.patbuf, str);
#endif /* SDEBUG */

	return edited_match(str, el->edited_search.patbuf);
}


/* edited_c_setpat():
 *	Set the history seatch pattern
 */
libedited_private void
edited_c_setpat(Edited *el)
{
	if (el->edited_state.lastcmd != EDITED_ED_SEARCH_PREV_HISTORY &&
	    el->edited_state.lastcmd != EDITED_ED_SEARCH_NEXT_HISTORY) {
		el->edited_search.patlen =
		    (size_t)(EL_CURSOR(el) - el->edited_line.buffer);
		if (el->edited_search.patlen >= EL_BUFSIZ)
			el->edited_search.patlen = EL_BUFSIZ - 1;
		(void) wcsncpy(el->edited_search.patbuf, el->edited_line.buffer,
		    el->edited_search.patlen);
		el->edited_search.patbuf[el->edited_search.patlen] = '\0';
	}
#ifdef SDEBUG
	(void) fprintf(el->edited_errfile, "\neventno = %d\n",
	    el->edited_history.eventno);
	(void) fprintf(el->edited_errfile, "patlen = %ld\n", el->edited_search.patlen);
	(void) fprintf(el->edited_errfile, "patbuf = \"%ls\"\n",
	    el->edited_search.patbuf);
	(void) fprintf(el->edited_errfile, "cursor %ld lastchar %ld\n",
	    EL_CURSOR(el) - el->edited_line.buffer,
	    el->edited_line.lastchar - el->edited_line.buffer);
#endif
}


/* edited_ce_inc_search():
 *	Emacs incremental search
 */
libedited_private edited_action_t
edited_ce_inc_search(Edited *el, int dir)
{
	static const wchar_t STRfwd[] = L"fwd", STRbck[] = L"bck";
	static wchar_t pchar = L':';  /* ':' = normal, '?' = failed */
	static wchar_t endcmd[2] = {'\0', '\0'};
	wchar_t *ocursor = el->edited_line.cursor, oldpchar = pchar, ch;
	const wchar_t *cp;

	edited_action_t ret = CC_NORM;

	int ohisteventno = el->edited_history.eventno;
	size_t oldpatlen = el->edited_search.patlen;
	int newdir = dir;
	int done, redo;

	if (el->edited_line.lastchar + sizeof(STRfwd) /
	    sizeof(*el->edited_line.lastchar) + 2 +
	    el->edited_search.patlen >= el->edited_line.limit)
		return CC_ERROR;

	for (;;) {

		if (el->edited_search.patlen == 0) {	/* first round */
			pchar = ':';
#ifdef ANCHOR
#define	LEN	2
			el->edited_search.patbuf[el->edited_search.patlen++] = '.';
			el->edited_search.patbuf[el->edited_search.patlen++] = '*';
#else
#define	LEN	0
#endif
		}
		done = redo = 0;
		*el->edited_line.lastchar++ = '\n';
		for (cp = (newdir == EDITED_ED_SEARCH_PREV_HISTORY) ? STRbck : STRfwd;
		    *cp; *el->edited_line.lastchar++ = *cp++)
			continue;
		*el->edited_line.lastchar++ = pchar;
		for (cp = &el->edited_search.patbuf[LEN];
		    cp < &el->edited_search.patbuf[el->edited_search.patlen];
		    *el->edited_line.lastchar++ = *cp++)
			continue;
		*el->edited_line.lastchar = '\0';
		edited_re_refresh(el);

		if (edited_wgetc(el, &ch) != 1)
			return edited_ed_end_of_file(el, 0);

		switch (el->edited_map.current[(unsigned char) ch]) {
		case EDITED_ED_INSERT:
		case EDITED_ED_DIGIT:
			if (el->edited_search.patlen >= EL_BUFSIZ - LEN)
				edited_term_beep(el);
			else {
				el->edited_search.patbuf[el->edited_search.patlen++] =
				    ch;
				*el->edited_line.lastchar++ = ch;
				*el->edited_line.lastchar = '\0';
				edited_re_refresh(el);
			}
			break;

		case EDITED_EM_INC_SEARCH_NEXT:
			newdir = EDITED_ED_SEARCH_NEXT_HISTORY;
			redo++;
			break;

		case EDITED_EM_INC_SEARCH_PREV:
			newdir = EDITED_ED_SEARCH_PREV_HISTORY;
			redo++;
			break;

		case EDITED_EM_DELETE_PREV_CHAR:
		case EDITED_ED_DELETE_PREV_CHAR:
			if (el->edited_search.patlen > LEN)
				done++;
			else
				edited_term_beep(el);
			break;

		default:
			switch (ch) {
			case 0007:	/* ^G: Abort */
				ret = CC_ERROR;
				done++;
				break;

			case 0027:	/* ^W: Append word */
			/* No can do if globbing characters in pattern */
				for (cp = &el->edited_search.patbuf[LEN];; cp++)
				    if (cp >= &el->edited_search.patbuf[
					el->edited_search.patlen]) {
					if (el->edited_line.cursor ==
					    el->edited_line.buffer)
						break;
					el->edited_line.cursor +=
					    el->edited_search.patlen - LEN - 1;
					cp = edited_c__next_word(el->edited_line.cursor,
					    el->edited_line.lastchar, 1,
					    edited_ce__isword);
					while (el->edited_line.cursor < cp &&
					    *el->edited_line.cursor != '\n') {
						if (el->edited_search.patlen >=
						    EL_BUFSIZ - LEN) {
							edited_term_beep(el);
							break;
						}
						el->edited_search.patbuf[el->edited_search.patlen++] =
						    *el->edited_line.cursor;
						*el->edited_line.lastchar++ =
						    *el->edited_line.cursor++;
					}
					el->edited_line.cursor = ocursor;
					*el->edited_line.lastchar = '\0';
					edited_re_refresh(el);
					break;
				    } else if (isglob(*cp)) {
					    edited_term_beep(el);
					    break;
				    }
				break;

			default:	/* Terminate and execute cmd */
				endcmd[0] = ch;
				edited_wpush(el, endcmd);
				/* FALLTHROUGH */

			case 0033:	/* ESC: Terminate */
				ret = CC_REFRESH;
				done++;
				break;
			}
			break;
		}

		while (el->edited_line.lastchar > el->edited_line.buffer &&
		    *el->edited_line.lastchar != '\n')
			*el->edited_line.lastchar-- = '\0';
		*el->edited_line.lastchar = '\0';

		if (!done) {

			/* Can't search if unmatched '[' */
			for (cp = &el->edited_search.patbuf[el->edited_search.patlen-1],
			    ch = L']';
			    cp >= &el->edited_search.patbuf[LEN];
			    cp--)
				if (*cp == '[' || *cp == ']') {
					ch = *cp;
					break;
				}
			if (el->edited_search.patlen > LEN && ch != L'[') {
				if (redo && newdir == dir) {
					if (pchar == '?') { /* wrap around */
						el->edited_history.eventno =
						    newdir == EDITED_ED_SEARCH_PREV_HISTORY ? 0 : 0x7fffffff;
						if (hist_get(el) == CC_ERROR)
							/* el->edited_history.event
							 * no was fixed by
							 * first call */
							(void) hist_get(el);
						el->edited_line.cursor = newdir ==
						    EDITED_ED_SEARCH_PREV_HISTORY ?
						    el->edited_line.lastchar :
						    el->edited_line.buffer;
					} else
						el->edited_line.cursor +=
						    newdir ==
						    EDITED_ED_SEARCH_PREV_HISTORY ?
						    -1 : 1;
				}
#ifdef ANCHOR
				el->edited_search.patbuf[el->edited_search.patlen++] =
				    '.';
				el->edited_search.patbuf[el->edited_search.patlen++] =
				    '*';
#endif
				el->edited_search.patbuf[el->edited_search.patlen] =
				    '\0';
				if (el->edited_line.cursor < el->edited_line.buffer ||
				    el->edited_line.cursor > el->edited_line.lastchar ||
				    (ret = edited_ce_search_line(el, newdir))
				    == CC_ERROR) {
					/* avoid edited_c_setpat */
					el->edited_state.lastcmd =
					    (edited_action_t) newdir;
					ret = (edited_action_t)
					    (newdir == EDITED_ED_SEARCH_PREV_HISTORY ?
					    edited_ed_search_prev_history(el, 0) :
					    edited_ed_search_next_history(el, 0));
					if (ret != CC_ERROR) {
						el->edited_line.cursor = newdir ==
						    EDITED_ED_SEARCH_PREV_HISTORY ?
						    el->edited_line.lastchar :
						    el->edited_line.buffer;
						(void) edited_ce_search_line(el,
						    newdir);
					}
				}
				el->edited_search.patlen -= LEN;
				el->edited_search.patbuf[el->edited_search.patlen] =
				    '\0';
				if (ret == CC_ERROR) {
					edited_term_beep(el);
					if (el->edited_history.eventno !=
					    ohisteventno) {
						el->edited_history.eventno =
						    ohisteventno;
						if (hist_get(el) == CC_ERROR)
							return CC_ERROR;
					}
					el->edited_line.cursor = ocursor;
					pchar = '?';
				} else {
					pchar = ':';
				}
			}
			ret = edited_ce_inc_search(el, newdir);

			if (ret == CC_ERROR && pchar == '?' && oldpchar == ':')
				/*
				 * break abort of failed search at last
				 * non-failed
				 */
				ret = CC_NORM;

		}
		if (ret == CC_NORM || (ret == CC_ERROR && oldpatlen == 0)) {
			/* restore on normal return or error exit */
			pchar = oldpchar;
			el->edited_search.patlen = oldpatlen;
			if (el->edited_history.eventno != ohisteventno) {
				el->edited_history.eventno = ohisteventno;
				if (hist_get(el) == CC_ERROR)
					return CC_ERROR;
			}
			el->edited_line.cursor = ocursor;
			if (ret == CC_ERROR)
				edited_re_refresh(el);
		}
		if (done || ret != CC_NORM)
			return ret;
	}
}


/* edited_cv_search():
 *	Vi search.
 */
libedited_private edited_action_t
edited_cv_search(Edited *el, int dir)
{
	wchar_t ch;
	wchar_t tmpbuf[EL_BUFSIZ];
	ssize_t tmplen;

#ifdef ANCHOR
	tmpbuf[0] = '.';
	tmpbuf[1] = '*';
#endif
	tmplen = LEN;

	el->edited_search.patdir = dir;

	tmplen = edited_c_gets(el, &tmpbuf[LEN],
		dir == EDITED_ED_SEARCH_PREV_HISTORY ? L"\n/" : L"\n?" );
	if (tmplen == -1)
		return CC_REFRESH;

	tmplen += LEN;
	ch = tmpbuf[tmplen];
	tmpbuf[tmplen] = '\0';

	if (tmplen == LEN) {
		/*
		 * Use the old pattern, but wild-card it.
		 */
		if (el->edited_search.patlen == 0) {
			edited_re_refresh(el);
			return CC_ERROR;
		}
#ifdef ANCHOR
		if (el->edited_search.patbuf[0] != '.' &&
		    el->edited_search.patbuf[0] != '*') {
			(void) wcsncpy(tmpbuf, el->edited_search.patbuf,
			    sizeof(tmpbuf) / sizeof(*tmpbuf) - 1);
			el->edited_search.patbuf[0] = '.';
			el->edited_search.patbuf[1] = '*';
			(void) wcsncpy(&el->edited_search.patbuf[2], tmpbuf,
			    EL_BUFSIZ - 3);
			el->edited_search.patlen++;
			el->edited_search.patbuf[el->edited_search.patlen++] = '.';
			el->edited_search.patbuf[el->edited_search.patlen++] = '*';
			el->edited_search.patbuf[el->edited_search.patlen] = '\0';
		}
#endif
	} else {
#ifdef ANCHOR
		tmpbuf[tmplen++] = '.';
		tmpbuf[tmplen++] = '*';
#endif
		tmpbuf[tmplen] = '\0';
		(void) wcsncpy(el->edited_search.patbuf, tmpbuf, EL_BUFSIZ - 1);
		el->edited_search.patlen = (size_t)tmplen;
	}
	el->edited_state.lastcmd = (edited_action_t) dir;	/* avoid edited_c_setpat */
	el->edited_line.cursor = el->edited_line.lastchar = el->edited_line.buffer;
	if ((dir == EDITED_ED_SEARCH_PREV_HISTORY ? edited_ed_search_prev_history(el, 0) :
	    edited_ed_search_next_history(el, 0)) == CC_ERROR) {
		edited_re_refresh(el);
		return CC_ERROR;
	}
	if (ch == 0033) {
		edited_re_refresh(el);
		return edited_ed_newline(el, 0);
	}
	return CC_REFRESH;
}


/* edited_ce_search_line():
 *	Look for a pattern inside a line
 */
libedited_private edited_action_t
edited_ce_search_line(Edited *el, int dir)
{
	wchar_t *cp = el->edited_line.cursor;
	wchar_t *pattern = el->edited_search.patbuf;
	wchar_t oc, *ocp;
#ifdef ANCHOR
	ocp = &pattern[1];
	oc = *ocp;
	*ocp = '^';
#else
	ocp = pattern;
	oc = *ocp;
#endif

	if (dir == EDITED_ED_SEARCH_PREV_HISTORY) {
		for (; cp >= el->edited_line.buffer; cp--) {
			if (edited_match(cp, ocp)) {
				*ocp = oc;
				el->edited_line.cursor = cp;
				return CC_NORM;
			}
		}
		*ocp = oc;
		return CC_ERROR;
	} else {
		for (; *cp != '\0' && cp < el->edited_line.limit; cp++) {
			if (edited_match(cp, ocp)) {
				*ocp = oc;
				el->edited_line.cursor = cp;
				return CC_NORM;
			}
		}
		*ocp = oc;
		return CC_ERROR;
	}
}


/* edited_cv_repeat_srch():
 *	Vi repeat search
 */
libedited_private edited_action_t
edited_cv_repeat_srch(Edited *el, wint_t c)
{

#ifdef SDEBUG
	static edited_ct_buffer_t conv;
	(void) fprintf(el->edited_errfile, "dir %d patlen %ld patbuf %s\n",
	    c, el->edited_search.patlen, edited_ct_encode_string(el->edited_search.patbuf, &conv));
#endif

	el->edited_state.lastcmd = (edited_action_t) c;	/* Hack to stop edited_c_setpat */
	el->edited_line.lastchar = el->edited_line.buffer;

	switch (c) {
	case EDITED_ED_SEARCH_NEXT_HISTORY:
		return edited_ed_search_next_history(el, 0);
	case EDITED_ED_SEARCH_PREV_HISTORY:
		return edited_ed_search_prev_history(el, 0);
	default:
		return CC_ERROR;
	}
}


/* edited_cv_csearch():
 *	Vi character search
 */
libedited_private edited_action_t
edited_cv_csearch(Edited *el, int direction, wint_t ch, int count, int tflag)
{
	wchar_t *cp;

	if (ch == 0)
		return CC_ERROR;

	if (ch == (wint_t)-1) {
		wchar_t c;
		if (edited_wgetc(el, &c) != 1)
			return edited_ed_end_of_file(el, 0);
		ch = c;
	}

	/* Save for ';' and ',' commands */
	el->edited_search.chacha = ch;
	el->edited_search.chadir = direction;
	el->edited_search.chatflg = (char)tflag;

	cp = el->edited_line.cursor;
	while (count--) {
		if ((wint_t)*cp == ch)
			cp += direction;
		for (;;cp += direction) {
			if (cp >= el->edited_line.lastchar)
				return CC_ERROR;
			if (cp < el->edited_line.buffer)
				return CC_ERROR;
			if ((wint_t)*cp == ch)
				break;
		}
	}

	if (tflag)
		cp -= direction;

	el->edited_line.cursor = cp;

	if (el->edited_chared.edited_c_vcmd.action != NOP) {
		if (direction > 0)
			el->edited_line.cursor++;
		edited_cv_delfini(el);
		return CC_REFRESH;
	}
	return CC_CURSOR;
}
