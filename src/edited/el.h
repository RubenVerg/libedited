/*	$NetBSD: el.h,v 1.47 2024/05/17 02:59:08 christos Exp $	*/

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
 *	@(#)el.h	8.1 (Berkeley) 6/4/93
 */

/*
 * el.h: Internal structures.
 */
#ifndef _h_el
#define	_h_el
/*
 * Local defaults
 */
#define	KSHVI
#define	VIDEFAULT
#define	ANCHOR

#include "edited/edited.h"
#include "edited/chartype.h"
#include "edited/style.h"

#define	EL_BUFSIZ	((size_t)1024)	/* Maximum line size		*/

#define	HANDLE_SIGNALS	0x001
#define	NO_TTY		0x002
#define	EDIT_DISABLED	0x004
#define	UNBUFFERED	0x008
#define	NARROW_HISTORY	0x040
#define	NO_RESET	0x080
#define	FIXIO		0x100
#define	FROM_ELLINE	0x200

typedef unsigned char edited_action_t;	/* Index to command array	*/

typedef struct coord_t {		/* Position on the screen	*/
	int	h;
	int	v;
} coord_t;

typedef struct edited_line_t {
	wchar_t		*buffer;	/* Input line			*/
	wchar_t	        *cursor;	/* Cursor position		*/
	wchar_t	        *lastchar;	/* Last character		*/
	const wchar_t	*limit;		/* Max position			*/
} edited_line_t;

/*
 * Editor state
 */
typedef struct edited_state_t {
	int		inputmode;	/* What mode are we in?		*/
	int		doingarg;	/* Are we getting an argument?	*/
	int		argument;	/* Numeric argument		*/
	int		metanext;	/* Is the next char a meta char */
	edited_action_t	lastcmd;	/* Previous command		*/
	edited_action_t	thiscmd;	/* this command			*/
	wchar_t		thisch;		/* char that generated it	*/
} edited_state_t;

/*
 * Until we come up with something better...
 */
#define	edited_malloc(a)	malloc(a)
#define	edited_calloc(a,b)	calloc(a, b)
#define	edited_realloc(a,b)	realloc(a, b)
#define	edited_free(a)	free(a)

#include "tty.h"
#include "prompt.h"
#include "literal.h"
#include "keymacro.h"
#include "terminal.h"
#include "refresh.h"
#include "chared.h"
#include "search.h"
#include "hist.h"
#include "map.h"
#include "sig.h"

struct edited_read_t;

struct edited {
	wchar_t		 *edited_prog;	/* the program name		*/
	FILE		 *edited_infile;	/* Stdio stuff			*/
	FILE		 *edited_outfile;	/* Stdio stuff			*/
	FILE		 *edited_errfile;	/* Stdio stuff			*/
	int		  edited_infd;	/* Input file descriptor	*/
	int		  edited_outfd;	/* Output file descriptor	*/
	int		  edited_errfd;	/* Error file descriptor	*/
	int		  edited_flags;	/* Various flags.		*/
	coord_t		  edited_cursor;	/* Cursor location		*/
	wint_t		**edited_display;	/* Real screen image = what is there */
	wint_t		**edited_vdisplay;	/* Virtual screen image = what we see */
	void		 *edited_data;	/* Client data			*/
	edited_line_t	  edited_line;	/* The current line information	*/
	edited_state_t	  edited_state;	/* Current editor state		*/
	edited_terminal_t	  edited_terminal;	/* Terminal dependent stuff	*/
	edited_tty_t	  edited_tty;	/* Tty dependent stuff		*/
	edited_refresh_t	  edited_refresh;	/* Refresh stuff		*/
	edited_prompt_t	  edited_prompt;	/* Prompt stuff			*/
	edited_prompt_t	  edited_rprompt;	/* Prompt stuff			*/
	edited_literal_t	  edited_literal;	/* prompt literal bits		*/
	edited_chared_t	  edited_chared;	/* Characted editor stuff	*/
	edited_map_t	  edited_map;	/* Key mapping stuff		*/
	edited_keymacro_t	  edited_keymacro;	/* Key binding stuff		*/
	edited_history_t	  edited_history;	/* History stuff		*/
	edited_search_t	  edited_search;	/* Search stuff			*/
	edited_signal_t	  edited_signal;	/* Signal handling stuff	*/
	struct edited_read_t *edited_read;	/* Character reading stuff	*/
	edited_ct_buffer_t       edited_visual;    /* Buffer for displayable str	*/
	edited_ct_buffer_t       edited_scratch;   /* Scratch conversion buffer    */
	edited_ct_buffer_t       edited_lgcyconv;  /* Buffer for legacy wrappers   */
	LineInfo          edited_lgcylinfo; /* Legacy LineInfo buffer       */
	int edited_use_style; /* Use styled and colored input */
	edited_stylefunc_t edited_style_func; /* Function to style the line of input */
	edited_style_t **edited_vstyle; /* Style of each character in vdisplay */
};

libedited_private int	edited_editmode(Edited *, int, const wchar_t **);
libedited_private Edited *edited_init_internal(const char *, FILE *, FILE *,
    FILE *, int, int, int, int);

#ifdef DEBUG
#define	EL_ABORT(a)	do { \
				fprintf(el->edited_errfile, "%s, %d: ", \
					 __FILE__, __LINE__); \
				fprintf a; \
				abort(); \
			} while( /*CONSTCOND*/0);
#else
#define EL_ABORT(a)	abort()
#endif
#endif /* _h_el */
