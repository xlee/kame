/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* YIPS @(#)$Id: policy.c,v 1.3 2000/01/10 22:38:39 itojun Exp $ */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "debug.h"

#include "policy.h"
#include "localconf.h"

static LIST_HEAD(_sptree, policyindex) sptree;

struct policyindex *
getspidx(spidx)
	struct policyindex *spidx;
{
	struct policyindex *p;

	LIST_FOREACH(p, &sptree, chain) {
		if (!cmpspidx(spidx, p))
			return p;
	}

	return NULL;
}

/*
 * compare policyindex.
 * OUT:	0:	equal
 *	1:	not equal
 */
int
cmpspidx(a, b)
	struct policyindex *a, *b;
{
	YIPSDEBUG(DEBUG_MISC,
		plog(logp, LOCATION, NULL, "comparing %p and %p\n");
		plog(logp, LOCATION, NULL, "%p: %s\n", a, spidx2str(a));
		plog(logp, LOCATION, NULL, "%p: %s\n", b, spidx2str(b)););

	if (a->dir != b->dir
	 || a->prefs != b->prefs
	 || a->prefd != b->prefd
	 || a->ul_proto != b->ul_proto
	 || a->action != b->action)
		return 1;

	if (cmpsaddr((struct sockaddr *)&a->src, (struct sockaddr *)&b->src))
		return 0;
	if (cmpsaddr((struct sockaddr *)&a->dst, (struct sockaddr *)&b->dst))
		return 0;

	return 0;
}

struct policyindex *
newspidx()
{
	struct policyindex *new;

	new = CALLOC(sizeof(*new), struct policyindex *);
	if (new == NULL) {
		plog(logp, LOCATION, NULL,
			"calloc (%s)", strerror(errno));
		return NULL;
	}

	new->policy = NULL;

	return new;
}

void
delspidx(spidx)
	struct policyindex *spidx;
{
	free(spidx);
}

void
insspidx(new)
	struct policyindex *new;
{
	LIST_INSERT_HEAD(&sptree, new, chain);
}

void
remspidx(spidx)
	struct policyindex *spidx;
{
	LIST_REMOVE(spidx, chain);
}

void
initspidx()
{
	LIST_INIT(&sptree);
}

struct ipsecpolicy *
newipsp()
{
	struct ipsecpolicy *new;

	new = CALLOC(sizeof(*new), struct ipsecpolicy *);
	if (new == NULL) {
		plog(logp, LOCATION, NULL,
			"calloc (%s)", strerror(errno));
		return NULL;
	}

	new->proposal = NULL;
	new->spidx = NULL;

	return new;
}

struct ipsecsa *
newipsa()
{
	struct ipsecsa *new;

	new = CALLOC(sizeof(*new), struct ipsecsa *);
	if (new == NULL) {
		plog(logp, LOCATION, NULL,
			"calloc (%s)", strerror(errno));
		return NULL;
	}

	new->bundles = NULL;
	new->next = NULL;
	new->ipsp = NULL;

	return new;
}

void
insipsa(new, ipsp)
	struct ipsecsa *new;
	struct ipsecpolicy *ipsp;
{
	struct ipsecsa *p;

	new->ipsp = ipsp;

	for (p = ipsp->proposal; p != NULL; p = p->next) {
		/* bundled SA ? */
		if (new->prop_no == p->prop_no) {
			if (p->bundles == NULL) {
				p->bundles = new;
				return;
			}
			for (p = p->bundles; p->bundles != NULL; p = p->bundles)
				;
			p->bundles = new;
			return;
		}
		if (p->next == NULL) {
			/* new SA suite */
			p->next = new;
			return;
		}
	}

	/* first sa */
	ipsp->proposal = new;

	return;
}

char *
spidx2str(spidx)
	struct policyindex *spidx;
{
	/* addr/pref[port] addr/pref[port] ul dir act */
	static char buf[256];
	char *p, *a, *b;
	int blen, i;

	blen = sizeof(buf) - 1;
	p = buf;

	a = saddr2str((struct sockaddr *)&spidx->src);
	for (b = a; *b != '\0'; b++)
		if (*b == '[') {
			*b = '\0';
			b++;
			break;
		}
	i = snprintf(p, blen, "%s/%d[%s ", a, spidx->prefs, b);
	p += i;
	blen -= i;

	a = saddr2str((struct sockaddr *)&spidx->dst);
	for (b = a; *b != '\0'; b++)
		if (*b == '[') {
			*b = '\0';
			b++;
			break;
		}
	i = snprintf(p, blen, "%s/%d[%s ", a, spidx->prefd, b);
	p += i;
	blen -= i;

	snprintf(p, blen, "proto=%d dir=%d action=%d",
		spidx->ul_proto, spidx->dir, spidx->action);

	return buf;
}
