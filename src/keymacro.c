/*	$NetBSD: keymacro.c,v 1.24 2019/07/23 10:18:52 christos Exp $	*/

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
static char sccsid[] = "@(#)key.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: keymacro.c,v 1.24 2019/07/23 10:18:52 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * keymacro.c: This module contains the procedures for maintaining
 *	       the extended-key map.
 *
 *      An extended-key (key) is a sequence of keystrokes introduced
 *	with a sequence introducer and consisting of an arbitrary
 *	number of characters.  This module maintains a map (the
 *	el->edited_keymacro.map)
 *	to convert these extended-key sequences into input strs
 *	(XK_STR) or editor functions (XK_CMD).
 *
 *      Warning:
 *	  If key is a substr of some other keys, then the longer
 *	  keys are lost!!  That is, if the keys "abcd" and "abcef"
 *	  are in el->edited_keymacro.map, adding the key "abc" will cause
 *	  the first two definitions to be lost.
 *
 *      Restrictions:
 *      -------------
 *      1) It is not possible to have one key that is a
 *	   substr of another.
 */
#include <stdlib.h>
#include <string.h>

#include "edited/el.h"
#include "edited/fcns.h"

/*
 * The Nodes of the el->edited_keymacro.map.  The el->edited_keymacro.map is a
 * linked list of these node elements
 */
struct edited_km_node_t {
	wchar_t		 ch;		/* single character of key	 */
	int		 type;		/* node type			 */
	edited_km_value_t val;		/* command code or pointer to str,  */
					/* if this is a leaf		 */
	struct edited_km_node_t *next;	/* ptr to next char of this key  */
	struct edited_km_node_t *sibling;/* ptr to another key with same prefix*/
};

static int		 node_trav(Edited *, edited_km_node_t *, wchar_t *,
    edited_km_value_t *);
static int		 node__try(Edited *, edited_km_node_t *,
    const wchar_t *, edited_km_value_t *, int);
static edited_km_node_t	*node__get(wint_t);
static void		 node__free(edited_km_node_t *);
static void		 node__put(Edited *, edited_km_node_t *);
static int		 node__delete(Edited *, edited_km_node_t **,
    const wchar_t *);
static int		 node_lookup(Edited *, const wchar_t *,
    edited_km_node_t *, size_t);
static int		 node_enum(Edited *, edited_km_node_t *, size_t);

#define	KEY_BUFSIZ	EL_BUFSIZ


/* edited_km_init():
 *	Initialize the key maps
 */
libedited_private int
edited_km_init(Edited *el)
{

	el->edited_keymacro.buf = edited_calloc(KEY_BUFSIZ,
	    sizeof(*el->edited_keymacro.buf));
	if (el->edited_keymacro.buf == NULL)
		return -1;
	el->edited_keymacro.map = NULL;
	edited_km_reset(el);
	return 0;
}

/* edited_km_end():
 *	Free the key maps
 */
libedited_private void
edited_km_end(Edited *el)
{

	edited_free(el->edited_keymacro.buf);
	el->edited_keymacro.buf = NULL;
	node__free(el->edited_keymacro.map);
}


/* edited_km_map_cmd():
 *	Associate cmd with a key value
 */
libedited_private edited_km_value_t *
edited_km_map_cmd(Edited *el, int cmd)
{

	el->edited_keymacro.val.cmd = (edited_action_t) cmd;
	return &el->edited_keymacro.val;
}


/* edited_km_map_str():
 *	Associate str with a key value
 */
libedited_private edited_km_value_t *
edited_km_map_str(Edited *el, wchar_t *str)
{

	el->edited_keymacro.val.str = str;
	return &el->edited_keymacro.val;
}


/* edited_km_reset():
 *	Takes all nodes on el->edited_keymacro.map and puts them on free list.
 *	Then initializes el->edited_keymacro.map with arrow keys
 *	[Always bind the ansi arrow keys?]
 */
libedited_private void
edited_km_reset(Edited *el)
{

	node__put(el, el->edited_keymacro.map);
	el->edited_keymacro.map = NULL;
	return;
}


/* edited_km_get():
 *	Calls the recursive function with entry point el->edited_keymacro.map
 *      Looks up *ch in map and then reads characters until a
 *      complete match is found or a mismatch occurs. Returns the
 *      type of the match found (XK_STR or XK_CMD).
 *      Returns NULL in val.str and XK_STR for no match.
 *      Returns XK_NOD for end of file or read error.
 *      The last character read is returned in *ch.
 */
libedited_private int
edited_km_get(Edited *el, wchar_t *ch, edited_km_value_t *val)
{

	return node_trav(el, el->edited_keymacro.map, ch, val);
}


/* edited_km_add():
 *      Adds key to the el->edited_keymacro.map and associates the value in
 *	val with it. If key is already is in el->edited_keymacro.map, the new
 *	code is applied to the existing key. Ntype specifies if code is a
 *	command, an out str or a unix command.
 */
libedited_private void
edited_km_add(Edited *el, const wchar_t *key, edited_km_value_t *val,
    int ntype)
{

	if (key[0] == '\0') {
		(void) fprintf(el->edited_errfile,
		    "edited_km_add: Null extended-key not allowed.\n");
		return;
	}
	if (ntype == XK_CMD && val->cmd == EDITED_ED_SEQUENCE_LEAD_IN) {
		(void) fprintf(el->edited_errfile,
		    "edited_km_add: sequence-lead-in command not allowed\n");
		return;
	}
	if (el->edited_keymacro.map == NULL)
		/* tree is initially empty.  Set up new node to match key[0] */
		el->edited_keymacro.map = node__get(key[0]);
			/* it is properly initialized */

	/* Now recurse through el->edited_keymacro.map */
	(void) node__try(el, el->edited_keymacro.map, key, val, ntype);
	return;
}


/* edited_km_clear():
 *
 */
libedited_private void
edited_km_clear(Edited *el, edited_action_t *map, const wchar_t *in)
{
        if (*in > N_KEYS) /* can't be in the map */
                return;
	if ((map[(unsigned char)*in] == EDITED_ED_SEQUENCE_LEAD_IN) &&
	    ((map == el->edited_map.key &&
	    el->edited_map.alt[(unsigned char)*in] != EDITED_ED_SEQUENCE_LEAD_IN) ||
	    (map == el->edited_map.alt &&
	    el->edited_map.key[(unsigned char)*in] != EDITED_ED_SEQUENCE_LEAD_IN)))
		(void) edited_km_delete(el, in);
}


/* edited_km_delete():
 *      Delete the key and all longer keys staring with key, if
 *      they exists.
 */
libedited_private int
edited_km_delete(Edited *el, const wchar_t *key)
{

	if (key[0] == '\0') {
		(void) fprintf(el->edited_errfile,
		    "edited_km_delete: Null extended-key not allowed.\n");
		return -1;
	}
	if (el->edited_keymacro.map == NULL)
		return 0;

	(void) node__delete(el, &el->edited_keymacro.map, key);
	return 0;
}


/* edited_km_print():
 *	Print the binding associated with key key.
 *	Print entire el->edited_keymacro.map if null
 */
libedited_private void
edited_km_print(Edited *el, const wchar_t *key)
{

	/* do nothing if el->edited_keymacro.map is empty and null key specified */
	if (el->edited_keymacro.map == NULL && *key == 0)
		return;

	el->edited_keymacro.buf[0] = '"';
	if (node_lookup(el, key, el->edited_keymacro.map, (size_t)1) <= -1)
		/* key is not bound */
		(void) fprintf(el->edited_errfile, "Unbound extended key \"%ls"
		    "\"\n", key);
	return;
}


/* node_trav():
 *	recursively traverses node in tree until match or mismatch is
 *	found.  May read in more characters.
 */
static int
node_trav(Edited *el, edited_km_node_t *ptr, wchar_t *ch,
    edited_km_value_t *val)
{

	if (ptr->ch == *ch) {
		/* match found */
		if (ptr->next) {
			/* key not complete so get next char */
			if (edited_wgetc(el, ch) != 1)
				return XK_NOD;
			return node_trav(el, ptr->next, ch, val);
		} else {
			*val = ptr->val;
			if (ptr->type != XK_CMD)
				*ch = '\0';
			return ptr->type;
		}
	} else {
		/* no match found here */
		if (ptr->sibling) {
			/* try next sibling */
			return node_trav(el, ptr->sibling, ch, val);
		} else {
			/* no next sibling -- mismatch */
			val->str = NULL;
			return XK_STR;
		}
	}
}


/* node__try():
 *	Find a node that matches *str or allocate a new one
 */
static int
node__try(Edited *el, edited_km_node_t *ptr, const wchar_t *str,
    edited_km_value_t *val, int ntype)
{

	if (ptr->ch != *str) {
		edited_km_node_t *xm;

		for (xm = ptr; xm->sibling != NULL; xm = xm->sibling)
			if (xm->sibling->ch == *str)
				break;
		if (xm->sibling == NULL)
			xm->sibling = node__get(*str);	/* setup new node */
		ptr = xm->sibling;
	}
	if (*++str == '\0') {
		/* we're there */
		if (ptr->next != NULL) {
			node__put(el, ptr->next);
				/* lose longer keys with this prefix */
			ptr->next = NULL;
		}
		switch (ptr->type) {
		case XK_CMD:
		case XK_NOD:
			break;
		case XK_STR:
			if (ptr->val.str)
				edited_free(ptr->val.str);
			break;
		default:
			EL_ABORT((el->edited_errfile, "Bad XK_ type %d\n",
			    ptr->type));
			break;
		}

		switch (ptr->type = ntype) {
		case XK_CMD:
			ptr->val = *val;
			break;
		case XK_STR:
			if ((ptr->val.str = wcsdup(val->str)) == NULL)
				return -1;
			break;
		default:
			EL_ABORT((el->edited_errfile, "Bad XK_ type %d\n", ntype));
			break;
		}
	} else {
		/* still more chars to go */
		if (ptr->next == NULL)
			ptr->next = node__get(*str);	/* setup new node */
		(void) node__try(el, ptr->next, str, val, ntype);
	}
	return 0;
}


/* node__delete():
 *	Delete node that matches str
 */
static int
node__delete(Edited *el, edited_km_node_t **inptr, const wchar_t *str)
{
	edited_km_node_t *ptr;
	edited_km_node_t *prev_ptr = NULL;

	ptr = *inptr;

	if (ptr->ch != *str) {
		edited_km_node_t *xm;

		for (xm = ptr; xm->sibling != NULL; xm = xm->sibling)
			if (xm->sibling->ch == *str)
				break;
		if (xm->sibling == NULL)
			return 0;
		prev_ptr = xm;
		ptr = xm->sibling;
	}
	if (*++str == '\0') {
		/* we're there */
		if (prev_ptr == NULL)
			*inptr = ptr->sibling;
		else
			prev_ptr->sibling = ptr->sibling;
		ptr->sibling = NULL;
		node__put(el, ptr);
		return 1;
	} else if (ptr->next != NULL &&
	    node__delete(el, &ptr->next, str) == 1) {
		if (ptr->next != NULL)
			return 0;
		if (prev_ptr == NULL)
			*inptr = ptr->sibling;
		else
			prev_ptr->sibling = ptr->sibling;
		ptr->sibling = NULL;
		node__put(el, ptr);
		return 1;
	} else {
		return 0;
	}
}


/* node__put():
 *	Puts a tree of nodes onto free list using free(3).
 */
static void
node__put(Edited *el, edited_km_node_t *ptr)
{
	if (ptr == NULL)
		return;

	if (ptr->next != NULL) {
		node__put(el, ptr->next);
		ptr->next = NULL;
	}
	node__put(el, ptr->sibling);

	switch (ptr->type) {
	case XK_CMD:
	case XK_NOD:
		break;
	case XK_STR:
		if (ptr->val.str != NULL)
			edited_free(ptr->val.str);
		break;
	default:
		EL_ABORT((el->edited_errfile, "Bad XK_ type %d\n", ptr->type));
		break;
	}
	edited_free(ptr);
}


/* node__get():
 *	Returns pointer to a edited_km_node_t for ch.
 */
static edited_km_node_t *
node__get(wint_t ch)
{
	edited_km_node_t *ptr;

	ptr = edited_malloc(sizeof(*ptr));
	if (ptr == NULL)
		return NULL;
	ptr->ch = ch;
	ptr->type = XK_NOD;
	ptr->val.str = NULL;
	ptr->next = NULL;
	ptr->sibling = NULL;
	return ptr;
}

static void
node__free(edited_km_node_t *k)
{
	if (k == NULL)
		return;
	node__free(k->sibling);
	node__free(k->next);
	edited_free(k);
}

/* node_lookup():
 *	look for the str starting at node ptr.
 *	Print if last node
 */
static int
node_lookup(Edited *el, const wchar_t *str, edited_km_node_t *ptr,
    size_t cnt)
{
	ssize_t used;

	if (ptr == NULL)
		return -1;	/* cannot have null ptr */

	if (!str || *str == 0) {
		/* no more chars in str.  node_enum from here. */
		(void) node_enum(el, ptr, cnt);
		return 0;
	} else {
		/* If match put this char into el->edited_keymacro.buf.  Recurse */
		if (ptr->ch == *str) {
			/* match found */
			used = edited_ct_visual_char(el->edited_keymacro.buf + cnt,
			    KEY_BUFSIZ - cnt, ptr->ch);
			if (used == -1)
				return -1; /* ran out of buffer space */
			if (ptr->next != NULL)
				/* not yet at leaf */
				return (node_lookup(el, str + 1, ptr->next,
				    (size_t)used + cnt));
			else {
			    /* next node is null so key should be complete */
				if (str[1] == 0) {
					size_t px = cnt + (size_t)used;
					el->edited_keymacro.buf[px] = '"';
					el->edited_keymacro.buf[px + 1] = '\0';
					edited_km_kprint(el, el->edited_keymacro.buf,
					    &ptr->val, ptr->type);
					return 0;
				} else
					return -1;
					/* mismatch -- str still has chars */
			}
		} else {
			/* no match found try sibling */
			if (ptr->sibling)
				return (node_lookup(el, str, ptr->sibling,
				    cnt));
			else
				return -1;
		}
	}
}


/* node_enum():
 *	Traverse the node printing the characters it is bound in buffer
 */
static int
node_enum(Edited *el, edited_km_node_t *ptr, size_t cnt)
{
        ssize_t used;

	if (cnt >= KEY_BUFSIZ - 5) {	/* buffer too small */
		el->edited_keymacro.buf[++cnt] = '"';
		el->edited_keymacro.buf[++cnt] = '\0';
		(void) fprintf(el->edited_errfile,
		    "Some extended keys too long for internal print buffer");
		(void) fprintf(el->edited_errfile, " \"%ls...\"\n",
		    el->edited_keymacro.buf);
		return 0;
	}
	if (ptr == NULL) {
#ifdef DEBUG_EDIT
		(void) fprintf(el->edited_errfile,
		    "node_enum: BUG!! Null ptr passed\n!");
#endif
		return -1;
	}
	/* put this char at end of str */
        used = edited_ct_visual_char(el->edited_keymacro.buf + cnt, KEY_BUFSIZ - cnt,
	    ptr->ch);
	if (ptr->next == NULL) {
		/* print this key and function */
		el->edited_keymacro.buf[cnt + (size_t)used   ] = '"';
		el->edited_keymacro.buf[cnt + (size_t)used + 1] = '\0';
		edited_km_kprint(el, el->edited_keymacro.buf, &ptr->val, ptr->type);
	} else
		(void) node_enum(el, ptr->next, cnt + (size_t)used);

	/* go to sibling if there is one */
	if (ptr->sibling)
		(void) node_enum(el, ptr->sibling, cnt);
	return 0;
}


/* edited_km_kprint():
 *	Print the specified key and its associated
 *	function specified by val
 */
libedited_private void
edited_km_kprint(Edited *el, const wchar_t *key, edited_km_value_t *val,
    int ntype)
{
	edited_bindings_t *fp;
	char unparsbuf[EL_BUFSIZ];
	static const char fmt[] = "%-15s->  %s\n";

	if (val != NULL)
		switch (ntype) {
		case XK_STR:
			(void) edited_km__decode_str(val->str, unparsbuf,
			    sizeof(unparsbuf),
			    ntype == XK_STR ? "\"\"" : "[]");
			(void) fprintf(el->edited_outfile, fmt,
			    edited_ct_encode_string(key, &el->edited_scratch), unparsbuf);
			break;
		case XK_CMD:
			for (fp = el->edited_map.help; fp->name; fp++)
				if (val->cmd == fp->func) {
                    wcstombs(unparsbuf, fp->name, sizeof(unparsbuf));
                    unparsbuf[sizeof(unparsbuf) -1] = '\0';
					(void) fprintf(el->edited_outfile, fmt,
                        edited_ct_encode_string(key, &el->edited_scratch), unparsbuf);
					break;
				}
#ifdef DEBUG_KEY
			if (fp->name == NULL)
				(void) fprintf(el->edited_outfile,
				    "BUG! Command not found.\n");
#endif

			break;
		default:
			EL_ABORT((el->edited_errfile, "Bad XK_ type %d\n", ntype));
			break;
		}
	else
		(void) fprintf(el->edited_outfile, fmt, edited_ct_encode_string(key,
		    &el->edited_scratch), "no input");
}


#define EDITED_VI_ADDC(c) \
	if (b < eb) \
		*b++ = c; \
	else \
		b++
/* edited_km__decode_str():
 *	Make a printable version of the ey
 */
libedited_private size_t
edited_km__decode_str(const wchar_t *str, char *buf, size_t len,
    const char *sep)
{
	char *b = buf, *eb = b + len;
	const wchar_t *p;

	b = buf;
	if (sep[0] != '\0') {
		EDITED_VI_ADDC(sep[0]);
	}
	if (*str == '\0') {
		EDITED_VI_ADDC('^');
		EDITED_VI_ADDC('@');
		goto add_endsep;
	}
	for (p = str; *p != 0; p++) {
		wchar_t dbuf[VISUAL_WIDTH_MAX];
		wchar_t *p2 = dbuf;
		ssize_t l = edited_ct_visual_char(dbuf, VISUAL_WIDTH_MAX, *p);
		while (l-- > 0) {
			ssize_t n = edited_ct_encode_char(b, (size_t)(eb - b), *p2++);
			if (n == -1) /* ran out of space */
				goto add_endsep;
			else
				b += n;
		}
	}
add_endsep:
	if (sep[0] != '\0' && sep[1] != '\0') {
		EDITED_VI_ADDC(sep[1]);
	}
	EDITED_VI_ADDC('\0');
	if ((size_t)(b - buf) >= len)
	    buf[len - 1] = '\0';
	return (size_t)(b - buf);
}
