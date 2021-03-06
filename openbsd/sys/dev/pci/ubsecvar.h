/*	$OpenBSD: ubsecvar.h,v 1.9 2000/08/13 22:06:48 deraadt Exp $	*/

/*
 * Copyright (c) 2000 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

struct ubsec_softc {
	struct	device		sc_dv;		/* generic device */
	void			*sc_ih;		/* interrupt handler cookie */
	bus_space_handle_t	sc_sh;		/* memory handle */
	bus_space_tag_t		sc_st;		/* memory tag */
	bus_dma_tag_t		sc_dmat;	/* dma tag */
	int			sc_5601;	/* device is 5601 */
	int32_t			sc_cid;		/* crypto tag */
	SIMPLEQ_HEAD(,ubsec_q)	sc_queue;	/* packet queue */
	int			sc_nqueue;	/* count enqueued */
	SIMPLEQ_HEAD(,ubsec_q)	sc_qchip;	/* on chip */
	int			sc_nsessions;	/* # of sessions */
	struct ubsec_session	*sc_sessions;	/* sessions */
};

struct ubsec_q {
	SIMPLEQ_ENTRY(ubsec_q)		q_next;
	struct cryptop			*q_crp;
	struct ubsec_mcr		*q_mcr;
	struct ubsec_pktbuf		q_srcpkt[MAX_SCATTER-1];
	struct ubsec_pktbuf		q_dstpkt[MAX_SCATTER-1];
	struct ubsec_pktctx		q_ctx;

	struct ubsec_softc		*q_sc;
	struct mbuf 		      	*q_src_m, *q_dst_m;

	long				q_src_packp[MAX_SCATTER];
	int				q_src_packl[MAX_SCATTER];
	int				q_src_npa, q_src_l;

	long				q_dst_packp[MAX_SCATTER];
	int				q_dst_packl[MAX_SCATTER];
	int				q_dst_npa, q_dst_l;
	u_int32_t			q_macbuf[5];
	int				q_sesn;
};

struct ubsec_session {
	u_int32_t	ses_used;
	u_int32_t	ses_deskey[6];		/* 3DES key */
	u_int32_t       ses_hminner[5];		/* hmac inner state */
	u_int32_t       ses_hmouter[5];		/* hmac outer state */
	u_int32_t       ses_iv[2];		/* [3]DES iv */
};

/* Maximum queue length */
#ifndef UBS_MAX_NQUEUE
#define UBS_MAX_NQUEUE		60
#endif
