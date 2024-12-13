/*	$NetBSD: hist.c,v 1.34 2019/07/23 10:19:35 christos Exp $	*/

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
static char sccsid[] = "@(#)hist.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: hist.c,v 1.34 2019/07/23 10:19:35 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * hist.c: History access functions
 */
#include <stdlib.h>
#include <string.h>
#include "edited/vis.h"

#include "edited/el.h"

/* hist_init():
 *	Initialization function.
 */
libedited_private int
hist_init(Edited *el)
{

	el->edited_history.fun = NULL;
	el->edited_history.ref = NULL;
	el->edited_history.buf = edited_calloc(EL_BUFSIZ, sizeof(*el->edited_history.buf));
	if (el->edited_history.buf == NULL)
		return -1;
	el->edited_history.sz  = EL_BUFSIZ;
	el->edited_history.last = el->edited_history.buf;
	return 0;
}


/* hist_end():
 *	clean up history;
 */
libedited_private void
hist_end(Edited *el)
{

	edited_free(el->edited_history.buf);
	el->edited_history.buf = NULL;
}


/* hist_set():
 *	Set new history interface
 */
libedited_private int
hist_set(Edited *el, hist_fun_t fun, void *ptr)
{

	el->edited_history.ref = ptr;
	el->edited_history.fun = fun;
	return 0;
}


/* hist_get():
 *	Get a history line and update it in the buffer.
 *	eventno tells us the event to get.
 */
libedited_private edited_action_t
hist_get(Edited *el)
{
	const wchar_t *hp;
	int h;
	size_t blen, hlen;

	if (el->edited_history.eventno == 0) {	/* if really the current line */
		(void) wcsncpy(el->edited_line.buffer, el->edited_history.buf,
		    el->edited_history.sz);
		el->edited_line.lastchar = el->edited_line.buffer +
		    (el->edited_history.last - el->edited_history.buf);

#ifdef KSHVI
		if (el->edited_map.type == MAP_VI)
			el->edited_line.cursor = el->edited_line.buffer;
		else
#endif /* KSHVI */
			el->edited_line.cursor = el->edited_line.lastchar;

		return CC_REFRESH;
	}
	if (el->edited_history.ref == NULL)
		return CC_ERROR;

	hp = HIST_FIRST(el);

	if (hp == NULL)
		return CC_ERROR;

	for (h = 1; h < el->edited_history.eventno; h++)
		if ((hp = HIST_NEXT(el)) == NULL)
			goto out;

	hlen = wcslen(hp) + 1;
	blen = (size_t)(el->edited_line.limit - el->edited_line.buffer);
	if (hlen > blen && !ch_enlargebufs(el, hlen))
		goto out;

	memcpy(el->edited_line.buffer, hp, hlen * sizeof(*hp));
	el->edited_line.lastchar = el->edited_line.buffer + hlen - 1;

	if (el->edited_line.lastchar > el->edited_line.buffer
	    && el->edited_line.lastchar[-1] == '\n')
		el->edited_line.lastchar--;
	if (el->edited_line.lastchar > el->edited_line.buffer
	    && el->edited_line.lastchar[-1] == ' ')
		el->edited_line.lastchar--;
#ifdef KSHVI
	if (el->edited_map.type == MAP_VI)
		el->edited_line.cursor = el->edited_line.buffer;
	else
#endif /* KSHVI */
		el->edited_line.cursor = el->edited_line.lastchar;

	return CC_REFRESH;
out:
	el->edited_history.eventno = h;
	return CC_ERROR;

}


/* hist_command()
 *	process a history command
 */
libedited_private int
hist_command(Edited *el, int argc, const wchar_t **argv)
{
	const wchar_t *str;
	int num;
	HistEventW ev;

	if (el->edited_history.ref == NULL)
		return -1;

	if (argc == 1 || wcscmp(argv[1], L"list") == 0) {
		size_t maxlen = 0;
		char *buf = NULL;
		int hno = 1;
		 /* List history entries */

		for (str = HIST_LAST(el); str != NULL; str = HIST_PREV(el)) {
			char *ptr =
			    edited_ct_encode_string(str, &el->edited_scratch);
			size_t len = strlen(ptr);
			if (len > 0 && ptr[len - 1] == '\n') 
				ptr[--len] = '\0';
			len = len * 4 + 1;
			if (len >= maxlen) {
				maxlen = len + 1024;
				char *nbuf = edited_realloc(buf, maxlen);
				if (nbuf == NULL) {
					edited_free(buf);
					return -1;
				}
				buf = nbuf;
			}
			strvis(buf, ptr, VIS_NL);
			(void) fprintf(el->edited_outfile, "%d\t%s\n",
			    hno++, buf);
		}
		edited_free(buf);
		return 0;
	}

	if (argc != 3)
		return -1;

	num = (int)wcstol(argv[2], NULL, 0);

	if (wcscmp(argv[1], L"size") == 0)
		return history_w(el->edited_history.ref, &ev, H_SETSIZE, num);

	if (wcscmp(argv[1], L"unique") == 0)
		return history_w(el->edited_history.ref, &ev, H_SETUNIQUE, num);

	return -1;
}

/* hist_enlargebuf()
 *	Enlarge history buffer to specified value. Called from edited_enlargebufs().
 *	Return 0 for failure, 1 for success.
 */
libedited_private int
/*ARGSUSED*/
hist_enlargebuf(Edited *el, size_t oldsz, size_t newsz)
{
	wchar_t *newbuf;

	newbuf = edited_realloc(el->edited_history.buf, newsz * sizeof(*newbuf));
	if (!newbuf)
		return 0;

	(void) memset(&newbuf[oldsz], '\0', (newsz - oldsz) * sizeof(*newbuf));

	el->edited_history.last = newbuf +
				(el->edited_history.last - el->edited_history.buf);
	el->edited_history.buf = newbuf;
	el->edited_history.sz  = newsz;

	return 1;
}

libedited_private wchar_t *
hist_convert(Edited *el, int fn, void *arg)
{
	HistEventW ev;
	if ((*(el)->edited_history.fun)((el)->edited_history.ref, &ev, fn, arg) == -1)
		return NULL;
	return edited_ct_decode_string((const char *)(const void *)ev.str,
	    &el->edited_scratch);
}
