/*	$NetBSD: vr4122ip.c,v 1.2 2002/02/11 11:44:36 takemura Exp $	*/

/*-
 * Copyright (c) 2002 TAKEMURA Shin
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
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include "opt_vr41xx.h"
#include <hpcmips/vr/vrcpudef.h>
#include <hpcmips/vr/vripunit.h>
#include <hpcmips/vr/vripreg.h>
#include <hpcmips/vr/vripvar.h>
#include <hpcmips/vr/icureg.h>
#include <hpcmips/vr/cmureg.h>

void	vr4122ipattach(struct device *, struct device *, void *);

struct cfattach vr4122ip_ca = {
	sizeof(struct vrip_softc), vripmatch, vr4122ipattach
};

static const struct vrip_unit vr4122ip_units[] = {
	[VRIP_UNIT_PMU] = { "pmu",
			    { VRIP_INTR_POWER,	VRIP_INTR_BAT,	},	},
	[VRIP_UNIT_RTC] = { "rtc",
			    { VRIP_INTR_RTCL1,	},		},
	[VRIP_UNIT_SIU] = { "siu",
			    { VRIP_INTR_SIU,	},		},
	[VRIP_UNIT_GIU] = { "giu",
			    { VRIP_INTR_GIU,	},
			    0,
			    VR4122_GIUINT_L_REG_W,VR4122_MGIUINT_L_REG_W,
			    VR4122_GIUINT_H_REG_W,VR4122_MGIUINT_H_REG_W},
	[VRIP_UNIT_LED] = { "led",
			    { VRIP_INTR_LED,	},		},
	[VRIP_UNIT_FIR] = { "fir",
			    { VRIP_INTR_FIR,	},
			    VR4122_CMUMSKFIR,
			    VR4122_FIRINT_REG_W,VR4122_MFIRINT_REG_W	},
	[VRIP_UNIT_DSIU]= { "dsiu",
			    { VRIP_INTR_DSIU,	},
			    VR4122_CMUMSKDSIU,
			    VR4122_DSIUINT_REG_W,VR4122_MDSIUINT_REG_W	},
	[VRIP_UNIT_PCIU]= { "pciu",
			    { VRIP_INTR_PCI,	},
			    VR4122_CMUMSKPCIU,
			    VR4122_PCIINT_REG_W,VR4122_MPCIINT_REG_W	},
	[VRIP_UNIT_SCU] = { "scu",
			    { VRIP_INTR_SCU,	},
			    0,
			    VR4122_SCUINT_REG_W,VR4122_MSCUINT_REG_W	},
	[VRIP_UNIT_CSI] = { "csi",
			    { VRIP_INTR_CSI,	},
			    VR4122_CMUMSKCSI,
			    VR4122_CSIINT_REG_W,VR4122_MCSIINT_REG_W	},
	[VRIP_UNIT_BCU] = { "bcu",
			    { VRIP_INTR_BCU,	},
			    0,
			    VR4122_BCUINT_REG_W,VR4122_MBCUINT_REG_W	}
};

void
vr4122ipattach(struct device *parent, struct device *self, void *aux)
{
	struct vrip_softc *sc = (struct vrip_softc*)self;

	printf("\n");

	sc->sc_units = vr4122ip_units;
	sc->sc_nunits = sizeof(vr4122ip_units)/sizeof(struct vrip_unit);
	sc->sc_icu_addr = VR4122_ICU_ADDR;
	sc->sc_sysint2 = VR4122_SYSINT2_REG_W;
	sc->sc_msysint2 = VR4122_MSYSINT2_REG_W;

	vripattach_common(parent, self, aux);
}
