/*	$NetBSD: ess.c,v 1.44.2.1 1999/06/22 14:20:24 perry Exp $	*/

/*
 * Copyright 1997
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

/*
**++
**
**  ess.c
**
**  FACILITY:
**
**	DIGITAL Network Appliance Reference Design (DNARD)
**
**  MODULE DESCRIPTION:
**
**      This module contains the device driver for the ESS
**      Technologies 1888/1887/888 sound chip. The code in sbdsp.c was
**	used as a reference point when implementing this driver.
**
**  AUTHORS:
**
**	Blair Fidler	Software Engineering Australia
**			Gold Coast, Australia.
**
**  CREATION DATE:  
**
**	March 10, 1997.
**
**  MODIFICATION HISTORY:
**
**	Heavily modified by Lennart Augustsson and Charles M. Hannum for
**	bus_dma, changes to audio interface, and many bug fixes.
**	ESS1788 support by Nathan J. Williams and Charles M. Hannum.
**--
*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/kernel.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/auconv.h>
#include <dev/mulaw.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isa/essvar.h>
#include <dev/isa/essreg.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (essdebug) printf x
#define DPRINTFN(n,x)	if (essdebug>(n)) printf x
int	essdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#if 0
unsigned uuu;
#define EREAD1(t, h, a) (uuu=bus_space_read_1(t, h, a),printf("EREAD  %02x=%02x\n", ((int)h&0xfff)+a, uuu),uuu)
#define EWRITE1(t, h, a, d) (printf("EWRITE %02x=%02x\n", ((int)h & 0xfff)+a, d), bus_space_write_1(t, h, a, d))
#else
#define EREAD1(t, h, a) bus_space_read_1(t, h, a)
#define EWRITE1(t, h, a, d) bus_space_write_1(t, h, a, d)
#endif


int	ess_setup_sc __P((struct ess_softc *, int));

int	ess_open __P((void *, int));
void	ess_1788_close __P((void *));
void	ess_1888_close __P((void *));
int	ess_getdev __P((void *, struct audio_device *));
int	ess_drain __P((void *));
	
int	ess_query_encoding __P((void *, struct audio_encoding *));

int	ess_set_params __P((void *, int, int, struct audio_params *, 
	    struct audio_params *));

int	ess_round_blocksize __P((void *, int));

int	ess_audio1_trigger_output __P((void *, void *, void *, int,
	    void (*)(void *), void *, struct audio_params *));
int	ess_audio2_trigger_output __P((void *, void *, void *, int,
	    void (*)(void *), void *, struct audio_params *));
int	ess_audio1_trigger_input __P((void *, void *, void *, int,
	    void (*)(void *), void *, struct audio_params *));
int	ess_audio1_halt __P((void *));
int	ess_audio2_halt __P((void *));
int	ess_audio1_intr __P((void *));
int	ess_audio2_intr __P((void *));
void	ess_audio1_poll __P((void *));
void	ess_audio2_poll __P((void *));

int	ess_speaker_ctl __P((void *, int));

int	ess_getdev __P((void *, struct audio_device *));
	
int	ess_set_port __P((void *, mixer_ctrl_t *));
int	ess_get_port __P((void *, mixer_ctrl_t *));

void   *ess_malloc __P((void *, int, size_t, int, int));
void	ess_free __P((void *, void *, int));
size_t	ess_round_buffersize __P((void *, int, size_t));
int	ess_mappage __P((void *, void *, int, int));


int	ess_query_devinfo __P((void *, mixer_devinfo_t *));
int	ess_1788_get_props __P((void *));
int	ess_1888_get_props __P((void *));

void	ess_speaker_on __P((struct ess_softc *));
void	ess_speaker_off __P((struct ess_softc *));

int	ess_config_addr __P((struct ess_softc *));
void	ess_config_irq __P((struct ess_softc *));
void	ess_config_drq __P((struct ess_softc *));
void	ess_setup __P((struct ess_softc *));
int	ess_identify __P((struct ess_softc *));

int	ess_reset __P((struct ess_softc *));
void	ess_set_gain __P((struct ess_softc *, int, int));
int	ess_set_in_port __P((struct ess_softc *, int));
int	ess_set_in_ports __P((struct ess_softc *, int));
u_int	ess_srtotc __P((u_int));
u_int	ess_srtofc __P((u_int));
u_char	ess_get_dsp_status __P((struct ess_softc *));
u_char	ess_dsp_read_ready __P((struct ess_softc *));
u_char	ess_dsp_write_ready __P((struct ess_softc *sc));
int	ess_rdsp __P((struct ess_softc *));
int	ess_wdsp __P((struct ess_softc *, u_char));
u_char	ess_read_x_reg __P((struct ess_softc *, u_char));
int	ess_write_x_reg __P((struct ess_softc *, u_char, u_char));
void	ess_clear_xreg_bits __P((struct ess_softc *, u_char, u_char));
void	ess_set_xreg_bits __P((struct ess_softc *, u_char, u_char));
u_char	ess_read_mix_reg __P((struct ess_softc *, u_char));
void	ess_write_mix_reg __P((struct ess_softc *, u_char, u_char));
void	ess_clear_mreg_bits __P((struct ess_softc *, u_char, u_char));
void	ess_set_mreg_bits __P((struct ess_softc *, u_char, u_char));

static char *essmodel[] = {
	"unsupported",
	"1888",
	"1887",
	"888",
	"1788"
};

struct audio_device ess_device = {
	"ESS Technology",
	"x",
	"ess"
};

/*
 * Define our interface to the higher level audio driver.
 */

struct audio_hw_if ess_1788_hw_if = {
	ess_open,
	ess_1788_close,
	ess_drain,
	ess_query_encoding,
	ess_set_params,
	ess_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	ess_audio1_halt,
	ess_audio1_halt,
	ess_speaker_ctl,
	ess_getdev,
	NULL,
	ess_set_port,
	ess_get_port,
	ess_query_devinfo,
	ess_malloc,
	ess_free,
	ess_round_buffersize,
	ess_mappage,
	ess_1788_get_props,
	ess_audio1_trigger_output,
	ess_audio1_trigger_input,
};

struct audio_hw_if ess_1888_hw_if = {
	ess_open,
	ess_1888_close,
	ess_drain,
	ess_query_encoding,
	ess_set_params,
	ess_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	ess_audio2_halt,
	ess_audio1_halt,
	ess_speaker_ctl,
	ess_getdev,
	NULL,
	ess_set_port,
	ess_get_port,
	ess_query_devinfo,
	ess_malloc,
	ess_free,
	ess_round_buffersize,
	ess_mappage,
	ess_1888_get_props,
	ess_audio2_trigger_output,
	ess_audio1_trigger_input,
};

#ifdef AUDIO_DEBUG
void ess_printsc __P((struct ess_softc *));
void ess_dump_mixer __P((struct ess_softc *));

void
ess_printsc(sc)
	struct ess_softc *sc;
{
	int i;
    
	printf("open %d iobase 0x%x outport %u inport %u speaker %s\n",
	       (int)sc->sc_open, sc->sc_iobase, sc->out_port,
	       sc->in_port, sc->spkr_state ? "on" : "off");

	printf("audio1: dmachan %d irq %d nintr %lu intr %p arg %p\n",
	       sc->sc_audio1.drq, sc->sc_audio1.irq, sc->sc_audio1.nintr,
	       sc->sc_audio1.intr, sc->sc_audio1.arg);

	if (sc->sc_model != ESS_1788) {
		printf("audio2: dmachan %d irq %d nintr %lu intr %p arg %p\n",
		       sc->sc_audio2.drq, sc->sc_audio2.irq, sc->sc_audio2.nintr,
		       sc->sc_audio2.intr, sc->sc_audio2.arg);
	}

	printf("gain:");
	for (i = 0; i < sc->ndevs; i++)
		printf(" %u,%u", sc->gain[i][ESS_LEFT], sc->gain[i][ESS_RIGHT]);
	printf("\n");
}

void
ess_dump_mixer(sc)
	struct ess_softc *sc;
{
	printf("ESS_DAC_PLAY_VOL: mix reg 0x%02x=0x%02x\n",
	       0x7C, ess_read_mix_reg(sc, 0x7C));
	printf("ESS_MIC_PLAY_VOL: mix reg 0x%02x=0x%02x\n",
	       0x1A, ess_read_mix_reg(sc, 0x1A));
	printf("ESS_LINE_PLAY_VOL: mix reg 0x%02x=0x%02x\n",
	       0x3E, ess_read_mix_reg(sc, 0x3E));
	printf("ESS_SYNTH_PLAY_VOL: mix reg 0x%02x=0x%02x\n",
	       0x36, ess_read_mix_reg(sc, 0x36));
	printf("ESS_CD_PLAY_VOL: mix reg 0x%02x=0x%02x\n",
	       0x38, ess_read_mix_reg(sc, 0x38));
	printf("ESS_AUXB_PLAY_VOL: mix reg 0x%02x=0x%02x\n",
	       0x3A, ess_read_mix_reg(sc, 0x3A));
	printf("ESS_MASTER_VOL: mix reg 0x%02x=0x%02x\n",
	       0x32, ess_read_mix_reg(sc, 0x32));
	printf("ESS_PCSPEAKER_VOL: mix reg 0x%02x=0x%02x\n",
	       0x3C, ess_read_mix_reg(sc, 0x3C));
	printf("ESS_DAC_REC_VOL: mix reg 0x%02x=0x%02x\n",
	       0x69, ess_read_mix_reg(sc, 0x69));
	printf("ESS_MIC_REC_VOL: mix reg 0x%02x=0x%02x\n",
	       0x68, ess_read_mix_reg(sc, 0x68));
	printf("ESS_LINE_REC_VOL: mix reg 0x%02x=0x%02x\n",
	       0x6E, ess_read_mix_reg(sc, 0x6E));
	printf("ESS_SYNTH_REC_VOL: mix reg 0x%02x=0x%02x\n",
	       0x6B, ess_read_mix_reg(sc, 0x6B));
	printf("ESS_CD_REC_VOL: mix reg 0x%02x=0x%02x\n",
	       0x6A, ess_read_mix_reg(sc, 0x6A));
	printf("ESS_AUXB_REC_VOL: mix reg 0x%02x=0x%02x\n",
	       0x6C, ess_read_mix_reg(sc, 0x6C));
	printf("ESS_RECORD_VOL: x reg 0x%02x=0x%02x\n",
	       0xB4, ess_read_x_reg(sc, 0xB4));
	printf("Audio 1 play vol (unused): mix reg 0x%02x=0x%02x\n",
	       0x14, ess_read_mix_reg(sc, 0x14));

	printf("ESS_MIC_PREAMP: x reg 0x%02x=0x%02x\n",
	       ESS_XCMD_PREAMP_CTRL, ess_read_x_reg(sc, ESS_XCMD_PREAMP_CTRL));
	printf("ESS_RECORD_MONITOR: x reg 0x%02x=0x%02x\n",
	       ESS_XCMD_AUDIO_CTRL, ess_read_x_reg(sc, ESS_XCMD_AUDIO_CTRL));
	printf("Record source: mix reg 0x%02x=0x%02x, 0x%02x=0x%02x\n",
	       ESS_MREG_ADC_SOURCE, ess_read_mix_reg(sc, ESS_MREG_ADC_SOURCE),
	       ESS_MREG_AUDIO2_CTRL2, ess_read_mix_reg(sc, ESS_MREG_AUDIO2_CTRL2));
}

#endif

/*
 * Configure the ESS chip for the desired audio base address.
 */
int
ess_config_addr(sc)
	struct ess_softc *sc;
{
	int iobase = sc->sc_iobase;
	bus_space_tag_t iot = sc->sc_iot;

	/*
	 * Configure using the System Control Register method.  This
	 * method is used when the AMODE line is tied high, which is
	 * the case for the Shark, but not for the evaluation board.
	 */

	bus_space_handle_t scr_access_ioh;
	bus_space_handle_t scr_ioh;
	u_short scr_value;

	/*
	 * Set the SCR bit to enable audio.
	 */
	scr_value = ESS_SCR_AUDIO_ENABLE;

	/*
	 * Set the SCR bits necessary to select the specified audio
	 * base address.
	 */
	switch(iobase) {
	case 0x220:
		scr_value |= ESS_SCR_AUDIO_220;
		break;
	case 0x230:
		scr_value |= ESS_SCR_AUDIO_230;
		break;
	case 0x240:
		scr_value |= ESS_SCR_AUDIO_240;
		break;
	case 0x250:
		scr_value |= ESS_SCR_AUDIO_250;
		break;
	default:
		printf("ess: configured iobase 0x%x invalid\n", iobase);
		return (1);
		break;
	}

	/*
	 * Get a mapping for the System Control Register (SCR) access
	 * registers and the SCR data registers.
	 */
	if (bus_space_map(iot, ESS_SCR_ACCESS_BASE, ESS_SCR_ACCESS_PORTS,
			  0, &scr_access_ioh)) {
		printf("ess: can't map SCR access registers\n");
		return (1);
	}
	if (bus_space_map(iot, ESS_SCR_BASE, ESS_SCR_PORTS,
			  0, &scr_ioh)) {
		printf("ess: can't map SCR registers\n");
		bus_space_unmap(iot, scr_access_ioh, ESS_SCR_ACCESS_PORTS);
		return (1);
	}

	/* Unlock the SCR. */
	EWRITE1(iot, scr_access_ioh, ESS_SCR_UNLOCK, 0);

	/* Write the base address information into SCR[0]. */
	EWRITE1(iot, scr_ioh, ESS_SCR_INDEX, 0);
	EWRITE1(iot, scr_ioh, ESS_SCR_DATA, scr_value);
	
	/* Lock the SCR. */
	EWRITE1(iot, scr_access_ioh, ESS_SCR_LOCK, 0);

	/* Unmap the SCR access ports and the SCR data ports. */
	bus_space_unmap(iot, scr_access_ioh, ESS_SCR_ACCESS_PORTS);
	bus_space_unmap(iot, scr_ioh, ESS_SCR_PORTS);

	return 0;
}


/*
 * Configure the ESS chip for the desired IRQ and DMA channels.
 * ESS  ISA
 * --------
 * IRQA irq9
 * IRQB irq5
 * IRQC irq7
 * IRQD irq10
 * IRQE irq15
 *
 * DRQA drq0
 * DRQB drq1
 * DRQC drq3
 * DRQD drq5
 */
void
ess_config_irq(sc)
	struct ess_softc *sc;
{
	int v;

	DPRINTFN(2,("ess_config_irq\n"));

	if (sc->sc_model == ESS_1887 &&
	    sc->sc_audio1.irq == sc->sc_audio2.irq &&
	    sc->sc_audio1.irq != -1) {
		/* Use new method, both interrupts are the same. */
		v = ESS_IS_SELECT_IRQ;	/* enable intrs */
		switch (sc->sc_audio1.irq) {
		case 5:
			v |= ESS_IS_INTRB;
			break;
		case 7:
			v |= ESS_IS_INTRC;
			break;
		case 9:
			v |= ESS_IS_INTRA;
			break;
		case 10:
			v |= ESS_IS_INTRD;
			break;
		case 15:
			v |= ESS_IS_INTRE;
			break;
#ifdef DIAGNOSTIC
		default:
			printf("ess_config_irq: configured irq %d not supported for Audio 1\n", 
			       sc->sc_audio1.irq);
			return;
#endif
		}
		/* Set the IRQ */
		ess_write_mix_reg(sc, ESS_MREG_INTR_ST, v);
		return;
	}

	if (sc->sc_model == ESS_1887) {
		/* Tell the 1887 to use the old interrupt method. */
		ess_write_mix_reg(sc, ESS_MREG_INTR_ST, ESS_IS_ES1888);
	}

	if (sc->sc_audio1.polled) {
		/* Turn off Audio1 interrupts. */
		v = 0;
	} else {
		/* Configure Audio 1 for the appropriate IRQ line. */
		v = ESS_IRQ_CTRL_MASK | ESS_IRQ_CTRL_EXT; /* All intrs on */
		switch (sc->sc_audio1.irq) {
		case 5:
			v |= ESS_IRQ_CTRL_INTRB;
			break;
		case 7:
			v |= ESS_IRQ_CTRL_INTRC;
			break;
		case 9:
			v |= ESS_IRQ_CTRL_INTRA;
			break;
		case 10:
			v |= ESS_IRQ_CTRL_INTRD;
			break;
#ifdef DIAGNOSTIC
		default:
			printf("ess: configured irq %d not supported for Audio 1\n", 
			       sc->sc_audio1.irq);
			return;
#endif
		}
	}
	ess_write_x_reg(sc, ESS_XCMD_IRQ_CTRL, v);

	if (sc->sc_model == ESS_1788)
		return;

	if (sc->sc_audio2.polled) {
		/* Turn off Audio2 interrupts. */
		ess_clear_mreg_bits(sc, ESS_MREG_AUDIO2_CTRL2, 
				    ESS_AUDIO2_CTRL2_IRQ2_ENABLE);
	} else {
		/* Audio2 is hardwired to INTRE in this mode. */
		ess_set_mreg_bits(sc, ESS_MREG_AUDIO2_CTRL2, 
				  ESS_AUDIO2_CTRL2_IRQ2_ENABLE);
	}
}


void
ess_config_drq(sc)
	struct ess_softc *sc;
{
	int v;

	DPRINTFN(2,("ess_config_drq\n"));

	/* Configure Audio 1 (record) for DMA on the appropriate channel. */
	v = ESS_DRQ_CTRL_PU | ESS_DRQ_CTRL_EXT;
	switch (sc->sc_audio1.drq) {
	case 0:
		v |= ESS_DRQ_CTRL_DRQA;
		break;
	case 1:
		v |= ESS_DRQ_CTRL_DRQB;
		break;
	case 3:
		v |= ESS_DRQ_CTRL_DRQC;
		break;
#ifdef DIAGNOSTIC
	default:
		printf("ess_config_drq: configured dma chan %d not supported for Audio 1\n", 
		       sc->sc_audio1.drq);
		return;
#endif
	}
	/* Set DRQ1 */
	ess_write_x_reg(sc, ESS_XCMD_DRQ_CTRL, v);

	if (sc->sc_model == ESS_1788)
		return;

	/* Configure DRQ2 */
	v = ESS_AUDIO2_CTRL3_DRQ_PD;
	switch (sc->sc_audio2.drq) {
	case 0:
		v |= ESS_AUDIO2_CTRL3_DRQA;
		break;
	case 1:
		v |= ESS_AUDIO2_CTRL3_DRQB;
		break;
	case 3:
		v |= ESS_AUDIO2_CTRL3_DRQC;
		break;
	case 5:
		v |= ESS_AUDIO2_CTRL3_DRQD;
		break;
#ifdef DIAGNOSTIC
	default:
		printf("ess_config_drq: configured dma chan %d not supported for Audio 2\n", 
		       sc->sc_audio2.drq);
		return;
#endif
	}
	ess_write_mix_reg(sc, ESS_MREG_AUDIO2_CTRL3, v);
	/* Enable DMA 2 */
	ess_set_mreg_bits(sc, ESS_MREG_AUDIO2_CTRL2, 
			  ESS_AUDIO2_CTRL2_DMA_ENABLE);
}

/* 
 * Set up registers after a reset. 
 */
void
ess_setup(sc)
	struct ess_softc *sc;
{

	ess_config_irq(sc);
	ess_config_drq(sc);

	DPRINTFN(2,("ess_setup: done\n"));
}

/*
 * Determine the model of ESS chip we are talking to.  Currently we
 * only support ES1888, ES1887 and ES888.  The method of determining
 * the chip is based on the information on page 27 of the ES1887 data
 * sheet. 
 *
 * This routine sets the values of sc->sc_model and sc->sc_version.
 */
int
ess_identify(sc)
	struct ess_softc *sc;
{
	u_char reg1;
	u_char reg2;
	u_char reg3;

	sc->sc_model = ESS_UNSUPPORTED;
	sc->sc_version = 0;


	/*
	 * 1. Check legacy ID bytes.  These should be 0x68 0x8n, where
	 *    n >= 8 for an ES1887 or an ES888.  Other values indicate
	 *    earlier (unsupported) chips.
	 */
	ess_wdsp(sc, ESS_ACMD_LEGACY_ID);

	if ((reg1 = ess_rdsp(sc)) != 0x68) {
		printf("ess: First ID byte wrong (0x%02x)\n", reg1);
		return 1;
	}

	reg2 = ess_rdsp(sc);
	if (((reg2 & 0xf0) != 0x80) ||
	    ((reg2 & 0x0f) < 8)) {
		printf("ess: Second ID byte wrong (0x%02x)\n", reg2);
		return 1;
	}

	/*
	 * Store the ID bytes as the version.
	 */
	sc->sc_version = (reg1 << 8) + reg2;


	/*
	 * 2. Verify we can change bit 2 in mixer register 0x64.  This
	 *    should be possible on all supported chips.
	 */
	reg1 = ess_read_mix_reg(sc, ESS_MREG_VOLUME_CTRL);
	reg2 = reg1 ^ 0x04;  /* toggle bit 2 */
	
	ess_write_mix_reg(sc, ESS_MREG_VOLUME_CTRL, reg2);
	
	if (ess_read_mix_reg(sc, ESS_MREG_VOLUME_CTRL) != reg2) {
		printf("ess: Hardware error (unable to toggle bit 2 of mixer register 0x64)\n");
		return 1;
	}

	/*
	 * Restore the original value of mixer register 0x64.
	 */
	ess_write_mix_reg(sc, ESS_MREG_VOLUME_CTRL, reg1);


	/*
	 * 3. Verify we can change the value of mixer register 
	 *    ESS_MREG_SAMPLE_RATE.
	 *    This is possible on the 1888/1887/888, but not on the 1788.
	 *    It is not necessary to restore the value of this mixer register.
	 */
	reg1 = ess_read_mix_reg(sc, ESS_MREG_SAMPLE_RATE);
	reg2 = reg1 ^ 0xff;  /* toggle all bits */

	ess_write_mix_reg(sc, ESS_MREG_SAMPLE_RATE, reg2);
	
	if (ess_read_mix_reg(sc, ESS_MREG_SAMPLE_RATE) != reg2) {
		/* If we got this far before failing, it's a 1788. */
		sc->sc_model = ESS_1788;
	} else {
		/*
		 * 4. Determine if we can change bit 5 in mixer register 0x64.
		 *    This determines whether we have an ES1887:
		 *
		 *    - can change indicates ES1887
		 *    - can't change indicates ES1888 or ES888
		 */
		reg1 = ess_read_mix_reg(sc, ESS_MREG_VOLUME_CTRL);
		reg2 = reg1 ^ 0x20;  /* toggle bit 5 */
	
		ess_write_mix_reg(sc, ESS_MREG_VOLUME_CTRL, reg2);
	
		if (ess_read_mix_reg(sc, ESS_MREG_VOLUME_CTRL) == reg2) {
			sc->sc_model = ESS_1887;

			/*
			 * Restore the original value of mixer register 0x64.
			 */
			ess_write_mix_reg(sc, ESS_MREG_VOLUME_CTRL, reg1);
		} else {
			/*
			 * 5. Determine if we can change the value of mixer
			 *    register 0x69 independently of mixer register
			 *    0x68. This determines which chip we have:
			 *
			 *    - can modify idependently indicates ES888
			 *    - register 0x69 is an alias of 0x68 indicates ES1888
			 */
			reg1 = ess_read_mix_reg(sc, 0x68);
			reg2 = ess_read_mix_reg(sc, 0x69);
			reg3 = reg2 ^ 0xff;  /* toggle all bits */

			/*
			 * Write different values to each register.
			 */
			ess_write_mix_reg(sc, 0x68, reg2);
			ess_write_mix_reg(sc, 0x69, reg3);

			if (ess_read_mix_reg(sc, 0x68) == reg2 &&
			    ess_read_mix_reg(sc, 0x69) == reg3)
				sc->sc_model = ESS_888;
			else
				sc->sc_model = ESS_1888;
		
			/*
			 * Restore the original value of the registers.
			 */
			ess_write_mix_reg(sc, 0x68, reg1);
			ess_write_mix_reg(sc, 0x69, reg2);
		}
	}

	return 0;
}


int
ess_setup_sc(sc, doinit)
	struct ess_softc *sc;
	int doinit;
{
	/* Reset the chip. */
	if (ess_reset(sc) != 0) {
		DPRINTF(("ess_setup_sc: couldn't reset chip\n"));
		return (1);
	}

	/* Identify the ESS chip, and check that it is supported. */
	if (ess_identify(sc)) {
		DPRINTF(("ess_setup_sc: couldn't identify\n"));
		return (1);
	}

	return (0);
}

/*
 * Probe for the ESS hardware.
 */
int
essmatch(sc)
	struct ess_softc *sc;
{
	if (!ESS_BASE_VALID(sc->sc_iobase)) {
		printf("ess: configured iobase 0x%x invalid\n", sc->sc_iobase);
		return (0);
	}

	/* Configure the ESS chip for the desired audio base address. */
	if (ess_config_addr(sc))
		return (0);

	if (ess_setup_sc(sc, 1)) 
		return (0);

	if (sc->sc_model == ESS_UNSUPPORTED) {
		DPRINTF(("ess: Unsupported model\n"));
		return (0);
	}

	/* Check that requested DMA channels are valid and different. */
	if (!ESS_DRQ1_VALID(sc->sc_audio1.drq)) {
		printf("ess: record drq %d invalid\n", sc->sc_audio1.drq);
		return (0);
	}
	if (!isa_drq_isfree(sc->sc_ic, sc->sc_audio1.drq))
		return (0);
	if (sc->sc_model != ESS_1788) {
		if (!ESS_DRQ2_VALID(sc->sc_audio2.drq)) {
			printf("ess: play drq %d invalid\n", sc->sc_audio2.drq);
			return (0);
		}
		if (sc->sc_audio1.drq == sc->sc_audio2.drq) {
			printf("ess: play and record drq both %d\n",
			       sc->sc_audio1.drq);
			return (0);
		}
		if (!isa_drq_isfree(sc->sc_ic, sc->sc_audio2.drq))
			return (0);
	}
	
	/* 
	 * The 1887 has an additional IRQ mode where both channels are mapped
	 * to the same IRQ.
	 */
	if (sc->sc_model == ESS_1887 &&
	    sc->sc_audio1.irq == sc->sc_audio2.irq &&
	    sc->sc_audio1.irq != -1 &&
	    ESS_IRQ12_VALID(sc->sc_audio1.irq))
		goto irq_not1888;

	/* Check that requested IRQ lines are valid and different. */
	if (sc->sc_audio1.irq != -1 &&
	    !ESS_IRQ1_VALID(sc->sc_audio1.irq)) {
		printf("ess: record irq %d invalid\n", sc->sc_audio1.irq);
		return (0);
	}
	if (sc->sc_model != ESS_1788) {
		if (sc->sc_audio2.irq != -1 &&
		    !ESS_IRQ2_VALID(sc->sc_audio2.irq)) {
			printf("ess: play irq %d invalid\n", sc->sc_audio2.irq);
			return (0);
		}
		if (sc->sc_audio1.irq == sc->sc_audio2.irq &&
		    sc->sc_audio1.irq != -1) {
			printf("ess: play and record irq both %d\n",
			       sc->sc_audio1.irq);
			return (0);
		}
	}

irq_not1888:
	/* XXX should we check IRQs as well? */

	return (1);
}


/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver.
 */
void
essattach(sc)
	struct ess_softc *sc;
{
	struct audio_attach_args arg;
	struct audio_params pparams, rparams;
	int i;
	u_int v;

	if (ess_setup_sc(sc, 0)) {
		printf(": setup failed\n");
		return;
	}

	printf(": ESS Technology ES%s [version 0x%04x]\n", 
	       essmodel[sc->sc_model], sc->sc_version);
	
	sc->sc_audio1.irq = -1;
	sc->sc_audio2.irq = -1;

	sc->sc_audio1.polled = sc->sc_audio1.irq == -1;
	if (!sc->sc_audio1.polled) {
		sc->sc_audio1.ih = isa_intr_establish(sc->sc_ic,
		    sc->sc_audio1.irq, sc->sc_audio1.ist, IPL_AUDIO,
		    ess_audio1_intr, sc);
		printf("%s: audio1 interrupting at irq %d\n",
		    sc->sc_dev.dv_xname, sc->sc_audio1.irq);
	} else
		printf("%s: audio1 polled\n", sc->sc_dev.dv_xname);
	if (isa_dmamap_create(sc->sc_ic, sc->sc_audio1.drq,
	    MAX_ISADMA, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW)) {
		printf("%s: can't create map for drq %d\n",
		       sc->sc_dev.dv_xname, sc->sc_audio1.drq);
		return;
	}

	if (sc->sc_model != ESS_1788) {
		sc->sc_audio2.polled = sc->sc_audio2.irq == -1;
		if (!sc->sc_audio2.polled) {
			sc->sc_audio2.ih = isa_intr_establish(sc->sc_ic,
			    sc->sc_audio2.irq, sc->sc_audio2.ist, IPL_AUDIO,
			    ess_audio2_intr, sc);
			printf("%s: audio2 interrupting at irq %d\n",
			    sc->sc_dev.dv_xname, sc->sc_audio2.irq);
		} else
			printf("%s: audio2 polled\n", sc->sc_dev.dv_xname);
		if (isa_dmamap_create(sc->sc_ic, sc->sc_audio2.drq,
		    MAX_ISADMA, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW)) {
			printf("%s: can't create map for drq %d\n",
			       sc->sc_dev.dv_xname, sc->sc_audio2.drq);
			return;
		}
	}

	/* 
	 * Set record and play parameters to default values defined in
	 * generic audio driver.
	 */
	pparams = audio_default;
	rparams = audio_default;
	ess_set_params(sc, AUMODE_RECORD|AUMODE_PLAY, 0, &pparams, &rparams);

	/* Do a hardware reset on the mixer. */
	ess_write_mix_reg(sc, ESS_MIX_RESET, ESS_MIX_RESET);

	/*
	 * Set volume of Audio 1 to zero and disable Audio 1 DAC input
	 * to playback mixer, since playback is always through Audio 2.
	 */
	if (sc->sc_model != ESS_1788)
		ess_write_mix_reg(sc, ESS_MREG_VOLUME_VOICE, 0);
	ess_wdsp(sc, ESS_ACMD_DISABLE_SPKR);

	if (sc->sc_model == ESS_1788) {
		ess_write_mix_reg(sc, ESS_MREG_ADC_SOURCE, ESS_SOURCE_MIC);
		sc->in_port = ESS_SOURCE_MIC;
		sc->ndevs = ESS_1788_NDEVS;
	} else {
		/*
		 * Set hardware record source to use output of the record
		 * mixer. We do the selection of record source in software by
		 * setting the gain of the unused sources to zero. (See
		 * ess_set_in_ports.)
		 */
		ess_write_mix_reg(sc, ESS_MREG_ADC_SOURCE, ESS_SOURCE_MIXER);
		sc->in_mask = 1 << ESS_MIC_REC_VOL;
		sc->ndevs = ESS_1888_NDEVS;
		ess_clear_mreg_bits(sc, ESS_MREG_AUDIO2_CTRL2, 0x10);
		ess_set_mreg_bits(sc, ESS_MREG_AUDIO2_CTRL2, 0x08);
	}

	/*
	 * Set gain on each mixer device to a sensible value.
	 * Devices not normally used are turned off, and other devices
	 * are set to 50% volume.
	 */
	for (i = 0; i < sc->ndevs; i++) {
		switch (i) {
		case ESS_MIC_PLAY_VOL:
		case ESS_LINE_PLAY_VOL:
		case ESS_CD_PLAY_VOL:
		case ESS_AUXB_PLAY_VOL:
		case ESS_DAC_REC_VOL:
		case ESS_LINE_REC_VOL:
		case ESS_SYNTH_REC_VOL:
		case ESS_CD_REC_VOL:
		case ESS_AUXB_REC_VOL:
			v = 0;
			break;
		default:
			v = ESS_4BIT_GAIN(AUDIO_MAX_GAIN / 2);
			break;
		}
		sc->gain[i][ESS_LEFT] = sc->gain[i][ESS_RIGHT] = v;
		ess_set_gain(sc, i, 1);
	}

	ess_setup(sc);

	/* Disable the speaker until the device is opened.  */
	ess_speaker_off(sc);
	sc->spkr_state = SPKR_OFF;

	sprintf(ess_device.name, "ES%s", essmodel[sc->sc_model]);
	sprintf(ess_device.version, "0x%04x", sc->sc_version);

	if (sc->sc_model == ESS_1788)
		audio_attach_mi(&ess_1788_hw_if, sc, &sc->sc_dev);
	else
		audio_attach_mi(&ess_1888_hw_if, sc, &sc->sc_dev);

	arg.type = AUDIODEV_TYPE_OPL;
	arg.hwif = 0;
	arg.hdl = 0;
	(void)config_found(&sc->sc_dev, &arg, audioprint);

#ifdef AUDIO_DEBUG
	if (essdebug > 0)
		ess_printsc(sc);
#endif
}

/*
 * Various routines to interface to higher level audio driver
 */

int
ess_open(addr, flags)
	void *addr;
	int flags;
{
	struct ess_softc *sc = addr;

	DPRINTF(("ess_open: sc=%p\n", sc));
    
	if (sc->sc_open != 0 || ess_reset(sc) != 0)
		return ENXIO;

	ess_setup(sc);		/* because we did a reset */

	sc->sc_open = 1;

	DPRINTF(("ess_open: opened\n"));

	return (0);
}

void
ess_1788_close(addr)
	void *addr;
{
	struct ess_softc *sc = addr;

	DPRINTF(("ess_1788_close: sc=%p\n", sc));

	ess_speaker_off(sc);
	sc->spkr_state = SPKR_OFF;

	ess_audio1_halt(sc);

	sc->sc_open = 0;
	DPRINTF(("ess_1788_close: closed\n"));
}

void
ess_1888_close(addr)
	void *addr;
{
	struct ess_softc *sc = addr;

	DPRINTF(("ess_1888_close: sc=%p\n", sc));

	ess_speaker_off(sc);
	sc->spkr_state = SPKR_OFF;

	ess_audio1_halt(sc);
	ess_audio2_halt(sc);

	sc->sc_open = 0;
	DPRINTF(("ess_1888_close: closed\n"));
}

/*
 * Wait for FIFO to drain, and analog section to settle.
 * XXX should check FIFO empty bit.
 */
int
ess_drain(addr)
	void *addr;
{
	tsleep(addr, PWAIT | PCATCH, "essdr", hz/20); /* XXX */
	return (0);
}

/* XXX should use reference count */
int
ess_speaker_ctl(addr, newstate)
	void *addr;
	int newstate;
{
	struct ess_softc *sc = addr;

	if ((newstate == SPKR_ON) && (sc->spkr_state == SPKR_OFF)) {
		ess_speaker_on(sc);
		sc->spkr_state = SPKR_ON;
	}
	if ((newstate == SPKR_OFF) && (sc->spkr_state == SPKR_ON)) {
		ess_speaker_off(sc);
		sc->spkr_state = SPKR_OFF;
	}
	return (0);
}

int
ess_getdev(addr, retp)
	void *addr;
	struct audio_device *retp;
{
	*retp = ess_device;
	return (0);
}

int
ess_query_encoding(addr, fp)
	void *addr;
	struct audio_encoding *fp;
{
	/*struct ess_softc *sc = addr;*/

	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		return (0);
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 3:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = 0;
		return (0);
	case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		return (0);
	case 5:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		return (0);
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return (0);
	default:
		return EINVAL;
	}
	return (0);
}

int
ess_set_params(addr, setmode, usemode, play, rec)
	void *addr;
	int setmode, usemode;
	struct audio_params *play, *rec;
{
	struct ess_softc *sc = addr;
	struct audio_params *p;
	int mode;
	int rate;

	DPRINTF(("ess_set_params: set=%d use=%d\n", setmode, usemode));

	/*
	 * The ES1887 manual (page 39, `Full-Duplex DMA Mode') claims that in
	 * full-duplex operation the sample rates must be the same for both
	 * channels.  This appears to be false; the only bit in common is the
	 * clock source selection.  However, we'll be conservative here.
	 * - mycroft
	 */
	if (play->sample_rate != rec->sample_rate &&
	    usemode == (AUMODE_PLAY | AUMODE_RECORD)) {
		if (setmode == AUMODE_PLAY) {
			rec->sample_rate = play->sample_rate;
			setmode |= AUMODE_RECORD;
		} else if (setmode == AUMODE_RECORD) {
			play->sample_rate = rec->sample_rate;
			setmode |= AUMODE_PLAY;
		} else
			return (EINVAL);
	}

	for (mode = AUMODE_RECORD; mode != -1; 
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		p = mode == AUMODE_PLAY ? play : rec;

		if (p->sample_rate < ESS_MINRATE ||
		    p->sample_rate > ESS_MAXRATE ||
		    (p->precision != 8 && p->precision != 16) ||
		    (p->channels != 1 && p->channels != 2))
			return (EINVAL);

		p->factor = 1;
		p->sw_code = 0;
		switch (p->encoding) {
		case AUDIO_ENCODING_SLINEAR_BE:
		case AUDIO_ENCODING_ULINEAR_BE:
			if (p->precision == 16)
				p->sw_code = swap_bytes;
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
		case AUDIO_ENCODING_ULINEAR_LE:
			break;
		case AUDIO_ENCODING_ULAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = mulaw_to_ulinear16;
			} else
				p->sw_code = ulinear8_to_mulaw;
			break;
		case AUDIO_ENCODING_ALAW:
			if (mode == AUMODE_PLAY) {
				p->factor = 2;
				p->sw_code = alaw_to_ulinear16;
			} else
				p->sw_code = ulinear8_to_alaw;
			break;
		default:
			return (EINVAL);
		}
	}

	if (usemode == AUMODE_RECORD)
		rate = rec->sample_rate;
	else
		rate = play->sample_rate;

	ess_write_x_reg(sc, ESS_XCMD_SAMPLE_RATE, ess_srtotc(rate));
	ess_write_x_reg(sc, ESS_XCMD_FILTER_CLOCK, ess_srtofc(rate));

	if (sc->sc_model != ESS_1788) {
		ess_write_mix_reg(sc, ESS_MREG_SAMPLE_RATE, ess_srtotc(rate));
		ess_write_mix_reg(sc, ESS_MREG_FILTER_CLOCK, ess_srtofc(rate));
	}

	return (0);
}

int
ess_audio1_trigger_output(addr, start, end, blksize, intr, arg, param)
	void *addr;
	void *start, *end;
	int blksize;
	void (*intr) __P((void *));
	void *arg;
	struct audio_params *param;
{
	struct ess_softc *sc = addr;
	u_int8_t reg;

	DPRINTFN(1, ("ess_audio1_trigger_output: sc=%p start=%p end=%p blksize=%d intr=%p(%p)\n",
	    addr, start, end, blksize, intr, arg));

	if (sc->sc_audio1.active)
		panic("ess_audio1_trigger_output: already running");

	sc->sc_audio1.active = 1;
	sc->sc_audio1.intr = intr;
	sc->sc_audio1.arg = arg;
	if (sc->sc_audio1.polled) {
		sc->sc_audio1.dmapos = 0;
		sc->sc_audio1.buffersize = (char *)end - (char *)start;
		sc->sc_audio1.dmacount = 0;
		sc->sc_audio1.blksize = blksize;
		timeout(ess_audio1_poll, sc, hz/30);
	}

	reg = ess_read_x_reg(sc, ESS_XCMD_AUDIO_CTRL);
	if (param->channels == 2) {
		reg &= ~ESS_AUDIO_CTRL_MONO;
		reg |= ESS_AUDIO_CTRL_STEREO;
	} else {
		reg |= ESS_AUDIO_CTRL_MONO;
		reg &= ~ESS_AUDIO_CTRL_STEREO;
	}
	ess_write_x_reg(sc, ESS_XCMD_AUDIO_CTRL, reg);

	reg = ess_read_x_reg(sc, ESS_XCMD_AUDIO1_CTRL1);
	if (param->precision * param->factor == 16)
		reg |= ESS_AUDIO1_CTRL1_FIFO_SIZE;
	else
		reg &= ~ESS_AUDIO1_CTRL1_FIFO_SIZE;
	if (param->channels == 2)
		reg |= ESS_AUDIO1_CTRL1_FIFO_STEREO;
	else
		reg &= ~ESS_AUDIO1_CTRL1_FIFO_STEREO;
	if (param->encoding == AUDIO_ENCODING_SLINEAR_BE ||
	    param->encoding == AUDIO_ENCODING_SLINEAR_LE)
		reg |= ESS_AUDIO1_CTRL1_FIFO_SIGNED;
	else
		reg &= ~ESS_AUDIO1_CTRL1_FIFO_SIGNED;
	reg |= ESS_AUDIO1_CTRL1_FIFO_CONNECT;
	ess_write_x_reg(sc, ESS_XCMD_AUDIO1_CTRL1, reg);

	isa_dmastart(sc->sc_ic, sc->sc_audio1.drq, start, 
		     (char *)end - (char *)start, NULL,
	    DMAMODE_WRITE | DMAMODE_LOOPDEMAND, BUS_DMA_NOWAIT);

	/* Program transfer count registers with 2's complement of count. */
	blksize = -blksize;
	ess_write_x_reg(sc, ESS_XCMD_XFER_COUNTLO, blksize);
	ess_write_x_reg(sc, ESS_XCMD_XFER_COUNTHI, blksize >> 8);

	/* Use 4 bytes per output DMA. */
	ess_set_xreg_bits(sc, ESS_XCMD_DEMAND_CTRL, ESS_DEMAND_CTRL_DEMAND_4);

	/* Start auto-init DMA */
  	ess_wdsp(sc, ESS_ACMD_ENABLE_SPKR);
	reg = ess_read_x_reg(sc, ESS_XCMD_AUDIO1_CTRL2);
	reg &= ~(ESS_AUDIO1_CTRL2_DMA_READ | ESS_AUDIO1_CTRL2_ADC_ENABLE);
	reg |= ESS_AUDIO1_CTRL2_FIFO_ENABLE | ESS_AUDIO1_CTRL2_AUTO_INIT;
	ess_write_x_reg(sc, ESS_XCMD_AUDIO1_CTRL2, reg);

	return (0);
}

int
ess_audio2_trigger_output(addr, start, end, blksize, intr, arg, param)
	void *addr;
	void *start, *end;
	int blksize;
	void (*intr) __P((void *));
	void *arg;
	struct audio_params *param;
{
	struct ess_softc *sc = addr;
	u_int8_t reg;

	DPRINTFN(1, ("ess_audio2_trigger_output: sc=%p start=%p end=%p blksize=%d intr=%p(%p)\n",
	    addr, start, end, blksize, intr, arg));

	if (sc->sc_audio2.active)
		panic("ess_audio2_trigger_output: already running");

	sc->sc_audio2.active = 1;
	sc->sc_audio2.intr = intr;
	sc->sc_audio2.arg = arg;
	if (sc->sc_audio2.polled) {
		sc->sc_audio2.dmapos = 0;
		sc->sc_audio2.buffersize = (char *)end - (char *)start;
		sc->sc_audio2.dmacount = 0;
		sc->sc_audio2.blksize = blksize;
		timeout(ess_audio2_poll, sc, hz/30);
	}

	reg = ess_read_mix_reg(sc, ESS_MREG_AUDIO2_CTRL2);
	if (param->precision * param->factor == 16)
		reg |= ESS_AUDIO2_CTRL2_FIFO_SIZE;
	else
		reg &= ~ESS_AUDIO2_CTRL2_FIFO_SIZE;
	if (param->channels == 2)
		reg |= ESS_AUDIO2_CTRL2_CHANNELS;
	else
		reg &= ~ESS_AUDIO2_CTRL2_CHANNELS;
	if (param->encoding == AUDIO_ENCODING_SLINEAR_BE ||
	    param->encoding == AUDIO_ENCODING_SLINEAR_LE)
		reg |= ESS_AUDIO2_CTRL2_FIFO_SIGNED;
	else
		reg &= ~ESS_AUDIO2_CTRL2_FIFO_SIGNED;
	ess_write_mix_reg(sc, ESS_MREG_AUDIO2_CTRL2, reg);

	isa_dmastart(sc->sc_ic, sc->sc_audio2.drq, start, 
		     (char *)end - (char *)start, NULL,
	    DMAMODE_WRITE | DMAMODE_LOOPDEMAND, BUS_DMA_NOWAIT);

	if (IS16BITDRQ(sc->sc_audio2.drq))
		blksize >>= 1;	/* use word count for 16 bit DMA */
	/* Program transfer count registers with 2's complement of count. */
	blksize = -blksize;
	ess_write_mix_reg(sc, ESS_MREG_XFER_COUNTLO, blksize);
	ess_write_mix_reg(sc, ESS_MREG_XFER_COUNTHI, blksize >> 8);

	reg = ess_read_mix_reg(sc, ESS_MREG_AUDIO2_CTRL1);
	if (IS16BITDRQ(sc->sc_audio2.drq))
		reg |= ESS_AUDIO2_CTRL1_XFER_SIZE;
	else
		reg &= ~ESS_AUDIO2_CTRL1_XFER_SIZE;
	reg |= ESS_AUDIO2_CTRL1_DEMAND_8;
	reg |= ESS_AUDIO2_CTRL1_DAC_ENABLE | ESS_AUDIO2_CTRL1_FIFO_ENABLE |
	       ESS_AUDIO2_CTRL1_AUTO_INIT;
	ess_write_mix_reg(sc, ESS_MREG_AUDIO2_CTRL1, reg);

	return (0);
}

int
ess_audio1_trigger_input(addr, start, end, blksize, intr, arg, param)
	void *addr;
	void *start, *end;
	int blksize;
	void (*intr) __P((void *));
	void *arg;
	struct audio_params *param;
{
	struct ess_softc *sc = addr;
	u_int8_t reg;

	DPRINTFN(1, ("ess_audio1_trigger_input: sc=%p start=%p end=%p blksize=%d intr=%p(%p)\n",
	    addr, start, end, blksize, intr, arg));

	if (sc->sc_audio1.active)
		panic("ess_audio1_trigger_input: already running");

	sc->sc_audio1.active = 1;
	sc->sc_audio1.intr = intr;
	sc->sc_audio1.arg = arg;
	if (sc->sc_audio1.polled) {
		sc->sc_audio1.dmapos = 0;
		sc->sc_audio1.buffersize = (char *)end - (char *)start;
		sc->sc_audio1.dmacount = 0;
		sc->sc_audio1.blksize = blksize;
		timeout(ess_audio1_poll, sc, hz/30);
	}

	reg = ess_read_x_reg(sc, ESS_XCMD_AUDIO_CTRL);
	if (param->channels == 2) {
		reg &= ~ESS_AUDIO_CTRL_MONO;
		reg |= ESS_AUDIO_CTRL_STEREO;
	} else {
		reg |= ESS_AUDIO_CTRL_MONO;
		reg &= ~ESS_AUDIO_CTRL_STEREO;
	}
	ess_write_x_reg(sc, ESS_XCMD_AUDIO_CTRL, reg);

	reg = ess_read_x_reg(sc, ESS_XCMD_AUDIO1_CTRL1);
	if (param->precision * param->factor == 16)
		reg |= ESS_AUDIO1_CTRL1_FIFO_SIZE;
	else
		reg &= ~ESS_AUDIO1_CTRL1_FIFO_SIZE;
	if (param->channels == 2)
		reg |= ESS_AUDIO1_CTRL1_FIFO_STEREO;
	else
		reg &= ~ESS_AUDIO1_CTRL1_FIFO_STEREO;
	if (param->encoding == AUDIO_ENCODING_SLINEAR_BE ||
	    param->encoding == AUDIO_ENCODING_SLINEAR_LE)
		reg |= ESS_AUDIO1_CTRL1_FIFO_SIGNED;
	else
		reg &= ~ESS_AUDIO1_CTRL1_FIFO_SIGNED;
	reg |= ESS_AUDIO1_CTRL1_FIFO_CONNECT;
	ess_write_x_reg(sc, ESS_XCMD_AUDIO1_CTRL1, reg);

	isa_dmastart(sc->sc_ic, sc->sc_audio1.drq, start, 
		     (char *)end - (char *)start, NULL,
	    DMAMODE_READ | DMAMODE_LOOPDEMAND, BUS_DMA_NOWAIT);

	/* Program transfer count registers with 2's complement of count. */
	blksize = -blksize;
	ess_write_x_reg(sc, ESS_XCMD_XFER_COUNTLO, blksize);
	ess_write_x_reg(sc, ESS_XCMD_XFER_COUNTHI, blksize >> 8);

	/* Use 4 bytes per input DMA. */
	ess_set_xreg_bits(sc, ESS_XCMD_DEMAND_CTRL, ESS_DEMAND_CTRL_DEMAND_4);

	/* Start auto-init DMA */
  	ess_wdsp(sc, ESS_ACMD_DISABLE_SPKR);
	reg = ess_read_x_reg(sc, ESS_XCMD_AUDIO1_CTRL2);
	reg |= ESS_AUDIO1_CTRL2_DMA_READ | ESS_AUDIO1_CTRL2_ADC_ENABLE;
	reg |= ESS_AUDIO1_CTRL2_FIFO_ENABLE | ESS_AUDIO1_CTRL2_AUTO_INIT;
	ess_write_x_reg(sc, ESS_XCMD_AUDIO1_CTRL2, reg);

	return (0);
}

int
ess_audio1_halt(addr)
	void *addr;
{
	struct ess_softc *sc = addr;

	DPRINTF(("ess_audio1_halt: sc=%p\n", sc));

	if (sc->sc_audio1.active) {
		ess_clear_xreg_bits(sc, ESS_XCMD_AUDIO1_CTRL2,
		    ESS_AUDIO1_CTRL2_FIFO_ENABLE);
		isa_dmaabort(sc->sc_ic, sc->sc_audio1.drq);
		if (sc->sc_audio1.polled)
			untimeout(ess_audio1_poll, sc);
		sc->sc_audio1.active = 0;
	}

	return (0);
}

int
ess_audio2_halt(addr)
	void *addr;
{
	struct ess_softc *sc = addr;

	DPRINTF(("ess_audio2_halt: sc=%p\n", sc));

	if (sc->sc_audio2.active) {
		ess_clear_mreg_bits(sc, ESS_MREG_AUDIO2_CTRL1,
		    ESS_AUDIO2_CTRL1_DAC_ENABLE |
		    ESS_AUDIO2_CTRL1_FIFO_ENABLE);
		isa_dmaabort(sc->sc_ic, sc->sc_audio2.drq);
		if (sc->sc_audio2.polled)
			untimeout(ess_audio2_poll, sc);
		sc->sc_audio2.active = 0;
	}

	return (0);
}

int
ess_audio1_intr(arg)
	void *arg;
{
	struct ess_softc *sc = arg;
	u_int8_t reg;

	DPRINTFN(1,("ess_audio1_intr: intr=%p\n", sc->sc_audio1.intr));

	/* Check and clear interrupt on Audio1. */
	reg = EREAD1(sc->sc_iot, sc->sc_ioh, ESS_DSP_RW_STATUS);
	if ((reg & ESS_DSP_READ_OFLOW) == 0)
		return (0);
	reg = EREAD1(sc->sc_iot, sc->sc_ioh, ESS_CLEAR_INTR);

	sc->sc_audio1.nintr++;

	if (sc->sc_audio1.active) {
		(*sc->sc_audio1.intr)(sc->sc_audio1.arg);
		return (1);
	} else
		return (0);
}

int
ess_audio2_intr(arg)
	void *arg;
{
	struct ess_softc *sc = arg;
	u_int8_t reg;

	DPRINTFN(1,("ess_audio2_intr: intr=%p\n", sc->sc_audio2.intr));

	/* Check and clear interrupt on Audio2. */
	reg = ess_read_mix_reg(sc, ESS_MREG_AUDIO2_CTRL2);
	if ((reg & ESS_AUDIO2_CTRL2_IRQ_LATCH) == 0)
		return (0);
	reg &= ~ESS_AUDIO2_CTRL2_IRQ_LATCH;
	ess_write_mix_reg(sc, ESS_MREG_AUDIO2_CTRL2, reg);

	sc->sc_audio2.nintr++;

	if (sc->sc_audio2.active) {
		(*sc->sc_audio2.intr)(sc->sc_audio2.arg);
		return (1);
	} else
		return (0);
}

void
ess_audio1_poll(addr)
	void *addr;
{
	struct ess_softc *sc = addr;
	int dmapos, dmacount;

	if (!sc->sc_audio1.active)
		return;

	sc->sc_audio1.nintr++;

	dmapos = isa_dmacount(sc->sc_ic, sc->sc_audio1.drq);
	dmacount = sc->sc_audio1.dmapos - dmapos;
	if (dmacount < 0)
		dmacount += sc->sc_audio1.buffersize;
	sc->sc_audio1.dmapos = dmapos;
#if 1
	dmacount += sc->sc_audio1.dmacount;
	while (dmacount > sc->sc_audio1.blksize) {
		dmacount -= sc->sc_audio1.blksize;
		(*sc->sc_audio1.intr)(sc->sc_audio1.arg);
	}
	sc->sc_audio1.dmacount = dmacount;
#else
	(*sc->sc_audio1.intr)(sc->sc_audio1.arg, dmacount);
#endif

	timeout(ess_audio1_poll, sc, hz/30);
}

void
ess_audio2_poll(addr)
	void *addr;
{
	struct ess_softc *sc = addr;
	int dmapos, dmacount;

	if (!sc->sc_audio2.active)
		return;

	sc->sc_audio2.nintr++;

	dmapos = isa_dmacount(sc->sc_ic, sc->sc_audio2.drq);
	dmacount = sc->sc_audio2.dmapos - dmapos;
	if (dmacount < 0)
		dmacount += sc->sc_audio2.buffersize;
	sc->sc_audio2.dmapos = dmapos;
#if 1
	dmacount += sc->sc_audio2.dmacount;
	while (dmacount > sc->sc_audio2.blksize) {
		dmacount -= sc->sc_audio2.blksize;
		(*sc->sc_audio2.intr)(sc->sc_audio2.arg);
	}
	sc->sc_audio2.dmacount = dmacount;
#else
	(*sc->sc_audio2.intr)(sc->sc_audio2.arg, dmacount);
#endif

	timeout(ess_audio2_poll, sc, hz/30);
}

int
ess_round_blocksize(addr, blk)
	void *addr;
	int blk;
{
	return (blk & -8);	/* round for max DMA size */
}

int
ess_set_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct ess_softc *sc = addr;
	int lgain, rgain;
    
	DPRINTFN(5,("ess_set_port: port=%d num_channels=%d\n",
		    cp->dev, cp->un.value.num_channels));

	switch (cp->dev) {
	/*
	 * The following mixer ports are all stereo. If we get a
	 * single-channel gain value passed in, then we duplicate it
	 * to both left and right channels.
	 */
	case ESS_MASTER_VOL:
	case ESS_DAC_PLAY_VOL:
	case ESS_MIC_PLAY_VOL:
	case ESS_LINE_PLAY_VOL:
	case ESS_SYNTH_PLAY_VOL:
	case ESS_CD_PLAY_VOL:
	case ESS_AUXB_PLAY_VOL:
	case ESS_RECORD_VOL:
		if (cp->type != AUDIO_MIXER_VALUE)
			return EINVAL;

		switch (cp->un.value.num_channels) {
		case 1:
			lgain = rgain = ESS_4BIT_GAIN(
			  cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
			break;
		case 2:
			lgain = ESS_4BIT_GAIN(
			  cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT]);
			rgain = ESS_4BIT_GAIN(
			  cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT]);
			break;
		default:
			return EINVAL;
		}

		sc->gain[cp->dev][ESS_LEFT]  = lgain;
		sc->gain[cp->dev][ESS_RIGHT] = rgain;
		ess_set_gain(sc, cp->dev, 1);
		return (0);

	/*
	 * The PC speaker port is mono. If we get a stereo gain value
	 * passed in, then we return EINVAL.
	 */
	case ESS_PCSPEAKER_VOL:
		if (cp->un.value.num_channels != 1)
			return EINVAL;

		sc->gain[cp->dev][ESS_LEFT] = sc->gain[cp->dev][ESS_RIGHT] =
		  ESS_3BIT_GAIN(cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
		ess_set_gain(sc, cp->dev, 1);
		return (0);

	case ESS_RECORD_SOURCE:
		if (sc->sc_model == ESS_1788) {
			if (cp->type == AUDIO_MIXER_ENUM)
				return (ess_set_in_port(sc, cp->un.ord));
			else
				return (EINVAL);
		} else {
			if (cp->type == AUDIO_MIXER_SET)
				return (ess_set_in_ports(sc, cp->un.mask));
			else
				return (EINVAL);
		}
		return (0);

	case ESS_RECORD_MONITOR:
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		if (cp->un.ord)
			/* Enable monitor */
			ess_set_xreg_bits(sc, ESS_XCMD_AUDIO_CTRL,
					  ESS_AUDIO_CTRL_MONITOR);
		else
			/* Disable monitor */
			ess_clear_xreg_bits(sc, ESS_XCMD_AUDIO_CTRL,
					    ESS_AUDIO_CTRL_MONITOR);
		return (0);
	}

	if (sc->sc_model == ESS_1788)
		return (EINVAL);

	switch (cp->dev) {
	case ESS_DAC_REC_VOL:
	case ESS_MIC_REC_VOL:
	case ESS_LINE_REC_VOL:
	case ESS_SYNTH_REC_VOL:
	case ESS_CD_REC_VOL:
	case ESS_AUXB_REC_VOL:
		if (cp->type != AUDIO_MIXER_VALUE)
			return EINVAL;

		switch (cp->un.value.num_channels) {
		case 1:
			lgain = rgain = ESS_4BIT_GAIN(
			  cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
			break;
		case 2:
			lgain = ESS_4BIT_GAIN(
			  cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT]);
			rgain = ESS_4BIT_GAIN(
			  cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT]);
			break;
		default:
			return EINVAL;
		}

		sc->gain[cp->dev][ESS_LEFT]  = lgain;
		sc->gain[cp->dev][ESS_RIGHT] = rgain;
		ess_set_gain(sc, cp->dev, 1);
		return (0);

	case ESS_MIC_PREAMP:
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		if (cp->un.ord)
			/* Enable microphone preamp */
			ess_set_xreg_bits(sc, ESS_XCMD_PREAMP_CTRL,
					  ESS_PREAMP_CTRL_ENABLE);
		else
			/* Disable microphone preamp */
			ess_clear_xreg_bits(sc, ESS_XCMD_PREAMP_CTRL,
					  ESS_PREAMP_CTRL_ENABLE);
		return (0);
	}

	return (EINVAL);
}

int
ess_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct ess_softc *sc = addr;
    
	DPRINTFN(5,("ess_get_port: port=%d\n", cp->dev));

	switch (cp->dev) {
	case ESS_MASTER_VOL:
	case ESS_DAC_PLAY_VOL:
	case ESS_MIC_PLAY_VOL:
	case ESS_LINE_PLAY_VOL:
	case ESS_SYNTH_PLAY_VOL:
	case ESS_CD_PLAY_VOL:
	case ESS_AUXB_PLAY_VOL:
	case ESS_RECORD_VOL:
		switch (cp->un.value.num_channels) {
		case 1:
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = 
				sc->gain[cp->dev][ESS_LEFT];
			break;
		case 2:
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = 
				sc->gain[cp->dev][ESS_LEFT];
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = 
				sc->gain[cp->dev][ESS_RIGHT];
			break;
		default:
			return EINVAL;
		}
		return (0);

	case ESS_PCSPEAKER_VOL:
		if (cp->un.value.num_channels != 1)
			return EINVAL;

		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = 
			sc->gain[cp->dev][ESS_LEFT];
		return (0);

	case ESS_RECORD_SOURCE:
		if (sc->sc_model == ESS_1788)
			cp->un.ord = sc->in_port;
		else
			cp->un.mask = sc->in_mask;
		return (0);

	case ESS_RECORD_MONITOR:
		cp->un.ord = (ess_read_x_reg(sc, ESS_XCMD_AUDIO_CTRL) &
			      ESS_AUDIO_CTRL_MONITOR) ? 1 : 0;
		return (0);
	}

	if (sc->sc_model == ESS_1788)
		return (EINVAL);

	switch (cp->dev) {
	case ESS_DAC_REC_VOL:
	case ESS_MIC_REC_VOL:
	case ESS_LINE_REC_VOL:
	case ESS_SYNTH_REC_VOL:
	case ESS_CD_REC_VOL:
	case ESS_AUXB_REC_VOL:
		switch (cp->un.value.num_channels) {
		case 1:
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = 
				sc->gain[cp->dev][ESS_LEFT];
			break;
		case 2:
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = 
				sc->gain[cp->dev][ESS_LEFT];
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = 
				sc->gain[cp->dev][ESS_RIGHT];
			break;
		default:
			return EINVAL;
		}
		return (0);

	case ESS_MIC_PREAMP:
		cp->un.ord = (ess_read_x_reg(sc, ESS_XCMD_PREAMP_CTRL) &
			      ESS_PREAMP_CTRL_ENABLE) ? 1 : 0;
		return (0);
	}

	return (EINVAL);
}

int
ess_query_devinfo(addr, dip)
	void *addr;
	mixer_devinfo_t *dip;
{
	struct ess_softc *sc = addr;

	DPRINTFN(5,("ess_query_devinfo: model=%d index=%d\n", 
		    sc->sc_model, dip->index));

	/*
	 * REVISIT: There are some slight differences between the
	 *          mixers on the different ESS chips, which can
	 *          be sorted out using the chip model rather than a
	 *          separate mixer model.
	 *          This is currently coded assuming an ES1887; we
	 *          need to work out which bits are not applicable to
	 *          the other models (1888 and 888).
	 */
	switch (dip->index) {
	case ESS_DAC_PLAY_VOL:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNdac);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_MIC_PLAY_VOL:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		if (sc->sc_model == ESS_1788)
			dip->next = AUDIO_MIXER_LAST;
		else
			dip->next = ESS_MIC_PREAMP;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_LINE_PLAY_VOL:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNline);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_SYNTH_PLAY_VOL:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNfmsynth);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_CD_PLAY_VOL:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNcd);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_AUXB_PLAY_VOL:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "auxb");
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_INPUT_CLASS:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCinputs);
		dip->type = AUDIO_MIXER_CLASS;
		return (0);

	case ESS_MASTER_VOL:
		dip->mixer_class = ESS_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmaster);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_PCSPEAKER_VOL:
		dip->mixer_class = ESS_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "pc_speaker");
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_OUTPUT_CLASS:
		dip->mixer_class = ESS_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCoutputs);
		dip->type = AUDIO_MIXER_CLASS;
		return (0);

	case ESS_RECORD_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNrecord);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_RECORD_SOURCE:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNsource);
		if (sc->sc_model == ESS_1788) {
			/*
			 * The 1788 doesn't use the input mixer control that
			 * the 1888 uses, because it's a pain when you only
			 * have one mixer. 
			 * Perhaps it could be emulated by keeping both sets of
			 * gain values, and doing a `context switch' of the
			 * mixer registers when shifting from playing to
			 * recording.
			 */
			dip->type = AUDIO_MIXER_ENUM;
			dip->un.e.num_mem = 4;
			strcpy(dip->un.e.member[0].label.name, AudioNmicrophone);
			dip->un.e.member[0].ord = ESS_SOURCE_MIC;
			strcpy(dip->un.e.member[1].label.name, AudioNline);
			dip->un.e.member[1].ord = ESS_SOURCE_LINE;
			strcpy(dip->un.e.member[2].label.name, AudioNcd);
			dip->un.e.member[2].ord = ESS_SOURCE_CD;
			strcpy(dip->un.e.member[3].label.name, AudioNmixerout);
			dip->un.e.member[3].ord = ESS_SOURCE_MIXER;
		} else {
			dip->type = AUDIO_MIXER_SET;
			dip->un.s.num_mem = 6;
			strcpy(dip->un.s.member[0].label.name, AudioNdac);
			dip->un.s.member[0].mask = 1 << ESS_DAC_REC_VOL;
			strcpy(dip->un.s.member[1].label.name, AudioNmicrophone);
			dip->un.s.member[1].mask = 1 << ESS_MIC_REC_VOL;
			strcpy(dip->un.s.member[2].label.name, AudioNline);
			dip->un.s.member[2].mask = 1 << ESS_LINE_REC_VOL;
			strcpy(dip->un.s.member[3].label.name, AudioNfmsynth);
			dip->un.s.member[3].mask = 1 << ESS_SYNTH_REC_VOL;
			strcpy(dip->un.s.member[4].label.name, AudioNcd);
			dip->un.s.member[4].mask = 1 << ESS_CD_REC_VOL;
			strcpy(dip->un.s.member[5].label.name, "auxb");
			dip->un.s.member[5].mask = 1 << ESS_AUXB_REC_VOL;
		}
		return (0);

	case ESS_RECORD_CLASS:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCrecord);
		dip->type = AUDIO_MIXER_CLASS;
		return (0);

	case ESS_RECORD_MONITOR:
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmute);
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = ESS_MONITOR_CLASS;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		return (0);

	case ESS_MONITOR_CLASS:
		dip->mixer_class = ESS_MONITOR_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCmonitor);
		dip->type = AUDIO_MIXER_CLASS;
		return (0);
	}

	if (sc->sc_model == ESS_1788)
		return (ENXIO);

	switch (dip->index) {
	case ESS_DAC_REC_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNdac);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_MIC_REC_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_LINE_REC_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNline);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_SYNTH_REC_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNfmsynth);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_CD_REC_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNcd);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_AUXB_REC_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "auxb");
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return (0);

	case ESS_MIC_PREAMP:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->prev = ESS_MIC_PLAY_VOL;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNpreamp);
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		return (0);
	}

	return (ENXIO);
}

void *
ess_malloc(addr, direction, size, pool, flags)
	void *addr;
	int direction;
	size_t size;
	int pool, flags;
{
	struct ess_softc *sc = addr;
	int drq;

	if (sc->sc_model != ESS_1788 && direction == AUMODE_PLAY)
		drq = sc->sc_audio2.drq;
	else
		drq = sc->sc_audio1.drq;
	return (isa_malloc(sc->sc_ic, drq, size, pool, flags));
}

void
ess_free(addr, ptr, pool)
	void *addr;
	void *ptr;
	int pool;
{
	isa_free(ptr, pool);
}

size_t
ess_round_buffersize(addr, direction, size)
	void *addr;
	int direction;
	size_t size;
{
	if (size > MAX_ISADMA)
		size = MAX_ISADMA;
	return (size);
}

int
ess_mappage(addr, mem, off, prot)
	void *addr;
	void *mem;
	int off;
	int prot;
{
	return (isa_mappage(mem, off, prot));
}

int
ess_1788_get_props(addr)
	void *addr;
{

	return (AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT);
}

int
ess_1888_get_props(addr)
	void *addr;
{

	return (AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX);
}

/* ============================================
 * Generic functions for ess, not used by audio h/w i/f
 * =============================================
 */

/*
 * Reset the chip.
 * Return non-zero if the chip isn't detected.
 */
int
ess_reset(sc)
	struct ess_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	sc->sc_audio1.active = 0;
	sc->sc_audio2.active = 0;

	EWRITE1(iot, ioh, ESS_DSP_RESET, ESS_RESET_EXT);
	delay(10000);
	EWRITE1(iot, ioh, ESS_DSP_RESET, 0);
	if (ess_rdsp(sc) != ESS_MAGIC)
		return (1);

	/* Enable access to the ESS extension commands. */
	ess_wdsp(sc, ESS_ACMD_ENABLE_EXT);

	return (0);
}

void
ess_set_gain(sc, port, on)
	struct ess_softc *sc;
	int port;
	int on;
{
	int gain, left, right;
	int mix;
	int src;
	int stereo;

	/*
	 * Most gain controls are found in the mixer registers and
	 * are stereo. Any that are not, must set mix and stereo as
	 * required.
	 */
	mix = 1;
	stereo = 1;

	switch (port) {
	case ESS_MASTER_VOL:
		src = ESS_MREG_VOLUME_MASTER;
		break;
	case ESS_DAC_PLAY_VOL:
		if (sc->sc_model == ESS_1788)
			src = ESS_MREG_VOLUME_VOICE;
		else
			src = 0x7C;
		break;
	case ESS_MIC_PLAY_VOL:
		src = ESS_MREG_VOLUME_MIC;
		break;
	case ESS_LINE_PLAY_VOL:
		src = ESS_MREG_VOLUME_LINE;
		break;
	case ESS_SYNTH_PLAY_VOL:
		src = ESS_MREG_VOLUME_SYNTH;
		break;
	case ESS_CD_PLAY_VOL:
		src = ESS_MREG_VOLUME_CD;
		break;
	case ESS_AUXB_PLAY_VOL:
		src = ESS_MREG_VOLUME_AUXB;
		break;
	case ESS_PCSPEAKER_VOL:
		src = ESS_MREG_VOLUME_PCSPKR;
		stereo = 0;
		break;
	case ESS_DAC_REC_VOL:
		src = 0x69;
		break;
	case ESS_MIC_REC_VOL:
		src = 0x68;
		break;
	case ESS_LINE_REC_VOL:
		src = 0x6E;
		break;
	case ESS_SYNTH_REC_VOL:
		src = 0x6B;
		break;
	case ESS_CD_REC_VOL:
		src = 0x6A;
		break;
	case ESS_AUXB_REC_VOL:
		src = 0x6C;
		break;
	case ESS_RECORD_VOL:
		src = ESS_XCMD_VOLIN_CTRL;
		mix = 0;
		break;
	default:
		return;
	}

	/* 1788 doesn't have a separate recording mixer */
	if (sc->sc_model == ESS_1788 && mix && src > 0x62)
		return;

	if (on) {
		left = sc->gain[port][ESS_LEFT];
		right = sc->gain[port][ESS_RIGHT];
	} else {
		left = right = 0;
	}

	if (stereo)
		gain = ESS_STEREO_GAIN(left, right);
	else
		gain = ESS_MONO_GAIN(left);

	if (mix)
		ess_write_mix_reg(sc, src, gain);
	else
		ess_write_x_reg(sc, src, gain);
}

/* Set the input device on devices without an input mixer. */
int
ess_set_in_port(sc, ord)
	struct ess_softc *sc;
	int ord;
{
	mixer_devinfo_t di;
	int i;

	DPRINTF(("ess_set_in_port: ord=0x%x\n", ord));

	/*
	 * Get the device info for the record source control,
	 * including the list of available sources.
	 */
	di.index = ESS_RECORD_SOURCE;
	if (ess_query_devinfo(sc, &di))
		return EINVAL;

	/* See if the given ord value was anywhere in the list. */
	for (i = 0; i < di.un.e.num_mem; i++) {
		if (ord == di.un.e.member[i].ord)
			break;
	}
	if (i == di.un.e.num_mem)
		return EINVAL;

	ess_write_mix_reg(sc, ESS_MREG_ADC_SOURCE, ord);

	sc->in_port = ord;
	return (0);
}

/* Set the input device levels on input-mixer-enabled devices. */
int
ess_set_in_ports(sc, mask)
	struct ess_softc *sc;
	int mask;
{
	mixer_devinfo_t di;
	int i, port;

	DPRINTF(("ess_set_in_ports: mask=0x%x\n", mask));

	/*
	 * Get the device info for the record source control,
	 * including the list of available sources.
	 */
	di.index = ESS_RECORD_SOURCE;
	if (ess_query_devinfo(sc, &di))
		return EINVAL;

	/*
	 * Set or disable the record volume control for each of the
	 * possible sources.
	 */
	for (i = 0; i < di.un.s.num_mem; i++) {
		/*
		 * Calculate the source port number from its mask.
		 */
		port = ffs(di.un.s.member[i].mask);

		/*
		 * Set the source gain:
		 *	to the current value if source is enabled
		 *	to zero if source is disabled
		 */
		ess_set_gain(sc, port, mask & di.un.s.member[i].mask);
	}

	sc->in_mask = mask;
	return (0);
}

void
ess_speaker_on(sc)
	struct ess_softc *sc;
{
	/* Disable mute on left- and right-master volume. */
	ess_clear_mreg_bits(sc, ESS_MREG_VOLUME_LEFT, ESS_VOLUME_MUTE);
	ess_clear_mreg_bits(sc, ESS_MREG_VOLUME_RIGHT, ESS_VOLUME_MUTE);
}

void
ess_speaker_off(sc)
	struct ess_softc *sc;
{
	/* Enable mute on left- and right-master volume. */
	ess_set_mreg_bits(sc, ESS_MREG_VOLUME_LEFT, ESS_VOLUME_MUTE);
	ess_set_mreg_bits(sc, ESS_MREG_VOLUME_RIGHT, ESS_VOLUME_MUTE);
}

/*
 * Calculate the time constant for the requested sampling rate.
 */
u_int
ess_srtotc(rate)
	u_int rate;
{
	u_int tc;

	/* The following formulae are from the ESS data sheet. */
	if (rate <= 22050)
		tc = 128 - 397700L / rate;
	else
		tc = 256 - 795500L / rate;

	return (tc);
}


/*
 * Calculate the filter constant for the reuqested sampling rate.
 */
u_int
ess_srtofc(rate)
	u_int rate;
{
	/*
	 * The following formula is derived from the information in
	 * the ES1887 data sheet, based on a roll-off frequency of
	 * 87%.
	 */
	return (256 - 200279L / rate);
}


/*
 * Return the status of the DSP.
 */
u_char
ess_get_dsp_status(sc)
	struct ess_softc *sc;
{
	return (EREAD1(sc->sc_iot, sc->sc_ioh, ESS_DSP_RW_STATUS));
}


/*
 * Return the read status of the DSP:	1 -> DSP ready for reading
 *					0 -> DSP not ready for reading
 */
u_char
ess_dsp_read_ready(sc)
	struct ess_softc *sc;
{
	return ((ess_get_dsp_status(sc) & ESS_DSP_READ_READY) ? 1 : 0);
}


/*
 * Return the write status of the DSP:	1 -> DSP ready for writing
 *					0 -> DSP not ready for writing
 */
u_char
ess_dsp_write_ready(sc)
	struct ess_softc *sc;
{
	return ((ess_get_dsp_status(sc) & ESS_DSP_WRITE_BUSY) ? 0 : 1);
}


/*
 * Read a byte from the DSP.
 */
int
ess_rdsp(sc)
	struct ess_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i;

	for (i = ESS_READ_TIMEOUT; i > 0; --i) {
		if (ess_dsp_read_ready(sc)) {
			i = EREAD1(iot, ioh, ESS_DSP_READ);
			DPRINTFN(8,("ess_rdsp() = 0x%02x\n", i));
			return i;
		} else
			delay(10);
	}

	DPRINTF(("ess_rdsp: timed out\n"));
	return (-1);
}

/*
 * Write a byte to the DSP.
 */
int
ess_wdsp(sc, v)
	struct ess_softc *sc;
	u_char v;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i;

	DPRINTFN(8,("ess_wdsp(0x%02x)\n", v));

	for (i = ESS_WRITE_TIMEOUT; i > 0; --i) {
		if (ess_dsp_write_ready(sc)) {
			EWRITE1(iot, ioh, ESS_DSP_WRITE, v);
			return (0);
		} else
			delay(10);
	}

	DPRINTF(("ess_wdsp(0x%02x): timed out\n", v));
	return (-1);
}

/*
 * Write a value to one of the ESS extended registers.
 */
int
ess_write_x_reg(sc, reg, val)
	struct ess_softc *sc;
	u_char reg;
	u_char val;
{
	int error;

	DPRINTFN(2,("ess_write_x_reg: %02x=%02x\n", reg, val));
	if ((error = ess_wdsp(sc, reg)) == 0)
		error = ess_wdsp(sc, val);

	return error;
}

/*
 * Read the value of one of the ESS extended registers.
 */
u_char
ess_read_x_reg(sc, reg)
	struct ess_softc *sc;
	u_char reg;
{
	int error;
	int val;

	if ((error = ess_wdsp(sc, 0xC0)) == 0)
		error = ess_wdsp(sc, reg);
	if (error)
		DPRINTF(("Error reading extended register 0x%02x\n", reg));
/* REVISIT: what if an error is returned above? */
	val = ess_rdsp(sc);
	DPRINTFN(2,("ess_read_x_reg: %02x=%02x\n", reg, val));
	return val;
}

void
ess_clear_xreg_bits(sc, reg, mask)
	struct ess_softc *sc;
	u_char reg;
	u_char mask;
{
	if (ess_write_x_reg(sc, reg, ess_read_x_reg(sc, reg) & ~mask) == -1)
		DPRINTF(("Error clearing bits in extended register 0x%02x\n",
			 reg));
}

void
ess_set_xreg_bits(sc, reg, mask)
	struct ess_softc *sc;
	u_char reg;
	u_char mask;
{
	if (ess_write_x_reg(sc, reg, ess_read_x_reg(sc, reg) | mask) == -1)
		DPRINTF(("Error setting bits in extended register 0x%02x\n",
			 reg));
}


/*
 * Write a value to one of the ESS mixer registers.
 */
void
ess_write_mix_reg(sc, reg, val)
	struct ess_softc *sc;
	u_char reg;
	u_char val;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int s;

	DPRINTFN(2,("ess_write_mix_reg: %x=%x\n", reg, val));

	s = splaudio();
	EWRITE1(iot, ioh, ESS_MIX_REG_SELECT, reg);
	EWRITE1(iot, ioh, ESS_MIX_REG_DATA, val);
	splx(s);
}

/*
 * Read the value of one of the ESS mixer registers.
 */
u_char
ess_read_mix_reg(sc, reg)
	struct ess_softc *sc;
	u_char reg;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int s;
	u_char val;

	s = splaudio();
	EWRITE1(iot, ioh, ESS_MIX_REG_SELECT, reg);
	val = EREAD1(iot, ioh, ESS_MIX_REG_DATA);
	splx(s);

	DPRINTFN(2,("ess_read_mix_reg: %x=%x\n", reg, val));
	return val;
}

void
ess_clear_mreg_bits(sc, reg, mask)
	struct ess_softc *sc;
	u_char reg;
	u_char mask;
{
	ess_write_mix_reg(sc, reg, ess_read_mix_reg(sc, reg) & ~mask);
}

void
ess_set_mreg_bits(sc, reg, mask)
	struct ess_softc *sc;
	u_char reg;
	u_char mask;
{
	ess_write_mix_reg(sc, reg, ess_read_mix_reg(sc, reg) | mask);
}
