/*	$OpenBSD: mappedcopy.c,v 1.1 1997/02/10 11:11:51 downsj Exp $	*/
/*	$NetBSD: mappedcopy.c,v 1.1 1997/02/02 06:54:10 thorpej Exp $	*/

/*
 * XXX This doesn't work yet.  Soon.  --thorpej@netbsd.org
 */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: vm_machdep.c 1.21 91/04/06$
 *
 *	@(#)vm_machdep.c	8.6 (Berkeley) 1/12/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/cpu.h>

#ifdef DEBUG
#define	MDB_COPYIN	0x01
#define	MDB_COPYOUT	0x02
int	mappedcopydebug = 0;

int	mappedcopyincount;
int	mappedcopyoutcount;
#endif

/*
 * This turns off mappedcopy by default.  Ports initialize the
 * threshold in initcpu().
 */
u_int	mappedcopysize = -1;

/*
 * N.B. Both of these routines assume PAGE_SIZE == NBPG.
 */

int
mappedcopyin(fromp, top, count)
	register void *fromp, *top;
	register size_t count;
{
	register vm_offset_t kva, upa;
	register size_t len;
	int off, alignable;
	pmap_t upmap;
	extern caddr_t CADDR1;

#ifdef DEBUG
	if (mappedcopydebug & MDB_COPYIN)
		printf("mappedcopyin(%p, %p, %lu), pid %d\n",
		    fromp, top, (u_long)count, curproc->p_pid);
	mappedcopyincount++;
#endif

	kva = (vm_offset_t)CADDR1;
	off = (int)((u_long)fromp & PAGE_MASK);
	alignable = (off == ((u_long)top & PAGE_MASK));
	upmap = vm_map_pmap(&curproc->p_vmspace->vm_map);
	while (count > 0) {
		/*
		 * First access of a page, use fubyte to make sure
		 * page is faulted in and read access allowed.
		 */
		if (fubyte(fromp) == -1)
			return (EFAULT);
		/*
		 * Map in the page and bcopy data in from it
		 */
		upa = pmap_extract(upmap, trunc_page(fromp));
		if (upa == 0)
			panic("mappedcopyin: null page frame");
		len = min(count, (PAGE_SIZE - off));
		pmap_enter(pmap_kernel(), kva, upa, VM_PROT_READ, TRUE);
		if (len == PAGE_SIZE && alignable && off == 0)
			copypage((caddr_t)kva, top);
		else
			bcopy((void *)(kva + off), top, len);
		fromp += len;
		top += len;
		count -= len;
		off = 0;
	}
	pmap_remove(pmap_kernel(), kva, kva + PAGE_SIZE);
	return (0);
}

int
mappedcopyout(fromp, top, count)
	register void *fromp, *top;
	register size_t count;
{
	register vm_offset_t kva, upa;
	register size_t len;
	int off, alignable;
	pmap_t upmap;
	extern caddr_t CADDR2;

#ifdef DEBUG
	if (mappedcopydebug & MDB_COPYOUT)
		printf("mappedcopyout(%p, %p, %lu), pid %d\n",
		    fromp, top, (u_long)count, curproc->p_pid);
	mappedcopyoutcount++;
#endif

	kva = (vm_offset_t) CADDR2;
	off = (int)((u_long)top & PAGE_MASK);
	alignable = (off == ((u_long)fromp & PAGE_MASK));
	upmap = vm_map_pmap(&curproc->p_vmspace->vm_map);
	while (count > 0) {
		/*
		 * First access of a page, use subyte to make sure
		 * page is faulted in and write access allowed.
		 */
		if (subyte(top, *((char *)fromp)) == -1)
			return (EFAULT);
		/*
		 * Map in the page and bcopy data out to it
		 */
		upa = pmap_extract(upmap, trunc_page(top));
		if (upa == 0)
			panic("mappedcopyout: null page frame");
		len = min(count, (PAGE_SIZE - off));
		pmap_enter(pmap_kernel(), kva, upa,
		    VM_PROT_READ|VM_PROT_WRITE, TRUE);
		if (len == PAGE_SIZE && alignable && off == 0)
			copypage(fromp, (caddr_t)kva);
		else
			bcopy(fromp, (void *)(kva + off), len);
		fromp += len;
		top += len;
		count -= len;
		off = 0;
	}
	pmap_remove(pmap_kernel(), kva, kva + PAGE_SIZE);
	return (0);
}
