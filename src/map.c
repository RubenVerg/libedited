/*	$NetBSD: map.c,v 1.55 2022/10/30 19:11:31 christos Exp $	*/

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
static char sccsid[] = "@(#)map.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: map.c,v 1.55 2022/10/30 19:11:31 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * map.c: Editor function definitions
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "edited/el.h"
#include "edited/common.h"
#include "edited/emacs.h"
#include "edited/vi.h"
#include "edited/fcns.h"
#include "edited/func.h"
#include "edited/help.h"
#include "edited/parse.h"

static void	edited_map_print_key(Edited *, edited_action_t *, const wchar_t *);
static void	edited_map_print_some_keys(Edited *, edited_action_t *, wint_t, wint_t);
static void	edited_map_print_all_keys(Edited *);
static void	edited_map_init_nls(Edited *);
static void	edited_map_init_meta(Edited *);

/* keymap tables ; should be N_KEYS*sizeof(KEYCMD) bytes long */


static const edited_action_t  edited_map_emacs[] = {
	/*   0 */	EDITED_EM_SET_MARK,		/* ^@ */
	/*   1 */	EDITED_ED_MOVE_TO_BEG,		/* ^A */
	/*   2 */	EDITED_ED_PREV_CHAR,		/* ^B */
	/*   3 */	EDITED_ED_IGNORE,		/* ^C */
	/*   4 */	EDITED_EM_DELETE_OR_LIST,	/* ^D */
	/*   5 */	EDITED_ED_MOVE_TO_END,		/* ^E */
	/*   6 */	EDITED_ED_NEXT_CHAR,		/* ^F */
	/*   7 */	EDITED_ED_UNASSIGNED,		/* ^G */
	/*   8 */	EDITED_EM_DELETE_PREV_CHAR,	/* ^H */
	/*   9 */	EDITED_ED_UNASSIGNED,		/* ^I */
	/*  10 */	EDITED_ED_NEWLINE,		/* ^J */
	/*  11 */	EDITED_ED_KILL_LINE,		/* ^K */
	/*  12 */	EDITED_ED_CLEAR_SCREEN,	/* ^L */
	/*  13 */	EDITED_ED_NEWLINE,		/* ^M */
	/*  14 */	EDITED_ED_NEXT_HISTORY,	/* ^N */
	/*  15 */	EDITED_ED_IGNORE,		/* ^O */
	/*  16 */	EDITED_ED_PREV_HISTORY,	/* ^P */
	/*  17 */	EDITED_ED_IGNORE,		/* ^Q */
	/*  18 */	EDITED_EM_INC_SEARCH_PREV,	/* ^R */
	/*  19 */	EDITED_ED_IGNORE,		/* ^S */
	/*  20 */	EDITED_ED_TRANSPOSE_CHARS,	/* ^T */
	/*  21 */	EDITED_EM_KILL_LINE,		/* ^U */
	/*  22 */	EDITED_ED_QUOTED_INSERT,	/* ^V */
	/*  23 */	EDITED_ED_DELETE_PREV_WORD,	/* ^W */
	/*  24 */	EDITED_ED_SEQUENCE_LEAD_IN,	/* ^X */
	/*  25 */	EDITED_EM_YANK,		/* ^Y */
	/*  26 */	EDITED_ED_IGNORE,		/* ^Z */
	/*  27 */	EDITED_EM_META_NEXT,		/* ^[ */
	/*  28 */	EDITED_ED_IGNORE,		/* ^\ */
	/*  29 */	EDITED_ED_IGNORE,		/* ^] */
	/*  30 */	EDITED_ED_UNASSIGNED,		/* ^^ */
	/*  31 */	EDITED_ED_UNASSIGNED,		/* ^_ */
	/*  32 */	EDITED_ED_INSERT,		/* SPACE */
	/*  33 */	EDITED_ED_INSERT,		/* ! */
	/*  34 */	EDITED_ED_INSERT,		/* " */
	/*  35 */	EDITED_ED_INSERT,		/* # */
	/*  36 */	EDITED_ED_INSERT,		/* $ */
	/*  37 */	EDITED_ED_INSERT,		/* % */
	/*  38 */	EDITED_ED_INSERT,		/* & */
	/*  39 */	EDITED_ED_INSERT,		/* ' */
	/*  40 */	EDITED_ED_INSERT,		/* ( */
	/*  41 */	EDITED_ED_INSERT,		/* ) */
	/*  42 */	EDITED_ED_INSERT,		/* * */
	/*  43 */	EDITED_ED_INSERT,		/* + */
	/*  44 */	EDITED_ED_INSERT,		/* , */
	/*  45 */	EDITED_ED_INSERT,		/* - */
	/*  46 */	EDITED_ED_INSERT,		/* . */
	/*  47 */	EDITED_ED_INSERT,		/* / */
	/*  48 */	EDITED_ED_DIGIT,		/* 0 */
	/*  49 */	EDITED_ED_DIGIT,		/* 1 */
	/*  50 */	EDITED_ED_DIGIT,		/* 2 */
	/*  51 */	EDITED_ED_DIGIT,		/* 3 */
	/*  52 */	EDITED_ED_DIGIT,		/* 4 */
	/*  53 */	EDITED_ED_DIGIT,		/* 5 */
	/*  54 */	EDITED_ED_DIGIT,		/* 6 */
	/*  55 */	EDITED_ED_DIGIT,		/* 7 */
	/*  56 */	EDITED_ED_DIGIT,		/* 8 */
	/*  57 */	EDITED_ED_DIGIT,		/* 9 */
	/*  58 */	EDITED_ED_INSERT,		/* : */
	/*  59 */	EDITED_ED_INSERT,		/* ; */
	/*  60 */	EDITED_ED_INSERT,		/* < */
	/*  61 */	EDITED_ED_INSERT,		/* = */
	/*  62 */	EDITED_ED_INSERT,		/* > */
	/*  63 */	EDITED_ED_INSERT,		/* ? */
	/*  64 */	EDITED_ED_INSERT,		/* @ */
	/*  65 */	EDITED_ED_INSERT,		/* A */
	/*  66 */	EDITED_ED_INSERT,		/* B */
	/*  67 */	EDITED_ED_INSERT,		/* C */
	/*  68 */	EDITED_ED_INSERT,		/* D */
	/*  69 */	EDITED_ED_INSERT,		/* E */
	/*  70 */	EDITED_ED_INSERT,		/* F */
	/*  71 */	EDITED_ED_INSERT,		/* G */
	/*  72 */	EDITED_ED_INSERT,		/* H */
	/*  73 */	EDITED_ED_INSERT,		/* I */
	/*  74 */	EDITED_ED_INSERT,		/* J */
	/*  75 */	EDITED_ED_INSERT,		/* K */
	/*  76 */	EDITED_ED_INSERT,		/* L */
	/*  77 */	EDITED_ED_INSERT,		/* M */
	/*  78 */	EDITED_ED_INSERT,		/* N */
	/*  79 */	EDITED_ED_INSERT,		/* O */
	/*  80 */	EDITED_ED_INSERT,		/* P */
	/*  81 */	EDITED_ED_INSERT,		/* Q */
	/*  82 */	EDITED_ED_INSERT,		/* R */
	/*  83 */	EDITED_ED_INSERT,		/* S */
	/*  84 */	EDITED_ED_INSERT,		/* T */
	/*  85 */	EDITED_ED_INSERT,		/* U */
	/*  86 */	EDITED_ED_INSERT,		/* V */
	/*  87 */	EDITED_ED_INSERT,		/* W */
	/*  88 */	EDITED_ED_INSERT,		/* X */
	/*  89 */	EDITED_ED_INSERT,		/* Y */
	/*  90 */	EDITED_ED_INSERT,		/* Z */
	/*  91 */	EDITED_ED_INSERT,		/* [ */
	/*  92 */	EDITED_ED_INSERT,		/* \ */
	/*  93 */	EDITED_ED_INSERT,		/* ] */
	/*  94 */	EDITED_ED_INSERT,		/* ^ */
	/*  95 */	EDITED_ED_INSERT,		/* _ */
	/*  96 */	EDITED_ED_INSERT,		/* ` */
	/*  97 */	EDITED_ED_INSERT,		/* a */
	/*  98 */	EDITED_ED_INSERT,		/* b */
	/*  99 */	EDITED_ED_INSERT,		/* c */
	/* 100 */	EDITED_ED_INSERT,		/* d */
	/* 101 */	EDITED_ED_INSERT,		/* e */
	/* 102 */	EDITED_ED_INSERT,		/* f */
	/* 103 */	EDITED_ED_INSERT,		/* g */
	/* 104 */	EDITED_ED_INSERT,		/* h */
	/* 105 */	EDITED_ED_INSERT,		/* i */
	/* 106 */	EDITED_ED_INSERT,		/* j */
	/* 107 */	EDITED_ED_INSERT,		/* k */
	/* 108 */	EDITED_ED_INSERT,		/* l */
	/* 109 */	EDITED_ED_INSERT,		/* m */
	/* 110 */	EDITED_ED_INSERT,		/* n */
	/* 111 */	EDITED_ED_INSERT,		/* o */
	/* 112 */	EDITED_ED_INSERT,		/* p */
	/* 113 */	EDITED_ED_INSERT,		/* q */
	/* 114 */	EDITED_ED_INSERT,		/* r */
	/* 115 */	EDITED_ED_INSERT,		/* s */
	/* 116 */	EDITED_ED_INSERT,		/* t */
	/* 117 */	EDITED_ED_INSERT,		/* u */
	/* 118 */	EDITED_ED_INSERT,		/* v */
	/* 119 */	EDITED_ED_INSERT,		/* w */
	/* 120 */	EDITED_ED_INSERT,		/* x */
	/* 121 */	EDITED_ED_INSERT,		/* y */
	/* 122 */	EDITED_ED_INSERT,		/* z */
	/* 123 */	EDITED_ED_INSERT,		/* { */
	/* 124 */	EDITED_ED_INSERT,		/* | */
	/* 125 */	EDITED_ED_INSERT,		/* } */
	/* 126 */	EDITED_ED_INSERT,		/* ~ */
	/* 127 */	EDITED_EM_DELETE_PREV_CHAR,	/* ^? */
	/* 128 */	EDITED_ED_UNASSIGNED,		/* M-^@ */
	/* 129 */	EDITED_ED_UNASSIGNED,		/* M-^A */
	/* 130 */	EDITED_ED_UNASSIGNED,		/* M-^B */
	/* 131 */	EDITED_ED_UNASSIGNED,		/* M-^C */
	/* 132 */	EDITED_ED_UNASSIGNED,		/* M-^D */
	/* 133 */	EDITED_ED_UNASSIGNED,		/* M-^E */
	/* 134 */	EDITED_ED_UNASSIGNED,		/* M-^F */
	/* 135 */	EDITED_ED_UNASSIGNED,		/* M-^G */
	/* 136 */	EDITED_ED_DELETE_PREV_WORD,	/* M-^H */
	/* 137 */	EDITED_ED_UNASSIGNED,		/* M-^I */
	/* 138 */	EDITED_ED_UNASSIGNED,		/* M-^J */
	/* 139 */	EDITED_ED_UNASSIGNED,		/* M-^K */
	/* 140 */	EDITED_ED_CLEAR_SCREEN,	/* M-^L */
	/* 141 */	EDITED_ED_UNASSIGNED,		/* M-^M */
	/* 142 */	EDITED_ED_UNASSIGNED,		/* M-^N */
	/* 143 */	EDITED_ED_UNASSIGNED,		/* M-^O */
	/* 144 */	EDITED_ED_UNASSIGNED,		/* M-^P */
	/* 145 */	EDITED_ED_UNASSIGNED,		/* M-^Q */
	/* 146 */	EDITED_ED_UNASSIGNED,		/* M-^R */
	/* 147 */	EDITED_ED_UNASSIGNED,		/* M-^S */
	/* 148 */	EDITED_ED_UNASSIGNED,		/* M-^T */
	/* 149 */	EDITED_ED_UNASSIGNED,		/* M-^U */
	/* 150 */	EDITED_ED_UNASSIGNED,		/* M-^V */
	/* 151 */	EDITED_ED_UNASSIGNED,		/* M-^W */
	/* 152 */	EDITED_ED_UNASSIGNED,		/* M-^X */
	/* 153 */	EDITED_ED_UNASSIGNED,		/* M-^Y */
	/* 154 */	EDITED_ED_UNASSIGNED,		/* M-^Z */
	/* 155 */	EDITED_ED_UNASSIGNED,		/* M-^[ */
	/* 156 */	EDITED_ED_UNASSIGNED,		/* M-^\ */
	/* 157 */	EDITED_ED_UNASSIGNED,		/* M-^] */
	/* 158 */	EDITED_ED_UNASSIGNED,		/* M-^^ */
	/* 159 */	EDITED_EM_COPY_PREV_WORD,	/* M-^_ */
	/* 160 */	EDITED_ED_UNASSIGNED,		/* M-SPACE */
	/* 161 */	EDITED_ED_UNASSIGNED,		/* M-! */
	/* 162 */	EDITED_ED_UNASSIGNED,		/* M-" */
	/* 163 */	EDITED_ED_UNASSIGNED,		/* M-# */
	/* 164 */	EDITED_ED_UNASSIGNED,		/* M-$ */
	/* 165 */	EDITED_ED_UNASSIGNED,		/* M-% */
	/* 166 */	EDITED_ED_UNASSIGNED,		/* M-& */
	/* 167 */	EDITED_ED_UNASSIGNED,		/* M-' */
	/* 168 */	EDITED_ED_UNASSIGNED,		/* M-( */
	/* 169 */	EDITED_ED_UNASSIGNED,		/* M-) */
	/* 170 */	EDITED_ED_UNASSIGNED,		/* M-* */
	/* 171 */	EDITED_ED_UNASSIGNED,		/* M-+ */
	/* 172 */	EDITED_ED_UNASSIGNED,		/* M-, */
	/* 173 */	EDITED_ED_UNASSIGNED,		/* M-- */
	/* 174 */	EDITED_ED_UNASSIGNED,		/* M-. */
	/* 175 */	EDITED_ED_UNASSIGNED,		/* M-/ */
	/* 176 */	EDITED_ED_ARGUMENT_DIGIT,	/* M-0 */
	/* 177 */	EDITED_ED_ARGUMENT_DIGIT,	/* M-1 */
	/* 178 */	EDITED_ED_ARGUMENT_DIGIT,	/* M-2 */
	/* 179 */	EDITED_ED_ARGUMENT_DIGIT,	/* M-3 */
	/* 180 */	EDITED_ED_ARGUMENT_DIGIT,	/* M-4 */
	/* 181 */	EDITED_ED_ARGUMENT_DIGIT,	/* M-5 */
	/* 182 */	EDITED_ED_ARGUMENT_DIGIT,	/* M-6 */
	/* 183 */	EDITED_ED_ARGUMENT_DIGIT,	/* M-7 */
	/* 184 */	EDITED_ED_ARGUMENT_DIGIT,	/* M-8 */
	/* 185 */	EDITED_ED_ARGUMENT_DIGIT,	/* M-9 */
	/* 186 */	EDITED_ED_UNASSIGNED,		/* M-: */
	/* 187 */	EDITED_ED_UNASSIGNED,		/* M-; */
	/* 188 */	EDITED_ED_UNASSIGNED,		/* M-< */
	/* 189 */	EDITED_ED_UNASSIGNED,		/* M-= */
	/* 190 */	EDITED_ED_UNASSIGNED,		/* M-> */
	/* 191 */	EDITED_ED_UNASSIGNED,		/* M-? */
	/* 192 */	EDITED_ED_UNASSIGNED,		/* M-@ */
	/* 193 */	EDITED_ED_UNASSIGNED,		/* M-A */
	/* 194 */	EDITED_ED_PREV_WORD,		/* M-B */
	/* 195 */	EDITED_EM_CAPITOL_CASE,	/* M-C */
	/* 196 */	EDITED_EM_DELETE_NEXT_WORD,	/* M-D */
	/* 197 */	EDITED_ED_UNASSIGNED,		/* M-E */
	/* 198 */	EDITED_EM_NEXT_WORD,		/* M-F */
	/* 199 */	EDITED_ED_UNASSIGNED,		/* M-G */
	/* 200 */	EDITED_ED_UNASSIGNED,		/* M-H */
	/* 201 */	EDITED_ED_UNASSIGNED,		/* M-I */
	/* 202 */	EDITED_ED_UNASSIGNED,		/* M-J */
	/* 203 */	EDITED_ED_UNASSIGNED,		/* M-K */
	/* 204 */	EDITED_EM_LOWER_CASE,		/* M-L */
	/* 205 */	EDITED_ED_UNASSIGNED,		/* M-M */
	/* 206 */	EDITED_ED_SEARCH_NEXT_HISTORY,	/* M-N */
	/* 207 */	EDITED_ED_SEQUENCE_LEAD_IN,	/* M-O */
	/* 208 */	EDITED_ED_SEARCH_PREV_HISTORY,	/* M-P */
	/* 209 */	EDITED_ED_UNASSIGNED,		/* M-Q */
	/* 210 */	EDITED_ED_UNASSIGNED,		/* M-R */
	/* 211 */	EDITED_ED_UNASSIGNED,		/* M-S */
	/* 212 */	EDITED_ED_UNASSIGNED,		/* M-T */
	/* 213 */	EDITED_EM_UPPER_CASE,		/* M-U */
	/* 214 */	EDITED_ED_UNASSIGNED,		/* M-V */
	/* 215 */	EDITED_EM_COPY_REGION,		/* M-W */
	/* 216 */	EDITED_ED_COMMAND,		/* M-X */
	/* 217 */	EDITED_ED_UNASSIGNED,		/* M-Y */
	/* 218 */	EDITED_ED_UNASSIGNED,		/* M-Z */
	/* 219 */	EDITED_ED_SEQUENCE_LEAD_IN,	/* M-[ */
	/* 220 */	EDITED_ED_UNASSIGNED,		/* M-\ */
	/* 221 */	EDITED_ED_UNASSIGNED,		/* M-] */
	/* 222 */	EDITED_ED_UNASSIGNED,		/* M-^ */
	/* 223 */	EDITED_ED_UNASSIGNED,		/* M-_ */
	/* 223 */	EDITED_ED_UNASSIGNED,		/* M-` */
	/* 224 */	EDITED_ED_UNASSIGNED,		/* M-a */
	/* 225 */	EDITED_ED_PREV_WORD,		/* M-b */
	/* 226 */	EDITED_EM_CAPITOL_CASE,	/* M-c */
	/* 227 */	EDITED_EM_DELETE_NEXT_WORD,	/* M-d */
	/* 228 */	EDITED_ED_UNASSIGNED,		/* M-e */
	/* 229 */	EDITED_EM_NEXT_WORD,		/* M-f */
	/* 230 */	EDITED_ED_UNASSIGNED,		/* M-g */
	/* 231 */	EDITED_ED_UNASSIGNED,		/* M-h */
	/* 232 */	EDITED_ED_UNASSIGNED,		/* M-i */
	/* 233 */	EDITED_ED_UNASSIGNED,		/* M-j */
	/* 234 */	EDITED_ED_UNASSIGNED,		/* M-k */
	/* 235 */	EDITED_EM_LOWER_CASE,		/* M-l */
	/* 236 */	EDITED_ED_UNASSIGNED,		/* M-m */
	/* 237 */	EDITED_ED_SEARCH_NEXT_HISTORY,	/* M-n */
	/* 238 */	EDITED_ED_UNASSIGNED,		/* M-o */
	/* 239 */	EDITED_ED_SEARCH_PREV_HISTORY,	/* M-p */
	/* 240 */	EDITED_ED_UNASSIGNED,		/* M-q */
	/* 241 */	EDITED_ED_UNASSIGNED,		/* M-r */
	/* 242 */	EDITED_ED_UNASSIGNED,		/* M-s */
	/* 243 */	EDITED_ED_UNASSIGNED,		/* M-t */
	/* 244 */	EDITED_EM_UPPER_CASE,		/* M-u */
	/* 245 */	EDITED_ED_UNASSIGNED,		/* M-v */
	/* 246 */	EDITED_EM_COPY_REGION,		/* M-w */
	/* 247 */	EDITED_ED_COMMAND,		/* M-x */
	/* 248 */	EDITED_ED_UNASSIGNED,		/* M-y */
	/* 249 */	EDITED_ED_UNASSIGNED,		/* M-z */
	/* 250 */	EDITED_ED_UNASSIGNED,		/* M-{ */
	/* 251 */	EDITED_ED_UNASSIGNED,		/* M-| */
	/* 252 */	EDITED_ED_UNASSIGNED,		/* M-} */
	/* 253 */	EDITED_ED_UNASSIGNED,		/* M-~ */
	/* 254 */	EDITED_ED_DELETE_PREV_WORD	/* M-^? */
	/* 255 */
};


/*
 * keymap table for vi.  Each index into above tbl; should be
 * N_KEYS entries long.  Vi mode uses a sticky-extend to do command mode:
 * insert mode characters are in the normal keymap, and command mode
 * in the extended keymap.
 */
static const edited_action_t  edited_map_vi_insert[] = {
#ifdef KSHVI
	/*   0 */	EDITED_ED_UNASSIGNED,		/* ^@ */
	/*   1 */	EDITED_ED_INSERT,		/* ^A */
	/*   2 */	EDITED_ED_INSERT,		/* ^B */
	/*   3 */	EDITED_ED_INSERT,		/* ^C */
	/*   4 */	EDITED_VI_LIST_OR_EOF,		/* ^D */
	/*   5 */	EDITED_ED_INSERT,		/* ^E */
	/*   6 */	EDITED_ED_INSERT,		/* ^F */
	/*   7 */	EDITED_ED_INSERT,		/* ^G */
	/*   8 */	EDITED_VI_DELETE_PREV_CHAR,	/* ^H */   /* BackSpace key */
	/*   9 */	EDITED_ED_INSERT,		/* ^I */   /* Tab Key  */
	/*  10 */	EDITED_ED_NEWLINE,		/* ^J */
	/*  11 */	EDITED_ED_INSERT,		/* ^K */
	/*  12 */	EDITED_ED_INSERT,		/* ^L */
	/*  13 */	EDITED_ED_NEWLINE,		/* ^M */
	/*  14 */	EDITED_ED_INSERT,		/* ^N */
	/*  15 */	EDITED_ED_INSERT,		/* ^O */
	/*  16 */	EDITED_ED_INSERT,		/* ^P */
	/*  17 */	EDITED_ED_IGNORE,		/* ^Q */
	/*  18 */	EDITED_ED_INSERT,		/* ^R */
	/*  19 */	EDITED_ED_IGNORE,		/* ^S */
	/*  20 */	EDITED_ED_INSERT,		/* ^T */
	/*  21 */	EDITED_VI_KILL_LINE_PREV,	/* ^U */
	/*  22 */	EDITED_ED_QUOTED_INSERT,	/* ^V */
	/*  23 */	EDITED_ED_DELETE_PREV_WORD,	/* ^W */
		/* EDITED_ED_DELETE_PREV_WORD: Only until strt edit pos */
	/*  24 */	EDITED_ED_INSERT,		/* ^X */
	/*  25 */	EDITED_ED_INSERT,		/* ^Y */
	/*  26 */	EDITED_ED_INSERT,		/* ^Z */
	/*  27 */	EDITED_VI_COMMAND_MODE,	/* ^[ */  /* [ Esc ] key */
	/*  28 */	EDITED_ED_IGNORE,		/* ^\ */
	/*  29 */	EDITED_ED_INSERT,		/* ^] */
	/*  30 */	EDITED_ED_INSERT,		/* ^^ */
	/*  31 */	EDITED_ED_INSERT,		/* ^_ */
#else /* !KSHVI */
				/*
				 * NOTE: These mappings do NOT Correspond well
				 * to the KSH VI editing assignments.
				 * On the other and they are convenient and
				 * many people have have gotten used to them.
				 */
	/*   0 */	EDITED_ED_UNASSIGNED,		/* ^@ */
	/*   1 */	EDITED_ED_MOVE_TO_BEG,		/* ^A */
	/*   2 */	EDITED_ED_PREV_CHAR,		/* ^B */
	/*   3 */	EDITED_ED_IGNORE,		/* ^C */
	/*   4 */	EDITED_VI_LIST_OR_EOF,		/* ^D */
	/*   5 */	EDITED_ED_MOVE_TO_END,		/* ^E */
	/*   6 */	EDITED_ED_NEXT_CHAR,		/* ^F */
	/*   7 */	EDITED_ED_UNASSIGNED,		/* ^G */
	/*   8 */	DELETE_PREV_CHAR,	/* ^H */   /* BackSpace key */
	/*   9 */	EDITED_ED_UNASSIGNED,		/* ^I */   /* Tab Key */
	/*  10 */	EDITED_ED_NEWLINE,		/* ^J */
	/*  11 */	EDITED_ED_KILL_LINE,		/* ^K */
	/*  12 */	EDITED_ED_CLEAR_SCREEN,	/* ^L */
	/*  13 */	EDITED_ED_NEWLINE,		/* ^M */
	/*  14 */	EDITED_ED_NEXT_HISTORY,	/* ^N */
	/*  15 */	EDITED_ED_IGNORE,		/* ^O */
	/*  16 */	EDITED_ED_PREV_HISTORY,	/* ^P */
	/*  17 */	EDITED_ED_IGNORE,		/* ^Q */
	/*  18 */	EDITED_ED_REDISPLAY,		/* ^R */
	/*  19 */	EDITED_ED_IGNORE,		/* ^S */
	/*  20 */	EDITED_ED_TRANSPOSE_CHARS,	/* ^T */
	/*  21 */	KILL_LINE_PREV,	/* ^U */
	/*  22 */	EDITED_ED_QUOTED_INSERT,	/* ^V */
	/*  23 */	EDITED_ED_DELETE_PREV_WORD,	/* ^W */
	/*  24 */	EDITED_ED_UNASSIGNED,		/* ^X */
	/*  25 */	EDITED_ED_IGNORE,		/* ^Y */
	/*  26 */	EDITED_ED_IGNORE,		/* ^Z */
	/*  27 */	COMMAND_MODE,	/* ^[ */
	/*  28 */	EDITED_ED_IGNORE,		/* ^\ */
	/*  29 */	EDITED_ED_UNASSIGNED,		/* ^] */
	/*  30 */	EDITED_ED_UNASSIGNED,		/* ^^ */
	/*  31 */	EDITED_ED_UNASSIGNED,		/* ^_ */
#endif  /* KSHVI */
	/*  32 */	EDITED_ED_INSERT,		/* SPACE */
	/*  33 */	EDITED_ED_INSERT,		/* ! */
	/*  34 */	EDITED_ED_INSERT,		/* " */
	/*  35 */	EDITED_ED_INSERT,		/* # */
	/*  36 */	EDITED_ED_INSERT,		/* $ */
	/*  37 */	EDITED_ED_INSERT,		/* % */
	/*  38 */	EDITED_ED_INSERT,		/* & */
	/*  39 */	EDITED_ED_INSERT,		/* ' */
	/*  40 */	EDITED_ED_INSERT,		/* ( */
	/*  41 */	EDITED_ED_INSERT,		/* ) */
	/*  42 */	EDITED_ED_INSERT,		/* * */
	/*  43 */	EDITED_ED_INSERT,		/* + */
	/*  44 */	EDITED_ED_INSERT,		/* , */
	/*  45 */	EDITED_ED_INSERT,		/* - */
	/*  46 */	EDITED_ED_INSERT,		/* . */
	/*  47 */	EDITED_ED_INSERT,		/* / */
	/*  48 */	EDITED_ED_INSERT,		/* 0 */
	/*  49 */	EDITED_ED_INSERT,		/* 1 */
	/*  50 */	EDITED_ED_INSERT,		/* 2 */
	/*  51 */	EDITED_ED_INSERT,		/* 3 */
	/*  52 */	EDITED_ED_INSERT,		/* 4 */
	/*  53 */	EDITED_ED_INSERT,		/* 5 */
	/*  54 */	EDITED_ED_INSERT,		/* 6 */
	/*  55 */	EDITED_ED_INSERT,		/* 7 */
	/*  56 */	EDITED_ED_INSERT,		/* 8 */
	/*  57 */	EDITED_ED_INSERT,		/* 9 */
	/*  58 */	EDITED_ED_INSERT,		/* : */
	/*  59 */	EDITED_ED_INSERT,		/* ; */
	/*  60 */	EDITED_ED_INSERT,		/* < */
	/*  61 */	EDITED_ED_INSERT,		/* = */
	/*  62 */	EDITED_ED_INSERT,		/* > */
	/*  63 */	EDITED_ED_INSERT,		/* ? */
	/*  64 */	EDITED_ED_INSERT,		/* @ */
	/*  65 */	EDITED_ED_INSERT,		/* A */
	/*  66 */	EDITED_ED_INSERT,		/* B */
	/*  67 */	EDITED_ED_INSERT,		/* C */
	/*  68 */	EDITED_ED_INSERT,		/* D */
	/*  69 */	EDITED_ED_INSERT,		/* E */
	/*  70 */	EDITED_ED_INSERT,		/* F */
	/*  71 */	EDITED_ED_INSERT,		/* G */
	/*  72 */	EDITED_ED_INSERT,		/* H */
	/*  73 */	EDITED_ED_INSERT,		/* I */
	/*  74 */	EDITED_ED_INSERT,		/* J */
	/*  75 */	EDITED_ED_INSERT,		/* K */
	/*  76 */	EDITED_ED_INSERT,		/* L */
	/*  77 */	EDITED_ED_INSERT,		/* M */
	/*  78 */	EDITED_ED_INSERT,		/* N */
	/*  79 */	EDITED_ED_INSERT,		/* O */
	/*  80 */	EDITED_ED_INSERT,		/* P */
	/*  81 */	EDITED_ED_INSERT,		/* Q */
	/*  82 */	EDITED_ED_INSERT,		/* R */
	/*  83 */	EDITED_ED_INSERT,		/* S */
	/*  84 */	EDITED_ED_INSERT,		/* T */
	/*  85 */	EDITED_ED_INSERT,		/* U */
	/*  86 */	EDITED_ED_INSERT,		/* V */
	/*  87 */	EDITED_ED_INSERT,		/* W */
	/*  88 */	EDITED_ED_INSERT,		/* X */
	/*  89 */	EDITED_ED_INSERT,		/* Y */
	/*  90 */	EDITED_ED_INSERT,		/* Z */
	/*  91 */	EDITED_ED_INSERT,		/* [ */
	/*  92 */	EDITED_ED_INSERT,		/* \ */
	/*  93 */	EDITED_ED_INSERT,		/* ] */
	/*  94 */	EDITED_ED_INSERT,		/* ^ */
	/*  95 */	EDITED_ED_INSERT,		/* _ */
	/*  96 */	EDITED_ED_INSERT,		/* ` */
	/*  97 */	EDITED_ED_INSERT,		/* a */
	/*  98 */	EDITED_ED_INSERT,		/* b */
	/*  99 */	EDITED_ED_INSERT,		/* c */
	/* 100 */	EDITED_ED_INSERT,		/* d */
	/* 101 */	EDITED_ED_INSERT,		/* e */
	/* 102 */	EDITED_ED_INSERT,		/* f */
	/* 103 */	EDITED_ED_INSERT,		/* g */
	/* 104 */	EDITED_ED_INSERT,		/* h */
	/* 105 */	EDITED_ED_INSERT,		/* i */
	/* 106 */	EDITED_ED_INSERT,		/* j */
	/* 107 */	EDITED_ED_INSERT,		/* k */
	/* 108 */	EDITED_ED_INSERT,		/* l */
	/* 109 */	EDITED_ED_INSERT,		/* m */
	/* 110 */	EDITED_ED_INSERT,		/* n */
	/* 111 */	EDITED_ED_INSERT,		/* o */
	/* 112 */	EDITED_ED_INSERT,		/* p */
	/* 113 */	EDITED_ED_INSERT,		/* q */
	/* 114 */	EDITED_ED_INSERT,		/* r */
	/* 115 */	EDITED_ED_INSERT,		/* s */
	/* 116 */	EDITED_ED_INSERT,		/* t */
	/* 117 */	EDITED_ED_INSERT,		/* u */
	/* 118 */	EDITED_ED_INSERT,		/* v */
	/* 119 */	EDITED_ED_INSERT,		/* w */
	/* 120 */	EDITED_ED_INSERT,		/* x */
	/* 121 */	EDITED_ED_INSERT,		/* y */
	/* 122 */	EDITED_ED_INSERT,		/* z */
	/* 123 */	EDITED_ED_INSERT,		/* { */
	/* 124 */	EDITED_ED_INSERT,		/* | */
	/* 125 */	EDITED_ED_INSERT,		/* } */
	/* 126 */	EDITED_ED_INSERT,		/* ~ */
	/* 127 */	EDITED_VI_DELETE_PREV_CHAR,	/* ^? */
	/* 128 */	EDITED_ED_INSERT,		/* M-^@ */
	/* 129 */	EDITED_ED_INSERT,		/* M-^A */
	/* 130 */	EDITED_ED_INSERT,		/* M-^B */
	/* 131 */	EDITED_ED_INSERT,		/* M-^C */
	/* 132 */	EDITED_ED_INSERT,		/* M-^D */
	/* 133 */	EDITED_ED_INSERT,		/* M-^E */
	/* 134 */	EDITED_ED_INSERT,		/* M-^F */
	/* 135 */	EDITED_ED_INSERT,		/* M-^G */
	/* 136 */	EDITED_ED_INSERT,		/* M-^H */
	/* 137 */	EDITED_ED_INSERT,		/* M-^I */
	/* 138 */	EDITED_ED_INSERT,		/* M-^J */
	/* 139 */	EDITED_ED_INSERT,		/* M-^K */
	/* 140 */	EDITED_ED_INSERT,		/* M-^L */
	/* 141 */	EDITED_ED_INSERT,		/* M-^M */
	/* 142 */	EDITED_ED_INSERT,		/* M-^N */
	/* 143 */	EDITED_ED_INSERT,		/* M-^O */
	/* 144 */	EDITED_ED_INSERT,		/* M-^P */
	/* 145 */	EDITED_ED_INSERT,		/* M-^Q */
	/* 146 */	EDITED_ED_INSERT,		/* M-^R */
	/* 147 */	EDITED_ED_INSERT,		/* M-^S */
	/* 148 */	EDITED_ED_INSERT,		/* M-^T */
	/* 149 */	EDITED_ED_INSERT,		/* M-^U */
	/* 150 */	EDITED_ED_INSERT,		/* M-^V */
	/* 151 */	EDITED_ED_INSERT,		/* M-^W */
	/* 152 */	EDITED_ED_INSERT,		/* M-^X */
	/* 153 */	EDITED_ED_INSERT,		/* M-^Y */
	/* 154 */	EDITED_ED_INSERT,		/* M-^Z */
	/* 155 */	EDITED_ED_INSERT,		/* M-^[ */
	/* 156 */	EDITED_ED_INSERT,		/* M-^\ */
	/* 157 */	EDITED_ED_INSERT,		/* M-^] */
	/* 158 */	EDITED_ED_INSERT,		/* M-^^ */
	/* 159 */	EDITED_ED_INSERT,		/* M-^_ */
	/* 160 */	EDITED_ED_INSERT,		/* M-SPACE */
	/* 161 */	EDITED_ED_INSERT,		/* M-! */
	/* 162 */	EDITED_ED_INSERT,		/* M-" */
	/* 163 */	EDITED_ED_INSERT,		/* M-# */
	/* 164 */	EDITED_ED_INSERT,		/* M-$ */
	/* 165 */	EDITED_ED_INSERT,		/* M-% */
	/* 166 */	EDITED_ED_INSERT,		/* M-& */
	/* 167 */	EDITED_ED_INSERT,		/* M-' */
	/* 168 */	EDITED_ED_INSERT,		/* M-( */
	/* 169 */	EDITED_ED_INSERT,		/* M-) */
	/* 170 */	EDITED_ED_INSERT,		/* M-* */
	/* 171 */	EDITED_ED_INSERT,		/* M-+ */
	/* 172 */	EDITED_ED_INSERT,		/* M-, */
	/* 173 */	EDITED_ED_INSERT,		/* M-- */
	/* 174 */	EDITED_ED_INSERT,		/* M-. */
	/* 175 */	EDITED_ED_INSERT,		/* M-/ */
	/* 176 */	EDITED_ED_INSERT,		/* M-0 */
	/* 177 */	EDITED_ED_INSERT,		/* M-1 */
	/* 178 */	EDITED_ED_INSERT,		/* M-2 */
	/* 179 */	EDITED_ED_INSERT,		/* M-3 */
	/* 180 */	EDITED_ED_INSERT,		/* M-4 */
	/* 181 */	EDITED_ED_INSERT,		/* M-5 */
	/* 182 */	EDITED_ED_INSERT,		/* M-6 */
	/* 183 */	EDITED_ED_INSERT,		/* M-7 */
	/* 184 */	EDITED_ED_INSERT,		/* M-8 */
	/* 185 */	EDITED_ED_INSERT,		/* M-9 */
	/* 186 */	EDITED_ED_INSERT,		/* M-: */
	/* 187 */	EDITED_ED_INSERT,		/* M-; */
	/* 188 */	EDITED_ED_INSERT,		/* M-< */
	/* 189 */	EDITED_ED_INSERT,		/* M-= */
	/* 190 */	EDITED_ED_INSERT,		/* M-> */
	/* 191 */	EDITED_ED_INSERT,		/* M-? */
	/* 192 */	EDITED_ED_INSERT,		/* M-@ */
	/* 193 */	EDITED_ED_INSERT,		/* M-A */
	/* 194 */	EDITED_ED_INSERT,		/* M-B */
	/* 195 */	EDITED_ED_INSERT,		/* M-C */
	/* 196 */	EDITED_ED_INSERT,		/* M-D */
	/* 197 */	EDITED_ED_INSERT,		/* M-E */
	/* 198 */	EDITED_ED_INSERT,		/* M-F */
	/* 199 */	EDITED_ED_INSERT,		/* M-G */
	/* 200 */	EDITED_ED_INSERT,		/* M-H */
	/* 201 */	EDITED_ED_INSERT,		/* M-I */
	/* 202 */	EDITED_ED_INSERT,		/* M-J */
	/* 203 */	EDITED_ED_INSERT,		/* M-K */
	/* 204 */	EDITED_ED_INSERT,		/* M-L */
	/* 205 */	EDITED_ED_INSERT,		/* M-M */
	/* 206 */	EDITED_ED_INSERT,		/* M-N */
	/* 207 */	EDITED_ED_INSERT,		/* M-O */
	/* 208 */	EDITED_ED_INSERT,		/* M-P */
	/* 209 */	EDITED_ED_INSERT,		/* M-Q */
	/* 210 */	EDITED_ED_INSERT,		/* M-R */
	/* 211 */	EDITED_ED_INSERT,		/* M-S */
	/* 212 */	EDITED_ED_INSERT,		/* M-T */
	/* 213 */	EDITED_ED_INSERT,		/* M-U */
	/* 214 */	EDITED_ED_INSERT,		/* M-V */
	/* 215 */	EDITED_ED_INSERT,		/* M-W */
	/* 216 */	EDITED_ED_INSERT,		/* M-X */
	/* 217 */	EDITED_ED_INSERT,		/* M-Y */
	/* 218 */	EDITED_ED_INSERT,		/* M-Z */
	/* 219 */	EDITED_ED_INSERT,		/* M-[ */
	/* 220 */	EDITED_ED_INSERT,		/* M-\ */
	/* 221 */	EDITED_ED_INSERT,		/* M-] */
	/* 222 */	EDITED_ED_INSERT,		/* M-^ */
	/* 223 */	EDITED_ED_INSERT,		/* M-_ */
	/* 224 */	EDITED_ED_INSERT,		/* M-` */
	/* 225 */	EDITED_ED_INSERT,		/* M-a */
	/* 226 */	EDITED_ED_INSERT,		/* M-b */
	/* 227 */	EDITED_ED_INSERT,		/* M-c */
	/* 228 */	EDITED_ED_INSERT,		/* M-d */
	/* 229 */	EDITED_ED_INSERT,		/* M-e */
	/* 230 */	EDITED_ED_INSERT,		/* M-f */
	/* 231 */	EDITED_ED_INSERT,		/* M-g */
	/* 232 */	EDITED_ED_INSERT,		/* M-h */
	/* 233 */	EDITED_ED_INSERT,		/* M-i */
	/* 234 */	EDITED_ED_INSERT,		/* M-j */
	/* 235 */	EDITED_ED_INSERT,		/* M-k */
	/* 236 */	EDITED_ED_INSERT,		/* M-l */
	/* 237 */	EDITED_ED_INSERT,		/* M-m */
	/* 238 */	EDITED_ED_INSERT,		/* M-n */
	/* 239 */	EDITED_ED_INSERT,		/* M-o */
	/* 240 */	EDITED_ED_INSERT,		/* M-p */
	/* 241 */	EDITED_ED_INSERT,		/* M-q */
	/* 242 */	EDITED_ED_INSERT,		/* M-r */
	/* 243 */	EDITED_ED_INSERT,		/* M-s */
	/* 244 */	EDITED_ED_INSERT,		/* M-t */
	/* 245 */	EDITED_ED_INSERT,		/* M-u */
	/* 246 */	EDITED_ED_INSERT,		/* M-v */
	/* 247 */	EDITED_ED_INSERT,		/* M-w */
	/* 248 */	EDITED_ED_INSERT,		/* M-x */
	/* 249 */	EDITED_ED_INSERT,		/* M-y */
	/* 250 */	EDITED_ED_INSERT,		/* M-z */
	/* 251 */	EDITED_ED_INSERT,		/* M-{ */
	/* 252 */	EDITED_ED_INSERT,		/* M-| */
	/* 253 */	EDITED_ED_INSERT,		/* M-} */
	/* 254 */	EDITED_ED_INSERT,		/* M-~ */
	/* 255 */	EDITED_ED_INSERT		/* M-^? */
};

static const edited_action_t edited_map_vi_command[] = {
	/*   0 */	EDITED_ED_UNASSIGNED,		/* ^@ */
	/*   1 */	EDITED_ED_MOVE_TO_BEG,		/* ^A */
	/*   2 */	EDITED_ED_UNASSIGNED,		/* ^B */
	/*   3 */	EDITED_ED_IGNORE,		/* ^C */
	/*   4 */	EDITED_ED_UNASSIGNED,		/* ^D */
	/*   5 */	EDITED_ED_MOVE_TO_END,		/* ^E */
	/*   6 */	EDITED_ED_UNASSIGNED,		/* ^F */
	/*   7 */	EDITED_ED_UNASSIGNED,		/* ^G */
	/*   8 */	EDITED_ED_DELETE_PREV_CHAR,	/* ^H */
	/*   9 */	EDITED_ED_UNASSIGNED,		/* ^I */
	/*  10 */	EDITED_ED_NEWLINE,		/* ^J */
	/*  11 */	EDITED_ED_KILL_LINE,		/* ^K */
	/*  12 */	EDITED_ED_CLEAR_SCREEN,	/* ^L */
	/*  13 */	EDITED_ED_NEWLINE,		/* ^M */
	/*  14 */	EDITED_ED_NEXT_HISTORY,	/* ^N */
	/*  15 */	EDITED_ED_IGNORE,		/* ^O */
	/*  16 */	EDITED_ED_PREV_HISTORY,	/* ^P */
	/*  17 */	EDITED_ED_IGNORE,		/* ^Q */
	/*  18 */	EDITED_ED_REDISPLAY,		/* ^R */
	/*  19 */	EDITED_ED_IGNORE,		/* ^S */
	/*  20 */	EDITED_ED_UNASSIGNED,		/* ^T */
	/*  21 */	EDITED_VI_KILL_LINE_PREV,	/* ^U */
	/*  22 */	EDITED_ED_UNASSIGNED,		/* ^V */
	/*  23 */	EDITED_ED_DELETE_PREV_WORD,	/* ^W */
	/*  24 */	EDITED_ED_UNASSIGNED,		/* ^X */
	/*  25 */	EDITED_ED_UNASSIGNED,		/* ^Y */
	/*  26 */	EDITED_ED_UNASSIGNED,		/* ^Z */
	/*  27 */	EDITED_EM_META_NEXT,		/* ^[ */
	/*  28 */	EDITED_ED_IGNORE,		/* ^\ */
	/*  29 */	EDITED_ED_UNASSIGNED,		/* ^] */
	/*  30 */	EDITED_ED_UNASSIGNED,		/* ^^ */
	/*  31 */	EDITED_ED_UNASSIGNED,		/* ^_ */
	/*  32 */	EDITED_ED_NEXT_CHAR,		/* SPACE */
	/*  33 */	EDITED_ED_UNASSIGNED,		/* ! */
	/*  34 */	EDITED_ED_UNASSIGNED,		/* " */
	/*  35 */	EDITED_VI_COMMENT_OUT,		/* # */
	/*  36 */	EDITED_ED_MOVE_TO_END,		/* $ */
	/*  37 */	EDITED_VI_MATCH,		/* % */
	/*  38 */	EDITED_ED_UNASSIGNED,		/* & */
	/*  39 */	EDITED_ED_UNASSIGNED,		/* ' */
	/*  40 */	EDITED_ED_UNASSIGNED,		/* ( */
	/*  41 */	EDITED_ED_UNASSIGNED,		/* ) */
	/*  42 */	EDITED_ED_UNASSIGNED,		/* * */
	/*  43 */	EDITED_ED_NEXT_HISTORY,	/* + */
	/*  44 */	EDITED_VI_REPEAT_PREV_CHAR,	/* , */
	/*  45 */	EDITED_ED_PREV_HISTORY,	/* - */
	/*  46 */	EDITED_VI_REDO,		/* . */
	/*  47 */	EDITED_VI_SEARCH_PREV,		/* / */
	/*  48 */	EDITED_VI_ZERO,		/* 0 */
	/*  49 */	EDITED_ED_ARGUMENT_DIGIT,	/* 1 */
	/*  50 */	EDITED_ED_ARGUMENT_DIGIT,	/* 2 */
	/*  51 */	EDITED_ED_ARGUMENT_DIGIT,	/* 3 */
	/*  52 */	EDITED_ED_ARGUMENT_DIGIT,	/* 4 */
	/*  53 */	EDITED_ED_ARGUMENT_DIGIT,	/* 5 */
	/*  54 */	EDITED_ED_ARGUMENT_DIGIT,	/* 6 */
	/*  55 */	EDITED_ED_ARGUMENT_DIGIT,	/* 7 */
	/*  56 */	EDITED_ED_ARGUMENT_DIGIT,	/* 8 */
	/*  57 */	EDITED_ED_ARGUMENT_DIGIT,	/* 9 */
	/*  58 */	EDITED_ED_COMMAND,		/* : */
	/*  59 */	EDITED_VI_REPEAT_NEXT_CHAR,	/* ; */
	/*  60 */	EDITED_ED_UNASSIGNED,		/* < */
	/*  61 */	EDITED_ED_UNASSIGNED,		/* = */
	/*  62 */	EDITED_ED_UNASSIGNED,		/* > */
	/*  63 */	EDITED_VI_SEARCH_NEXT,		/* ? */
	/*  64 */	EDITED_VI_ALIAS,		/* @ */
	/*  65 */	EDITED_VI_ADD_AT_EOL,		/* A */
	/*  66 */	EDITED_VI_PREV_BIG_WORD,	/* B */
	/*  67 */	EDITED_VI_CHANGE_TO_EOL,	/* C */
	/*  68 */	EDITED_ED_KILL_LINE,		/* D */
	/*  69 */	EDITED_VI_END_BIG_WORD,	/* E */
	/*  70 */	EDITED_VI_PREV_CHAR,		/* F */
	/*  71 */	EDITED_VI_TO_HISTORY_LINE,	/* G */
	/*  72 */	EDITED_ED_UNASSIGNED,		/* H */
	/*  73 */	EDITED_VI_INSERT_AT_BOL,	/* I */
	/*  74 */	EDITED_ED_SEARCH_NEXT_HISTORY,	/* J */
	/*  75 */	EDITED_ED_SEARCH_PREV_HISTORY,	/* K */
	/*  76 */	EDITED_ED_UNASSIGNED,		/* L */
	/*  77 */	EDITED_ED_UNASSIGNED,		/* M */
	/*  78 */	EDITED_VI_REPEAT_SEARCH_PREV,	/* N */
	/*  79 */	EDITED_ED_SEQUENCE_LEAD_IN,	/* O */
	/*  80 */	EDITED_VI_PASTE_PREV,		/* P */
	/*  81 */	EDITED_ED_UNASSIGNED,		/* Q */
	/*  82 */	EDITED_VI_REPLACE_MODE,	/* R */
	/*  83 */	EDITED_VI_SUBSTITUTE_LINE,	/* S */
	/*  84 */	EDITED_VI_TO_PREV_CHAR,	/* T */
	/*  85 */	EDITED_VI_UNDO_LINE,		/* U */
	/*  86 */	EDITED_ED_UNASSIGNED,		/* V */
	/*  87 */	EDITED_VI_NEXT_BIG_WORD,	/* W */
	/*  88 */	EDITED_ED_DELETE_PREV_CHAR,	/* X */
	/*  89 */	EDITED_VI_YANK_END,		/* Y */
	/*  90 */	EDITED_ED_UNASSIGNED,		/* Z */
	/*  91 */	EDITED_ED_SEQUENCE_LEAD_IN,	/* [ */
	/*  92 */	EDITED_ED_UNASSIGNED,		/* \ */
	/*  93 */	EDITED_ED_UNASSIGNED,		/* ] */
	/*  94 */	EDITED_ED_MOVE_TO_BEG,		/* ^ */
	/*  95 */	EDITED_VI_HISTORY_WORD,	/* _ */
	/*  96 */	EDITED_ED_UNASSIGNED,		/* ` */
	/*  97 */	EDITED_VI_ADD,			/* a */
	/*  98 */	EDITED_VI_PREV_WORD,		/* b */
	/*  99 */	EDITED_VI_CHANGE_META,		/* c */
	/* 100 */	EDITED_VI_DELETE_META,		/* d */
	/* 101 */	EDITED_VI_END_WORD,		/* e */
	/* 102 */	EDITED_VI_NEXT_CHAR,		/* f */
	/* 103 */	EDITED_ED_UNASSIGNED,		/* g */
	/* 104 */	EDITED_ED_PREV_CHAR,		/* h */
	/* 105 */	INSERT,		/* i */
	/* 106 */	EDITED_ED_NEXT_HISTORY,	/* j */
	/* 107 */	EDITED_ED_PREV_HISTORY,	/* k */
	/* 108 */	EDITED_ED_NEXT_CHAR,		/* l */
	/* 109 */	EDITED_ED_UNASSIGNED,		/* m */
	/* 110 */	EDITED_VI_REPEAT_SEARCH_NEXT,	/* n */
	/* 111 */	EDITED_ED_UNASSIGNED,		/* o */
	/* 112 */	EDITED_VI_PASTE_NEXT,		/* p */
	/* 113 */	EDITED_ED_UNASSIGNED,		/* q */
	/* 114 */	EDITED_VI_REPLACE_CHAR,	/* r */
	/* 115 */	EDITED_VI_SUBSTITUTE_CHAR,	/* s */
	/* 116 */	EDITED_VI_TO_NEXT_CHAR,	/* t */
	/* 117 */	EDITED_VI_UNDO,		/* u */
	/* 118 */	EDITED_VI_HISTEDIT,		/* v */
	/* 119 */	EDITED_VI_NEXT_WORD,		/* w */
	/* 120 */	EDITED_ED_DELETE_NEXT_CHAR,	/* x */
	/* 121 */	YANK,		/* y */
	/* 122 */	EDITED_ED_UNASSIGNED,		/* z */
	/* 123 */	EDITED_ED_UNASSIGNED,		/* { */
	/* 124 */	EDITED_VI_TO_COLUMN,		/* | */
	/* 125 */	EDITED_ED_UNASSIGNED,		/* } */
	/* 126 */	EDITED_VI_CHANGE_CASE,		/* ~ */
	/* 127 */	EDITED_ED_DELETE_PREV_CHAR,	/* ^? */
	/* 128 */	EDITED_ED_UNASSIGNED,		/* M-^@ */
	/* 129 */	EDITED_ED_UNASSIGNED,		/* M-^A */
	/* 130 */	EDITED_ED_UNASSIGNED,		/* M-^B */
	/* 131 */	EDITED_ED_UNASSIGNED,		/* M-^C */
	/* 132 */	EDITED_ED_UNASSIGNED,		/* M-^D */
	/* 133 */	EDITED_ED_UNASSIGNED,		/* M-^E */
	/* 134 */	EDITED_ED_UNASSIGNED,		/* M-^F */
	/* 135 */	EDITED_ED_UNASSIGNED,		/* M-^G */
	/* 136 */	EDITED_ED_UNASSIGNED,		/* M-^H */
	/* 137 */	EDITED_ED_UNASSIGNED,		/* M-^I */
	/* 138 */	EDITED_ED_UNASSIGNED,		/* M-^J */
	/* 139 */	EDITED_ED_UNASSIGNED,		/* M-^K */
	/* 140 */	EDITED_ED_UNASSIGNED,		/* M-^L */
	/* 141 */	EDITED_ED_UNASSIGNED,		/* M-^M */
	/* 142 */	EDITED_ED_UNASSIGNED,		/* M-^N */
	/* 143 */	EDITED_ED_UNASSIGNED,		/* M-^O */
	/* 144 */	EDITED_ED_UNASSIGNED,		/* M-^P */
	/* 145 */	EDITED_ED_UNASSIGNED,		/* M-^Q */
	/* 146 */	EDITED_ED_UNASSIGNED,		/* M-^R */
	/* 147 */	EDITED_ED_UNASSIGNED,		/* M-^S */
	/* 148 */	EDITED_ED_UNASSIGNED,		/* M-^T */
	/* 149 */	EDITED_ED_UNASSIGNED,		/* M-^U */
	/* 150 */	EDITED_ED_UNASSIGNED,		/* M-^V */
	/* 151 */	EDITED_ED_UNASSIGNED,		/* M-^W */
	/* 152 */	EDITED_ED_UNASSIGNED,		/* M-^X */
	/* 153 */	EDITED_ED_UNASSIGNED,		/* M-^Y */
	/* 154 */	EDITED_ED_UNASSIGNED,		/* M-^Z */
	/* 155 */	EDITED_ED_UNASSIGNED,		/* M-^[ */
	/* 156 */	EDITED_ED_UNASSIGNED,		/* M-^\ */
	/* 157 */	EDITED_ED_UNASSIGNED,		/* M-^] */
	/* 158 */	EDITED_ED_UNASSIGNED,		/* M-^^ */
	/* 159 */	EDITED_ED_UNASSIGNED,		/* M-^_ */
	/* 160 */	EDITED_ED_UNASSIGNED,		/* M-SPACE */
	/* 161 */	EDITED_ED_UNASSIGNED,		/* M-! */
	/* 162 */	EDITED_ED_UNASSIGNED,		/* M-" */
	/* 163 */	EDITED_ED_UNASSIGNED,		/* M-# */
	/* 164 */	EDITED_ED_UNASSIGNED,		/* M-$ */
	/* 165 */	EDITED_ED_UNASSIGNED,		/* M-% */
	/* 166 */	EDITED_ED_UNASSIGNED,		/* M-& */
	/* 167 */	EDITED_ED_UNASSIGNED,		/* M-' */
	/* 168 */	EDITED_ED_UNASSIGNED,		/* M-( */
	/* 169 */	EDITED_ED_UNASSIGNED,		/* M-) */
	/* 170 */	EDITED_ED_UNASSIGNED,		/* M-* */
	/* 171 */	EDITED_ED_UNASSIGNED,		/* M-+ */
	/* 172 */	EDITED_ED_UNASSIGNED,		/* M-, */
	/* 173 */	EDITED_ED_UNASSIGNED,		/* M-- */
	/* 174 */	EDITED_ED_UNASSIGNED,		/* M-. */
	/* 175 */	EDITED_ED_UNASSIGNED,		/* M-/ */
	/* 176 */	EDITED_ED_UNASSIGNED,		/* M-0 */
	/* 177 */	EDITED_ED_UNASSIGNED,		/* M-1 */
	/* 178 */	EDITED_ED_UNASSIGNED,		/* M-2 */
	/* 179 */	EDITED_ED_UNASSIGNED,		/* M-3 */
	/* 180 */	EDITED_ED_UNASSIGNED,		/* M-4 */
	/* 181 */	EDITED_ED_UNASSIGNED,		/* M-5 */
	/* 182 */	EDITED_ED_UNASSIGNED,		/* M-6 */
	/* 183 */	EDITED_ED_UNASSIGNED,		/* M-7 */
	/* 184 */	EDITED_ED_UNASSIGNED,		/* M-8 */
	/* 185 */	EDITED_ED_UNASSIGNED,		/* M-9 */
	/* 186 */	EDITED_ED_UNASSIGNED,		/* M-: */
	/* 187 */	EDITED_ED_UNASSIGNED,		/* M-; */
	/* 188 */	EDITED_ED_UNASSIGNED,		/* M-< */
	/* 189 */	EDITED_ED_UNASSIGNED,		/* M-= */
	/* 190 */	EDITED_ED_UNASSIGNED,		/* M-> */
	/* 191 */	EDITED_ED_UNASSIGNED,		/* M-? */
	/* 192 */	EDITED_ED_UNASSIGNED,		/* M-@ */
	/* 193 */	EDITED_ED_UNASSIGNED,		/* M-A */
	/* 194 */	EDITED_ED_UNASSIGNED,		/* M-B */
	/* 195 */	EDITED_ED_UNASSIGNED,		/* M-C */
	/* 196 */	EDITED_ED_UNASSIGNED,		/* M-D */
	/* 197 */	EDITED_ED_UNASSIGNED,		/* M-E */
	/* 198 */	EDITED_ED_UNASSIGNED,		/* M-F */
	/* 199 */	EDITED_ED_UNASSIGNED,		/* M-G */
	/* 200 */	EDITED_ED_UNASSIGNED,		/* M-H */
	/* 201 */	EDITED_ED_UNASSIGNED,		/* M-I */
	/* 202 */	EDITED_ED_UNASSIGNED,		/* M-J */
	/* 203 */	EDITED_ED_UNASSIGNED,		/* M-K */
	/* 204 */	EDITED_ED_UNASSIGNED,		/* M-L */
	/* 205 */	EDITED_ED_UNASSIGNED,		/* M-M */
	/* 206 */	EDITED_ED_UNASSIGNED,		/* M-N */
	/* 207 */	EDITED_ED_SEQUENCE_LEAD_IN,	/* M-O */
	/* 208 */	EDITED_ED_UNASSIGNED,		/* M-P */
	/* 209 */	EDITED_ED_UNASSIGNED,		/* M-Q */
	/* 210 */	EDITED_ED_UNASSIGNED,		/* M-R */
	/* 211 */	EDITED_ED_UNASSIGNED,		/* M-S */
	/* 212 */	EDITED_ED_UNASSIGNED,		/* M-T */
	/* 213 */	EDITED_ED_UNASSIGNED,		/* M-U */
	/* 214 */	EDITED_ED_UNASSIGNED,		/* M-V */
	/* 215 */	EDITED_ED_UNASSIGNED,		/* M-W */
	/* 216 */	EDITED_ED_UNASSIGNED,		/* M-X */
	/* 217 */	EDITED_ED_UNASSIGNED,		/* M-Y */
	/* 218 */	EDITED_ED_UNASSIGNED,		/* M-Z */
	/* 219 */	EDITED_ED_SEQUENCE_LEAD_IN,	/* M-[ */
	/* 220 */	EDITED_ED_UNASSIGNED,		/* M-\ */
	/* 221 */	EDITED_ED_UNASSIGNED,		/* M-] */
	/* 222 */	EDITED_ED_UNASSIGNED,		/* M-^ */
	/* 223 */	EDITED_ED_UNASSIGNED,		/* M-_ */
	/* 224 */	EDITED_ED_UNASSIGNED,		/* M-` */
	/* 225 */	EDITED_ED_UNASSIGNED,		/* M-a */
	/* 226 */	EDITED_ED_UNASSIGNED,		/* M-b */
	/* 227 */	EDITED_ED_UNASSIGNED,		/* M-c */
	/* 228 */	EDITED_ED_UNASSIGNED,		/* M-d */
	/* 229 */	EDITED_ED_UNASSIGNED,		/* M-e */
	/* 230 */	EDITED_ED_UNASSIGNED,		/* M-f */
	/* 231 */	EDITED_ED_UNASSIGNED,		/* M-g */
	/* 232 */	EDITED_ED_UNASSIGNED,		/* M-h */
	/* 233 */	EDITED_ED_UNASSIGNED,		/* M-i */
	/* 234 */	EDITED_ED_UNASSIGNED,		/* M-j */
	/* 235 */	EDITED_ED_UNASSIGNED,		/* M-k */
	/* 236 */	EDITED_ED_UNASSIGNED,		/* M-l */
	/* 237 */	EDITED_ED_UNASSIGNED,		/* M-m */
	/* 238 */	EDITED_ED_UNASSIGNED,		/* M-n */
	/* 239 */	EDITED_ED_UNASSIGNED,		/* M-o */
	/* 240 */	EDITED_ED_UNASSIGNED,		/* M-p */
	/* 241 */	EDITED_ED_UNASSIGNED,		/* M-q */
	/* 242 */	EDITED_ED_UNASSIGNED,		/* M-r */
	/* 243 */	EDITED_ED_UNASSIGNED,		/* M-s */
	/* 244 */	EDITED_ED_UNASSIGNED,		/* M-t */
	/* 245 */	EDITED_ED_UNASSIGNED,		/* M-u */
	/* 246 */	EDITED_ED_UNASSIGNED,		/* M-v */
	/* 247 */	EDITED_ED_UNASSIGNED,		/* M-w */
	/* 248 */	EDITED_ED_UNASSIGNED,		/* M-x */
	/* 249 */	EDITED_ED_UNASSIGNED,		/* M-y */
	/* 250 */	EDITED_ED_UNASSIGNED,		/* M-z */
	/* 251 */	EDITED_ED_UNASSIGNED,		/* M-{ */
	/* 252 */	EDITED_ED_UNASSIGNED,		/* M-| */
	/* 253 */	EDITED_ED_UNASSIGNED,		/* M-} */
	/* 254 */	EDITED_ED_UNASSIGNED,		/* M-~ */
	/* 255 */	EDITED_ED_UNASSIGNED		/* M-^? */
};


/* edited_map_init():
 *	Initialize and allocate the maps
 */
libedited_private int
edited_map_init(Edited *el)
{

	/*
         * Make sure those are correct before starting.
         */
#ifdef MAP_DEBUG
	if (sizeof(edited_map_emacs) != N_KEYS * sizeof(edited_action_t))
		EL_ABORT((el->edited_errfile, "Emacs map incorrect\n"));
	if (sizeof(edited_map_vi_command) != N_KEYS * sizeof(edited_action_t))
		EL_ABORT((el->edited_errfile, "Vi command map incorrect\n"));
	if (sizeof(edited_map_vi_insert) != N_KEYS * sizeof(edited_action_t))
		EL_ABORT((el->edited_errfile, "Vi insert map incorrect\n"));
#endif

	el->edited_map.alt = edited_calloc(N_KEYS, sizeof(*el->edited_map.alt));
	if (el->edited_map.alt == NULL)
		return -1;
	el->edited_map.key = edited_calloc(N_KEYS, sizeof(*el->edited_map.key));
	if (el->edited_map.key == NULL)
		goto out;
	el->edited_map.emacs = edited_map_emacs;
	el->edited_map.vic = edited_map_vi_command;
	el->edited_map.vii = edited_map_vi_insert;
	el->edited_map.help = edited_calloc(EL_NUM_FCNS, sizeof(*el->edited_map.help));
	if (el->edited_map.help == NULL)
		goto out;
	(void) memcpy(el->edited_map.help, edited_func_help,
	    sizeof(*el->edited_map.help) * EL_NUM_FCNS);
	el->edited_map.func = edited_calloc(EL_NUM_FCNS, sizeof(*el->edited_map.func));
	if (el->edited_map.func == NULL)
		goto out;
	memcpy(el->edited_map.func, edited_func, sizeof(*el->edited_map.func)
	    * EL_NUM_FCNS);
	el->edited_map.nfunc = EL_NUM_FCNS;

#ifdef VIDEFAULT
	edited_map_init_vi(el);
#else
	edited_map_init_emacs(el);
#endif /* VIDEFAULT */
	return 0;
out:
	edited_map_end(el);
	return -1;
}


/* edited_map_end():
 *	Free the space taken by the editor maps
 */
libedited_private void
edited_map_end(Edited *el)
{

	edited_free(el->edited_map.alt);
	el->edited_map.alt = NULL;
	edited_free(el->edited_map.key);
	el->edited_map.key = NULL;
	el->edited_map.emacs = NULL;
	el->edited_map.vic = NULL;
	el->edited_map.vii = NULL;
	edited_free(el->edited_map.help);
	el->edited_map.help = NULL;
	edited_free(el->edited_map.func);
	el->edited_map.func = NULL;
}


/* edited_map_init_nls():
 *	Find all the printable keys and bind them to self insert
 */
static void
edited_map_init_nls(Edited *el)
{
	int i;

	edited_action_t *map = el->edited_map.key;

	for (i = 0200; i <= 0377; i++)
		if (iswprint(i))
			map[i] = EDITED_ED_INSERT;
}


/* edited_map_init_meta():
 *	Bind all the meta keys to the appropriate ESC-<key> sequence
 */
static void
edited_map_init_meta(Edited *el)
{
	wchar_t buf[3];
	int i;
	edited_action_t *map = el->edited_map.key;
	edited_action_t *alt = el->edited_map.alt;

	for (i = 0; i <= 0377 && map[i] != EDITED_EM_META_NEXT; i++)
		continue;

	if (i > 0377) {
		for (i = 0; i <= 0377 && alt[i] != EDITED_EM_META_NEXT; i++)
			continue;
		if (i > 0377) {
			i = 033;
			if (el->edited_map.type == MAP_VI)
				map = alt;
		} else
			map = alt;
	}
	buf[0] = (wchar_t)i;
	buf[2] = 0;
	for (i = 0200; i <= 0377; i++)
		switch (map[i]) {
		case EDITED_ED_INSERT:
		case EDITED_ED_UNASSIGNED:
		case EDITED_ED_SEQUENCE_LEAD_IN:
			break;
		default:
			buf[1] = i & 0177;
			edited_km_add(el, buf, edited_km_map_cmd(el, (int) map[i]), XK_CMD);
			break;
		}
	map[(int) buf[0]] = EDITED_ED_SEQUENCE_LEAD_IN;
}


/* edited_map_init_vi():
 *	Initialize the vi bindings
 */
libedited_private void
edited_map_init_vi(Edited *el)
{
	int i;
	edited_action_t *key = el->edited_map.key;
	edited_action_t *alt = el->edited_map.alt;
	const edited_action_t *vii = el->edited_map.vii;
	const edited_action_t *vic = el->edited_map.vic;

	el->edited_map.type = MAP_VI;
	el->edited_map.current = el->edited_map.key;

	edited_km_reset(el);

	for (i = 0; i < N_KEYS; i++) {
		key[i] = vii[i];
		alt[i] = vic[i];
	}

	edited_map_init_meta(el);
	edited_map_init_nls(el);

	edited_tty_bind_char(el, 1);
	edited_term_bind_arrow(el);
}


/* edited_map_init_emacs():
 *	Initialize the emacs bindings
 */
libedited_private void
edited_map_init_emacs(Edited *el)
{
	int i;
	wchar_t buf[3];
	edited_action_t *key = el->edited_map.key;
	edited_action_t *alt = el->edited_map.alt;
	const edited_action_t *emacs = el->edited_map.emacs;

	el->edited_map.type = MAP_EMACS;
	el->edited_map.current = el->edited_map.key;
	edited_km_reset(el);

	for (i = 0; i < N_KEYS; i++) {
		key[i] = emacs[i];
		alt[i] = EDITED_ED_UNASSIGNED;
	}

	edited_map_init_meta(el);
	edited_map_init_nls(el);

	buf[0] = CONTROL('X');
	buf[1] = CONTROL('X');
	buf[2] = 0;
	edited_km_add(el, buf, edited_km_map_cmd(el, EDITED_EM_EXCHANGE_MARK), XK_CMD);

	edited_tty_bind_char(el, 1);
	edited_term_bind_arrow(el);
}


/* edited_map_set_editor():
 *	Set the editor
 */
libedited_private int
edited_map_set_editor(Edited *el, wchar_t *editor)
{

	if (wcscmp(editor, L"emacs") == 0) {
		edited_map_init_emacs(el);
		return 0;
	}
	if (wcscmp(editor, L"vi") == 0) {
		edited_map_init_vi(el);
		return 0;
	}
	return -1;
}


/* edited_map_get_editor():
 *	Retrieve the editor
 */
libedited_private int
edited_map_get_editor(Edited *el, const wchar_t **editor)
{

	if (editor == NULL)
		return -1;
	switch (el->edited_map.type) {
	case MAP_EMACS:
		*editor = L"emacs";
		return 0;
	case MAP_VI:
		*editor = L"vi";
		return 0;
	}
	return -1;
}


/* edited_map_print_key():
 *	Print the function description for 1 key
 */
static void
edited_map_print_key(Edited *el, edited_action_t *map, const wchar_t *in)
{
	char outbuf[EL_BUFSIZ];
	edited_bindings_t *bp, *ep;

	if (in[0] == '\0' || in[1] == '\0') {
		(void) edited_km__decode_str(in, outbuf, sizeof(outbuf), "");
		ep = &el->edited_map.help[el->edited_map.nfunc];
		for (bp = el->edited_map.help; bp < ep; bp++)
			if (bp->func == map[(unsigned char) *in]) {
				(void) fprintf(el->edited_outfile,
				    "%s\t->\t%ls\n", outbuf, bp->name);
				return;
			}
	} else
		edited_km_print(el, in);
}


/* edited_map_print_some_keys():
 *	Print keys from first to last
 */
static void
edited_map_print_some_keys(Edited *el, edited_action_t *map, wint_t first, wint_t last)
{
	edited_bindings_t *bp, *ep;
	wchar_t firstbuf[2], lastbuf[2];
	char unparsbuf[EL_BUFSIZ], extrabuf[EL_BUFSIZ];

	firstbuf[0] = first;
	firstbuf[1] = 0;
	lastbuf[0] = last;
	lastbuf[1] = 0;
	if (map[first] == EDITED_ED_UNASSIGNED) {
		if (first == last) {
			(void) edited_km__decode_str(firstbuf, unparsbuf,
			    sizeof(unparsbuf), STRQQ);
			(void) fprintf(el->edited_outfile,
			    "%-15s->  is undefined\n", unparsbuf);
		}
		return;
	}
	ep = &el->edited_map.help[el->edited_map.nfunc];
	for (bp = el->edited_map.help; bp < ep; bp++) {
		if (bp->func == map[first]) {
			if (first == last) {
				(void) edited_km__decode_str(firstbuf, unparsbuf,
				    sizeof(unparsbuf), STRQQ);
				(void) fprintf(el->edited_outfile, "%-15s->  %ls\n",
				    unparsbuf, bp->name);
			} else {
				(void) edited_km__decode_str(firstbuf, unparsbuf,
				    sizeof(unparsbuf), STRQQ);
				(void) edited_km__decode_str(lastbuf, extrabuf,
				    sizeof(extrabuf), STRQQ);
				(void) fprintf(el->edited_outfile,
				    "%-4s to %-7s->  %ls\n",
				    unparsbuf, extrabuf, bp->name);
			}
			return;
		}
	}
#ifdef MAP_DEBUG
	if (map == el->edited_map.key) {
		(void) edited_km__decode_str(firstbuf, unparsbuf,
		    sizeof(unparsbuf), STRQQ);
		(void) fprintf(el->edited_outfile,
		    "BUG!!! %s isn't bound to anything.\n", unparsbuf);
		(void) fprintf(el->edited_outfile, "el->edited_map.key[%d] == %d\n",
		    first, el->edited_map.key[first]);
	} else {
		(void) edited_km__decode_str(firstbuf, unparsbuf,
		    sizeof(unparsbuf), STRQQ);
		(void) fprintf(el->edited_outfile,
		    "BUG!!! %s isn't bound to anything.\n", unparsbuf);
		(void) fprintf(el->edited_outfile, "el->edited_map.alt[%d] == %d\n",
		    first, el->edited_map.alt[first]);
	}
#endif
	EL_ABORT((el->edited_errfile, "Error printing keys\n"));
}


/* edited_map_print_all_keys():
 *	Print the function description for all keys.
 */
static void
edited_map_print_all_keys(Edited *el)
{
	int prev, i;

	(void) fprintf(el->edited_outfile, "Standard key bindings\n");
	prev = 0;
	for (i = 0; i < N_KEYS; i++) {
		if (el->edited_map.key[prev] == el->edited_map.key[i])
			continue;
		edited_map_print_some_keys(el, el->edited_map.key, prev, i - 1);
		prev = i;
	}
	edited_map_print_some_keys(el, el->edited_map.key, prev, i - 1);

	(void) fprintf(el->edited_outfile, "Alternative key bindings\n");
	prev = 0;
	for (i = 0; i < N_KEYS; i++) {
		if (el->edited_map.alt[prev] == el->edited_map.alt[i])
			continue;
		edited_map_print_some_keys(el, el->edited_map.alt, prev, i - 1);
		prev = i;
	}
	edited_map_print_some_keys(el, el->edited_map.alt, prev, i - 1);

	(void) fprintf(el->edited_outfile, "Multi-character bindings\n");
	edited_km_print(el, L"");
	(void) fprintf(el->edited_outfile, "Arrow key bindings\n");
	edited_term_print_arrow(el, L"");
}


/* edited_map_bind():
 *	Add/remove/change bindings
 */
libedited_private int
edited_map_bind(Edited *el, int argc, const wchar_t **argv)
{
	edited_action_t *map;
	int ntype, rem;
	const wchar_t *p;
	wchar_t inbuf[EL_BUFSIZ];
	wchar_t outbuf[EL_BUFSIZ];
	const wchar_t *in = NULL;
	wchar_t *out;
	edited_bindings_t *bp, *ep;
	int cmd;
	int key;

	if (argv == NULL)
		return -1;

	map = el->edited_map.key;
	ntype = XK_CMD;
	key = rem = 0;
	for (argc = 1; (p = argv[argc]) != NULL; argc++)
		if (p[0] == '-')
			switch (p[1]) {
			case 'a':
				map = el->edited_map.alt;
				break;

			case 's':
				ntype = XK_STR;
				break;
			case 'k':
				key = 1;
				break;

			case 'r':
				rem = 1;
				break;

			case 'v':
				edited_map_init_vi(el);
				return 0;

			case 'e':
				edited_map_init_emacs(el);
				return 0;

			case 'l':
				ep = &el->edited_map.help[el->edited_map.nfunc];
				for (bp = el->edited_map.help; bp < ep; bp++)
					(void) fprintf(el->edited_outfile,
					    "%ls\n\t%ls\n",
					    bp->name, bp->description);
				return 0;
			default:
				(void) fprintf(el->edited_errfile,
				    "%ls: Invalid switch `%lc'.\n",
				    argv[0], (wint_t)p[1]);
			}
		else
			break;

	if (argv[argc] == NULL) {
		edited_map_print_all_keys(el);
		return 0;
	}
	if (key)
		in = argv[argc++];
	else if ((in = edited_parse__string(inbuf, argv[argc++])) == NULL) {
		(void) fprintf(el->edited_errfile,
		    "%ls: Invalid \\ or ^ in instring.\n",
		    argv[0]);
		return -1;
	}
	if (rem) {
		if (key) {
			(void) edited_term_clear_arrow(el, in);
			return -1;
		}
		if (in[1])
			(void) edited_km_delete(el, in);
		else if (map[(unsigned char) *in] == EDITED_ED_SEQUENCE_LEAD_IN)
			(void) edited_km_delete(el, in);
		else
			map[(unsigned char) *in] = EDITED_ED_UNASSIGNED;
		return 0;
	}
	if (argv[argc] == NULL) {
		if (key)
			edited_term_print_arrow(el, in);
		else
			edited_map_print_key(el, map, in);
		return 0;
	}
#ifdef notyet
	if (argv[argc + 1] != NULL) {
		bindkeymacro_usage();
		return -1;
	}
#endif

	switch (ntype) {
	case XK_STR:
		if ((out = edited_parse__string(outbuf, argv[argc])) == NULL) {
			(void) fprintf(el->edited_errfile,
			    "%ls: Invalid \\ or ^ in outstring.\n", argv[0]);
			return -1;
		}
		if (key)
			edited_term_set_arrow(el, in, edited_km_map_str(el, out), ntype);
		else
			edited_km_add(el, in, edited_km_map_str(el, out), ntype);
		map[(unsigned char) *in] = EDITED_ED_SEQUENCE_LEAD_IN;
		break;

	case XK_CMD:
		if ((cmd = edited_parse_cmd(el, argv[argc])) == -1) {
			(void) fprintf(el->edited_errfile,
			    "%ls: Invalid command `%ls'.\n",
			    argv[0], argv[argc]);
			return -1;
		}
		if (key)
			edited_term_set_arrow(el, in, edited_km_map_cmd(el, cmd), ntype);
		else {
			if (in[1]) {
				edited_km_add(el, in, edited_km_map_cmd(el, cmd), ntype);
				map[(unsigned char) *in] = EDITED_ED_SEQUENCE_LEAD_IN;
			} else {
				edited_km_clear(el, map, in);
				map[(unsigned char) *in] = (edited_action_t)cmd;
			}
		}
		break;

	/* coverity[dead_error_begin] */
	default:
		EL_ABORT((el->edited_errfile, "Bad XK_ type %d\n", ntype));
		break;
	}
	return 0;
}


/* edited_map_addfunc():
 *	add a user defined function
 */
libedited_private int
edited_map_addfunc(Edited *el, const wchar_t *name, const wchar_t *help,
    edited_func_t func)
{
	void *p;
	size_t nf = el->edited_map.nfunc + 1;

	if (name == NULL || help == NULL || func == NULL)
		return -1;

	if ((p = edited_realloc(el->edited_map.func, nf *
	    sizeof(*el->edited_map.func))) == NULL)
		return -1;
	el->edited_map.func = p;
	if ((p = edited_realloc(el->edited_map.help, nf * sizeof(*el->edited_map.help)))
	    == NULL)
		return -1;
	el->edited_map.help = p;

	nf = (size_t)el->edited_map.nfunc;
	el->edited_map.func[nf] = func;

	el->edited_map.help[nf].name = name;
	el->edited_map.help[nf].func = (int)nf;
	el->edited_map.help[nf].description = help;
	el->edited_map.nfunc++;

	return 0;
}
