/* $NetBSD: sfb.c,v 1.51.6.2 2002/08/23 03:08:27 lukem Exp $ */

/*
 * Copyright (c) 1998, 1999 Tohru Nishimura.  All rights reserved.
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
 *      This product includes software developed by Tohru Nishimura
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sfb.c,v 1.51.6.2 2002/08/23 03:08:27 lukem Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/ioctl.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

#include <dev/rasops/rasops.h>
#include <dev/wsfont/wsfont.h>

#include <dev/tc/tcvar.h>
#include <dev/ic/bt459reg.h>	
#include <dev/tc/sfbreg.h>

#include <uvm/uvm_extern.h>

#if defined(pmax)
#define	machine_btop(x) mips_btop(MIPS_KSEG1_TO_PHYS(x))
#endif

#if defined(alpha)
#define	machine_btop(x) alpha_btop(ALPHA_K0SEG_TO_PHYS(x))
#endif

/*
 * N.B., Bt459 registers are 8bit width.  Some of TC framebuffers have
 * obscure register layout such as 2nd and 3rd Bt459 registers are
 * adjacent each other in a word, i.e.,
 *	struct bt459triplet {
 * 		struct {
 *			u_int8_t u0;
 *			u_int8_t u1;
 *			u_int8_t u2;
 *			unsigned :8;
 *		} bt_lo;
 *
 * Although HX has single Bt459, 32bit R/W can be done w/o any trouble.
 *	struct bt459reg {
 *		   u_int32_t	   bt_lo;
 *		   u_int32_t	   bt_hi;
 *		   u_int32_t	   bt_reg;
 *		   u_int32_t	   bt_cmap;
 *	};
 */

/* Bt459 hardware registers */
#define	bt_lo	0
#define	bt_hi	1
#define	bt_reg	2
#define	bt_cmap 3

#define	REG(base, index)	*((u_int32_t *)(base) + (index))
#define	SELECT(vdac, regno) do {			\
	REG(vdac, bt_lo) = ((regno) & 0x00ff);		\
	REG(vdac, bt_hi) = ((regno) & 0x0f00) >> 8;	\
	tc_wmb();					\
   } while (0)

struct hwcmap256 {
#define	CMAP_SIZE	256	/* 256 R/G/B entries */
	u_int8_t r[CMAP_SIZE];
	u_int8_t g[CMAP_SIZE];
	u_int8_t b[CMAP_SIZE];
};

struct hwcursor64 {
	struct wsdisplay_curpos cc_pos;
	struct wsdisplay_curpos cc_hot;
	struct wsdisplay_curpos cc_size;
	struct wsdisplay_curpos cc_magic;
#define	CURSOR_MAX_SIZE	64
	u_int8_t cc_color[6];
	u_int64_t cc_image[64 + 64];
};

struct sfb_softc {
	struct device sc_dev;
	vaddr_t sc_vaddr;
	size_t sc_size;
	struct rasops_info *sc_ri;
	struct hwcmap256 sc_cmap;	/* software copy of colormap */
	struct hwcursor64 sc_cursor;	/* software copy of cursor */
	int sc_blanked;			/* video visibility disabled */
	int sc_curenb;			/* cursor sprite enabled */
	int sc_changed;			/* need update of hardware */
#define	WSDISPLAY_CMAP_DOLUT	0x20
	int nscreens;
};

#define	HX_MAGIC_X	368
#define	HX_MAGIC_Y	38

static int  sfbmatch __P((struct device *, struct cfdata *, void *));
static void sfbattach __P((struct device *, struct device *, void *));

const struct cfattach sfb_ca = {
	sizeof(struct sfb_softc), sfbmatch, sfbattach,
};

static void sfb_common_init __P((struct rasops_info *));
static struct rasops_info sfb_console_ri;
static tc_addr_t sfb_consaddr;

static void sfb_putchar __P((void *, int, int, u_int, long));
static void sfb_erasecols __P((void *, int, int, int, long));
static void sfb_eraserows __P((void *, int, int, long));
static void sfb_copyrows __P((void *, int, int, int));
static void sfb_do_cursor __P((struct rasops_info *));
#if 0
static void sfb_copycols __P((void *, int, int, int, int));
#endif

static struct wsscreen_descr sfb_stdscreen = {
	"std", 0, 0,
	0, /* textops */
	0, 0,
	WSSCREEN_REVERSE
};

static const struct wsscreen_descr *_sfb_scrlist[] = {
	&sfb_stdscreen,
};

static const struct wsscreen_list sfb_screenlist = {
	sizeof(_sfb_scrlist) / sizeof(struct wsscreen_descr *), _sfb_scrlist
};

static int	sfbioctl __P((void *, u_long, caddr_t, int, struct proc *));
static paddr_t	sfbmmap __P((void *, off_t, int));

static int	sfb_alloc_screen __P((void *, const struct wsscreen_descr *,
				      void **, int *, int *, long *));
static void	sfb_free_screen __P((void *, void *));
static int	sfb_show_screen __P((void *, void *, int,
				     void (*) (void *, int, int), void *));

static const struct wsdisplay_accessops sfb_accessops = {
	sfbioctl,
	sfbmmap,
	sfb_alloc_screen,
	sfb_free_screen,
	sfb_show_screen,
	0 /* load_font */
};

int  sfb_cnattach __P((tc_addr_t));
static int  sfbintr __P((void *));
static void sfbhwinit __P((caddr_t));

static int  get_cmap __P((struct sfb_softc *, struct wsdisplay_cmap *));
static int  set_cmap __P((struct sfb_softc *, struct wsdisplay_cmap *));
static int  set_cursor __P((struct sfb_softc *, struct wsdisplay_cursor *));
static int  get_cursor __P((struct sfb_softc *, struct wsdisplay_cursor *));
static void set_curpos __P((struct sfb_softc *, struct wsdisplay_curpos *));

/*
 * Compose 2 bit/pixel cursor image.  Bit order will be reversed.
 *   M M M M I I I I		M I M I M I M I
 *	[ before ]		   [ after ]
 *   3 2 1 0 3 2 1 0		0 0 1 1 2 2 3 3
 *   7 6 5 4 7 6 5 4		4 4 5 5 6 6 7 7
 */
static const u_int8_t shuffle[256] = {
	0x00, 0x40, 0x10, 0x50, 0x04, 0x44, 0x14, 0x54,
	0x01, 0x41, 0x11, 0x51, 0x05, 0x45, 0x15, 0x55,
	0x80, 0xc0, 0x90, 0xd0, 0x84, 0xc4, 0x94, 0xd4,
	0x81, 0xc1, 0x91, 0xd1, 0x85, 0xc5, 0x95, 0xd5,
	0x20, 0x60, 0x30, 0x70, 0x24, 0x64, 0x34, 0x74,
	0x21, 0x61, 0x31, 0x71, 0x25, 0x65, 0x35, 0x75,
	0xa0, 0xe0, 0xb0, 0xf0, 0xa4, 0xe4, 0xb4, 0xf4,
	0xa1, 0xe1, 0xb1, 0xf1, 0xa5, 0xe5, 0xb5, 0xf5,
	0x08, 0x48, 0x18, 0x58, 0x0c, 0x4c, 0x1c, 0x5c,
	0x09, 0x49, 0x19, 0x59, 0x0d, 0x4d, 0x1d, 0x5d,
	0x88, 0xc8, 0x98, 0xd8, 0x8c, 0xcc, 0x9c, 0xdc,
	0x89, 0xc9, 0x99, 0xd9, 0x8d, 0xcd, 0x9d, 0xdd,
	0x28, 0x68, 0x38, 0x78, 0x2c, 0x6c, 0x3c, 0x7c,
	0x29, 0x69, 0x39, 0x79, 0x2d, 0x6d, 0x3d, 0x7d,
	0xa8, 0xe8, 0xb8, 0xf8, 0xac, 0xec, 0xbc, 0xfc,
	0xa9, 0xe9, 0xb9, 0xf9, 0xad, 0xed, 0xbd, 0xfd,
	0x02, 0x42, 0x12, 0x52, 0x06, 0x46, 0x16, 0x56,
	0x03, 0x43, 0x13, 0x53, 0x07, 0x47, 0x17, 0x57,
	0x82, 0xc2, 0x92, 0xd2, 0x86, 0xc6, 0x96, 0xd6,
	0x83, 0xc3, 0x93, 0xd3, 0x87, 0xc7, 0x97, 0xd7,
	0x22, 0x62, 0x32, 0x72, 0x26, 0x66, 0x36, 0x76,
	0x23, 0x63, 0x33, 0x73, 0x27, 0x67, 0x37, 0x77,
	0xa2, 0xe2, 0xb2, 0xf2, 0xa6, 0xe6, 0xb6, 0xf6,
	0xa3, 0xe3, 0xb3, 0xf3, 0xa7, 0xe7, 0xb7, 0xf7,
	0x0a, 0x4a, 0x1a, 0x5a, 0x0e, 0x4e, 0x1e, 0x5e,
	0x0b, 0x4b, 0x1b, 0x5b, 0x0f, 0x4f, 0x1f, 0x5f,
	0x8a, 0xca, 0x9a, 0xda, 0x8e, 0xce, 0x9e, 0xde,
	0x8b, 0xcb, 0x9b, 0xdb, 0x8f, 0xcf, 0x9f, 0xdf,
	0x2a, 0x6a, 0x3a, 0x7a, 0x2e, 0x6e, 0x3e, 0x7e,
	0x2b, 0x6b, 0x3b, 0x7b, 0x2f, 0x6f, 0x3f, 0x7f,
	0xaa, 0xea, 0xba, 0xfa, 0xae, 0xee, 0xbe, 0xfe,
	0xab, 0xeb, 0xbb, 0xfb, 0xaf, 0xef, 0xbf, 0xff,
};

static int
sfbmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct tc_attach_args *ta = aux;

	if (strncmp("PMAGB-BA", ta->ta_modname, TC_ROM_LLEN) != 0)
		return (0);
	return (1);
}

static void
sfbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sfb_softc *sc = (struct sfb_softc *)self;
	struct tc_attach_args *ta = aux;
	struct rasops_info *ri;
	struct wsemuldisplaydev_attach_args waa;
	struct hwcmap256 *cm;
	const u_int8_t *p;
	caddr_t asic;
	int console, index;

	console = (ta->ta_addr == sfb_consaddr);
	if (console) {
		sc->sc_ri = ri = &sfb_console_ri;
		sc->nscreens = 1;
	}
	else {
		MALLOC(ri, struct rasops_info *, sizeof(struct rasops_info),
			M_DEVBUF, M_NOWAIT);
		if (ri == NULL) {
			printf(": can't alloc memory\n");
			return;
		}
		memset(ri, 0, sizeof(struct rasops_info));

		ri->ri_hw = (void *)ta->ta_addr;
		sfb_common_init(ri);
		sc->sc_ri = ri;
	}
	printf(": %dx%d, %dbpp\n", ri->ri_width, ri->ri_height, ri->ri_depth);

	cm = &sc->sc_cmap;
	p = rasops_cmap;
	for (index = 0; index < CMAP_SIZE; index++, p += 3) {
		cm->r[index] = p[0];
		cm->g[index] = p[1];
		cm->b[index] = p[2];
	}

	sc->sc_vaddr = ta->ta_addr;
	sc->sc_cursor.cc_magic.x = HX_MAGIC_X;
	sc->sc_cursor.cc_magic.y = HX_MAGIC_Y;
	sc->sc_blanked = sc->sc_curenb = 0;

	tc_intr_establish(parent, ta->ta_cookie, IPL_TTY, sfbintr, sc);

	asic = (caddr_t)ri->ri_hw + SFB_ASIC_OFFSET;
	*(u_int32_t *)(asic + SFB_ASIC_CLEAR_INTR) = 0;
	*(u_int32_t *)(asic + SFB_ASIC_ENABLE_INTR) = 1;

	waa.console = console;
	waa.scrdata = &sfb_screenlist;
	waa.accessops = &sfb_accessops;
	waa.accesscookie = sc;

	config_found(self, &waa, wsemuldisplaydevprint);
}

static void
sfb_common_init(ri)
	struct rasops_info *ri;
{
	caddr_t base, asic;
	int hsetup, vsetup, vbase, cookie;

	base = (caddr_t)ri->ri_hw;
	asic = base + SFB_ASIC_OFFSET;
	hsetup = *(u_int32_t *)(asic + SFB_ASIC_VIDEO_HSETUP);
	vsetup = *(u_int32_t *)(asic + SFB_ASIC_VIDEO_VSETUP);

	*(u_int32_t *)(asic + SFB_ASIC_VIDEO_BASE) = vbase = 1;
	*(u_int32_t *)(asic + SFB_ASIC_PLANEMASK) = ~0;
	*(u_int32_t *)(asic + SFB_ASIC_PIXELMASK) = ~0;
	*(u_int32_t *)(asic + SFB_ASIC_MODE) = 0;	/* MODE_SIMPLE */
	*(u_int32_t *)(asic + SFB_ASIC_ROP) = 3; 	/* ROP_COPY */
	*(u_int32_t *)(asic + 0x180000) = 0; 		/* Bt459 reset */

	/* initialize colormap and cursor hardware */
	sfbhwinit(base);

	ri->ri_flg = RI_CENTER;
	ri->ri_depth = 8;
	ri->ri_width = (hsetup & 0x1ff) << 2;
	ri->ri_height = (vsetup & 0x7ff);
	ri->ri_stride = ri->ri_width * (ri->ri_depth / 8);
	ri->ri_bits = base + SFB_FB_OFFSET + vbase * 4096;

	/* clear the screen */
	memset(ri->ri_bits, 0, ri->ri_stride * ri->ri_height);

	wsfont_init();
	/* prefer 12 pixel wide font */
	cookie = wsfont_find(NULL, 12, 0, 0, WSDISPLAY_FONTORDER_R2L,
	    WSDISPLAY_FONTORDER_L2R);
	if (cookie <= 0)
		cookie = wsfont_find(NULL, 0, 0, 0, WSDISPLAY_FONTORDER_R2L,
		    WSDISPLAY_FONTORDER_L2R);
	if (cookie <= 0) {
		printf("sfb: font table is empty\n");
		return;
	}

	/* the accelerated sfb_putchar() needs LSbit left */
	if (wsfont_lock(cookie, &ri->ri_font)) {
		printf("sfb: couldn't lock font\n");
		return;
	}
	ri->ri_wsfcookie = cookie;

	rasops_init(ri, 34, 80);

	/* add our accelerated functions */
	ri->ri_ops.putchar = sfb_putchar;
	ri->ri_ops.erasecols = sfb_erasecols;
	ri->ri_ops.copyrows = sfb_copyrows;
	ri->ri_ops.eraserows = sfb_eraserows;
	ri->ri_do_cursor = sfb_do_cursor;

	/* XXX shouldn't be global */
	sfb_stdscreen.nrows = ri->ri_rows;
	sfb_stdscreen.ncols = ri->ri_cols;
	sfb_stdscreen.textops = &ri->ri_ops;
	sfb_stdscreen.capabilities = ri->ri_caps;
}

static int
sfbioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct sfb_softc *sc = v;
	struct rasops_info *ri = sc->sc_ri;
	int turnoff;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_SFB;
		return (0);

	case WSDISPLAYIO_GINFO:
#define	wsd_fbip ((struct wsdisplay_fbinfo *)data)
		wsd_fbip->height = ri->ri_height;
		wsd_fbip->width = ri->ri_width;
		wsd_fbip->depth = ri->ri_depth;
		wsd_fbip->cmsize = CMAP_SIZE;
#undef fbt
		return (0);

	case WSDISPLAYIO_GETCMAP:
		return get_cmap(sc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		return set_cmap(sc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_SVIDEO:
		turnoff = *(int *)data == WSDISPLAYIO_VIDEO_OFF;
		if (sc->sc_blanked ^ turnoff) {
			caddr_t asic = (caddr_t)ri->ri_hw + SFB_ASIC_OFFSET;
			*(u_int32_t *)(asic + SFB_ASIC_VIDEO_VALID)
				= !turnoff;
			tc_wmb();
			sc->sc_blanked = turnoff;
		}
		return (0);

	case WSDISPLAYIO_GVIDEO:
		*(u_int *)data = sc->sc_blanked ?
		    WSDISPLAYIO_VIDEO_OFF : WSDISPLAYIO_VIDEO_ON;
		return (0);

	case WSDISPLAYIO_GCURPOS:
		*(struct wsdisplay_curpos *)data = sc->sc_cursor.cc_pos;
		return (0);

	case WSDISPLAYIO_SCURPOS:
		set_curpos(sc, (struct wsdisplay_curpos *)data);
		sc->sc_changed |= WSDISPLAY_CURSOR_DOPOS;
		return (0);

	case WSDISPLAYIO_GCURMAX:
		((struct wsdisplay_curpos *)data)->x =
		((struct wsdisplay_curpos *)data)->y = CURSOR_MAX_SIZE;
		return (0);

	case WSDISPLAYIO_GCURSOR:
		return get_cursor(sc, (struct wsdisplay_cursor *)data);

	case WSDISPLAYIO_SCURSOR:
		return set_cursor(sc, (struct wsdisplay_cursor *)data);
	}
	return (EPASSTHROUGH);
}

static paddr_t
sfbmmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct sfb_softc *sc = v;

	if (offset >= SFB_SIZE || offset < 0)
		return (-1);
	return machine_btop(sc->sc_vaddr + offset);
}

static int
sfb_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct sfb_softc *sc = v;
	struct rasops_info *ri = sc->sc_ri;
	long defattr;

	if (sc->nscreens > 0)
		return (ENOMEM);

	*cookiep = ri;	 /* one and only for now */
	*curxp = 0;
	*curyp = 0;
	(*ri->ri_ops.alloc_attr)(ri, 0, 0, 0, &defattr);
	*attrp = defattr;
	sc->nscreens++;
	return (0);
}

static void
sfb_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct sfb_softc *sc = v;

	if (sc->sc_ri == &sfb_console_ri)
		panic("sfb_free_screen: console");

	sc->nscreens--;
}

static int
sfb_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb) __P((void *, int, int));
	void *cbarg;
{

	return (0);
}

/* EXPORT */ int
sfb_cnattach(addr)
	tc_addr_t addr;
{
	struct rasops_info *ri;
	long defattr;

	ri = &sfb_console_ri;
	ri->ri_hw = (void *)addr;
	sfb_common_init(ri);
	(*ri->ri_ops.alloc_attr)(&ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&sfb_stdscreen, ri, 0, 0, defattr);
	sfb_consaddr = addr;
	return (0);
}

static int
sfbintr(arg)
	void *arg;
{
	struct sfb_softc *sc = arg;
	caddr_t base, asic, vdac;
	int v;
	
	base = (caddr_t)sc->sc_ri->ri_hw;
	asic = base + SFB_ASIC_OFFSET;
	*(u_int32_t *)(asic + SFB_ASIC_CLEAR_INTR) = 0;
	/* *(u_int32_t *)(asic + SFB_ASIC_ENABLE_INTR) = 1; */

	if (sc->sc_changed == 0)
		goto done;

	vdac = base + SFB_RAMDAC_OFFSET;
	v = sc->sc_changed;
	if (v & WSDISPLAY_CURSOR_DOCUR) {
		SELECT(vdac, BT459_IREG_CCR);
		REG(vdac, bt_reg) = (sc->sc_curenb) ? 0xc0 : 0x00;
	}
	if (v & (WSDISPLAY_CURSOR_DOPOS | WSDISPLAY_CURSOR_DOHOT)) {
		int x, y;

		x = sc->sc_cursor.cc_pos.x - sc->sc_cursor.cc_hot.x;
		y = sc->sc_cursor.cc_pos.y - sc->sc_cursor.cc_hot.y;
		x += sc->sc_cursor.cc_magic.x;
		y += sc->sc_cursor.cc_magic.y;

		SELECT(vdac, BT459_IREG_CURSOR_X_LOW);
		REG(vdac, bt_reg) = x;		tc_wmb();
		REG(vdac, bt_reg) = x >> 8;	tc_wmb();
		REG(vdac, bt_reg) = y;		tc_wmb();
		REG(vdac, bt_reg) = y >> 8;	tc_wmb();
	}
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		u_int8_t *cp = sc->sc_cursor.cc_color;

		SELECT(vdac, BT459_IREG_CCOLOR_2);
		REG(vdac, bt_reg) = cp[1];	tc_wmb();
		REG(vdac, bt_reg) = cp[3];	tc_wmb();
		REG(vdac, bt_reg) = cp[5];	tc_wmb();

		REG(vdac, bt_reg) = cp[0];	tc_wmb();
		REG(vdac, bt_reg) = cp[2];	tc_wmb();
		REG(vdac, bt_reg) = cp[4];	tc_wmb();
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		u_int8_t *ip, *mp, img, msk;
		u_int8_t u;
		int bcnt;

		ip = (u_int8_t *)sc->sc_cursor.cc_image;
		mp = (u_int8_t *)(sc->sc_cursor.cc_image + CURSOR_MAX_SIZE);

		bcnt = 0;
		SELECT(vdac, BT459_IREG_CRAM_BASE+0);
		/* 64 pixel scan line is consisted with 16 byte cursor ram */
		while (bcnt < sc->sc_cursor.cc_size.y * 16) {
			/* pad right half 32 pixel when smaller than 33 */
			if ((bcnt & 0x8) && sc->sc_cursor.cc_size.x < 33) {
				REG(vdac, bt_reg) = 0; tc_wmb();
				REG(vdac, bt_reg) = 0; tc_wmb();
			}
			else {
				img = *ip++;
				msk = *mp++;
				img &= msk;	/* cookie off image */
				u = (msk & 0x0f) << 4 | (img & 0x0f);
				REG(vdac, bt_reg) = shuffle[u];	tc_wmb();
				u = (msk & 0xf0) | (img & 0xf0) >> 4;
				REG(vdac, bt_reg) = shuffle[u];	tc_wmb();
			}
			bcnt += 2;
		}
		/* pad unoccupied scan lines */
		while (bcnt < CURSOR_MAX_SIZE * 16) {
			REG(vdac, bt_reg) = 0; tc_wmb();
			REG(vdac, bt_reg) = 0; tc_wmb();
			bcnt += 2;
		}
	}
	if (v & WSDISPLAY_CMAP_DOLUT) {
		struct hwcmap256 *cm = &sc->sc_cmap;
		int index;

		SELECT(vdac, 0);
		for (index = 0; index < CMAP_SIZE; index++) {
			REG(vdac, bt_cmap) = cm->r[index];	tc_wmb();
			REG(vdac, bt_cmap) = cm->g[index];	tc_wmb();
			REG(vdac, bt_cmap) = cm->b[index];	tc_wmb();
		}
	}
	sc->sc_changed = 0;
done:
	return (1);
}

static void
sfbhwinit(base)
	caddr_t base;
{
	caddr_t vdac = base + SFB_RAMDAC_OFFSET;
	const u_int8_t *p;
	int i;

	SELECT(vdac, BT459_IREG_COMMAND_0);
	REG(vdac, bt_reg) = 0x40; /* CMD0 */	tc_wmb();
	REG(vdac, bt_reg) = 0x0;  /* CMD1 */	tc_wmb();
	REG(vdac, bt_reg) = 0xc0; /* CMD2 */	tc_wmb();
	REG(vdac, bt_reg) = 0xff; /* PRM */	tc_wmb();
	REG(vdac, bt_reg) = 0;    /* 205 */	tc_wmb();
	REG(vdac, bt_reg) = 0x0;  /* PBM */	tc_wmb();
	REG(vdac, bt_reg) = 0;    /* 207 */	tc_wmb();
	REG(vdac, bt_reg) = 0x0;  /* ORM */	tc_wmb();
	REG(vdac, bt_reg) = 0x0;  /* OBM */	tc_wmb();
	REG(vdac, bt_reg) = 0x0;  /* ILV */	tc_wmb();
	REG(vdac, bt_reg) = 0x0;  /* TEST */	tc_wmb();

	SELECT(vdac, BT459_IREG_CCR);
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();
	REG(vdac, bt_reg) = 0x0;	tc_wmb();

	/* build sane colormap */
	SELECT(vdac, 0);
	p = rasops_cmap;
	for (i = 0; i < CMAP_SIZE; i++, p += 3) {
		REG(vdac, bt_cmap) = p[0];	tc_wmb();
		REG(vdac, bt_cmap) = p[1];	tc_wmb();
		REG(vdac, bt_cmap) = p[2];	tc_wmb();
	}

	/* clear out cursor image */
	SELECT(vdac, BT459_IREG_CRAM_BASE);
	for (i = 0; i < 1024; i++)
		REG(vdac, bt_reg) = 0xff;	tc_wmb();

	/*
	 * 2 bit/pixel cursor.  Assign MSB for cursor mask and LSB for
	 * cursor image.  CCOLOR_2 for mask color, while CCOLOR_3 for
	 * image color.  CCOLOR_1 will be never used.
	 */
	SELECT(vdac, BT459_IREG_CCOLOR_1);
	REG(vdac, bt_reg) = 0xff;	tc_wmb();
	REG(vdac, bt_reg) = 0xff;	tc_wmb();
	REG(vdac, bt_reg) = 0xff;	tc_wmb();

	REG(vdac, bt_reg) = 0;		tc_wmb();
	REG(vdac, bt_reg) = 0;		tc_wmb();
	REG(vdac, bt_reg) = 0;		tc_wmb();

	REG(vdac, bt_reg) = 0xff;	tc_wmb();
	REG(vdac, bt_reg) = 0xff;	tc_wmb();
	REG(vdac, bt_reg) = 0xff;	tc_wmb();
}

static int
get_cmap(sc, p)
	struct sfb_softc *sc;
	struct wsdisplay_cmap *p;
{
	u_int index = p->index, count = p->count;

	if (index >= CMAP_SIZE || count > CMAP_SIZE - index)
		return (EINVAL);

	if (!uvm_useracc(p->red, count, B_WRITE) ||
	    !uvm_useracc(p->green, count, B_WRITE) ||
	    !uvm_useracc(p->blue, count, B_WRITE))
		return (EFAULT);

	copyout(&sc->sc_cmap.r[index], p->red, count);
	copyout(&sc->sc_cmap.g[index], p->green, count);
	copyout(&sc->sc_cmap.b[index], p->blue, count);

	return (0);
}

static int
set_cmap(sc, p)
	struct sfb_softc *sc;
	struct wsdisplay_cmap *p;
{
	u_int index = p->index, count = p->count;

	if (index >= CMAP_SIZE || (index + count) > CMAP_SIZE)
		return (EINVAL);

	if (!uvm_useracc(p->red, count, B_READ) ||
	    !uvm_useracc(p->green, count, B_READ) ||
	    !uvm_useracc(p->blue, count, B_READ))
		return (EFAULT);

	copyin(p->red, &sc->sc_cmap.r[index], count);
	copyin(p->green, &sc->sc_cmap.g[index], count);
	copyin(p->blue, &sc->sc_cmap.b[index], count);
	sc->sc_changed |= WSDISPLAY_CMAP_DOLUT;
	return (0);
}

static int
set_cursor(sc, p)
	struct sfb_softc *sc;
	struct wsdisplay_cursor *p;
{
#define	cc (&sc->sc_cursor)
	u_int v, index, count, icount;

	v = p->which;
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		index = p->cmap.index;
		count = p->cmap.count;
		if (index >= 2 || (index + count) > 2)
			return (EINVAL);
		if (!uvm_useracc(p->cmap.red, count, B_READ) ||
		    !uvm_useracc(p->cmap.green, count, B_READ) ||
		    !uvm_useracc(p->cmap.blue, count, B_READ))
			return (EFAULT);
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		if (p->size.x > CURSOR_MAX_SIZE || p->size.y > CURSOR_MAX_SIZE)
			return (EINVAL);
		icount = ((p->size.x < 33) ? 4 : 8) * p->size.y;
		if (!uvm_useracc(p->image, icount, B_READ) ||
		    !uvm_useracc(p->mask, icount, B_READ))
			return (EFAULT);
	}

	if (v & WSDISPLAY_CURSOR_DOCUR)
		sc->sc_curenb = p->enable;
	if (v & WSDISPLAY_CURSOR_DOPOS)
		set_curpos(sc, &p->pos);
	if (v & WSDISPLAY_CURSOR_DOHOT)
		cc->cc_hot = p->hot;
	if (v & WSDISPLAY_CURSOR_DOCMAP) {
		copyin(p->cmap.red, &cc->cc_color[index], count);
		copyin(p->cmap.green, &cc->cc_color[index + 2], count);
		copyin(p->cmap.blue, &cc->cc_color[index + 4], count);
	}
	if (v & WSDISPLAY_CURSOR_DOSHAPE) {
		cc->cc_size = p->size;
		memset(cc->cc_image, 0, sizeof cc->cc_image);
		copyin(p->image, cc->cc_image, icount);
		copyin(p->mask, cc->cc_image+CURSOR_MAX_SIZE, icount);
	}
	sc->sc_changed |= v;

	return (0);
#undef cc
}

static int
get_cursor(sc, p)
	struct sfb_softc *sc;
	struct wsdisplay_cursor *p;
{

	return (EPASSTHROUGH); /* XXX */
}

static void
set_curpos(sc, curpos)
	struct sfb_softc *sc;
	struct wsdisplay_curpos *curpos;
{
	struct rasops_info *ri = sc->sc_ri;
	int x = curpos->x, y = curpos->y;

	if (y < 0)
		y = 0;
	else if (y > ri->ri_height);
		y = ri->ri_height;
	if (x < 0)
		x = 0;
	else if (x > ri->ri_width);
		x = ri->ri_width;
	sc->sc_cursor.cc_pos.x = x;
	sc->sc_cursor.cc_pos.y = y;
}

#define	MODE_SIMPLE		0
#define	MODE_OPAQUESTIPPLE	1
#define	MODE_OPAQUELINE		2
#define	MODE_TRANSPARENTSTIPPLE	5
#define	MODE_TRANSPARENTLINE	6
#define	MODE_COPY		7

/* parameters for 8bpp configuration */
#define	SFBALIGNMASK		0x7
#define	SFBSTIPPLEALL1		0xffffffff
#define	SFBSTIPPLEBITS		32
#define	SFBSTIPPLEBITMASK	0x1f
#define	SFBSTIPPLEBYTESDONE	32
#define	SFBCOPYALL1		0xffffffff
#define	SFBCOPYBITS		32
#define	SFBCOPYBITMASK		0x1f
#define	SFBCOPYBYTESDONE	32

#if defined(pmax)
#define	WRITE_MB()
#define	BUMP(p) (p)
#endif

#if defined(alpha)
#define	WRITE_MB() tc_wmb()
/* SFB registers replicated in 128B stride; cycle after eight iterations */
#define	BUMP(p) ((p) = (caddr_t)(((long)(p) + 0x80) & ~0x400))
#endif

#define	SFBMODE(p, v) \
		(*(u_int32_t *)(BUMP(p) + SFB_ASIC_MODE) = (v))
#define	SFBROP(p, v) \
		(*(u_int32_t *)(BUMP(p) + SFB_ASIC_ROP) = (v))
#define	SFBPLANEMASK(p, v) \
		(*(u_int32_t *)(BUMP(p) + SFB_ASIC_PLANEMASK) = (v))
#define	SFBPIXELMASK(p, v) \
		(*(u_int32_t *)(BUMP(p) + SFB_ASIC_PIXELMASK) = (v))
#define	SFBADDRESS(p, v) \
		(*(u_int32_t *)(BUMP(p) + SFB_ASIC_ADDRESS) = (v))
#define	SFBSTART(p, v) \
		(*(u_int32_t *)(BUMP(p) + SFB_ASIC_START) = (v))
#define	SFBPIXELSHIFT(p, v) \
		(*(u_int32_t *)(BUMP(p) + SFB_ASIC_PIXELSHIFT) = (v))
#define	SFBFG(p, v) \
		(*(u_int32_t *)(BUMP(p) + SFB_ASIC_FG) = (v))
#define	SFBBG(p, v) \
		(*(u_int32_t *)(BUMP(p) + SFB_ASIC_BG) = (v))

/*
 * Paint the cursor.
 */
static void
sfb_do_cursor(ri)
	struct rasops_info *ri;
{
	caddr_t sfb, p;
	int scanspan, height, width, align, x, y;
	u_int32_t lmask, rmask;

	x = ri->ri_ccol * ri->ri_font->fontwidth;
	y = ri->ri_crow * ri->ri_font->fontheight;
	scanspan = ri->ri_stride;
	height = ri->ri_font->fontheight;

	p = ri->ri_bits + y * scanspan + x;
	align = (long)p & SFBALIGNMASK;
	p -= align;
	width = ri->ri_font->fontwidth + align;
	lmask = SFBSTIPPLEALL1 << align;
	rmask = SFBSTIPPLEALL1 >> (-width & SFBSTIPPLEBITMASK);
	sfb = (caddr_t)ri->ri_hw + SFB_ASIC_OFFSET;

	SFBMODE(sfb, MODE_TRANSPARENTSTIPPLE);
	SFBPLANEMASK(sfb, ~0);
	SFBROP(sfb, 6);  /* ROP_XOR */
	SFBFG(sfb, ~0);

	lmask = lmask & rmask;
	while (height > 0) {
		SFBADDRESS(sfb, (long)p);
		SFBSTART(sfb, lmask);
		p += scanspan;
		height--;
	}
	SFBMODE(sfb, MODE_SIMPLE);
	SFBROP(sfb, 3); /* ROP_COPY */
}

/*
 * Paint a character.
 */
static void
sfb_putchar(id, row, col, uc, attr)
	void *id;
	int row, col;
	u_int uc;
	long attr;
{
	struct rasops_info *ri = id;
	caddr_t sfb, p;
	int scanspan, height, width, align, x, y;
	u_int32_t lmask, rmask, glyph;
	u_int8_t *g;

	x = col * ri->ri_font->fontwidth;
	y = row * ri->ri_font->fontheight;
	scanspan = ri->ri_stride;
	height = ri->ri_font->fontheight;
	uc -= ri->ri_font->firstchar;
	g = (u_char *)ri->ri_font->data + uc * ri->ri_fontscale;

	p = ri->ri_bits + y * scanspan + x;
	align = (long)p & SFBALIGNMASK;
	p -= align;
	width = ri->ri_font->fontwidth + align;
	lmask = SFBSTIPPLEALL1 << align;
	rmask = SFBSTIPPLEALL1 >> (-width & SFBSTIPPLEBITMASK);
	sfb = (caddr_t)ri->ri_hw + SFB_ASIC_OFFSET;

	SFBMODE(sfb, MODE_OPAQUESTIPPLE);
	SFBPLANEMASK(sfb, ~0);
	SFBFG(sfb, ri->ri_devcmap[(attr >> 24) & 15]);
	SFBBG(sfb, ri->ri_devcmap[(attr >> 16) & 15]);

	/* XXX 2B stride fonts only XXX */
	lmask = lmask & rmask;
	while (height > 0) {
		glyph = *(u_int16_t *)g;		/* XXX */
		SFBPIXELMASK(sfb, lmask);
		SFBADDRESS(sfb, (long)p);
		SFBSTART(sfb, glyph << align);
		p += scanspan;
		g += 2;					/* XXX */
		height--;
	}
	if (attr & 1 /* UNDERLINE */) {
		p -= scanspan * 2;
		SFBMODE(sfb, MODE_TRANSPARENTSTIPPLE);
		SFBADDRESS(sfb, (long)p);
		SFBSTART(sfb, lmask);
	}

	SFBMODE(sfb, MODE_SIMPLE);
	SFBPIXELMASK(sfb, ~0);		/* entire pixel */
}

#if 0
/*
 * Copy characters in a line.
 */
static void
sfb_copycols(id, row, srccol, dstcol, ncols)
	void *id;
	int row, srccol, dstcol, ncols;
{
	struct rasops_info *ri = id;
	caddr_t sp, dp, basex, sfb;
	int scanspan, height, width, aligns, alignd, shift, w, y;
	u_int32_t lmaskd, rmaskd;

	scanspan = ri->ri_stride;
	y = row * ri->ri_font->fontheight;
	basex = ri->ri_bits + y * scanspan;
	height = ri->ri_font->fontheight;
	w = ri->ri_font->fontwidth * ncols;

	sp = basex + ri->ri_font->fontwidth * srccol;
	aligns = (long)sp & SFBALIGNMASK;
	dp = basex + ri->ri_font->fontwidth * dstcol;
	alignd = (long)dp & SFBALIGNMASK;
	sfb = (caddr_t)ri->ri_hw + SFB_ASIC_OFFSET;

	SFBMODE(sfb, MODE_COPY);
	SFBPLANEMASK(sfb, ~0);
	/* small enough to fit in a single 32bit */
	if ((aligns + w) <= SFBCOPYBITS && (alignd + w) <= SFBCOPYBITS) {
		SFBPIXELSHIFT(sfb, alignd - aligns);
		lmaskd = SFBCOPYALL1 << alignd;
		rmaskd = SFBCOPYALL1 >> (-(alignd + w) & SFBCOPYBITMASK);
		lmaskd = lmaskd & rmaskd;
		sp -= aligns;
		dp -= alignd;
		while (height > 0) {
			*(u_int32_t *)sp = SFBCOPYALL1;	WRITE_MB();
			*(u_int32_t *)dp = lmaskd;	WRITE_MB();
			sp += scanspan;
			dp += scanspan;
			height--;
		}
	}
	/* copy forward (left-to-right) */
	else if (dstcol < srccol || srccol + ncols < dstcol) {
		caddr_t sq, dq;

		shift = alignd - aligns;
		if (shift < 0) {
			shift = 8 + shift;	/* enforce right rotate */
			alignd += 8;		/* bearing on left edge */
		}
		width = alignd + w;
		lmaskd = SFBCOPYALL1 << alignd;
		rmaskd = SFBCOPYALL1 >> (-width & SFBCOPYBITMASK);
		sp -= aligns;
		dp -= alignd;

		SFBPIXELSHIFT(sfb, shift);
		w = width;
		sq = sp;
		dq = dp;
		while (height > 0) {
			*(u_int32_t *)sp = SFBCOPYALL1;	WRITE_MB();
			*(u_int32_t *)dp = lmaskd;	WRITE_MB();
			width -= 2 * SFBCOPYBITS;
			while (width > 0) {
				sp += SFBCOPYBYTESDONE;
				dp += SFBCOPYBYTESDONE;
				*(u_int32_t *)sp = SFBCOPYALL1; WRITE_MB();
				*(u_int32_t *)dp = SFBCOPYALL1; WRITE_MB();
				width -= SFBCOPYBITS;
			}
			sp += SFBCOPYBYTESDONE;
			dp += SFBCOPYBYTESDONE;
			*(u_int32_t *)sp = SFBCOPYALL1;	WRITE_MB();
			*(u_int32_t *)dp = rmaskd;	WRITE_MB();
			sp = (sq += scanspan);
			dp = (dq += scanspan);
			width = w;
			height--;
		}
	}
	/* copy backward (right-to-left) */
	else {
		caddr_t sq, dq;

		shift = alignd - aligns;
		if (shift > 0) {
			shift = shift - 8;	/* force left rotate */
			alignd += 24;
		}
		width = alignd + w;
		lmaskd = SFBCOPYALL1 << alignd;
		rmaskd = SFBCOPYALL1 >> (-width & SFBCOPYBITMASK);
		sp -= aligns;
		dp -= alignd;

		SFBPIXELSHIFT(sfb, shift);
		w = width;
		sq = sp += (((aligns + w) - 1) & ~31);
		dq = dp += (((alignd + w) - 1) & ~31);
		while (height > 0) {
			*(u_int32_t *)sp = SFBCOPYALL1; WRITE_MB();
			*(u_int32_t *)dp = rmaskd;	WRITE_MB();
			width -= 2 * SFBCOPYBITS;
			while (width > 0) {
				sp -= SFBCOPYBYTESDONE;
				dp -= SFBCOPYBYTESDONE;
				*(u_int32_t *)sp = SFBCOPYALL1; WRITE_MB();
				*(u_int32_t *)dp = SFBCOPYALL1; WRITE_MB();
				width -= SFBCOPYBITS;
			}
			sp -= SFBCOPYBYTESDONE;
			dp -= SFBCOPYBYTESDONE;
			*(u_int32_t *)sp = SFBCOPYALL1;	WRITE_MB();
			*(u_int32_t *)dp = lmaskd;	WRITE_MB();

			sp = (sq += scanspan);
			dp = (dq += scanspan);
			width = w;
			height--;
		}
	}
	SFBMODE(sfb, MODE_SIMPLE);
	SFBPIXELSHIFT(sfb, 0);
}
#endif

/*
 * Clear characters in a line.
 */
static void
sfb_erasecols(id, row, startcol, ncols, attr)
	void *id;
	int row, startcol, ncols;
	long attr;
{
	struct rasops_info *ri = id;
	caddr_t sfb, p;
	int scanspan, startx, height, width, align, w, y;
	u_int32_t lmask, rmask;

	scanspan = ri->ri_stride;
	y = row * ri->ri_font->fontheight;
	startx = startcol * ri->ri_font->fontwidth;
	height = ri->ri_font->fontheight;
	w = ri->ri_font->fontwidth * ncols;

	p = ri->ri_bits + y * scanspan + startx;
	align = (long)p & SFBALIGNMASK;
	p -= align;
	width = w + align;
	lmask = SFBSTIPPLEALL1 << align;
	rmask = SFBSTIPPLEALL1 >> (-width & SFBSTIPPLEBITMASK);
	sfb = (caddr_t)ri->ri_hw + SFB_ASIC_OFFSET;

	SFBMODE(sfb, MODE_TRANSPARENTSTIPPLE);
	SFBPLANEMASK(sfb, ~0);
	SFBFG(sfb, ri->ri_devcmap[(attr >> 16) & 15]); /* fill with bg */
	if (width <= SFBSTIPPLEBITS) {
		lmask = lmask & rmask;
		while (height > 0) {
			SFBADDRESS(sfb, (long)p);
			SFBSTART(sfb, lmask);
			p += scanspan;
			height--;
		}
	}
	else {
		caddr_t q = p;
		while (height > 0) {
			*(u_int32_t *)p = lmask;
			WRITE_MB();
			width -= 2 * SFBSTIPPLEBITS;
			while (width > 0) {
				p += SFBSTIPPLEBYTESDONE;
				*(u_int32_t *)p = SFBSTIPPLEALL1;
				WRITE_MB();
				width -= SFBSTIPPLEBITS;
			}
			p += SFBSTIPPLEBYTESDONE;
			*(u_int32_t *)p = rmask;
			WRITE_MB();

			p = (q += scanspan);
			width = w + align;
			height--;
		}
	}
	SFBMODE(sfb, MODE_SIMPLE);
}

/*
 * Copy lines.
 */
static void
sfb_copyrows(id, srcrow, dstrow, nrows)
	void *id;
	int srcrow, dstrow, nrows;
{
	struct rasops_info *ri = id;
	caddr_t sfb, p;
	int scanspan, offset, srcy, height, width, align, w;
	u_int32_t lmask, rmask;

	scanspan = ri->ri_stride;
	height = ri->ri_font->fontheight * nrows;
	offset = (dstrow - srcrow) * ri->ri_yscale;
	srcy = ri->ri_font->fontheight * srcrow;
	if (srcrow < dstrow && srcrow + nrows > dstrow) {
		scanspan = -scanspan;
		srcy += height;
	}

	p = ri->ri_bits + srcy * ri->ri_stride;
	align = (long)p & SFBALIGNMASK;
	p -= align;
	w = ri->ri_emuwidth;
	width = w + align;
	lmask = SFBCOPYALL1 << align;
	rmask = SFBCOPYALL1 >> (-width & SFBCOPYBITMASK);
	sfb = (caddr_t)ri->ri_hw + SFB_ASIC_OFFSET;

	SFBMODE(sfb, MODE_COPY);
	SFBPLANEMASK(sfb, ~0);
	SFBPIXELSHIFT(sfb, 0);
	if (width <= SFBCOPYBITS) {
		/* never happens */;
	}
	else {
		caddr_t q = p;
		while (height > 0) {
			*(u_int32_t *)p = lmask;
			*(u_int32_t *)(p + offset) = lmask;
			width -= 2 * SFBCOPYBITS;
			while (width > 0) {
				p += SFBCOPYBYTESDONE;
				*(u_int32_t *)p = SFBCOPYALL1;
				*(u_int32_t *)(p + offset) = SFBCOPYALL1;
				width -= SFBCOPYBITS;
			}
			p += SFBCOPYBYTESDONE;
			*(u_int32_t *)p = rmask;
			*(u_int32_t *)(p + offset) = rmask;

			p = (q += scanspan);
			width = w + align;
			height--;
		}
	}
	SFBMODE(sfb, MODE_SIMPLE);
}

/*
 * Erase lines.
 */
void
sfb_eraserows(id, startrow, nrows, attr)
	void *id;
	int startrow, nrows;
	long attr;
{
	struct rasops_info *ri = id;
	caddr_t sfb, p;
	int scanspan, starty, height, width, align, w;
	u_int32_t lmask, rmask;

	scanspan = ri->ri_stride;
	starty = ri->ri_font->fontheight * startrow;
	height = ri->ri_font->fontheight * nrows;

	p = ri->ri_bits + starty * scanspan;
	align = (long)p & SFBALIGNMASK;
	p -= align;
	w = ri->ri_emuwidth;
	width = w + align;
	lmask = SFBSTIPPLEALL1 << align;
	rmask = SFBSTIPPLEALL1 >> (-width & SFBSTIPPLEBITMASK);
	sfb = (caddr_t)ri->ri_hw + SFB_ASIC_OFFSET;

	SFBMODE(sfb, MODE_TRANSPARENTSTIPPLE);
	SFBPLANEMASK(sfb, ~0);
	SFBFG(sfb, ri->ri_devcmap[(attr >> 16) & 15]); /* fill with bg */
	if (width <= SFBSTIPPLEBITS) {
		/* never happens */;
	}
	else {
		caddr_t q = p;
		while (height > 0) {
			*(u_int32_t *)p = lmask;
			WRITE_MB();
			width -= 2 * SFBSTIPPLEBITS;
			while (width > 0) {
				p += SFBSTIPPLEBYTESDONE;
				*(u_int32_t *)p = SFBSTIPPLEALL1;
				WRITE_MB();
				width -= SFBSTIPPLEBITS;
			}
			p += SFBSTIPPLEBYTESDONE;
			*(u_int32_t *)p = rmask;
			WRITE_MB();

			p = (q += scanspan);
			width = w + align;
			height--;
		}
	}
	SFBMODE(sfb, MODE_SIMPLE);
}
