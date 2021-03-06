/*	$NetBSD: pciconf.c,v 1.15.10.1 2002/06/27 01:26:59 lukem Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Allen Briggs for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Derived in part from code from PMON/2000 (http://pmon.groupbsd.org/).
 */

/*
 * To do:
 *    - Perform all data structure allocation dynamically, don't have
 *	statically-sized arrays ("oops, you lose because you have too
 *	many slots filled!")
 *    - Do this in 2 passes, with an MD hook to control the behavior:
 *		(1) Configure the bus (possibly including expansion
 *		    ROMs.
 *		(2) Another pass to disable expansion ROMs if they're
 *		    mapped (since you're not supposed to leave them
 *		    mapped when you're not using them).
 *	This would facilitate MD code executing the expansion ROMs
 *	if necessary (possibly with an x86 emulator) to configure
 *	devices (e.g. VGA cards).
 *    - Deal with "anything can be hot-plugged" -- i.e., carry configuration
 *	information around & be able to reconfigure on the fly
 *    - Deal with segments (See IA64 System Abstraction Layer)
 *    - Deal with subtractive bridges (& non-spec positive/subtractive decode)
 *    - Deal with ISA/VGA/VGA palette snooping
 *    - Deal with device capabilities on bridges
 *    - Worry about changing a bridge to/from transparency
 * From thorpej (05/25/01)
 *    - Try to handle devices that are already configured (perhaps using that
 *      as a hint to where we put other devices)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pciconf.c,v 1.15.10.1 2002/06/27 01:26:59 lukem Exp $");

#include "opt_pci.h"

#include <sys/param.h>
#include <sys/extent.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pciconf.h>
#include <dev/pci/pcidevs.h>

int pci_conf_debug = 0;

#if !defined(MIN)
#define	MIN(a,b) (((a)<(b))?(a):(b))
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif

/* per-bus constants. */
#define MAX_CONF_DEV	32			/* Arbitrary */
#define MAX_CONF_MEM	(3 * MAX_CONF_DEV)	/* Avg. 3 per device -- Arb. */
#define MAX_CONF_IO	(3 * MAX_CONF_DEV)	/* Avg. 1 per device -- Arb. */

#define PCI_BUSNO_SPACING	(1 << 5)

struct _s_pciconf_bus_t;			/* Forward declaration */

typedef struct _s_pciconf_dev_t {
	int		ipin;
	int		iline;
	int		min_gnt;
	int		max_lat;
	int		enable;
	pcitag_t	tag;
	pci_chipset_tag_t	pc;
	struct _s_pciconf_bus_t	*ppb;		/* I am really a bridge */
} pciconf_dev_t;

typedef struct _s_pciconf_win_t {
	pciconf_dev_t	*dev;
	int		reg;			/* 0 for busses */
	int		align;
	int		prefetch;
	u_int64_t	size;
	u_int64_t	address;
} pciconf_win_t;

typedef struct _s_pciconf_bus_t {
	int		busno;
	int		next_busno;
	int		last_busno;
	int		busno_spacing;
	int		max_mingnt;
	int		min_maxlat;
	int		cacheline_size;
	int		prefetch;
	int		fast_b2b;
	int		freq_66;
	int		def_ltim;
	int		max_ltim;
	int		bandwidth_used;
	int		swiz;
	int		io_32bit;
	int		pmem_64bit;

	int		ndevs;
	pciconf_dev_t	device[MAX_CONF_DEV];

	/* These should be sorted in order of decreasing size */
	int		nmemwin;
	pciconf_win_t	pcimemwin[MAX_CONF_MEM];
	int		niowin;
	pciconf_win_t	pciiowin[MAX_CONF_IO];

	bus_size_t	io_total;
	bus_size_t	mem_total;
	bus_size_t	pmem_total;

	struct extent	*ioext;
	struct extent	*memext;
	struct extent	*pmemext;

	pci_chipset_tag_t	pc;
	struct _s_pciconf_bus_t *parent_bus;
} pciconf_bus_t;

static int	probe_bus(pciconf_bus_t *);
static void	alloc_busno(pciconf_bus_t *, pciconf_bus_t *);
static int	pci_do_device_query(pciconf_bus_t *, pcitag_t, int, int, int);
static int	setup_iowins(pciconf_bus_t *);
static int	setup_memwins(pciconf_bus_t *);
static int	configure_bridge(pciconf_dev_t *);
static int	configure_bus(pciconf_bus_t *);
static u_int64_t	pci_allocate_range(struct extent *, u_int64_t, int);
static pciconf_win_t	*get_io_desc(pciconf_bus_t *, bus_size_t);
static pciconf_win_t	*get_mem_desc(pciconf_bus_t *, bus_size_t);
static pciconf_bus_t	*query_bus(pciconf_bus_t *, pciconf_dev_t *, int);

static void	print_tag(pci_chipset_tag_t, pcitag_t);

static void
print_tag(pci_chipset_tag_t pc, pcitag_t tag)
{
	int	bus, dev, func;

	pci_decompose_tag(pc, tag, &bus, &dev, &func);
	printf("PCI: bus %d, device %d, function %d: ", bus, dev, func);
}

/************************************************************************/
/************************************************************************/
/***********************   Bus probing routines   ***********************/
/************************************************************************/
/************************************************************************/
static pciconf_win_t *
get_io_desc(pciconf_bus_t *pb, bus_size_t size)
{
	int	i, n;

	n = pb->niowin;
	for (i=n; i > 0 && size > pb->pciiowin[i-1].size; i--)
		pb->pciiowin[i] = pb->pciiowin[i-1]; /* struct copy */
	return &pb->pciiowin[i];
}

static pciconf_win_t *
get_mem_desc(pciconf_bus_t *pb, bus_size_t size)
{
	int	i, n;

	n = pb->nmemwin;
	for (i=n; i > 0 && size > pb->pcimemwin[i-1].size; i--)
		pb->pcimemwin[i] = pb->pcimemwin[i-1]; /* struct copy */
	return &pb->pcimemwin[i];
}

/*
 * Set up bus common stuff, then loop over devices & functions.
 * If we find something, call pci_do_device_query()).
 */
static int
probe_bus(pciconf_bus_t *pb)
{
	int device, maxdevs;
#ifdef __PCI_BUS_DEVORDER
	char devs[32];
	int  i;
#endif

	maxdevs = pci_bus_maxdevs(pb->pc, pb->busno);
	pb->ndevs = 0;
	pb->niowin = 0;
	pb->nmemwin = 0;
	pb->freq_66 = 1;
	pb->fast_b2b = 1;
	pb->prefetch = 1;
	pb->max_mingnt = 0;	/* we are looking for the maximum */
	pb->min_maxlat = 0x100;	/* we are looking for the minimum */
	pb->bandwidth_used = 0;

#ifdef __PCI_BUS_DEVORDER
	pci_bus_devorder(pb->pc, pb->busno, devs);
	for (i=0; (device=devs[i]) < 32 && device >= 0; i++) {
#else
	for (device=0; device < maxdevs; device++) {
#endif
		pcitag_t tag;
		pcireg_t id, bhlcr;
		int function, nfunction;
		int confmode;

		tag = pci_make_tag(pb->pc, pb->busno, device, 0);
		if (pci_conf_debug) {
			print_tag(pb->pc, tag);
		}
		id = pci_conf_read(pb->pc, tag, PCI_ID_REG);

		if (pci_conf_debug) {
			printf("id=%x: Vendor=%x, Product=%x\n",
			    id, PCI_VENDOR(id),PCI_PRODUCT(id));
		}
		/* Invalid vendor ID value? */
		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
			continue;

		bhlcr = pci_conf_read(pb->pc, tag, PCI_BHLC_REG);
		nfunction = PCI_HDRTYPE_MULTIFN(bhlcr) ? 8 : 1;
		for (function = 0 ; function < nfunction ; function++) {
			tag = pci_make_tag(pb->pc, pb->busno, device, function);
			id = pci_conf_read(pb->pc, tag, PCI_ID_REG);
			if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
				continue;
			if (pb->ndevs+1 < MAX_CONF_DEV) {
				if (pci_conf_debug) {
					print_tag(pb->pc, tag);
					printf("Found dev 0x%04x 0x%04x -- "
					    "really probing.\n",
					PCI_VENDOR(id), PCI_PRODUCT(id));
				}
#ifdef __HAVE_PCI_CONF_HOOK
				confmode = pci_conf_hook(pb->pc, pb->busno,
				    device, function, id);
				if (confmode == 0)
					continue;
#else
				/*
				 * Don't enable expansion ROMS -- some cards
				 * share address decoders between the EXPROM
				 * and PCI memory space, and enabling the ROM
				 * when not needed will cause all sorts of
				 * lossage.
				 */
				confmode = PCI_CONF_ALL & ~PCI_CONF_MAP_ROM;
#endif
				if (pci_do_device_query(pb, tag, device,
				    function, confmode))
					return -1;
				pb->ndevs++;
			}
		}
	}
	return 0;
}

static void
alloc_busno(pciconf_bus_t *parent, pciconf_bus_t *pb)
{
	pb->busno = parent->next_busno;
	if (parent->next_busno + parent->busno_spacing > parent->last_busno)
		panic("Too many PCI busses on bus %d", parent->busno);
	parent->next_busno = parent->next_busno + parent->busno_spacing;
	pb->next_busno = pb->busno+1;
	pb->busno_spacing = parent->busno_spacing >> 1;
	if (!pb->busno_spacing)
		panic("PCI busses nested too deep.");
	pb->last_busno = parent->next_busno - 1;
}

static pciconf_bus_t *
query_bus(pciconf_bus_t *parent, pciconf_dev_t *pd, int dev)
{
	pciconf_bus_t	*pb;
	pcireg_t	busreg, io, pmem;
	pciconf_win_t	*pi, *pm;

	pb = malloc (sizeof (pciconf_bus_t), M_DEVBUF, M_NOWAIT);
	if (!pb)
		panic("Unable to allocate memory for PCI configuration.");

	pb->cacheline_size = parent->cacheline_size;
	pb->parent_bus = parent;
	alloc_busno(parent, pb);
	if (pci_conf_debug)
		printf("PCI bus bridge covers busses %d-%d\n",
			pb->busno, pb->last_busno);

	busreg  =  parent->busno << PCI_BRIDGE_BUS_PRIMARY_SHIFT;
	busreg |=      pb->busno << PCI_BRIDGE_BUS_SECONDARY_SHIFT;
	busreg |= pb->last_busno << PCI_BRIDGE_BUS_SUBORDINATE_SHIFT;
	pci_conf_write(parent->pc, pd->tag, PCI_BRIDGE_BUS_REG, busreg);

	pb->swiz = parent->swiz + dev;

	pb->ioext = NULL;
	pb->memext = NULL;
	pb->pmemext = NULL;
	pb->pc = parent->pc;
	pb->io_total = pb->mem_total = pb->pmem_total = 0;

	pb->io_32bit = 0;
	if (parent->io_32bit) {
		io = pci_conf_read(parent->pc, pd->tag, PCI_BRIDGE_STATIO_REG);
		if (PCI_BRIDGE_IO_32BITS(io)) {
			pb->io_32bit = 1;
		}
	}

	pb->pmem_64bit = 0;
	if (parent->pmem_64bit) {
		pmem = pci_conf_read(parent->pc, pd->tag,
		    PCI_BRIDGE_PREFETCHMEM_REG);
		if (PCI_BRIDGE_PREFETCHMEM_64BITS(pmem)) {
			pb->pmem_64bit = 1;
		}
	}

	if (probe_bus(pb)) {
		printf("Failed to probe bus %d\n", pb->busno);
		goto err;
	}

	if (pb->io_total > 0) {
		if (parent->niowin >= MAX_CONF_IO) {
			printf("pciconf: too many I/O windows\n");
			goto err;
		}
		pb->io_total |= 0xfff;	/* Round up */
		pi = get_io_desc(parent, pb->io_total);
		pi->dev = pd;
		pi->reg = 0;
		pi->size = pb->io_total;
		pi->align = 0x1000;	/* 4K alignment */
		pi->prefetch = 0;
		parent->niowin++;
		parent->io_total += pb->io_total;
	}

	if (pb->mem_total > 0) {
		if (parent->nmemwin >= MAX_CONF_MEM) {
			printf("pciconf: too many MEM windows\n");
			goto err;
		}
		pb->mem_total |= 0xfffff;	/* Round up */
		pm = get_mem_desc(parent, pb->mem_total);
		pm->dev = pd;
		pm->reg = 0;
		pm->size = pb->mem_total;
		pm->align = 0x100000;	/* 1M alignment */
		pm->prefetch = 0;
		parent->nmemwin++;
		parent->mem_total += pb->mem_total;
	}

	if (pb->pmem_total > 0) {
		if (parent->nmemwin >= MAX_CONF_MEM) {
			printf("pciconf: too many MEM windows\n");
			goto err;
		}
		pb->pmem_total |= 0xfffff;	/* Round up */
		pm = get_mem_desc(parent, pb->pmem_total);
		pm->dev = pd;
		pm->reg = 0;
		pm->size = pb->pmem_total;
		pm->align = 0x100000;		/* 1M alignment */
		pm->prefetch = 1;
		parent->nmemwin++;
		parent->pmem_total += pb->pmem_total;
	}

	return pb;
err:
	free(pb, M_DEVBUF);
	return NULL;
}

static int
pci_do_device_query(pciconf_bus_t *pb, pcitag_t tag, int dev, int func, int mode)
{
	pciconf_dev_t	*pd;
	pciconf_win_t	*pi, *pm;
	pcireg_t	class, cmd, icr, bar, mask, bar64, mask64;
	u_int64_t	size;
	int		br, width;

	pd = &pb->device[pb->ndevs];
	pd->pc = pb->pc;
	pd->tag = tag;
	pd->ppb = NULL;
	pd->enable = mode;

	class = pci_conf_read(pb->pc, tag, PCI_CLASS_REG);

	cmd = pci_conf_read(pb->pc, tag, PCI_COMMAND_STATUS_REG);

	if (PCI_CLASS(class) != PCI_CLASS_BRIDGE) {
		cmd &= ~(PCI_COMMAND_MASTER_ENABLE |
		    PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE);
		pci_conf_write(pb->pc, tag, PCI_COMMAND_STATUS_REG, cmd);
	} else if (pci_conf_debug) {
		print_tag(pb->pc, tag);
		printf("device is a bridge; not clearing enables\n");
	}

	if ((cmd & PCI_STATUS_BACKTOBACK_SUPPORT) == 0)
		pb->fast_b2b = 0;

	if ((cmd & PCI_STATUS_66MHZ_SUPPORT) == 0)
		pb->freq_66 = 0;

	if (   (PCI_CLASS(class) == PCI_CLASS_BRIDGE)
	    && (PCI_SUBCLASS(class) == PCI_SUBCLASS_BRIDGE_PCI)) {
		pd->ppb = query_bus(pb, pd, dev);
		if (pd->ppb == NULL)
			return -1;
		return 0;
	}

	icr = pci_conf_read(pb->pc, tag, PCI_INTERRUPT_REG);
	pd->ipin = PCI_INTERRUPT_PIN(icr);
	pd->iline = PCI_INTERRUPT_LINE(icr);
	pd->min_gnt = PCI_MIN_GNT(icr);
	pd->max_lat = PCI_MAX_LAT(icr);
	if (pd->iline || pd->ipin) {
		pci_conf_interrupt(pb->pc, pb->busno, dev, pd->ipin, pb->swiz,
		    &pd->iline);
		icr &= ~(PCI_INTERRUPT_LINE_MASK << PCI_INTERRUPT_LINE_SHIFT);
		icr |= (pd->iline << PCI_INTERRUPT_LINE_SHIFT);
		pci_conf_write(pb->pc, tag, PCI_INTERRUPT_REG, icr);
	}

	if (pd->min_gnt != 0 || pd->max_lat != 0) {
		if (pd->min_gnt != 0 && pd->min_gnt > pb->max_mingnt)
			pb->max_mingnt = pd->min_gnt;

		if (pd->max_lat != 0 && pd->max_lat < pb->min_maxlat)
			pb->min_maxlat = pd->max_lat;

		pb->bandwidth_used += pd->min_gnt * 4000000 /
				(pd->min_gnt + pd->max_lat);
	}

	width = 4;
	for (br = PCI_MAPREG_START; br < PCI_MAPREG_END; br += width) {
#if 0
/* XXX Should only ignore if IDE not in legacy mode? */
		if (PCI_CLASS(class) == PCI_CLASS_MASS_STORAGE &&
		    PCI_SUBCLASS(class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
			break;
		}
#endif
		bar = pci_conf_read(pb->pc, tag, br);
		pci_conf_write(pb->pc, tag, br, 0xffffffff);
		mask = pci_conf_read(pb->pc, tag, br);
		pci_conf_write(pb->pc, tag, br, bar);
		width = 4;

		if (   (mode & PCI_CONF_MAP_IO)
		    && (PCI_MAPREG_TYPE(mask) == PCI_MAPREG_TYPE_IO)) {
			/*
			 * Upper 16 bits must be one.  Devices may hardwire
			 * them to zero, though, per PCI 2.2, 6.2.5.1, p 203.
			 */
			mask |= 0xffff0000;

			size = PCI_MAPREG_IO_SIZE(mask);
			if (size == 0) {
				if (pci_conf_debug) {
					print_tag(pb->pc, tag);
					printf("I/O BAR 0x%x is void\n", br);
				}
				continue;
			}

			if (pb->niowin >= MAX_CONF_IO) {
				printf("pciconf: too many I/O windows\n");
				return -1;
			}

			pi = get_io_desc(pb, size);
			pi->dev = pd;
			pi->reg = br;
			pi->size = (u_int64_t) size;
			pi->align = 4;
			pi->prefetch = 0;
			if (pci_conf_debug) {
				print_tag(pb->pc, tag);
				printf("Register 0x%x, I/O size %llu\n",
				    br, pi->size);
			}
			pb->niowin++;
			pb->io_total += size;
		} else if ((mode & PCI_CONF_MAP_MEM)
			   && (PCI_MAPREG_TYPE(mask) == PCI_MAPREG_TYPE_MEM)) {
			switch (PCI_MAPREG_MEM_TYPE(mask)) {
			case PCI_MAPREG_MEM_TYPE_32BIT:
			case PCI_MAPREG_MEM_TYPE_32BIT_1M:
				size = (u_int64_t) PCI_MAPREG_MEM_SIZE(mask);
				break;
			case PCI_MAPREG_MEM_TYPE_64BIT:
				bar64 = pci_conf_read(pb->pc, tag, br + 4);
				pci_conf_write(pb->pc, tag, br + 4, 0xffffffff);
				mask64 = pci_conf_read(pb->pc, tag, br + 4);
				pci_conf_write(pb->pc, tag, br + 4, bar64);
				size = (u_int64_t) PCI_MAPREG_MEM64_SIZE(
				      (((u_int64_t) mask64) << 32) | mask);
				width = 8;
				break;
			default:
				print_tag(pb->pc, tag);
				printf("reserved mapping type 0x%x\n",
					PCI_MAPREG_MEM_TYPE(mask));
				continue;
			}

			if (size == 0) {
				if (pci_conf_debug) {
					print_tag(pb->pc, tag);
					printf("MEM%d BAR 0x%x is void\n",
					    PCI_MAPREG_MEM_TYPE(mask) ==
						PCI_MAPREG_MEM_TYPE_64BIT ?
						64 : 32, br);
				}
				continue;
			} else {
				if (pci_conf_debug) {
					print_tag(pb->pc, tag);
					printf("MEM%d BAR 0x%x has size %lx\n",
					    PCI_MAPREG_MEM_TYPE(mask) ==
						PCI_MAPREG_MEM_TYPE_64BIT ?
						64 : 32, br, (unsigned long)size);
				}
			}

			if (pb->nmemwin >= MAX_CONF_MEM) {
				printf("pciconf: too many memory windows\n");
				return -1;
			}

			pm = get_mem_desc(pb, size);
			pm->dev = pd;
			pm->reg = br;
			pm->size = size;
			pm->align = 4;
			pm->prefetch = PCI_MAPREG_MEM_PREFETCHABLE(mask);
			if (pci_conf_debug) {
				print_tag(pb->pc, tag);
				printf("Register 0x%x, memory size %llu\n",
				    br, pm->size);
			}
			pb->nmemwin++;
			if (pm->prefetch) {
				pb->pmem_total += size;
			} else {
				pb->mem_total += size;
			}
		}
	}

	if (mode & PCI_CONF_MAP_ROM) {
		bar = pci_conf_read(pb->pc, tag, PCI_MAPREG_ROM);
		pci_conf_write(pb->pc, tag, PCI_MAPREG_ROM, 0xfffffffe);
		mask = pci_conf_read(pb->pc, tag, PCI_MAPREG_ROM);
		pci_conf_write(pb->pc, tag, PCI_MAPREG_ROM, bar);

		if (mask != 0 && mask != 0xffffffff) {
			if (pb->nmemwin >= MAX_CONF_MEM) {
				printf("pciconf: too many memory windows\n");
				return -1;
			}
			size = (u_int64_t) PCI_MAPREG_MEM_SIZE(mask);

			pm = get_mem_desc(pb, size);
			pm->dev = pd;
			pm->reg = PCI_MAPREG_ROM;
			pm->size = size;
			pm->align = 4;
			pm->prefetch = 1;
			if (pci_conf_debug) {
				print_tag(pb->pc, tag);
				printf("Expansion ROM memory size %llu\n", pm->size);
			}
			pb->nmemwin++;
			pb->pmem_total += size;
		}
	} else {
		/* Ensure ROM is disabled */
		bar = pci_conf_read(pb->pc, tag, PCI_MAPREG_ROM);
		pci_conf_write(pb->pc, tag, PCI_MAPREG_ROM, 0xfffffffe);
		mask = pci_conf_read(pb->pc, tag, PCI_MAPREG_ROM);
		pci_conf_write(pb->pc, tag, PCI_MAPREG_ROM,
		    bar & ~PCI_MAPREG_ROM_ENABLE);
	}

	return 0;
}

/************************************************************************/
/************************************************************************/
/********************   Bus configuration routines   ********************/
/************************************************************************/
/************************************************************************/
static u_int64_t
pci_allocate_range(struct extent *ex, u_int64_t amt, int align)
{
	int	r;
	u_long	addr;

	r = extent_alloc(ex, amt, align, 0, EX_NOWAIT, &addr);
	if (r) {
		addr = (u_long) -1;
		printf("extent_alloc(%p, %llu, %d) returned %d\n",
		    ex, amt, align, r);
		extent_print(ex);
	}
	return (pcireg_t) addr;
}

static int
setup_iowins(pciconf_bus_t *pb)
{
	pciconf_win_t	*pi;
	pciconf_dev_t	*pd;

	for (pi=pb->pciiowin; pi < &pb->pciiowin[pb->niowin] ; pi++) {
		if (pi->size == 0)
			continue;

		pd = pi->dev;
		pi->address = pci_allocate_range(pb->ioext, pi->size,
		    pi->align);
		if (pi->address == -1) {
			print_tag(pd->pc, pd->tag);
			printf("Failed to allocate PCI I/O space (%llu req)\n",
			   pi->size);
			return -1;
		}
		if (!pb->io_32bit && pi->address > 0xFFFF) {
			pi->address = 0;
			pd->enable = 0;
		}
		if (pd->ppb && pi->reg == 0) {
			pd->ppb->ioext = extent_create("pciconf", pi->address,
			    pi->address + pi->size, M_DEVBUF, NULL, 0,
			    EX_NOWAIT);
			if (pd->ppb->ioext == NULL) {
				print_tag(pd->pc, pd->tag);
				printf("Failed to alloc I/O ext. for bus %d\n",
				    pd->ppb->busno);
				return -1;
			}
			continue;
		}
		pd->enable |= PCI_CONF_ENABLE_IO;
		if (pci_conf_debug) {
			print_tag(pd->pc, pd->tag);
			printf("Putting %llu I/O bytes @ %#llx (reg %x)\n",
			    pi->size, pi->address, pi->reg);
		}
		pci_conf_write(pd->pc, pd->tag, pi->reg,
		    PCI_MAPREG_IO_ADDR(pi->address) | PCI_MAPREG_TYPE_IO);
	}
	return 0;
}

static int
setup_memwins(pciconf_bus_t *pb)
{
	pciconf_win_t	*pm;
	pciconf_dev_t	*pd;
	pcireg_t	base;
	struct extent	*ex;

	for (pm=pb->pcimemwin; pm < &pb->pcimemwin[pb->nmemwin] ; pm++) {
		if (pm->size == 0)
			continue;

		pd = pm->dev;
		ex = (pm->prefetch) ? pb->pmemext : pb->memext;
		pm->address = pci_allocate_range(ex, pm->size, pm->align);
		if (pm->address == -1) {
			print_tag(pd->pc, pd->tag);
			printf(
			   "Failed to allocate PCI memory space (%llu req)\n",
			   pm->size);
			return -1;
		}
		if (pd->ppb && pm->reg == 0) {
			ex = extent_create("pciconf", pm->address,
			    pm->address + pm->size, M_DEVBUF, NULL, 0,
			    EX_NOWAIT);
			if (ex == NULL) {
				print_tag(pd->pc, pd->tag);
				printf("Failed to alloc MEM ext. for bus %d\n",
				    pd->ppb->busno);
				return -1;
			}
			if (pm->prefetch) {
				pd->ppb->pmemext = ex;
			} else {
				pd->ppb->memext = ex;
			}
			continue;
		}
		if (pm->prefetch && !pb->pmem_64bit &&
		    pm->address > 0xFFFFFFFFULL) {
			pm->address = 0;
			pd->enable = 0;
		} else {
			pd->enable |= PCI_CONF_ENABLE_MEM;
		}
		if (pm->reg != PCI_MAPREG_ROM) {
			if (pci_conf_debug) {
				print_tag(pd->pc, pd->tag);
				printf(
				    "Putting %llu MEM bytes @ %#llx (reg %x)\n",
				     pm->size, pm->address, pm->reg);
			}
			base = pci_conf_read(pd->pc, pd->tag, pm->reg);
			base = PCI_MAPREG_MEM_ADDR(pm->address) |
			    PCI_MAPREG_MEM_TYPE(base);
			pci_conf_write(pd->pc, pd->tag, pm->reg, base);
			if (PCI_MAPREG_MEM_TYPE(base) ==
			    PCI_MAPREG_MEM_TYPE_64BIT) {
				base = (pcireg_t)
				    (PCI_MAPREG_MEM64_ADDR(pm->address) >> 32);
				pci_conf_write(pd->pc, pd->tag, pm->reg + 4,
				    base);
			}
		}
	}
	for (pm=pb->pcimemwin; pm < &pb->pcimemwin[pb->nmemwin] ; pm++) {
		if (pm->reg == PCI_MAPREG_ROM && pm->address != -1) {
			pd = pm->dev;
			if (pci_conf_debug) {
				print_tag(pd->pc, pd->tag);
				printf(
				    "Putting %llu ROM bytes @ %#llx (reg %x)\n",
				    pm->size, pm->address, pm->reg);
			}
			base = (pcireg_t) (pm->address | PCI_MAPREG_ROM_ENABLE);
			pci_conf_write(pd->pc, pd->tag, pm->reg, base);
		}
	}
	return 0;
}

/*
 * Configure I/O, memory, and prefetcable memory spaces, then make
 * a call to configure_bus().
 */
static int
configure_bridge(pciconf_dev_t *pd)
{
	unsigned long	io_base, io_limit, mem_base, mem_limit;
	pciconf_bus_t	*pb;
	pcireg_t	io, iohigh, mem, cmd;
	int		rv;

	pb = pd->ppb;
	/* Configure I/O base & limit*/
	if (pb->ioext) {
		io_base = pb->ioext->ex_start;
		io_limit = pb->ioext->ex_end;
	} else {
		io_base  = 0x1000;	/* 4K */
		io_limit = 0x0000;
	}
	if (pb->io_32bit) {
		iohigh =
		    ((io_base >> 16) << PCI_BRIDGE_IOHIGH_BASE_SHIFT) |
		    ((io_limit >> 16) << PCI_BRIDGE_IOHIGH_LIMIT_SHIFT);
	} else {
		if (io_limit > 0xFFFF) {
			printf("Bus %d bridge does not support 32-bit I/O.  ",
			    pb->busno);
			printf("Disabling I/O accesses\n");
			io_base  = 0x1000;	/* 4K */
			io_limit = 0x0000;
		}
		iohigh = 0;
	}
	io = pci_conf_read(pb->pc, pd->tag, PCI_BRIDGE_STATIO_REG) &
	    (PCI_BRIDGE_STATIO_STATUS_MASK << PCI_BRIDGE_STATIO_STATUS_SHIFT);
	io |= (((io_base >> 8) & PCI_BRIDGE_STATIO_IOBASE_MASK)
	    << PCI_BRIDGE_STATIO_IOBASE_SHIFT);
	io |= (((io_limit >> 8) & PCI_BRIDGE_STATIO_IOLIMIT_MASK)
	    << PCI_BRIDGE_STATIO_IOLIMIT_SHIFT);
	pci_conf_write(pb->pc, pd->tag, PCI_BRIDGE_STATIO_REG, io);
	pci_conf_write(pb->pc, pd->tag, PCI_BRIDGE_IOHIGH_REG, iohigh);

	/* Configure mem base & limit */
	if (pb->memext) {
		mem_base = pb->memext->ex_start;
		mem_limit = pb->memext->ex_end;
	} else {
		mem_base  = 0x100000;	/* 1M */
		mem_limit = 0x000000;
	}
	if (mem_limit > 0xFFFFFFFFULL) {
		printf("Bus %d bridge MEM range out of range.  ", pb->busno);
		printf("Disabling MEM accesses\n");
		mem_base  = 0x100000;	/* 1M */
		mem_limit = 0x000000;
	}
	mem = (((mem_base >> 20) & PCI_BRIDGE_MEMORY_BASE_MASK)
	    << PCI_BRIDGE_MEMORY_BASE_SHIFT);
	mem |= (((mem_limit >> 20) & PCI_BRIDGE_MEMORY_LIMIT_MASK)
	    << PCI_BRIDGE_MEMORY_LIMIT_SHIFT);
	pci_conf_write(pb->pc, pd->tag, PCI_BRIDGE_MEMORY_REG, mem);

	/* Configure prefetchable mem base & limit */
	if (pb->pmemext) {
		mem_base = pb->pmemext->ex_start;
		mem_limit = pb->pmemext->ex_end;
	} else {
		mem_base  = 0x100000;	/* 1M */
		mem_limit = 0x000000;
	}
	mem = pci_conf_read(pb->pc, pd->tag, PCI_BRIDGE_PREFETCHMEM_REG);
	if (!PCI_BRIDGE_PREFETCHMEM_64BITS(mem) && mem_limit > 0xFFFFFFFFULL) {
		printf("Bus %d bridge does not support 64-bit PMEM.  ",
		    pb->busno);
		printf("Disabling prefetchable-MEM accesses\n");
		mem_base  = 0x100000;	/* 1M */
		mem_limit = 0x000000;
	}
	mem = (((mem_base >> 20) & PCI_BRIDGE_PREFETCHMEM_BASE_MASK)
	    << PCI_BRIDGE_PREFETCHMEM_BASE_SHIFT);
	mem |= (((mem_limit >> 20) & PCI_BRIDGE_PREFETCHMEM_LIMIT_MASK)
	    << PCI_BRIDGE_PREFETCHMEM_LIMIT_SHIFT);
	pci_conf_write(pb->pc, pd->tag, PCI_BRIDGE_PREFETCHMEM_REG, mem);
	/*
	 * XXX -- 64-bit systems need a lot more than just this...
	 */
	if (sizeof(u_long) > 4) {
		mem_base  = (int64_t) mem_base  >> 32;
		mem_limit = (int64_t) mem_limit >> 32;
	}
	pci_conf_write(pb->pc, pd->tag, PCI_BRIDGE_PREFETCHBASE32_REG,
	    mem_base & 0xffffffff);
	pci_conf_write(pb->pc, pd->tag, PCI_BRIDGE_PREFETCHLIMIT32_REG,
	    mem_limit & 0xffffffff);

	rv = configure_bus(pb);

	if (pb->ioext)
		extent_destroy(pb->ioext);
	if (pb->memext)
		extent_destroy(pb->memext);
	if (pb->pmemext)
		extent_destroy(pb->pmemext);
	if (rv == 0) {
		cmd = pci_conf_read(pd->pc, pd->tag, PCI_BRIDGE_CONTROL_REG);
		cmd &= PCI_BRIDGE_CONTROL_MASK;
		cmd |= (PCI_BRIDGE_CONTROL_PERE | PCI_BRIDGE_CONTROL_SERR)
		    << PCI_BRIDGE_CONTROL_SHIFT;
		if (pb->fast_b2b) {
			cmd |= PCI_BRIDGE_CONTROL_SECFASTB2B
			    << PCI_BRIDGE_CONTROL_SHIFT;
		}
		pci_conf_write(pd->pc, pd->tag, PCI_BRIDGE_CONTROL_REG, cmd);
		cmd = pci_conf_read(pd->pc, pd->tag, PCI_COMMAND_STATUS_REG);
		cmd |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE;
		pci_conf_write(pd->pc, pd->tag, PCI_COMMAND_STATUS_REG, cmd);
	}

	return rv;
}

/*
 * Calculate latency values, allocate I/O and MEM segments, then set them
 * up.  If a PCI-PCI bridge is found, configure the bridge separately,
 * which will cause a recursive call back here.
 */
static int
configure_bus(pciconf_bus_t *pb)
{
	pciconf_dev_t	*pd;
	int		def_ltim, max_ltim, band, bus_mhz;

	bus_mhz = pb->freq_66 ? 66 : 33;
	max_ltim = pb->max_mingnt * bus_mhz / 4;	/* cvt to cycle count */
	band = 40000000;			/* 0.25us cycles/sec */
	if (band < pb->bandwidth_used) {
		printf("PCI bus %d: Warning: Total bandwidth exceeded!?\n",
		    pb->busno);
		def_ltim = -1;
	} else {
		def_ltim = (band - pb->bandwidth_used) / pb->ndevs;
		if (def_ltim > pb->min_maxlat)
			def_ltim = pb->min_maxlat;
		def_ltim = def_ltim * bus_mhz / 4;
	}
	def_ltim = (def_ltim + 7) & ~7;
	max_ltim = (max_ltim + 7) & ~7;

	pb->def_ltim = MIN( def_ltim, 255 );
	pb->max_ltim = MIN( MAX(max_ltim, def_ltim ), 255 );

	/*
	 * Now we have what we need to initialize the devices.
	 * It would probably be better if we could allocate all of these
	 * for all busses at once, but "not right now".  First, get a list
	 * of free memory ranges from the m.d. system.
	 */
	if (setup_iowins(pb) || setup_memwins(pb)) {
		printf("PCI bus configuration failed: ");
		printf("unable to assign all I/O and memory ranges.");
		return -1;
	}

	/*
	 * Configure the latency for the devices, and enable them.
	 */
	for (pd=pb->device ; pd < &pb->device[pb->ndevs] ; pd++) {
		pcireg_t cmd, class, misc;
		int	ltim;

		if (pci_conf_debug) {
			print_tag(pd->pc, pd->tag);
			printf("Configuring device.\n");
		}
		class = pci_conf_read(pd->pc, pd->tag, PCI_CLASS_REG);
		misc = pci_conf_read(pd->pc, pd->tag, PCI_BHLC_REG);
		cmd = pci_conf_read(pd->pc, pd->tag, PCI_COMMAND_STATUS_REG);
		cmd |= PCI_COMMAND_SERR_ENABLE | PCI_COMMAND_PARITY_ENABLE;
		if (pb->fast_b2b)
			cmd |= PCI_COMMAND_BACKTOBACK_ENABLE;
		if (PCI_CLASS(class) != PCI_CLASS_BRIDGE ||
		    PCI_SUBCLASS(class) != PCI_SUBCLASS_BRIDGE_PCI) {
			if (pd->enable & PCI_CONF_ENABLE_IO)
				cmd |= PCI_COMMAND_IO_ENABLE;
			if (pd->enable & PCI_CONF_ENABLE_MEM)
				cmd |= PCI_COMMAND_MEM_ENABLE;
			if (pd->enable & PCI_CONF_ENABLE_BM)
				cmd |= PCI_COMMAND_MASTER_ENABLE;
			ltim = pd->min_gnt * bus_mhz / 4;
			ltim = MIN (MAX (pb->def_ltim, ltim), pb->max_ltim);
		} else {
			cmd |= PCI_COMMAND_MASTER_ENABLE;
			ltim = MIN (pb->def_ltim, pb->max_ltim);
		}
		if (!(pd->enable)) {
			print_tag(pd->pc, pd->tag);
			printf("Disabled due to lack of resources.\n");
			cmd &= ~(PCI_COMMAND_MASTER_ENABLE |
			    PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE);
		}
		pci_conf_write(pd->pc, pd->tag, PCI_COMMAND_STATUS_REG, cmd);

		misc &= ~((PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT) |
		    (PCI_CACHELINE_MASK << PCI_CACHELINE_SHIFT));
		misc |= (ltim & PCI_LATTIMER_MASK) << PCI_LATTIMER_SHIFT;
		misc |= ((pb->cacheline_size >> 2) & PCI_CACHELINE_MASK) <<
		    PCI_CACHELINE_SHIFT;
		pci_conf_write(pd->pc, pd->tag, PCI_BHLC_REG, misc);

		if (pd->ppb) {
			if (configure_bridge(pd) < 0)
				return -1;
			continue;
		}
	}

	if (pci_conf_debug) {
		printf("PCI bus %d configured\n", pb->busno);
	}

	return 0;
}

/*
 * Let's configure the PCI bus.
 * This consists of basically scanning for all existing devices,
 * identifying their needs, and then making another pass over them
 * to set:
 *	1. I/O addresses
 *	2. Memory addresses (Prefetchable and not)
 *	3. PCI command register
 *	4. The latency part of the PCI BHLC (BIST (Built-In Self Test),
 *	    Header type, Latency timer, Cache line size) register
 *
 * The command register is set to enable fast back-to-back transactions
 * if the host bridge says it can handle it.  We also configure 
 * Master Enable, SERR enable, parity enable, and (if this is not a
 * PCI-PCI bridge) the I/O and Memory spaces.  Apparently some devices
 * will not report some I/O space.
 *
 * The latency is computed to be a "fair share" of the bus bandwidth.
 * The bus bandwidth variable is initialized to the number of PCI cycles
 * in one second.  The number of cycles taken for one transaction by each
 * device (MAX_LAT + MIN_GNT) is then subtracted from the bandwidth.
 * Care is taken to ensure that the latency timer won't be set such that
 * it would exceed the critical time for any device.
 *
 * This is complicated somewhat due to the presence of bridges.  PCI-PCI
 * bridges are probed and configured recursively.
 */
int
pci_configure_bus(pci_chipset_tag_t pc, struct extent *ioext,
    struct extent *memext, struct extent *pmemext, int firstbus,
    int cacheline_size)
{
	pciconf_bus_t	*pb;
	int		rv;

	pb = malloc (sizeof (pciconf_bus_t), M_DEVBUF, M_NOWAIT);
	pb->busno = firstbus;
	pb->busno_spacing = PCI_BUSNO_SPACING;
	pb->next_busno = pb->busno + 1;
	pb->last_busno = 255;
	pb->cacheline_size = cacheline_size;
	pb->parent_bus = NULL;
	pb->swiz = 0;
	pb->io_32bit = 1;
	pb->pmem_64bit = 0;
	pb->ioext = ioext;
	pb->memext = memext;
	if (pmemext == NULL) {
		pb->pmemext = memext;
	} else {
		pb->pmemext = pmemext;
	}
	pb->pc = pc;
	pb->io_total = pb->mem_total = pb->pmem_total = 0;

	rv = probe_bus(pb);
	if (rv == 0) {
		rv = configure_bus(pb);
	}

	/*
	 * All done!
	 */
	free(pb, M_DEVBUF);
	return rv;
}
