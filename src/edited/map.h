/*	$NetBSD: map.h,v 1.13 2016/05/09 21:46:56 christos Exp $	*/

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
 *	@(#)map.h	8.1 (Berkeley) 6/4/93
 */

/*
 * el.map.h:	Editor maps
 */
#ifndef _h_map
#define	_h_map

typedef edited_action_t (*edited_func_t)(Edited *, wint_t);

typedef struct edited_bindings_t {	/* for the "bind" shell command */
	const wchar_t	*name;		/* function name for bind command */
	int		 func;		/* function numeric value */
	const wchar_t	*description;	/* description of function */
} edited_bindings_t;

typedef struct edited_map_t {
	edited_action_t	*alt;		/* The current alternate key map */
	edited_action_t	*key;		/* The current normal key map	*/
	edited_action_t	*current;	/* The keymap we are using	*/
	const edited_action_t *emacs;	/* The default emacs key map	*/
	const edited_action_t *vic;		/* The vi command mode key map	*/
	const edited_action_t *vii;		/* The vi insert mode key map	*/
	int		 type;		/* Emacs or vi			*/
	edited_bindings_t	*help;		/* The help for the editor functions */
	edited_func_t	*func;		/* List of available functions	*/
	size_t		 nfunc;		/* The number of functions/help items */
} edited_map_t;

#define	MAP_EMACS	0
#define	MAP_VI		1

#define N_KEYS      256

libedited_private int	edited_map_bind(Edited *, int, const wchar_t **);
libedited_private int	edited_map_init(Edited *);
libedited_private void	edited_map_end(Edited *);
libedited_private void	edited_map_init_vi(Edited *);
libedited_private void	edited_map_init_emacs(Edited *);
libedited_private int	edited_map_set_editor(Edited *, wchar_t *);
libedited_private int	edited_map_get_editor(Edited *, const wchar_t **);
libedited_private int	edited_map_addfunc(Edited *, const wchar_t *, const wchar_t *,
    edited_func_t);

#endif /* _h_map */
