/*	$NetBSD: sig.c,v 1.27 2023/02/03 19:47:38 christos Exp $	*/

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
static char sccsid[] = "@(#)sig.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: sig.c,v 1.27 2023/02/03 19:47:38 christos Exp $");
#endif
#endif /* not lint && not SCCSID */

/*
 * sig.c: Signal handling stuff.
 *	  our policy is to trap all signals, set a good state
 *	  and pass the ball to our caller.
 */
#include <errno.h>
#include <stdlib.h>

#include "edited/el.h"
#include "edited/common.h"

static Edited *sel = NULL;

static const int sighdl[] = {
#define	_DO(a)	(a),
	ALLSIGS
#undef	_DO
	- 1
};

static void edited_sig_handler(int);

/* edited_sig_handler():
 *	This is the handler called for all signals
 *	XXX: we cannot pass any data so we just store the old editline
 *	state in a private variable
 */
static void
edited_sig_handler(int signo)
{
	int i, save_errno;
	sigset_t nset, oset;

	save_errno = errno;
	(void) sigemptyset(&nset);
	(void) sigaddset(&nset, signo);
	(void) sigprocmask(SIG_BLOCK, &nset, &oset);

	sel->edited_signal->edited_sig_no = signo;

	switch (signo) {
	case SIGCONT:
		edited_tty_rawmode(sel);
		if (edited_ed_redisplay(sel, 0) == CC_REFRESH)
			edited_re_refresh(sel);
		edited_term__flush(sel);
		break;

	case SIGWINCH:
		edited_resize(sel);
		break;

	default:
		edited_tty_cookedmode(sel);
		break;
	}

	for (i = 0; sighdl[i] != -1; i++)
		if (signo == sighdl[i])
			break;

	(void) sigaction(signo, &sel->edited_signal->edited_sig_action[i], NULL);
	sel->edited_signal->edited_sig_action[i].sa_handler = SIG_ERR;
	sel->edited_signal->edited_sig_action[i].sa_flags = 0;
	sigemptyset(&sel->edited_signal->edited_sig_action[i].sa_mask);
	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
	(void) kill(0, signo);
	errno = save_errno;
}


/* edited_sig_init():
 *	Initialize all signal stuff
 */
libedited_private int
edited_sig_init(Edited *el)
{
	size_t i;
	sigset_t *nset, oset;

	el->edited_signal = edited_malloc(sizeof(*el->edited_signal));
	if (el->edited_signal == NULL)
		return -1;

	nset = &el->edited_signal->edited_sig_set;
	(void) sigemptyset(nset);
#define	_DO(a) (void) sigaddset(nset, a);
	ALLSIGS
#undef	_DO
	(void) sigprocmask(SIG_BLOCK, nset, &oset);

	for (i = 0; sighdl[i] != -1; i++) {
		el->edited_signal->edited_sig_action[i].sa_handler = SIG_ERR;
		el->edited_signal->edited_sig_action[i].sa_flags = 0;
		sigemptyset(&el->edited_signal->edited_sig_action[i].sa_mask);
	}

	(void) sigprocmask(SIG_SETMASK, &oset, NULL);

	return 0;
}


/* edited_sig_end():
 *	Clear all signal stuff
 */
libedited_private void
edited_sig_end(Edited *el)
{

	edited_free(el->edited_signal);
	el->edited_signal = NULL;
}


/* edited_sig_set():
 *	set all the signal handlers
 */
libedited_private void
edited_sig_set(Edited *el)
{
	size_t i;
	sigset_t oset;
	struct sigaction osa, nsa;

	nsa.sa_handler = edited_sig_handler;
	nsa.sa_flags = 0;
	sigemptyset(&nsa.sa_mask);

	sel = el;
	(void) sigprocmask(SIG_BLOCK, &el->edited_signal->edited_sig_set, &oset);

	for (i = 0; sighdl[i] != -1; i++) {
		/* This could happen if we get interrupted */
		if (sigaction(sighdl[i], &nsa, &osa) != -1 &&
		    osa.sa_handler != edited_sig_handler)
			el->edited_signal->edited_sig_action[i] = osa;
	}
	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
}


/* edited_sig_clr():
 *	clear all the signal handlers
 */
libedited_private void
edited_sig_clr(Edited *el)
{
	size_t i;
	sigset_t oset;

	(void) sigprocmask(SIG_BLOCK, &el->edited_signal->edited_sig_set, &oset);

	for (i = 0; sighdl[i] != -1; i++)
		if (el->edited_signal->edited_sig_action[i].sa_handler != SIG_ERR)
			(void)sigaction(sighdl[i],
			    &el->edited_signal->edited_sig_action[i], NULL);

	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
}
