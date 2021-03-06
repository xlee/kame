/*	$OpenBSD: clock.c,v 1.3 1996/04/28 11:06:02 deraadt Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Lawrence Berkeley Laboratory.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *      @(#)clock.c     8.1 (Berkeley) 6/11/93
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/psl.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>

#include "pcc.h"
#include "mc.h"
#include "pcctwo.h"

#if NPCC > 0
#include <mvme68k/dev/pccreg.h>
#endif
#if NPCCTWO > 0
#include <mvme68k/dev/pcctworeg.h>
#endif
#if NMC > 0
#include <mvme68k/dev/mcreg.h>
#endif

#if defined(GPROF)
#include <sys/gmon.h>
#endif

/*
 * Statistics clock interval and variance, in usec.  Variance must be a
 * power of two.  Since this gives us an even number, not an odd number,
 * we discard one case and compensate.  That is, a variance of 8192 would
 * give us offsets in [0..8191].  Instead, we take offsets in [1..8191].
 * This is symmetric about the point 2048, or statvar/2, and thus averages
 * to that value (assuming uniform random numbers).
 */
int statvar = 8192;
int statmin;		/* statclock interval - 1/2*variance */

struct clocksoftc {
	struct device	sc_dev;
	struct intrhand sc_profih;
	struct intrhand sc_statih;
};

void	clockattach __P((struct device *, struct device *, void *));
int	clockmatch __P((struct device *, void *, void *));

struct cfattach clock_ca = {
	sizeof(struct clocksoftc), clockmatch, clockattach
};

struct cfdriver clock_cd = {
	NULL, "clock", DV_DULL, 0
};

int	clockintr __P((void *));
int	statintr __P((void *));

int	clockbus;
u_char	stat_reset, prof_reset;

/*
 * Every machine must have a clock tick device of some sort; for this
 * platform this file manages it, no matter what form it takes.
 */
int
clockmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	return (1);
}

void
clockattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct clocksoftc *sc = (struct clocksoftc *)self;

	sc->sc_profih.ih_fn = clockintr;
	sc->sc_profih.ih_arg = 0;
	sc->sc_profih.ih_wantframe = 1;
	sc->sc_profih.ih_ipl = ca->ca_ipl;

	sc->sc_statih.ih_fn = statintr;
	sc->sc_statih.ih_arg = 0;
	sc->sc_statih.ih_wantframe = 1;
	sc->sc_statih.ih_ipl = ca->ca_ipl;

	clockbus = ca->ca_bustype;
	switch (ca->ca_bustype) {
#if NPCC > 0
	case BUS_PCC:
		prof_reset = ca->ca_ipl | PCC_IRQ_IEN | PCC_TIMERACK;
		stat_reset = ca->ca_ipl | PCC_IRQ_IEN | PCC_TIMERACK;
		pccintr_establish(PCCV_TIMER1, &sc->sc_profih);
		pccintr_establish(PCCV_TIMER2, &sc->sc_statih);
		break;
#endif
#if NMC > 0
	case BUS_MC:
		prof_reset = ca->ca_ipl | MC_IRQ_IEN | MC_IRQ_ICLR;
		stat_reset = ca->ca_ipl | MC_IRQ_IEN | MC_IRQ_ICLR;
		mcintr_establish(MCV_TIMER1, &sc->sc_profih);
		mcintr_establish(MCV_TIMER2, &sc->sc_statih);
		break;
#endif
#if NPCCTWO > 0
	case BUS_PCCTWO:
		prof_reset = ca->ca_ipl | PCC2_IRQ_IEN | PCC2_IRQ_ICLR;
		stat_reset = ca->ca_ipl | PCC2_IRQ_IEN | PCC2_IRQ_ICLR;
		pcctwointr_establish(PCC2V_TIMER1, &sc->sc_profih);
		pcctwointr_establish(PCC2V_TIMER2, &sc->sc_statih);
		break;
#endif
	}

	printf("\n");
}

/*
 * clockintr: ack intr and call hardclock
 */
int
clockintr(arg)
	void *arg;
{
	switch (clockbus) {
#if NPCC > 0
	case BUS_PCC:
		sys_pcc->pcc_t1irq = prof_reset;
		break;
#endif
#if NMC > 0
	case BUS_MC:
		sys_mc->mc_t1irq = prof_reset;
		break;
#endif
#if NPCCTWO > 0
	case BUS_PCCTWO:
		sys_pcc2->pcc2_t1irq = prof_reset;
		break;
#endif
	}
	hardclock(arg);
	return (1);
}

/*
 * Set up real-time clock; we don't have a statistics clock at
 * present.
 */
cpu_initclocks()
{
	register int statint, minint;

	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
		tick = 1000000 / hz;
	}
	if (stathz == 0)
		stathz = hz;
	if (1000000 % stathz) {
		printf("cannot get %d Hz statclock; using 100 Hz\n", stathz);
		stathz = 100;
	}
	profhz = stathz;		/* always */

	statint = 1000000 / stathz;
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;
	switch (clockbus) {
#if NPCC > 0
	case BUS_PCC:
		sys_pcc->pcc_t1pload = pcc_timer_us2lim(tick);
		sys_pcc->pcc_t1ctl = PCC_TIMERCLEAR;
		sys_pcc->pcc_t1ctl = PCC_TIMERSTART;
		sys_pcc->pcc_t1irq = prof_reset;

		sys_pcc->pcc_t2pload = pcc_timer_us2lim(statint);
		sys_pcc->pcc_t2ctl = PCC_TIMERCLEAR;
		sys_pcc->pcc_t2ctl = PCC_TIMERSTART;
		sys_pcc->pcc_t2irq = stat_reset;
		break;
#endif
#if NMC > 0
	case BUS_MC:
		/* profclock */
		sys_mc->mc_t1ctl = 0;
		sys_mc->mc_t1cmp = mc_timer_us2lim(tick);
		sys_mc->mc_t1count = 0;
		sys_mc->mc_t1ctl = MC_TCTL_CEN | MC_TCTL_COC | MC_TCTL_COVF;
		sys_mc->mc_t1irq = prof_reset;

		/* statclock */
		sys_mc->mc_t2ctl = 0;
		sys_mc->mc_t2cmp = mc_timer_us2lim(statint);
		sys_mc->mc_t2count = 0;
		sys_mc->mc_t2ctl = MC_TCTL_CEN | MC_TCTL_COC | MC_TCTL_COVF;
		sys_mc->mc_t2irq = stat_reset;
		break;
#endif
#if NPCCTWO > 0
	case BUS_PCCTWO:
		/* profclock */
		sys_pcc2->pcc2_t1ctl = 0;
		sys_pcc2->pcc2_t1cmp = pcc2_timer_us2lim(tick);
		sys_pcc2->pcc2_t1count = 0;
		sys_pcc2->pcc2_t1ctl = PCC2_TCTL_CEN | PCC2_TCTL_COC |
		    PCC2_TCTL_COVF;
		sys_pcc2->pcc2_t1irq = prof_reset;

		/* statclock */
		sys_pcc2->pcc2_t2ctl = 0;
		sys_pcc2->pcc2_t2cmp = pcc2_timer_us2lim(statint);
		sys_pcc2->pcc2_t2count = 0;
		sys_pcc2->pcc2_t2ctl = PCC2_TCTL_CEN | PCC2_TCTL_COC |
		    PCC2_TCTL_COVF;
		sys_pcc2->pcc2_t2irq = stat_reset;
		break;
#endif
	}
	statmin = statint - (statvar >> 1);
}

void
setstatclockrate(newhz)
	int newhz;
{
}

int
statintr(cap)
	void *cap;
{
	register u_long newint, r, var;

	switch (clockbus) {
#if NPCC > 0
	case BUS_PCC:
		sys_pcc->pcc_t2irq = stat_reset;
		break;
#endif
#if NMC > 0
	case BUS_MC:
		sys_mc->mc_t2irq = stat_reset;
		break;
#endif
#if NPCCTWO > 0
	case BUS_PCCTWO:
		sys_pcc2->pcc2_t2irq = stat_reset;
		break;
#endif
	}

	statclock((struct clockframe *)cap);

	/*
	 * Compute new randomized interval.  The intervals are uniformly
	 * distributed on [statint - statvar / 2, statint + statvar / 2],
	 * and therefore have mean statint, giving a stathz frequency clock.
	 */
	var = statvar;
	do {
		r = random() & (var - 1);
	} while (r == 0);
	newint = statmin + r;

	switch (clockbus) {
#if NPCC > 0
	case BUS_PCC:
		sys_pcc->pcc_t2pload = pcc_timer_us2lim(newint);
		sys_pcc->pcc_t2ctl = PCC_TIMERCLEAR;
		sys_pcc->pcc_t2ctl = PCC_TIMERSTART;
		sys_pcc->pcc_t2irq = stat_reset;
		break;
#endif
#if NMC > 0
	case BUS_MC:
		sys_mc->mc_t2ctl = 0;
		sys_mc->mc_t2cmp = mc_timer_us2lim(newint);
		sys_mc->mc_t2count = 0;		/* should I? */
		sys_mc->mc_t2irq = stat_reset;
		sys_mc->mc_t2ctl = MC_TCTL_CEN | MC_TCTL_COC;
		break;
#endif
#if NPCCTWO > 0
	case BUS_PCCTWO:
		sys_pcc2->pcc2_t2ctl = 0;
		sys_pcc2->pcc2_t2cmp = pcc2_timer_us2lim(newint);
		sys_pcc2->pcc2_t2count = 0;		/* should I? */
		sys_pcc2->pcc2_t2irq = stat_reset;
		sys_pcc2->pcc2_t2ctl = PCC2_TCTL_CEN | PCC2_TCTL_COC;
		break;
#endif
	}
	return (1);
}

delay(us)
	register int us;
{
	volatile register int c;

	switch (clockbus) {
#if NPCC > 0
	case BUS_PCC:
		/*
		 * XXX MVME147 doesn't have a 3rd free-running timer,
		 * so we use a stupid loop. Fix the code to watch t1:
		 * the profiling timer.
		 */
		c = 2 * us;
		while (--c > 0)
			;
		return (0);
#endif
#if NMC > 0
	case BUS_MC:
		/*
		 * Reset and restart a free-running timer 1MHz, watch
		 * for it to reach the required count.
		 */
		sys_mc->mc_t3irq = 0;
		sys_mc->mc_t3ctl = 0;
		sys_mc->mc_t3count = 0;
		sys_mc->mc_t3ctl = MC_TCTL_CEN | MC_TCTL_COVF;

		while (sys_mc->mc_t3count < us)
			;
		return (0);
#endif
#if NPCCTWO > 0
	case BUS_PCCTWO:
		/*
		 * XXX MVME167 doesn't have a 3rd free-running timer,
		 * so we use a stupid loop. Fix the code to watch t1:
		 * the profiling timer.
		 */
		c = 4 * us;
		while (--c > 0)
			;
		return (0);
#endif
	}
}
