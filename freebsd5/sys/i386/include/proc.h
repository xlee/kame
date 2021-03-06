/*
 * Copyright (c) 1991 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	from: @(#)proc.h	7.1 (Berkeley) 5/15/91
 * $FreeBSD: src/sys/i386/include/proc.h,v 1.19 2002/10/25 20:06:16 jhb Exp $
 */

#ifndef _MACHINE_PROC_H_
#define	_MACHINE_PROC_H_

#include <machine/segments.h>

struct proc_ldt {
        caddr_t ldt_base;
        int     ldt_len;
        int     ldt_refcnt;
        u_long  ldt_active;
        struct  segment_descriptor ldt_sd;
};

/*
 * Machine-dependent part of the proc structure for i386.
 */
struct mdthread {
#ifdef lint
	int	dummy;
#endif
};

struct mdproc {
	struct proc_ldt *md_ldt;	/* (j) per-process ldt */
};

#ifdef	_KERNEL

void 	set_user_ldt(struct mdproc *);
struct 	proc_ldt *user_ldt_alloc(struct mdproc *, int);
void 	user_ldt_free(struct thread *);

#endif	/* _KERNEL */

#endif /* !_MACHINE_PROC_H_ */
