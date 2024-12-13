/*	$NetBSD: literal.c,v 1.5 2019/07/23 13:10:11 christos Exp $	*/

/*-
 * Copyright (c) 2017 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
__RCSID("$NetBSD: literal.c,v 1.5 2019/07/23 13:10:11 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * literal.c: Literal sequences handling.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "edited/el.h"

libedited_private void
edited_lit_init(Edited *el)
{
	edited_literal_t *l = &el->edited_literal;

	memset(l, 0, sizeof(*l));
}

libedited_private void
edited_lit_end(Edited *el)
{
	edited_lit_clear(el);
}

libedited_private void
edited_lit_clear(Edited *el)
{
	edited_literal_t *l = &el->edited_literal;
	size_t i;

	if (l->l_len == 0)
		return;

	for (i = 0; i < l->l_idx; i++)
		edited_free(l->l_buf[i]);
	edited_free(l->l_buf);
	l->l_buf = NULL;
	l->l_len = 0;
	l->l_idx = 0;
}

libedited_private wint_t
edited_lit_add(Edited *el, const wchar_t *buf, const wchar_t *end, int *wp)
{
	edited_literal_t *l = &el->edited_literal;
	size_t i, len;
	ssize_t w, n;
	char *b;

	w = wcwidth(end[1]);	/* column width of the visible char */
	*wp = (int)w;

	if (w <= 0)		/* we require something to be printed */
		return 0;

	len = (size_t)(end - buf);
	for (w = 0, i = 0; i < len; i++)
		w += edited_ct_enc_width(buf[i]);
	w += edited_ct_enc_width(end[1]);

	b = edited_malloc((size_t)(w + 1));
	if (b == NULL)
		return 0;

	for (n = 0, i = 0; i < len; i++)
		n += edited_ct_encode_char(b + n, (size_t)(w - n), buf[i]);
	n += edited_ct_encode_char(b + n, (size_t)(w - n), end[1]);
	b[n] = '\0';

	/*
	 * Then save this literal string in the list of such strings,
	 * and return a "magic character" to put into the terminal buffer.
	 * When that magic char is 'printed' the saved string (which includes
	 * the char that belongs in that position) gets sent instead.
	 */
	if (l->l_idx == l->l_len) {
		char **bp;

		l->l_len += 4;
		bp = edited_realloc(l->l_buf, sizeof(*l->l_buf) * l->l_len);
		if (bp == NULL) {
			free(b);
			l->l_len -= 4;
			return 0;
		}
		l->l_buf = bp;
	}
	l->l_buf[l->l_idx++] = b;
	return EL_LITERAL | (wint_t)(l->l_idx - 1);
}

libedited_private const char *
edited_lit_get(Edited *el, wint_t idx)
{
	edited_literal_t *l = &el->edited_literal;

	assert(idx & EL_LITERAL);
	idx &= ~EL_LITERAL;
	assert(l->l_idx > (size_t)idx);
	return l->l_buf[idx];
}
