/*	$NetBSD: histedit.h,v 1.62 2023/02/03 22:01:42 christos Exp $	*/

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
 *	@(#)histedit.h	8.2 (Berkeley) 1/3/94
 */

/*
 * edited.h: Line editor and history interface.
 */
#ifndef _h_edited
#define	_h_edited

#define	LIBEDIT_MAJOR 3
#define	LIBEDIT_MINOR 0

#include <sys/types.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ==== Editing ====
 */

typedef struct edited Edited;

/*
 * For user-defined function interface
 */
typedef struct lineinfo {
	const char	*buffer;
	const char	*cursor;
	const char	*lastchar;
} LineInfo;

/*
 * EditLine editor function return codes.
 * For user-defined function interface
 */
#define	CC_NORM		0
#define	CC_NEWLINE	1
#define	CC_EOF		2
#define	CC_ARGHACK	3
#define	CC_REFRESH	4
#define	CC_CURSOR	5
#define	CC_ERROR	6
#define	CC_FATAL	7
#define	CC_REDISPLAY	8
#define	CC_REFRESH_BEEP	9

/*
 * Initialization, cleanup, and resetting
 */
Edited	*edited_init(const char *, FILE *, FILE *, FILE *);
Edited	*edited_init_fd(const char *, FILE *, FILE *, FILE *,
    int, int, int);
void		 edited_end(Edited *);
void		 edited_reset(Edited *);

/*
 * Get a line, a character or push a string back in the input queue
 */
const char	*edited_gets(Edited *, int *);
int		 edited_getc(Edited *, char *);
void		 edited_push(Edited *, const char *);

/*
 * Beep!
 */
void		 edited_beep(Edited *);

/*
 * High level function internals control
 * Parses argc, argv array and executes builtin editline commands
 */
int		 edited_parse(Edited *, int, const char **);

/*
 * Low level editline access functions
 */
int		 edited_set(Edited *, int, ...);
int		 edited_get(Edited *, int, ...);
unsigned char	_fn_complete(Edited *, int);
unsigned char	_fn_sh_complete(Edited *, int);

/*
 * edited_set/edited_get parameters
 *
 * When using edited_wset/edited_wget (as opposed to edited_set/edited_get):
 *   Char is wchar_t, otherwise it is char.
 *   edited_prompt_func is edited_wpfunc_t, otherwise it is edited_pfunc_t .

 * Prompt function prototypes are:
 *   typedef char    *(*edited_pfunct_t)  (EditLine *);
 *   typedef wchar_t *(*edited_wpfunct_t) (EditLine *);
 *
 * For operations that support set or set/get, the argument types listed are for
 * the "set" operation. For "get", each listed type must be a pointer.
 * E.g. EL_EDITMODE takes an int when set, but an int* when get.
 *
 * Operations that only support "get" have the correct argument types listed.
 */
#define	EL_PROMPT	0	/* , edited_prompt_func);		      set/get */
#define	EL_TERMINAL	1	/* , const char *);		      set/get */
#define	EL_EDITOR	2	/* , const Char *);		      set/get */
#define	EL_SIGNAL	3	/* , int);			      set/get */
#define	EL_BIND		4	/* , const Char *, ..., NULL);	      set     */
#define	EL_TELLTC	5	/* , const Char *, ..., NULL);	      set     */
#define	EL_SETTC	6	/* , const Char *, ..., NULL);	      set     */
#define	EL_ECHOTC	7	/* , const Char *, ..., NULL);        set     */
#define	EL_SETTY	8	/* , const Char *, ..., NULL);        set     */
#define	EL_ADDFN	9	/* , const Char *, const Char,        set     */
				/*   edited_func_t);			      */
#define	EL_HIST		10	/* , hist_fun_t, const void *);	      set     */
#define	EL_EDITMODE	11	/* , int);			      set/get */
#define	EL_RPROMPT	12	/* , edited_prompt_func);		      set/get */
#define	EL_GETCFN	13	/* , edited_rfunc_t);		      set/get */
#define	EL_CLIENTDATA	14	/* , void *);			      set/get */
#define	EL_UNBUFFERED	15	/* , int);			      set/get */
#define	EL_PREP_TERM	16	/* , int);			      set     */
#define	EL_GETTC	17	/* , const Char *, ..., NULL);		  get */
#define	EL_GETFP	18	/* , int, FILE **);		          get */
#define	EL_SETFP	19	/* , int, FILE *);		      set     */
#define	EL_REFRESH	20	/* , void);			      set     */
#define	EL_PROMPT_ESC	21	/* , edited_prompt_func, Char);	      set/get */
#define	EL_RPROMPT_ESC	22	/* , edited_prompt_func, Char);	      set/get */
#define	EL_RESIZE	23	/* , edited_zfunc_t, void *);	      set     */
#define	EL_ALIAS_TEXT	24	/* , edited_afunc_t, void *);	      set     */
#define	EL_SAFEREAD	25	/* , int);			      set/get */

#define	EL_BUILTIN_GETCFN	(NULL)

/*
 * Source named file or $PWD/.editrc or $HOME/.editrc
 */
int		edited_source(Edited *, const char *);

/*
 * Must be called when the terminal changes size; If EL_SIGNAL
 * is set this is done automatically otherwise it is the responsibility
 * of the application
 */
void		 edited_resize(Edited *);

/*
 * User-defined function interface.
 */
const LineInfo	*edited_line(Edited *);
int		 edited_insertstr(Edited *, const char *);
void		 edited_deletestr(Edited *, int);
int		 edited_replacestr(Edited *, const char *);
int		 edited_deletestr1(Edited *, int, int);

/*
 * ==== History ====
 */

typedef struct history History;

typedef struct HistEvent {
	int		 num;
	const char	*str;
} HistEvent;

/*
 * History access functions.
 */
History *	history_init(void);
void		history_end(History *);

int		history(History *, HistEvent *, int, ...);

#define	H_FUNC		 0	/* , UTSL		*/
#define	H_SETSIZE	 1	/* , const int);	*/
#define	H_GETSIZE	 2	/* , void);		*/
#define	H_FIRST		 3	/* , void);		*/
#define	H_LAST		 4	/* , void);		*/
#define	H_PREV		 5	/* , void);		*/
#define	H_NEXT		 6	/* , void);		*/
#define	H_CURR		 8	/* , const int);	*/
#define	H_SET		 7	/* , int);		*/
#define	H_ADD		 9	/* , const wchar_t *);	*/
#define	H_ENTER		10	/* , const wchar_t *);	*/
#define	H_APPEND	11	/* , const wchar_t *);	*/
#define	H_END		12	/* , void);		*/
#define	H_NEXT_STR	13	/* , const wchar_t *);	*/
#define	H_PREV_STR	14	/* , const wchar_t *);	*/
#define	H_NEXT_EVENT	15	/* , const int);	*/
#define	H_PREV_EVENT	16	/* , const int);	*/
#define	H_LOAD		17	/* , const char *);	*/
#define	H_SAVE		18	/* , const char *);	*/
#define	H_CLEAR		19	/* , void);		*/
#define	H_SETUNIQUE	20	/* , int);		*/
#define	H_GETUNIQUE	21	/* , void);		*/
#define	H_DEL		22	/* , int);		*/
#define	H_NEXT_EVDATA	23	/* , const int, histdata_t *);	*/
#define	H_DELDATA	24	/* , int, histdata_t *);*/
#define	H_REPLACE	25	/* , const char *, histdata_t);	*/
#define	H_SAVE_FP	26	/* , FILE *);		*/
#define	H_NSAVE_FP	27	/* , size_t, FILE *);	*/



/*
 * ==== Tokenization ====
 */

typedef struct tokenizer Tokenizer;

/*
 * String tokenization functions, using simplified sh(1) quoting rules
 */
Tokenizer	*tok_init(const char *);
void		 tok_end(Tokenizer *);
void		 tok_reset(Tokenizer *);
int		 tok_line(Tokenizer *, const LineInfo *,
		    int *, const char ***, int *, int *);
int		 tok_str(Tokenizer *, const char *,
		    int *, const char ***);

/*
 * Begin Wide Character Support
 */
#include <wchar.h>
#include <wctype.h>

#ifndef HAVE_WCSDUP
wchar_t * wcsdup(const wchar_t *str);
#endif

/*
 * ==== Editing ====
 */
typedef struct lineinfow {
	const wchar_t	*buffer;
	const wchar_t	*cursor;
	const wchar_t	*lastchar;
} LineInfoW;

typedef int	(*edited_rfunc_t)(Edited *, wchar_t *);

const wchar_t	*edited_wgets(Edited *, int *);
int		 edited_wgetc(Edited *, wchar_t *);
void		 edited_wpush(Edited *, const wchar_t *);

int		 edited_wparse(Edited *, int, const wchar_t **);

int		 edited_wset(Edited *, int, ...);
int		 edited_wget(Edited *, int, ...);

int		 edited_cursor(Edited *, int);
const LineInfoW	*edited_wline(Edited *);
int		 edited_winsertstr(Edited *, const wchar_t *);
#define          edited_wdeletestr  edited_deletestr
int		 edited_wreplacestr(Edited *, const wchar_t *);

/*
 * ==== History ====
 */
typedef struct histeventW {
	int		 num;
	const wchar_t	*str;
} HistEventW;

typedef struct historyW HistoryW;

HistoryW *	history_winit(void);
void		history_wend(HistoryW *);

int		history_w(HistoryW *, HistEventW *, int, ...);

/*
 * ==== Tokenization ====
 */
typedef struct tokenizerW TokenizerW;

/* Wide character tokenizer support */
TokenizerW	*tok_winit(const wchar_t *);
void		 tok_wend(TokenizerW *);
void		 tok_wreset(TokenizerW *);
int		 tok_wline(TokenizerW *, const LineInfoW *,
		    int *, const wchar_t ***, int *, int *);
int		 tok_wstr(TokenizerW *, const wchar_t *,
		    int *, const wchar_t ***);

#ifdef __cplusplus
}
#endif

#endif /* _h_edited */
