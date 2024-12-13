/*	$NetBSD: terminal.h,v 1.9 2016/05/09 21:46:56 christos Exp $	*/

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
 *	@(#)term.h	8.1 (Berkeley) 6/4/93
 */

/*
 * el.term.h: Termcap header
 */
#ifndef _h_terminal
#define	_h_terminal

typedef struct {		/* Symbolic function key bindings	*/
	const wchar_t	*name;	/* name of the key			*/
	int		 key;	/* Index in termcap table		*/
	edited_km_value_t	 fun;	/* Function bound to it			*/
	int		 type;	/* Type of function			*/
} funckey_t;

typedef struct {
	const char *t_name;		/* the terminal name	*/
	coord_t	  t_size;		/* # lines and cols	*/
	int	  t_flags;
#define	TERM_CAN_INSERT		0x001	/* Has insert cap	*/
#define	TERM_CAN_DELETE		0x002	/* Has delete cap	*/
#define	TERM_CAN_CEOL		0x004	/* Has CEOL cap		*/
#define	TERM_CAN_TAB		0x008	/* Can use tabs		*/
#define	TERM_CAN_ME		0x010	/* Can turn all attrs.	*/
#define	TERM_CAN_UP		0x020	/* Can move up		*/
#define	TERM_HAS_META		0x040	/* Has a meta key	*/
#define	TERM_HAS_AUTO_MARGINS	0x080	/* Has auto margins	*/
#define	TERM_HAS_MAGIC_MARGINS	0x100	/* Has magic margins	*/
	char	 *t_buf;		/* Termcap buffer	*/
	size_t	  t_loc;		/* location used	*/
	char	**t_str;		/* termcap strings	*/
	int	 *t_val;		/* termcap values	*/
	char	 *t_cap;		/* Termcap buffer	*/
	funckey_t	 *t_fkey;		/* Array of keys	*/
} edited_terminal_t;

/*
 * fKey indexes
 */
#define	A_K_DN		0
#define	A_K_UP		1
#define	A_K_LT		2
#define	A_K_RT		3
#define	A_K_HO		4
#define	A_K_EN		5
#define	A_K_DE		6
#define	A_K_NKEYS	7

#ifdef __sun
extern int tgetent(char *, const char *);
extern int tgetflag(char *);
extern int tgetnum(char *);
extern int tputs(const char *, int, int (*)(int));
extern char* tgoto(const char*, int, int);
extern char* tgetstr(char*, char**);
#endif

libedited_private void	edited_term_move_to_line(Edited *, int);
libedited_private void	edited_term_move_to_char(Edited *, int);
libedited_private void	edited_term_clear_EOL(Edited *, int);
libedited_private void	edited_term_overwrite(Edited *, const wchar_t *, size_t);
libedited_private void	edited_term_insertwrite(Edited *, wchar_t *, int);
libedited_private void	edited_term_deletechars(Edited *, int);
libedited_private void	edited_term_clear_screen(Edited *);
libedited_private void	edited_term_beep(Edited *);
libedited_private int	edited_term_change_size(Edited *, int, int);
libedited_private int	edited_term_get_size(Edited *, int *, int *);
libedited_private int	edited_term_init(Edited *);
libedited_private void	edited_term_bind_arrow(Edited *);
libedited_private void	edited_term_print_arrow(Edited *, const wchar_t *);
libedited_private int	edited_term_clear_arrow(Edited *, const wchar_t *);
libedited_private int	edited_term_set_arrow(Edited *, const wchar_t *,
    edited_km_value_t *, int);
libedited_private void	edited_term_end(Edited *);
libedited_private void	edited_term_get(Edited *, const char **);
libedited_private int	edited_term_set(Edited *, const char *);
libedited_private int	edited_term_settc(Edited *, int, const wchar_t **);
libedited_private int	edited_term_gettc(Edited *, int, char **);
libedited_private int	edited_term_telltc(Edited *, int, const wchar_t **);
libedited_private int	edited_term_echotc(Edited *, int, const wchar_t **);
libedited_private void	edited_term_writec(Edited *, wint_t);
libedited_private int	edited_term__putc(Edited *, wint_t);
libedited_private void	edited_term__flush(Edited *);
libedited_private void edited_term_overwrite_styled(Edited *, const wchar_t *, size_t, edited_style_t *);
libedited_private void edited_term_insertwrite_styled(Edited *, wchar_t *, int, edited_style_t *);

/*
 * Easy access macros
 */
#define	EL_FLAGS	(el)->edited_terminal.t_flags

#define	EL_CAN_INSERT		(EL_FLAGS & TERM_CAN_INSERT)
#define	EL_CAN_DELETE		(EL_FLAGS & TERM_CAN_DELETE)
#define	EL_CAN_CEOL		(EL_FLAGS & TERM_CAN_CEOL)
#define	EL_CAN_TAB		(EL_FLAGS & TERM_CAN_TAB)
#define	EL_CAN_ME		(EL_FLAGS & TERM_CAN_ME)
#define EL_CAN_UP		(EL_FLAGS & TERM_CAN_UP)
#define	EL_HAS_META		(EL_FLAGS & TERM_HAS_META)
#define	EL_HAS_AUTO_MARGINS	(EL_FLAGS & TERM_HAS_AUTO_MARGINS)
#define	EL_HAS_MAGIC_MARGINS	(EL_FLAGS & TERM_HAS_MAGIC_MARGINS)

#endif /* _h_terminal */
