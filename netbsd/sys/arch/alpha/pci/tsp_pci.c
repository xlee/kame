/* $NetBSD: tsp_pci.c,v 1.1 1999/06/29 06:46:47 ross Exp $ */

/*-
 * Copyright (c) 1999 by Ross Harvey.  All rights reserved.
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
 *	This product includes software developed by Ross Harvey.
 * 4. The name of Ross Harvey may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ROSS HARVEY ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURP0SE
 * ARE DISCLAIMED.  IN NO EVENT SHALL ROSS HARVEY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>

__KERNEL_RCSID(0, "$NetBSD");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <vm/vm.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <machine/autoconf.h>
#include <machine/rpb.h>

#include <alpha/pci/tsreg.h>
#include <alpha/pci/tsvar.h>

#define tsp_pci() { Generate ctags(1) key. }

void		tsp_attach_hook __P((struct device *, struct device *,
		    struct pcibus_attach_args *));
int		tsp_bus_maxdevs __P((void *, int));
pcitag_t	tsp_make_tag __P((void *, int, int, int));
void		tsp_decompose_tag __P((void *, pcitag_t, int *, int *,
		    int *));
pcireg_t	tsp_conf_read __P((void *, pcitag_t, int));
void		tsp_conf_write __P((void *, pcitag_t, int, pcireg_t));

void
tsp_pci_init(pc, v)
	pci_chipset_tag_t pc;
	void *v;
{
	pc->pc_conf_v = v;
	pc->pc_attach_hook = tsp_attach_hook;
	pc->pc_bus_maxdevs = tsp_bus_maxdevs;
	pc->pc_make_tag = tsp_make_tag;
	pc->pc_decompose_tag = tsp_decompose_tag;
	pc->pc_conf_read = tsp_conf_read;
	pc->pc_conf_write = tsp_conf_write;
}

void
tsp_attach_hook(parent, self, pba)
	struct device *parent, *self;
	struct pcibus_attach_args *pba;
{
}

int
tsp_bus_maxdevs(cpv, busno)
	void *cpv;
	int busno;
{
	return 32;
}

pcitag_t
tsp_make_tag(cpv, b, d, f)
	void *cpv;
	int b, d, f;
{
	return b << 16 | d << 11 | f << 8;
}

void
tsp_decompose_tag(cpv, tag, bp, dp, fp)
	void *cpv;
	pcitag_t tag;
	int *bp, *dp, *fp;
{
	if (bp != NULL)
		*bp = (tag >> 16) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 11) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 8) & 0x7;
}
/*
 * Tsunami makes this a lot easier than it used to be, automatically
 * generating type 0 or type 1 cycles, and quietly returning -1 with
 * no errors on unanswered probes.
 */
pcireg_t
tsp_conf_read(cpv, tag, offset)
	void *cpv;
	pcitag_t tag;
	int offset;
{
	pcireg_t *datap, data;
	struct tsp_config *pcp = cpv;

	datap = S_PAGE(pcp->pc_iobase | P_PCI_CONFIG | tag | (offset & ~3));
	alpha_mb();
	data = *datap;
	alpha_mb();
	return data;
}

void
tsp_conf_write(cpv, tag, offset, data)
	void *cpv;
	pcitag_t tag;
	int offset;
	pcireg_t data;
{
	pcireg_t *datap;
	struct tsp_config *pcp = cpv;

	datap = S_PAGE(pcp->pc_iobase | P_PCI_CONFIG | tag | (offset & ~3));
	alpha_mb();
	*datap = data;
	alpha_mb();
}
