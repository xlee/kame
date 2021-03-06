/* $OpenBSD: pci_bwx_bus_io_chipdep.c,v 1.2 2000/11/08 20:59:25 ericj Exp $ */
/* $NetBSD: pcs_bus_io_common.c,v 1.14 1996/12/02 22:19:35 cgd Exp $ */

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Common PCI Chipset "bus I/O" functions, for chipsets which have to
 * deal with only a single PCI interface chip in a machine.
 *
 * uses:
 *	CHIP		name of the 'chip' it's being compiled for.
 *	CHIP_IO_BASE	Sparse I/O space base to use.
 */

#include <sys/extent.h>

#include <machine/bwx.h>

#define	__C(A,B)	__CONCAT(A,B)
#define	__S(S)		__STRING(S)

/* mapping/unmapping */
int		__C(CHIP,_io_map) __P((void *, bus_addr_t, bus_size_t, int,
		    bus_space_handle_t *));
void		__C(CHIP,_io_unmap) __P((void *, bus_space_handle_t,
		    bus_size_t));
int		__C(CHIP,_io_subregion) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_size_t, bus_space_handle_t *));

/* allocation/deallocation */
int		__C(CHIP,_io_alloc) __P((void *, bus_addr_t, bus_addr_t,
		    bus_size_t, bus_size_t, bus_addr_t, int, bus_addr_t *,
                    bus_space_handle_t *));
void		__C(CHIP,_io_free) __P((void *, bus_space_handle_t,
		    bus_size_t));

/* barrier */
inline void	__C(CHIP,_io_barrier) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_size_t, int));

/* read (single) */
inline u_int8_t	__C(CHIP,_io_read_1) __P((void *, bus_space_handle_t,
		    bus_size_t));
inline u_int16_t __C(CHIP,_io_read_2) __P((void *, bus_space_handle_t,
		    bus_size_t));
inline u_int32_t __C(CHIP,_io_read_4) __P((void *, bus_space_handle_t,
		    bus_size_t));
inline u_int64_t __C(CHIP,_io_read_8) __P((void *, bus_space_handle_t,
		    bus_size_t));

/* read multiple */
void		__C(CHIP,_io_read_multi_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t *, bus_size_t));
void		__C(CHIP,_io_read_multi_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t *, bus_size_t));
void		__C(CHIP,_io_read_multi_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t *, bus_size_t));
void		__C(CHIP,_io_read_multi_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t *, bus_size_t));

/* read region */
void		__C(CHIP,_io_read_region_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t *, bus_size_t));
void		__C(CHIP,_io_read_region_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t *, bus_size_t));
void		__C(CHIP,_io_read_region_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t *, bus_size_t));
void		__C(CHIP,_io_read_region_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t *, bus_size_t));

/* write (single) */
inline void	__C(CHIP,_io_write_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t));
inline void	__C(CHIP,_io_write_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t));
inline void	__C(CHIP,_io_write_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t));
inline void	__C(CHIP,_io_write_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t));

/* write multiple */
void		__C(CHIP,_io_write_multi_1) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int8_t *, bus_size_t));
void		__C(CHIP,_io_write_multi_2) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int16_t *, bus_size_t));
void		__C(CHIP,_io_write_multi_4) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int32_t *, bus_size_t));
void		__C(CHIP,_io_write_multi_8) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int64_t *, bus_size_t));

/* write region */
void		__C(CHIP,_io_write_region_1) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int8_t *, bus_size_t));
void		__C(CHIP,_io_write_region_2) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int16_t *, bus_size_t));
void		__C(CHIP,_io_write_region_4) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int32_t *, bus_size_t));
void		__C(CHIP,_io_write_region_8) __P((void *, bus_space_handle_t,
		    bus_size_t, const u_int64_t *, bus_size_t));

/* set multiple */
void		__C(CHIP,_io_set_multi_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t, bus_size_t));
void		__C(CHIP,_io_set_multi_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t, bus_size_t));
void		__C(CHIP,_io_set_multi_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t, bus_size_t));
void		__C(CHIP,_io_set_multi_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t, bus_size_t));

/* set region */
void		__C(CHIP,_io_set_region_1) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t, bus_size_t));
void		__C(CHIP,_io_set_region_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int16_t, bus_size_t));
void		__C(CHIP,_io_set_region_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int32_t, bus_size_t));
void		__C(CHIP,_io_set_region_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int64_t, bus_size_t));

/* copy */
void		__C(CHIP,_io_copy_1) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_space_handle_t, bus_size_t, bus_size_t));
void		__C(CHIP,_io_copy_2) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_space_handle_t, bus_size_t, bus_size_t));
void		__C(CHIP,_io_copy_4) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_space_handle_t, bus_size_t, bus_size_t));
void		__C(CHIP,_io_copy_8) __P((void *, bus_space_handle_t,
		    bus_size_t, bus_space_handle_t, bus_size_t, bus_size_t));

/* read multiple raw */
void		__C(CHIP,_io_read_raw_multi_2) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t *, bus_size_t));
void		__C(CHIP,_io_read_raw_multi_4) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t *, bus_size_t));
void		__C(CHIP,_io_read_raw_multi_8) __P((void *, bus_space_handle_t,
		    bus_size_t, u_int8_t *, bus_size_t));

/* write multiple raw */
void		__C(CHIP,_io_write_raw_multi_2) __P((void *,
		    bus_space_handle_t, bus_size_t, const u_int8_t *,
		    bus_size_t));
void		__C(CHIP,_io_write_raw_multi_4) __P((void *,
		    bus_space_handle_t, bus_size_t, const u_int8_t *,
		    bus_size_t));
void		__C(CHIP,_io_write_raw_multi_8) __P((void *,
		    bus_space_handle_t, bus_size_t, const u_int8_t *,
		    bus_size_t));

static long
    __C(CHIP,_io_ex_storage)[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof(long)];

static struct alpha_bus_space __C(CHIP,_io_space) = {
	/* cookie */
	NULL,

	/* mapping/unmapping */
	__C(CHIP,_io_map),
	__C(CHIP,_io_unmap),
	__C(CHIP,_io_subregion),

	/* allocation/deallocation */
	__C(CHIP,_io_alloc),
	__C(CHIP,_io_free),

	/* barrier */
	__C(CHIP,_io_barrier),
	
	/* read (single) */
	__C(CHIP,_io_read_1),
	__C(CHIP,_io_read_2),
	__C(CHIP,_io_read_4),
	__C(CHIP,_io_read_8),
	
	/* read multiple */
	__C(CHIP,_io_read_multi_1),
	__C(CHIP,_io_read_multi_2),
	__C(CHIP,_io_read_multi_4),
	__C(CHIP,_io_read_multi_8),
	
	/* read region */
	__C(CHIP,_io_read_region_1),
	__C(CHIP,_io_read_region_2),
	__C(CHIP,_io_read_region_4),
	__C(CHIP,_io_read_region_8),
	
	/* write (single) */
	__C(CHIP,_io_write_1),
	__C(CHIP,_io_write_2),
	__C(CHIP,_io_write_4),
	__C(CHIP,_io_write_8),
	
	/* write multiple */
	__C(CHIP,_io_write_multi_1),
	__C(CHIP,_io_write_multi_2),
	__C(CHIP,_io_write_multi_4),
	__C(CHIP,_io_write_multi_8),
	
	/* write region */
	__C(CHIP,_io_write_region_1),
	__C(CHIP,_io_write_region_2),
	__C(CHIP,_io_write_region_4),
	__C(CHIP,_io_write_region_8),

	/* set multiple */
	__C(CHIP,_io_set_multi_1),
	__C(CHIP,_io_set_multi_2),
	__C(CHIP,_io_set_multi_4),
	__C(CHIP,_io_set_multi_8),
	
	/* set region */
	__C(CHIP,_io_set_region_1),
	__C(CHIP,_io_set_region_2),
	__C(CHIP,_io_set_region_4),
	__C(CHIP,_io_set_region_8),

	/* copy */
	__C(CHIP,_io_copy_1),
	__C(CHIP,_io_copy_2),
	__C(CHIP,_io_copy_4),
	__C(CHIP,_io_copy_8),

	/* read multiple raw */
	__C(CHIP,_io_read_raw_multi_2),
	__C(CHIP,_io_read_raw_multi_4),
	__C(CHIP,_io_read_raw_multi_8),
	
	/* write multiple raw*/
	__C(CHIP,_io_write_raw_multi_2),
	__C(CHIP,_io_write_raw_multi_4),
	__C(CHIP,_io_write_raw_multi_8),
};

bus_space_tag_t
__C(CHIP,_bus_io_init)(v)
	void *v;
{
        bus_space_tag_t t = &__C(CHIP,_io_space);
	struct extent *ex;

	t->abs_cookie = v;

	ex = extent_create(__S(__C(CHIP,_bus_io)), 0x0UL, 0xffffffffUL,
	    M_DEVBUF, (caddr_t)__C(CHIP,_io_ex_storage),
	    sizeof(__C(CHIP,_io_ex_storage)), EX_NOWAIT|EX_NOCOALESCE);

	CHIP_IO_EXTENT(v) = ex;

	return (t);
}

int
__C(CHIP,_io_map)(v, ioaddr, iosize, cacheable, iohp)
	void *v;
	bus_addr_t ioaddr;
	bus_size_t iosize;
	int cacheable;
	bus_space_handle_t *iohp;
{
	int error;

#ifdef EXTENT_DEBUG
	printf("io: allocating 0x%lx to 0x%lx\n", ioaddr, ioaddr + iosize - 1);
#endif
        error = extent_alloc_region(CHIP_IO_EXTENT(v), ioaddr, iosize,
            EX_NOWAIT | (CHIP_EX_MALLOC_SAFE(v) ? EX_MALLOCOK : 0));
	if (error) {
#ifdef EXTENT_DEBUG
		printf("io: allocation failed (%d)\n", error);
		extent_print(CHIP_IO_EXTENT(v));
#endif
		return (error);
	}

	*iohp = ALPHA_PHYS_TO_K0SEG(CHIP_IO_SYS_START(v)) + ioaddr;

	return (0);
}

void
__C(CHIP,_io_unmap)(v, ioh, iosize)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t iosize;
{
	bus_addr_t ioaddr;
	int error;

#ifdef EXTENT_DEBUG
	printf("io: freeing handle 0x%lx for 0x%lx\n", ioh, iosize);
#endif

	ioaddr = ioh - ALPHA_PHYS_TO_K0SEG(CHIP_IO_SYS_START(v));

#ifdef EXTENT_DEBUG
	printf("io: freeing 0x%lx to 0x%lx\n", ioaddr, ioaddr + iosize - 1);
#endif
        error = extent_free(CHIP_IO_EXTENT(v), ioaddr, iosize,
            EX_NOWAIT | (CHIP_EX_MALLOC_SAFE(v) ? EX_MALLOCOK : 0));
	if (error) {
		printf("%s: WARNING: could not unmap 0x%lx-0x%lx (error %d)\n",
		   __S(__C(CHIP,_io_unmap)), ioaddr, ioaddr + iosize - 1,
		   error);
#ifdef EXTENT_DEBUG
		extent_print(CHIP_IO_EXTENT(v));
#endif
	}	
}

int
__C(CHIP,_io_subregion)(v, ioh, offset, size, nioh)
	void *v;
	bus_space_handle_t ioh, *nioh;
	bus_size_t offset, size;
{

	*nioh = ioh + offset;
	return (0);
}

int
__C(CHIP,_io_alloc)(v, rstart, rend, size, align, boundary, cacheable,
    addrp, bshp)
	void *v;
	bus_addr_t rstart, rend, *addrp;
	bus_size_t size, align, boundary;
	int cacheable;
	bus_space_handle_t *bshp;
{

	/* XXX XXX XXX XXX XXX XXX */
	panic("%s not implemented", __S(__C(CHIP,_io_alloc)));
}

void
__C(CHIP,_io_free)(v, bsh, size)
	void *v;
	bus_space_handle_t bsh;
	bus_size_t size;
{

	/* XXX XXX XXX XXX XXX XXX */
	panic("%s not implemented", __S(__C(CHIP,_io_free)));
}

inline void
__C(CHIP,_io_barrier)(v, h, o, l, f)
	void *v;
	bus_space_handle_t h;
	bus_size_t o, l;
	int f;
{

	if ((f & BUS_BARRIER_READ) != 0)
		alpha_mb();
	else if ((f & BUS_BARRIER_WRITE) != 0)
		alpha_wmb();
}

inline u_int8_t
__C(CHIP,_io_read_1)(v, ioh, off)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
{
	bus_addr_t addr;

	addr = ioh + off;
	alpha_mb();
	return (alpha_ldbu((u_int8_t *)addr));
}

inline u_int16_t
__C(CHIP,_io_read_2)(v, ioh, off)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
{
	bus_addr_t addr;

	addr = ioh + off;
#ifdef DIAGNOSTIC
	if (addr & 1)
		panic(__S(__C(CHIP,_io_read_2)) ": addr 0x%lx not aligned",
		    addr);
#endif
	alpha_mb();
	return (alpha_ldwu((u_int16_t *)addr));
}

inline u_int32_t
__C(CHIP,_io_read_4)(v, ioh, off)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
{
	bus_addr_t addr;

	addr = ioh + off;
#ifdef DIAGNOSTIC
	if (addr & 3)
		panic(__S(__C(CHIP,_io_read_4)) ": addr 0x%lx not aligned",
		    addr);
#endif
	alpha_mb();
	return (*(u_int32_t *)addr);
}

inline u_int64_t
__C(CHIP,_io_read_8)(v, ioh, off)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
{

	/* XXX XXX XXX */
	panic("%s not implemented", __S(__C(CHIP,_io_read_8)));
}

#define CHIP_io_read_multi_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_io_read_multi_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__C(CHIP,_io_barrier)(v, h, o, sizeof *a,		\
		    BUS_BARRIER_READ);					\
		*a++ = __C(__C(CHIP,_io_read_),BYTES)(v, h, o);		\
	}								\
}
CHIP_io_read_multi_N(1,u_int8_t)
CHIP_io_read_multi_N(2,u_int16_t)
CHIP_io_read_multi_N(4,u_int32_t)
CHIP_io_read_multi_N(8,u_int64_t)

#define CHIP_io_read_region_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_io_read_region_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		*a++ = __C(__C(CHIP,_io_read_),BYTES)(v, h, o);		\
		o += sizeof *a;						\
	}								\
}
CHIP_io_read_region_N(1,u_int8_t)
CHIP_io_read_region_N(2,u_int16_t)
CHIP_io_read_region_N(4,u_int32_t)
CHIP_io_read_region_N(8,u_int64_t)

inline void
__C(CHIP,_io_write_1)(v, ioh, off, val)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
	u_int8_t val;
{
	bus_addr_t addr;

	addr = ioh + off;
	alpha_stb((u_int8_t *)addr, val);
        alpha_mb();
}

inline void
__C(CHIP,_io_write_2)(v, ioh, off, val)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
	u_int16_t val;
{
	bus_addr_t addr;

	addr = ioh + off;
#ifdef DIAGNOSTIC
	if (addr & 1)
		panic(__S(__C(CHIP,_io_write_2)) ": addr 0x%lx not aligned",
		    addr);
#endif
	alpha_stw((u_int16_t *)addr, val);
	alpha_mb();
}

inline void
__C(CHIP,_io_write_4)(v, ioh, off, val)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
	u_int32_t val;
{
	bus_addr_t addr;

	addr = ioh + off;
#ifdef DIAGNOSTIC
	if (addr & 3)
		panic(__S(__C(CHIP,_io_write_4)) ": addr 0x%lx not aligned",
		    addr);
#endif
	*(u_int32_t *)addr = val;
	alpha_mb();
}

inline void
__C(CHIP,_io_write_8)(v, ioh, off, val)
	void *v;
	bus_space_handle_t ioh;
	bus_size_t off;
	u_int64_t val;
{

	/* XXX XXX XXX */
	panic("%s not implemented", __S(__C(CHIP,_io_write_8)));
	alpha_mb();
}

#define CHIP_io_write_multi_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_io_write_multi_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	const TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__C(__C(CHIP,_io_write_),BYTES)(v, h, o, *a++);		\
		__C(CHIP,_io_barrier)(v, h, o, sizeof *a,		\
		    BUS_BARRIER_WRITE);					\
	}								\
}
CHIP_io_write_multi_N(1,u_int8_t)
CHIP_io_write_multi_N(2,u_int16_t)
CHIP_io_write_multi_N(4,u_int32_t)
CHIP_io_write_multi_N(8,u_int64_t)

#define CHIP_io_write_region_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_io_write_region_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	const TYPE *a;							\
{									\
									\
	while (c-- > 0) {						\
		__C(__C(CHIP,_io_write_),BYTES)(v, h, o, *a++);		\
		o += sizeof *a;						\
	}								\
}
CHIP_io_write_region_N(1,u_int8_t)
CHIP_io_write_region_N(2,u_int16_t)
CHIP_io_write_region_N(4,u_int32_t)
CHIP_io_write_region_N(8,u_int64_t)

#define CHIP_io_set_multi_N(BYTES,TYPE)					\
void									\
__C(__C(CHIP,_io_set_multi_),BYTES)(v, h, o, val, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE val;							\
{									\
									\
	while (c-- > 0) {						\
		__C(__C(CHIP,_io_write_),BYTES)(v, h, o, val);		\
		__C(CHIP,_io_barrier)(v, h, o, sizeof val,		\
		    BUS_BARRIER_WRITE);					\
	}								\
}
CHIP_io_set_multi_N(1,u_int8_t)
CHIP_io_set_multi_N(2,u_int16_t)
CHIP_io_set_multi_N(4,u_int32_t)
CHIP_io_set_multi_N(8,u_int64_t)

#define CHIP_io_set_region_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_io_set_region_),BYTES)(v, h, o, val, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	TYPE val;							\
{									\
									\
	while (c-- > 0) {						\
		__C(__C(CHIP,_io_write_),BYTES)(v, h, o, val);		\
		o += sizeof val;					\
	}								\
}
CHIP_io_set_region_N(1,u_int8_t)
CHIP_io_set_region_N(2,u_int16_t)
CHIP_io_set_region_N(4,u_int32_t)
CHIP_io_set_region_N(8,u_int64_t)

#define	CHIP_io_copy_N(BYTES)						\
void									\
__C(__C(CHIP,_io_copy_),BYTES)(v, h1, o1, h2, o2, c)			\
	void *v;							\
	bus_space_handle_t h1, h2;					\
	bus_size_t o1, o2, c;						\
{									\
	bus_size_t i, o;						\
									\
	for (i = 0, o = 0; i < c; i++, o += BYTES)			\
		__C(__C(CHIP,_io_write_),BYTES)(v, h2, o2 + o,		\
		    __C(__C(CHIP,_io_read_),BYTES)(v, h1, o1 + o));	\
}
CHIP_io_copy_N(1)
CHIP_io_copy_N(2)
CHIP_io_copy_N(4)
CHIP_io_copy_N(8)

#define CHIP_io_read_raw_multi_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_io_read_raw_multi_),BYTES)(v, h, o, a, c)			\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	u_int8_t *a;							\
{									\
	TYPE temp;							\
	int i;								\
									\
	while (c > 0) {							\
		__C(CHIP,_io_barrier)(v, h, o, BYTES, BUS_BARRIER_READ); \
		temp = __C(__C(CHIP,_io_read_),BYTES)(v, h, o);		\
		i = MIN(c, BYTES);					\
		c -= i;							\
		while (i--) {						\
			*a++ = temp & 0xff;				\
			temp >>= 8;					\
		}							\
	}								\
}
CHIP_io_read_raw_multi_N(2,u_int16_t)
CHIP_io_read_raw_multi_N(4,u_int32_t)
CHIP_io_read_raw_multi_N(8,u_int64_t)

#define CHIP_io_write_raw_multi_N(BYTES,TYPE)				\
void									\
__C(__C(CHIP,_io_write_raw_multi_),BYTES)(v, h, o, a, c)		\
	void *v;							\
	bus_space_handle_t h;						\
	bus_size_t o, c;						\
	const u_int8_t *a;						\
{									\
	TYPE temp;							\
	int i;								\
									\
	while (c > 0) {							\
		temp = 0;						\
		for (i = BYTES - 1; i >= 0; i--) {			\
			temp <<= 8;					\
			if (i < c)					\
				temp |= *(a + i);			\
		}							\
		__C(__C(CHIP,_io_write_),BYTES)(v, h, o, temp);		\
		__C(CHIP,_io_barrier)(v, h, o, BYTES, BUS_BARRIER_WRITE); \
		i = MIN(c, BYTES); 					\
		c -= i;							\
		a += i;							\
	}								\
}
CHIP_io_write_raw_multi_N(2,u_int16_t)
CHIP_io_write_raw_multi_N(4,u_int32_t)
CHIP_io_write_raw_multi_N(8,u_int64_t)
