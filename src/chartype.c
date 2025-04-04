/*	$NetBSD: chartype.c,v 1.37 2023/08/10 20:38:00 mrg Exp $	*/

/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * chartype.c: character classification and meta information
 */
#include "config.h"
#if !defined(lint) && !defined(SCCSID)
__RCSID("$NetBSD: chartype.c,v 1.37 2023/08/10 20:38:00 mrg Exp $");
#endif /* not lint && not SCCSID */

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "edited/el.h"

#define CT_BUFSIZ ((size_t)1024)

static int edited_ct_conv_cbuff_resize(edited_ct_buffer_t *, size_t);
static int edited_ct_conv_wbuff_resize(edited_ct_buffer_t *, size_t);

static int
edited_ct_conv_cbuff_resize(edited_ct_buffer_t *conv, size_t csize)
{
	void *p;

	if (csize <= conv->csize)
		return 0;

	conv->csize = csize;

	p = edited_realloc(conv->cbuff, conv->csize * sizeof(*conv->cbuff));
	if (p == NULL) {
		conv->csize = 0;
		edited_free(conv->cbuff);
		conv->cbuff = NULL;
		return -1;
	}
	conv->cbuff = p;
	return 0;
}

static int
edited_ct_conv_wbuff_resize(edited_ct_buffer_t *conv, size_t wsize)
{
	void *p;

	if (wsize <= conv->wsize)
		return 0;

	conv->wsize = wsize;

	p = edited_realloc(conv->wbuff, conv->wsize * sizeof(*conv->wbuff));
	if (p == NULL) {
		conv->wsize = 0;
		edited_free(conv->wbuff);
		conv->wbuff = NULL;
		return -1;
	}
	conv->wbuff = p;
	return 0;
}


char *
edited_ct_encode_string(const wchar_t *s, edited_ct_buffer_t *conv)
{
	char *dst;
	ssize_t used;

	if (!s)
		return NULL;

	dst = conv->cbuff;
	for (;;) {
		used = (ssize_t)(dst - conv->cbuff);
		if ((conv->csize - (size_t)used) < 5) {
			if (edited_ct_conv_cbuff_resize(conv,
			    conv->csize + CT_BUFSIZ) == -1)
				return NULL;
			dst = conv->cbuff + used;
		}
		if (!*s)
			break;
		used = edited_ct_encode_char(dst, (size_t)5, *s);
		if (used == -1) /* failed to encode, need more buffer space */
			abort();
		++s;
		dst += used;
	}
	*dst = '\0';
	return conv->cbuff;
}

wchar_t *
edited_ct_decode_string(const char *s, edited_ct_buffer_t *conv)
{
	size_t len;

	if (!s)
		return NULL;

	len = mbstowcs(NULL, s, (size_t)0);
	if (len == (size_t)-1)
		return NULL;

	if (conv->wsize < ++len)
		if (edited_ct_conv_wbuff_resize(conv, len + CT_BUFSIZ) == -1)
			return NULL;

	mbstowcs(conv->wbuff, s, conv->wsize);
	return conv->wbuff;
}


libedited_private wchar_t **
edited_ct_decode_argv(int argc, const char *argv[], edited_ct_buffer_t *conv)
{
	size_t bufspace;
	int i;
	wchar_t *p;
	wchar_t **wargv;
	ssize_t bytes;

	/* Make sure we have enough space in the conversion buffer to store all
	 * the argv strings. */
	for (i = 0, bufspace = 0; i < argc; ++i)
		bufspace += argv[i] ? strlen(argv[i]) + 1 : 0;
	if (conv->wsize < ++bufspace)
		if (edited_ct_conv_wbuff_resize(conv, bufspace + CT_BUFSIZ) == -1)
			return NULL;

	wargv = edited_calloc((size_t)(argc + 1), sizeof(*wargv));
	if (wargv == NULL)
		return NULL;

	for (i = 0, p = conv->wbuff; i < argc; ++i) {
		if (!argv[i]) {   /* don't pass null pointers to mbstowcs */
			wargv[i] = NULL;
			continue;
		} else {
			wargv[i] = p;
			bytes = (ssize_t)mbstowcs(p, argv[i], bufspace);
		}
		if (bytes == -1) {
			edited_free(wargv);
			return NULL;
		} else
			bytes++;  /* include '\0' in the count */
		bufspace -= (size_t)bytes;
		p += bytes;
	}
	wargv[i] = NULL;

	return wargv;
}


libedited_private size_t
edited_ct_enc_width(wchar_t c)
{
	mbstate_t mbs;
	char buf[MB_LEN_MAX];
	size_t size;
	memset(&mbs, 0, sizeof(mbs));

	if ((size = wcrtomb(buf, c, &mbs)) == (size_t)-1)
		return 0;
	return size;
}

libedited_private ssize_t
edited_ct_encode_char(char *dst, size_t len, wchar_t c)
{
	ssize_t l = 0;
	if (len < edited_ct_enc_width(c))
		return -1;
	l = wctomb(dst, c);

	if (l < 0) {
		wctomb(NULL, L'\0');
		l = 0;
	}
	return l;
}

libedited_private const wchar_t *
edited_ct_visual_string(const wchar_t *s, edited_ct_buffer_t *conv)
{
	wchar_t *dst;
	ssize_t used;

	if (!s)
		return NULL;

	if (edited_ct_conv_wbuff_resize(conv, CT_BUFSIZ) == -1)
		return NULL;

	used = 0;
	dst = conv->wbuff;
	while (*s) {
		used = edited_ct_visual_char(dst,
		    conv->wsize - (size_t)(dst - conv->wbuff), *s);
		if (used != -1) {
			++s;
			dst += used;
			continue;
		}

		/* failed to encode, need more buffer space */
		uintptr_t sused = (uintptr_t)dst - (uintptr_t)conv->wbuff;
		if (edited_ct_conv_wbuff_resize(conv, conv->wsize + CT_BUFSIZ) == -1)
			return NULL;
		dst = conv->wbuff + sused;
	}

	if (dst >= (conv->wbuff + conv->wsize)) { /* sigh */
		uintptr_t sused = (uintptr_t)dst - (uintptr_t)conv->wbuff;
		if (edited_ct_conv_wbuff_resize(conv, conv->wsize + CT_BUFSIZ) == -1)
			return NULL;
		dst = conv->wbuff + sused;
	}

	*dst = L'\0';
	return conv->wbuff;
}



libedited_private int
edited_ct_visual_width(wchar_t c)
{
	int t = edited_ct_chr_class(c);
	switch (t) {
	case CHTYPE_ASCIICTL:
		return 2; /* ^@ ^? etc. */
	case CHTYPE_TAB:
		return 1; /* Hmm, this really need to be handled outside! */
	case CHTYPE_NL:
		return 0; /* Should this be 1 instead? */
	case CHTYPE_PRINT:
		return wcwidth(c);
	case CHTYPE_NONPRINT:
		if (c > 0xffff) /* prefer standard 4-byte display over 5-byte */
			return 8; /* \U+12345 */
		else
			return 7; /* \U+1234 */
	default:
		return 0; /* should not happen */
	}
}


libedited_private ssize_t
edited_ct_visual_char(wchar_t *dst, size_t len, wchar_t c)
{
	int t = edited_ct_chr_class(c);
	switch (t) {
	case CHTYPE_TAB:
	case CHTYPE_NL:
	case CHTYPE_ASCIICTL:
		if (len < 2)
			return -1;   /* insufficient space */
		*dst++ = '^';
		if (c == '\177')
			*dst = '?'; /* DEL -> ^? */
		else
			*dst = c | 0100;    /* uncontrolify it */
		return 2;
	case CHTYPE_PRINT:
		if (len < 1)
			return -1;  /* insufficient space */
		*dst = c;
		return 1;
	case CHTYPE_NONPRINT:
		/* we only use single-width glyphs for display,
		 * so this is right */
		if ((ssize_t)len < edited_ct_visual_width(c))
			return -1;   /* insufficient space */
		*dst++ = '\\';
		*dst++ = 'U';
		*dst++ = '+';
#define tohexdigit(v) "0123456789ABCDEF"[v]
		if (c > 0xffff) /* prefer standard 4-byte display over 5-byte */
			*dst++ = tohexdigit(((unsigned int) c >> 16) & 0xf);
		*dst++ = tohexdigit(((unsigned int) c >> 12) & 0xf);
		*dst++ = tohexdigit(((unsigned int) c >>  8) & 0xf);
		*dst++ = tohexdigit(((unsigned int) c >>  4) & 0xf);
		*dst   = tohexdigit(((unsigned int) c      ) & 0xf);
		return c > 0xffff ? 8 : 7;
		/*FALLTHROUGH*/
	/* these two should be handled outside this function */
	default:            /* we should never hit the default */
		return 0;
	}
}




libedited_private int
edited_ct_chr_class(wchar_t c)
{
	if (c == '\t')
		return CHTYPE_TAB;
	else if (c == '\n')
		return CHTYPE_NL;
	else if (c < 0x100 && iswcntrl(c))
		return CHTYPE_ASCIICTL;
	else if (iswprint(c))
		return CHTYPE_PRINT;
	else
		return CHTYPE_NONPRINT;
}
