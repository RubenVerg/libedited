/*	$NetBSD: search.h,v 1.14 2016/05/09 21:46:56 christos Exp $	*/

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
 *
 *	@(#)search.h	8.1 (Berkeley) 6/4/93
 */

/*
 * el.search.h: Line and history searching utilities
 */
#ifndef _h_search
#define	_h_search

typedef struct edited_search_t {
	wchar_t	*patbuf;		/* The pattern buffer		*/
	size_t	 patlen;		/* Length of the pattern buffer	*/
	int	 patdir;		/* Direction of the last search	*/
	int	 chadir;		/* Character search direction	*/
	wchar_t	 chacha;		/* Character we are looking for	*/
	char	 chatflg;		/* 0 if f, 1 if t */
} edited_search_t;


libedited_private int		edited_match(const wchar_t *, const wchar_t *);
libedited_private int		search_init(Edited *);
libedited_private void		search_end(Edited *);
libedited_private int		edited_c_hmatch(Edited *, const wchar_t *);
libedited_private void		edited_c_setpat(Edited *);
libedited_private edited_action_t	edited_ce_inc_search(Edited *, int);
libedited_private edited_action_t	edited_cv_search(Edited *, int);
libedited_private edited_action_t	edited_ce_search_line(Edited *, int);
libedited_private edited_action_t	edited_cv_repeat_srch(Edited *, wint_t);
libedited_private edited_action_t	edited_cv_csearch(Edited *, int, wint_t, int, int);

#endif /* _h_search */
