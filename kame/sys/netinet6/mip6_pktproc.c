/*	$KAME: mip6_pktproc.c,v 1.2 2002/05/15 06:37:09 k-sugyou Exp $	*/

/*
 * Copyright (C) 2002 WIDE Project.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include "opt_ipsec.h"
#include "opt_inet6.h"
#include "opt_mip6.h"
#endif
#ifdef __NetBSD__
#include "opt_ipsec.h"
#endif

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>

#include <net/if_hif.h>

#include <netinet6/mip6_var.h>
#include <netinet6/mip6.h>

#include <net/net_osdep.h>

extern struct mip6_bc_list mip6_bc_list;
extern struct mip6_prefix_list mip6_prefix_list;

static int mip6_ip6mu_process __P((struct mbuf *,
				   struct ip6m_binding_update *, int));
static int mip6_ip6ma_process __P((struct mbuf *,
				   struct ip6m_binding_ack *, int));
static int mip6_cksum __P((struct sockaddr_in6 *,
			   struct sockaddr_in6 *,
			   u_int32_t, u_int8_t,	char *));

int
mip6_ip6mu_input(m, ip6mu, ip6mulen)
	struct mbuf *m;
	struct ip6m_binding_update *ip6mu;
	int ip6mulen;
{
	struct ip6_hdr *ip6;
	struct sockaddr_in6 *src_sa, *dst_sa;
	struct mbuf *n;
	struct ip6aux *ip6a;
	struct mip6_bc *mbc;
	u_int16_t seqno;
	int error;

	ip6 = mtod(m, struct ip6_hdr *);
	if (ip6_getpktaddrs(m, &src_sa, &dst_sa)) {
		/* must not happen. */
		return (EINVAL);
	}

#if 0
	/* home registration must be protected by ESP. */
	if (((ip6mu_flags & IP6MU_HOME) != 0) &&
	    ((m->m_flags & M_DECRYPTED) == 0)) {
		mip6log((LOG_NOTICE,
			 "%s:%d: unprotected binging update from host %s\n",
			 __FILE__, __LINE__,
			 ip6_sprintf(&src_sa->sin6_addr)));
		/* discard the packet. */
		return (EACCES);
	}
#endif
	
	/* check if this packet has a HAO. */
	n = ip6_findaux(m);
	if (n == NULL) {
		mip6log((LOG_NOTICE,
			 "%s:%d: no ip6aux found "
			 "in the binding update from host %s.\n",
			 __FILE__, __LINE__,
			 ip6_sprintf(&src_sa->sin6_addr)));
		/* discard. */
		return (EINVAL);
	}
	ip6a = mtod(n, struct ip6aux *);
	if ((ip6a == NULL) || (ip6a->ip6a_flags & IP6A_HASEEN) == 0) {
		mip6log((LOG_NOTICE,
			 "%s:%d: no home address destination option found "
			 "in the binding update from host %s.\n",
			 __FILE__, __LINE__,
			 ip6_sprintf(&src_sa->sin6_addr)));
		/* discard. */
		return (EINVAL);
	}

	/* packet length check. */
	if (ip6mulen < sizeof(struct ip6m_binding_update)) {
		mip6log((LOG_NOTICE,
			 "%s:%d: too short binding update (len = %d) "
			 "from host %s.\n",
			 __FILE__, __LINE__,
			 ip6mulen,
			 ip6_sprintf(&src_sa->sin6_addr)));
		/* discard */
		return (EINVAL);
	}

	/* ip6_src and HAO has been already swapped at this point. */
	mbc = mip6_bc_list_find_withphaddr(&mip6_bc_list, src_sa);
	if ((mbc == NULL) &&
	    ((ip6mu->ip6mu_flags & IP6MU_HOME) == 0)) {
		/*
		 * this packet must have been dropped in
		 * dest6_input()/esp_input() already.
		 */
		panic("this packet must be dropped before.");
	}

	if (mbc == NULL)
		goto check_mobility_options;

	/* check a sequence number. */
	seqno = ntohs(ip6mu->ip6mu_seqno);
	if (MIP6_LEQ(seqno, mbc->mbc_seqno)) {
		mip6log((LOG_NOTICE,
			 "%s:%d: received sequence no (%d) <= current "
			 "seq no (%d) in BU from host %s.\n",
			 __FILE__, __LINE__,
			 seqno,
			 mbc->mbc_seqno, ip6_sprintf(&ip6->ip6_src)));
		/*
		 * the seqno of this binding update is smaller than the
		 * corresponding binding cache.  we send TOO_SMALL
		 * binding ack as an error.  in this case, we use the
		 * coa of the incoming packet instead of the coa
		 * stored in the binding cache as a destination
		 * addrress.  because the sending mobile node's coa
		 * might have changed after it had registered before.
		 */
		{
			struct sockaddr_in6 pcoa;
			pcoa = ip6a->ip6a_src;
			pcoa.sin6_addr = ip6a->ip6a_coa;
			in6_clearscope(&pcoa.sin6_addr);
			error = mip6_bc_send_ba(&mbc->mbc_addr,
						&mbc->mbc_phaddr,
						&pcoa,
						MIP6_BA_STATUS_SEQNO_TOO_SMALL,
						mbc->mbc_seqno,
						0, 0);
		}
		/* discard. */
		return (EINVAL);
	}
	

 check_mobility_options:

	/* XXX parameter processing. */

	/* XXX cookie processing. */

	return (mip6_ip6mu_process(m, ip6mu, ip6mulen));
}

static int
mip6_ip6mu_process(m, ip6mu, ip6mulen)
	struct mbuf *m;
	struct ip6m_binding_update *ip6mu;
	int ip6mulen;
{
	struct ip6_hdr *ip6;
	struct sockaddr_in6 *src_sa, *dst_sa;
	struct sockaddr_in6 *coa_sa, coa_storage;
	struct mbuf *n;
	struct ip6aux *ip6a;
	u_int32_t lifetime;
	u_int16_t seqno;
	int error;

	ip6 = mtod(m, struct ip6_hdr *);
	if (ip6_getpktaddrs(m, &src_sa, &dst_sa)) {
		/* must not happen. */
		return (EINVAL);
	}
	n = ip6_findaux(m);
	if (!n)
		return (EINVAL);
	ip6a = mtod(n, struct ip6aux *);
	if (ip6a == NULL)
		return (EINVAL);

	/* XXX find alt CoA. */
	coa_storage = *src_sa;
	coa_storage.sin6_addr = ip6a->ip6a_coa;
	coa_sa = &coa_storage;

	lifetime = ntohl(ip6mu->ip6mu_lifetime);
	seqno = ntohs(ip6mu->ip6mu_seqno);

#define IS_REQUEST_TO_CACHE(lifetime, hoa, coa)	\
	(((lifetime) != 0) &&			\
	 (!SA6_ARE_ADDR_EQUAL((hoa), (coa))))

	if (ip6mu->ip6mu_flags & IP6MU_HOME) {
		/* request for the home (un)registration. */
		if (!MIP6_IS_HA) {
			/* this is not a homeagent. */
			/* XXX */
			mip6_bc_send_ba(dst_sa, src_sa, coa_sa,
					MIP6_BA_STATUS_NOT_SUPPORTED,
					seqno, 0, 0);
			return (0);
		}

		/* limit the max duration of bindings. */
		if (lifetime > mip6_config.mcfg_hrbc_lifetime_limit)
			lifetime = mip6_config.mcfg_hrbc_lifetime_limit;

		if (IS_REQUEST_TO_CACHE(lifetime, src_sa, coa_sa)) {
			error = mip6_process_hrbu(src_sa,
						  coa_sa,
						  ip6mu->ip6mu_flags,
						  seqno,
						  lifetime,
						  dst_sa);
			if (error) {
				mip6log((LOG_ERR,
					 "%s:%d: home registration failed\n",
					 __FILE__, __LINE__));
				/* continue. */
			}
		} else {
			error = mip6_process_hurbu(src_sa,
						   coa_sa,
						   ip6mu->ip6mu_flags,
						   seqno,
						   lifetime,
						   dst_sa);
			if (error) {
				mip6log((LOG_ERR,
					 "%s:%d: home unregistration failed\n",
					 __FILE__, __LINE__));
				/* continue. */
			}
		}
	} else {
		/* request to cache/remove a binding. */
		/* CN part XXX */
	}

	
	return (0);
}

int
mip6_ip6ma_input(m, ip6ma, ip6malen)
	struct mbuf *m;
	struct ip6m_binding_ack *ip6ma;
	int ip6malen;
{
	struct ip6_hdr *ip6;
	struct sockaddr_in6 *src_sa, *dst_sa;
	struct hif_softc *sc;
	struct mip6_bu *mbu;
	u_int16_t seqno;

	ip6 = mtod(m, struct ip6_hdr *);
	if (ip6_getpktaddrs(m, &src_sa, &dst_sa)) {
		/* must not happen. */
		return (EINVAL);
	}

	/* XXX autorization */

	/* packet length check. */
	if (ip6malen < sizeof(struct ip6m_binding_ack)) {
		mip6log((LOG_NOTICE,
			 "%s:%d: too short binding ack (len = %d) "
			 "from host %s.\n",
			 __FILE__, __LINE__,
			 ip6malen,
			 ip6_sprintf(&src_sa->sin6_addr)));
		/* discard */
		return (EINVAL);
	}

	/*
         * check if the sequence number of the binding update sent ==
         * the sequence number of the binding ack received.
         */
	sc = hif_list_find_withhaddr(dst_sa);
	if (sc == NULL) {
                /*
                 * if we receive a binding ack before sending binding
                 * updates(!), sc will be NULL.
                 */
                mip6log((LOG_NOTICE,
                         "%s:%d: no hif interface found.\n",
                         __FILE__, __LINE__));
                /* silently ignore. */
                return (EINVAL);
	}
	mbu = mip6_bu_list_find_withpaddr(&sc->hif_bu_list, src_sa, dst_sa);
	if (mbu == NULL) {
		mip6log((LOG_NOTICE,
                         "%s:%d: no matching binding update entry found.\n",
                         __FILE__, __LINE__));
                /* silently ignore */
                return (EINVAL);
	}
	seqno = htons(ip6ma->ip6ma_seqno);
	if (ip6ma->ip6ma_status == IP6MA_STATUS_SEQNO_TOO_SMALL) {
                /*
                 * our home agent has a greater sequence number in its
                 * binging cache entriy of mine.  we should resent
                 * binding update with greater than the sequence
                 * number of the binding cache already exists in our
                 * home agent.  this binding ack is valid though the
                 * sequence number doesn't match.
                 */
		goto check_mobility_options;
 	}
	if (seqno != mbu->mbu_seqno) {
                mip6log((LOG_NOTICE,
                         "%s:%d: unmached sequence no "
                         "(%d recv, %d sent) from host %s.\n",
                         __FILE__, __LINE__,
                         seqno,
                         mbu->mbu_seqno,
                         ip6_sprintf(&ip6->ip6_src)));
                /* silently ignore. */
                /* discard */
                return (EINVAL);
	}

 check_mobility_options:

	return(mip6_ip6ma_process(m, ip6ma, ip6malen));
}

static int
mip6_ip6ma_process(m, ip6ma, ip6malen)
	struct mbuf *m;
	struct ip6m_binding_ack *ip6ma;
	int ip6malen;
{
	struct ip6_hdr *ip6;
	struct sockaddr_in6 *src_sa, *dst_sa;
	struct hif_softc *sc;
	struct mip6_bu *mbu;
	u_int32_t lifetime;
#if !(defined(__FreeBSD__) && __FreeBSD__ >= 3)
	long time_second = time.tv_sec;
#endif
	int error = 0;

	ip6 = mtod(m, struct ip6_hdr *);
	if (ip6_getpktaddrs(m, &src_sa, &dst_sa)) {
		/* must not happen. */
		return (EINVAL);
	}

	sc = hif_list_find_withhaddr(dst_sa);
	if (sc == NULL) {
		/* must not happen? */
                mip6log((LOG_NOTICE,
                         "%s:%d: no hif interface found.\n",
                         __FILE__, __LINE__, ip6_sprintf(&ip6->ip6_src)));
		return (EINVAL);
	}

	mbu = mip6_bu_list_find_withpaddr(&sc->hif_bu_list, src_sa, dst_sa);
	if (mbu == NULL) {
                mip6log((LOG_NOTICE,
                         "%s:%d: no matching binding update entry found "
			 "from host %s.\n",
                         __FILE__, __LINE__, ip6_sprintf(&ip6->ip6_src)));
                /* ignore */
                return (EINVAL);
	}

	if (ip6ma->ip6ma_status >= IP6MA_STATUS_ERRORBASE) {
                mip6log((LOG_NOTICE, 
                         "%s:%d: a binding update was rejected "
			 "(error code %d).\n",
                         __FILE__, __LINE__, ip6ma->ip6ma_status));
		if (ip6ma->ip6ma_status == IP6MA_STATUS_NOT_HOME_AGENT &&
		    mbu->mbu_flags & IP6MU_HOME &&
		    mbu->mbu_reg_state == MIP6_BU_REG_STATE_REGWAITACK) {
			/* XXX no registration? */
			goto success;
		}
		if (ip6ma->ip6ma_status == IP6MA_STATUS_SEQNO_TOO_SMALL) {
			/* seqno is too small.  adjust it and resend. */
			mbu->mbu_seqno = ip6ma->ip6ma_seqno + 1;
			mbu->mbu_state |= MIP6_BU_STATE_WAITSENT;
			return (0);
		}

                /* sending binding update failed. */
                error = mip6_bu_list_remove(&sc->hif_bu_list, mbu);
                if (error) {
                        mip6log((LOG_ERR,
                                 "%s:%d: can't remove BU.\n",
                                 __FILE__, __LINE__));
                        return (error);
                }
                /* XXX some error recovery process needed. */
                return (0);
        }

 success:
	/*
	 * the binding update has been accepted.
	 */

	/* reset WAIT_ACK state. */
	mbu->mbu_state &= ~MIP6_BU_STATE_WAITACK;

	/* update lifetime and refresh time. */
	lifetime = htonl(ip6ma->ip6ma_lifetime);
	if (lifetime < mbu->mbu_lifetime) {
		mbu->mbu_expire -= (mbu->mbu_lifetime - lifetime);
		if (mbu->mbu_expire < time_second)
			mbu->mbu_expire = time_second;
	}
	mbu->mbu_refresh = htonl(ip6ma->ip6ma_refresh);
	mbu->mbu_refexpire = time_second + mbu->mbu_refresh;
	/* sanity check for overflow */
        if (mbu->mbu_refexpire < time_second)
                mbu->mbu_refexpire = 0x7fffffff;
        if (mbu->mbu_refresh > mbu->mbu_expire)
                mbu->mbu_refresh = mbu->mbu_expire;
	if (mbu->mbu_flags & IP6MU_HOME) {
		/* this is from our home agent. */
		if (mbu->mbu_reg_state == MIP6_BU_REG_STATE_DEREGWAITACK) {
			/* home unregsitration has completed. */

			/* notify all the CNs that we are home. */
			error = mip6_bu_list_notify_binding_change(sc);
			if (error) {
				mip6log((LOG_ERR,
					 "%s:%d: removing the bining cache entries of all CNs failed.\n",
					 __FILE__, __LINE__));
				return (error);
			}

			/* remove a tunnel to our homeagent. */
			error = mip6_tunnel_control(MIP6_TUNNEL_DELETE,
						   mbu,
						   mip6_bu_encapcheck,
						   &mbu->mbu_encap);
			if (error) {
				mip6log((LOG_ERR,
					 "%s:%d: tunnel removal failed.\n",
					 __FILE__, __LINE__));
				return (error);
			}

			error = mip6_bu_list_remove_all(&sc->hif_bu_list);
			if (error) {
				mip6log((LOG_ERR,
					 "%s:%d: BU remove all failed.\n",
					 __FILE__, __LINE__));
				return (error);
			}

			/* XXX: send a unsolicited na. */
		{
			struct sockaddr_in6 sa6, daddr, taddr; /* XXX */
			struct ifaddr *ifa;

			bzero(&sa6, sizeof(sa6));
			sa6.sin6_family = AF_INET6;
			sa6.sin6_len = sizeof(sa6);
			/* XXX or mbu_haddr.  XXX scope consideration  */
			sa6_copy_addr(&mbu->mbu_coa, &sa6);
#ifndef SCOPEDROUTING
			sa6.sin6_scope_id = 0;
#endif

			if ((ifa = ifa_ifwithaddr((struct sockaddr *)&sa6))
			    == NULL) {
				mip6log((LOG_ERR,
					 "%s:%d: can't find CoA interface\n",
					 __FILE__, __LINE__));
				return(EINVAL);	/* XXX */
			}

			bzero(&daddr, sizeof(daddr));
			daddr.sin6_family = AF_INET6;
			daddr.sin6_len = sizeof(daddr);
			daddr.sin6_addr = in6addr_linklocal_allnodes;
			if (in6_addr2zoneid(ifa->ifa_ifp, &daddr.sin6_addr,
					    &daddr.sin6_scope_id)) {
				/* XXX: should not happen */
				mip6log((LOG_ERR,
					 "%s:%d: in6_addr2zoneid failed\n",
					 __FILE__, __LINE__));
				return(EIO);
			}
			if ((error = in6_embedscope(&daddr.sin6_addr,
						    &daddr))) {
				/* XXX: should not happen */
				mip6log((LOG_ERR,
					 "%s:%d: in6_embedscope failed\n",
					 __FILE__, __LINE__));
				return(error);
			}

			bzero(&taddr, sizeof(taddr));
			taddr.sin6_family = AF_INET6;
			taddr.sin6_len = sizeof(taddr);
			sa6_copy_addr(&mbu->mbu_haddr, &taddr);

			nd6_na_output(ifa->ifa_ifp, &daddr,
					      &taddr,
					      ND_NA_FLAG_OVERRIDE,
					      1, NULL);
			mip6log((LOG_INFO,
				 "%s:%d: send a unsolicited na to %s\n",
				 __FILE__, __LINE__, if_name(ifa->ifa_ifp)));
		}
		} else if (mbu->mbu_reg_state
			   == MIP6_BU_REG_STATE_REGWAITACK) {
			if (lifetime == 0) {
				mip6log((LOG_WARNING,
					 "%s:%d: lifetime are zero.\n",
					 __FILE__, __LINE__));
				/* XXX ignored */
			}
			/* home registration completed */
			mbu->mbu_reg_state = MIP6_BU_REG_STATE_REG;

			/* create tunnel to HA */
			error = mip6_tunnel_control(MIP6_TUNNEL_CHANGE,
						    mbu,
						    mip6_bu_encapcheck,
						    &mbu->mbu_encap);
			if (error) {
				mip6log((LOG_ERR,
					 "%s:%d: tunnel move failed.\n",
					 __FILE__, __LINE__));
				return (error);
			}

			/* notify all the CNs that we have a new coa. */
			error = mip6_bu_list_notify_binding_change(sc);
			if (error) {
				mip6log((LOG_ERR,
					 "%s:%d: updating the bining cache entries of all CNs failed.\n",
					 __FILE__, __LINE__));
				return (error);
			}
		} else if (mbu->mbu_reg_state == MIP6_BU_REG_STATE_REG) {
			/* nothing to do. */
		} else {
			mip6log((LOG_NOTICE,
				 "%s:%d: unexpected condition.\n",
				 __FILE__, __LINE__));
		}
	}

	return (0);
}

int
mip6_bc_send_ba(src, dst, dstcoa, status, seqno, lifetime, refresh)
	struct sockaddr_in6 *src;
	struct sockaddr_in6 *dst;
	struct sockaddr_in6 *dstcoa;
	u_int8_t status;
	u_int16_t seqno;
	u_int32_t lifetime;
	u_int32_t refresh;
{
	struct mbuf *m;
	struct ip6_pktopts opt;
#if 0
	struct ip6_rthdr *pktopt_rthdr;
#endif
	int error = 0;

	init_ip6pktopts(&opt);

	m = mip6_create_ip6hdr(src, dst, IPPROTO_NONE, 0);
	if (m == NULL) {
		mip6log((LOG_ERR,
			 "%s:%d: creating ip6hdr failed.\n",
			 __FILE__, __LINE__));
		return (ENOMEM);
	}

	error =  mip6_ip6ma_create(&opt.ip6po_mobility, src, dst,
				   status, seqno, lifetime, refresh);
	if (error) {
		mip6log((LOG_ERR,
			 "%s:%d: ba destopt creation error (%d)\n",
			 __FILE__, __LINE__, error));
		m_freem(m);
 		goto free_ip6pktopts;
	}

#if 0
	if (!SA6_ARE_ADDR_EQUAL(dst, dstcoa) &&
	    mip6_bc_list_find_withphaddr(&mip6_bc_list, dst) == NULL) {
		error = mip6_rthdr_create(&pktopt_rthdr, dstcoa, NULL);
		if (error) {
			mip6log((LOG_ERR,
				 "%s:%d: ba rthdr creation error (%d)\n",
				 __FILE__, __LINE__, error));
			m_freem(m);
 			goto free_ip6pktopts;
		}
		opt.ip6po_rthdr = pktopt_rthdr;
	}
#endif

	error = ip6_output(m, &opt, NULL, 0, NULL, NULL);
	if (error) {
		mip6log((LOG_ERR,
			 "%s:%d: sending ip packet error. (%d)\n",
			 __FILE__, __LINE__, error));
 		goto free_ip6pktopts;
	}
 free_ip6pktopts:
#if 0
	if (opt.ip6po_rthdr)
		free(opt.ip6po_rthdr, M_IP6OPT);
#endif
	if (opt.ip6po_mobility)
		free(opt.ip6po_mobility, M_IP6OPT);

	return (error);
}

int
mip6_ip6mu_create(pktopt_mobility, src, dst, sc)
	struct ip6_mobility **pktopt_mobility;
	struct sockaddr_in6 *src, *dst;
	struct hif_softc *sc;
{
	struct ip6m_binding_update *ip6mu;
	int ip6mu_size;
	struct mip6_bu *mbu, *hrmbu;
#if !(defined(__FreeBSD__) && __FreeBSD__ >= 3)
	long time_second = time.tv_sec;
#endif

	*pktopt_mobility = NULL;

	mbu = mip6_bu_list_find_withpaddr(&sc->hif_bu_list, dst, src);
	hrmbu = mip6_bu_list_find_home_registration(&sc->hif_bu_list, src);
	if ((mbu == NULL) &&
	    (hrmbu != NULL) &&
	    (hrmbu->mbu_reg_state == MIP6_BU_REG_STATE_REG)) {
		/* XXX */
		/* create a binding update entry and send CoTI/HoTI. */
		return (0);
	}
	if (mbu == NULL) {
		/*
		 * this is the case that the home registration is on
		 * going.  that is, (mbu == NULL) && (hrmbu != NULL)
		 * but hrmbu->reg_state != STATE_REG.
		 */
		return (0);
	}
	if ((mbu->mbu_state & MIP6_BU_STATE_BUNOTSUPP) != 0) {
		/*
		 * MIP6_BU_STATE_NOBUSUPPORT is set when we receive
		 * ICMP6_PARAM_PROB against the binding update sent
		 * before.  this means the peer doesn't support MIP6
		 * (at least the BU destopt).  we should not send any
		 * BU to such a peer.
		 */
		return (0);
	}
	if (SA6_IS_ADDR_UNSPECIFIED(&mbu->mbu_paddr)) {
		/*
		 * the peer addr is unspecified.  this happens when
		 * home registration occurs but no home agent address
		 * is known.
		 */
		mip6log((LOG_INFO,
			 "%s:%d: the peer addr is unspecified.\n",
			 __FILE__, __LINE__));
		mip6_icmp6_ha_discov_req_output(sc);
		return (0);
	}
	if ((mbu->mbu_state & MIP6_BU_STATE_WAITSENT) == 0) {
		/* no need to send. */
		return (0);
	}

	ip6mu_size = sizeof(struct ip6m_binding_update);

	/* XXX nonce indice and authentication data size. */

	MALLOC(ip6mu, struct ip6m_binding_update *,
	       ip6mu_size, M_IP6OPT, M_NOWAIT);
	if (ip6mu == NULL)
		return (ENOMEM);

	/* update sequence number of this binding update entry. */
	mbu->mbu_seqno++;

	bzero(ip6mu, ip6mu_size);
	ip6mu->ip6mu_pproto = IPPROTO_NONE;
	ip6mu->ip6mu_len = (ip6mu_size - 8) >> 3;
	ip6mu->ip6mu_type = IP6M_BINDING_UPDATE;
	ip6mu->ip6mu_flags = mbu->mbu_flags;
	ip6mu->ip6mu_seqno = htons(mbu->mbu_seqno);
	if (SA6_ARE_ADDR_EQUAL(&mbu->mbu_haddr, &mbu->mbu_coa)) {
		/* this binding update is for home un-registration. */
		ip6mu->ip6mu_lifetime = 0;
	} else {
		struct mip6_prefix *mpfx;
		u_int32_t haddr_lifetime, coa_lifetime, lifetime;

		mpfx = mip6_prefix_list_find_withhaddr(&mip6_prefix_list,
						       src);
		haddr_lifetime = mpfx->mpfx_pltime;
		coa_lifetime = mip6_coa_get_lifetime(&mbu->mbu_coa.sin6_addr);
		lifetime = haddr_lifetime < coa_lifetime ?
			haddr_lifetime : coa_lifetime;
		if ((mbu->mbu_flags & IP6MU_HOME) == 0) {
			if (mip6_config.mcfg_bu_maxlifetime > 0 &&
			    lifetime > mip6_config.mcfg_bu_maxlifetime)
				lifetime = mip6_config.mcfg_bu_maxlifetime;
		} else {
			if (mip6_config.mcfg_hrbu_maxlifetime > 0 &&
			    lifetime > mip6_config.mcfg_hrbu_maxlifetime)
				lifetime = mip6_config.mcfg_hrbu_maxlifetime;
		}
#ifdef MIP6_SYNC_SA_LIFETIME
		/* XXX k-sugyou */
		if (sav != NULL) {
			u_int32_t sa_lifetime = 0;
			if (sav->lft_h != NULL &&
			    sav->lft_h->sadb_lifetime_addtime != 0) {
				sa_lifetime = sav->lft_h->sadb_lifetime_addtime
					      - (time_second - sav->created);
			}
			if (sa_lifetime > 0 && lifetime > sa_lifetime)
				lifetime = sa_lifetime;
		}
#endif /* MIP6_SYNC_SA_LIFETIME */
		mbu->mbu_lifetime = lifetime;
		mbu->mbu_expire = time_second + lifetime;
		mbu->mbu_refresh = mbu->mbu_lifetime;
		mbu->mbu_refexpire = time_second + mbu->mbu_refresh;
		ip6mu->ip6mu_lifetime = htonl(mbu->mbu_lifetime);
	}
	ip6mu->ip6mu_addr = mbu->mbu_haddr.sin6_addr;
	in6_clearscope(&ip6mu->ip6mu_addr);

	/* XXX */
	/* nonce indices and authdata insersion. */

	/* calculate checksum. */
	ip6mu->ip6mu_cksum = mip6_cksum(src, dst, ip6mu_size,
					IPPROTO_MOBILITY, (char *)ip6mu);

	*pktopt_mobility = (struct ip6_mobility *)ip6mu;

	/* hoping that the binding update will be sent with no accident. */
	mbu->mbu_state &= ~MIP6_BU_STATE_WAITSENT;

	return (0);
}

int
mip6_ip6ma_create(pktopt_mobility, src, dst, status, seqno, lifetime, refresh)
	struct ip6_mobility **pktopt_mobility;
	struct sockaddr_in6 *src;
	struct sockaddr_in6 *dst;
	u_int8_t status;
	u_int16_t seqno;
	u_int32_t lifetime;
	u_int32_t refresh;
{
	struct ip6m_binding_ack *ip6ma;
	int ip6ma_size;

	*pktopt_mobility = NULL;

	ip6ma_size = sizeof(struct ip6m_binding_ack);
	ip6ma_size += 4; /* XXX */

	MALLOC(ip6ma, struct ip6m_binding_ack *,
	       ip6ma_size, M_IP6OPT, M_NOWAIT);
	if (ip6ma == NULL)
		return (ENOMEM);

	bzero(ip6ma, ip6ma_size);
	ip6ma->ip6ma_pproto = IPPROTO_NONE;
	ip6ma->ip6ma_len = (ip6ma_size - 8) >> 3;
	ip6ma->ip6ma_type = IP6M_BINDING_ACK;
	ip6ma->ip6ma_status = status;
	ip6ma->ip6ma_seqno = htons(seqno);
	ip6ma->ip6ma_lifetime = htonl(lifetime);
	ip6ma->ip6ma_refresh = htonl(refresh);

	/* XXX authorization data processing. */

	/* calculate checksum. */
	ip6ma->ip6ma_cksum = mip6_cksum(src, dst, ip6ma_size,
					IPPROTO_MOBILITY, (char *)ip6ma);

	*pktopt_mobility = (struct ip6_mobility *)ip6ma;
	
	return (0);
}

int
mip6_ip6me_create(pktopt_mobility, src, dst, status, addr)
	struct ip6_mobility **pktopt_mobility;
	struct sockaddr_in6 *src;
	struct sockaddr_in6 *dst;
	u_int8_t status;
	struct sockaddr_in6 *addr;
{
	struct ip6m_binding_error *ip6me;
	int ip6me_size;

	*pktopt_mobility = NULL;

	ip6me_size = sizeof(struct ip6m_binding_error);

	MALLOC(ip6me, struct ip6m_binding_error *,
	       ip6me_size, M_IP6OPT, M_NOWAIT);
	if (ip6me == NULL)
		return (ENOMEM);

	bzero(ip6me, ip6me_size);
	ip6me->ip6me_pproto = IPPROTO_NONE;
	ip6me->ip6me_len = (sizeof(struct ip6m_binding_error) - 8) >> 3;
	ip6me->ip6me_type = IP6M_BINDING_ERROR;
	ip6me->ip6me_status = status;
	ip6me->ip6me_addr = addr->sin6_addr;
	in6_clearscope(&ip6me->ip6me_addr);

	/* calculate checksum. */
	ip6me->ip6me_cksum = mip6_cksum(src, dst, ip6me_size,
					IPPROTO_MOBILITY, (char *)ip6me);

	*pktopt_mobility = (struct ip6_mobility *)ip6me;
	
	return (0);
}

#define ADDCARRY(x)  (x > 65535 ? x -= 65535 : x)
#define REDUCE do {l_util.l = sum; sum = l_util.s[0] + l_util.s[1]; ADDCARRY(sum);} while(0);
static int
mip6_cksum(src_sa, dst_sa, plen, nh, mobility)
	struct sockaddr_in6 *src_sa;
	struct sockaddr_in6 *dst_sa;
	u_int32_t plen;
	u_int8_t nh;
	char *mobility;
{
	int sum, i;
	u_int16_t *payload;
	union {
		u_int16_t uphs[20];
		struct {
			struct in6_addr uph_src;
			struct in6_addr uph_dst;
			u_int32_t uph_plen;
			u_int8_t uph_zero[3];
			u_int8_t uph_nh;
		} uph_un __attribute__((__packed__));
	} uph;
	union {
		u_int16_t s[2];
		u_int32_t l;
	} l_util;

	bzero(&uph, sizeof(uph));
	uph.uph_un.uph_src = src_sa->sin6_addr;
	in6_clearscope(&uph.uph_un.uph_src);
	uph.uph_un.uph_dst = dst_sa->sin6_addr;
	in6_clearscope(&uph.uph_un.uph_dst);
	uph.uph_un.uph_plen = htonl(plen);
	uph.uph_un.uph_nh = nh;

	sum = 0;
	for (i = 0; i < 20; i++) {
		REDUCE;
		sum += uph.uphs[i];
	}
	payload = (u_int16_t *)mobility;
	for (i = 0; i < (plen / 2); i++) {
		REDUCE;
		sum += *payload++;
	}
	if (plen % 2) {
		union {
			u_int16_t s;
			u_int8_t c[2];
		} last;
		REDUCE;
		last.c[0] = *(char *)payload;
		last.c[1] = 0;
		sum += last.s;
	}

	REDUCE;
	return(~sum & 0xffff);
}
#undef ADDCARRY
#undef REDUCE