/*	$KAME: in_gif.c,v 1.58 2001/07/24 13:06:40 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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

#ifdef __FreeBSD__
#include "opt_mrouting.h"
#if __FreeBSD__ >= 3
#include "opt_inet.h"
#include "opt_inet6.h"
#endif
#endif
#ifdef __NetBSD__
#include "opt_mrouting.h"
#include "opt_inet.h"
#include "opt_iso.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#ifdef __FreeBSD__
#include <sys/kernel.h>
#include <sys/sysctl.h>
#endif
#if !defined(__FreeBSD__) || __FreeBSD__ < 3
#include <sys/ioctl.h>
#endif
#include <sys/syslog.h>

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/malloc.h>
#endif

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_gif.h>
#include <netinet/in_var.h>
#include <netinet/ip_encap.h>
#include <netinet/ip_ecn.h>
#ifdef __OpenBSD__
#include <netinet/ip_ipsp.h>
#endif

#ifdef INET6
#include <netinet/ip6.h>
#endif

#ifdef MROUTING
#include <netinet/ip_mroute.h>
#endif /* MROUTING */

#include <net/if_gif.h>	

#include "gif.h"
#ifdef __OpenBSD__
#include "bridge.h"
#endif

#include <machine/stdarg.h>

#include <net/net_osdep.h>

#if NGIF > 0
int ip_gif_ttl = GIF_TTL;
#else
int ip_gif_ttl = 0;
#endif
#ifdef __FreeBSD__
SYSCTL_INT(_net_inet_ip, IPCTL_GIF_TTL, gifttl, CTLFLAG_RW,
	&ip_gif_ttl,	0, "");
#endif

#ifndef offsetof
#define offsetof(s, e) ((int)&((s *)0)->e)
#endif

int
in_gif_output(ifp, family, m, rt)
	struct ifnet	*ifp;
	int		family;
	struct mbuf	*m;
	struct rtentry *rt;
{
#ifdef __OpenBSD__
	struct gif_softc *sc = (struct gif_softc*)ifp;
	struct sockaddr_in *sin_src = (struct sockaddr_in *)sc->gif_psrc;
	struct sockaddr_in *sin_dst = (struct sockaddr_in *)sc->gif_pdst;
	struct tdb tdb;
	struct xformsw xfs;
	int error;
	int hlen, poff;
	u_int16_t plen;
	struct mbuf *mp;

	if (sin_src == NULL || sin_dst == NULL ||
	    sin_src->sin_family != AF_INET ||
	    sin_dst->sin_family != AF_INET) {
		m_freem(m);
		return EAFNOSUPPORT;
	}

	/* setup dummy tdb.  it highly depends on ipipoutput() code. */
	bzero(&tdb, sizeof(tdb));
	bzero(&xfs, sizeof(xfs));
	tdb.tdb_src.sin.sin_family = AF_INET;
	tdb.tdb_src.sin.sin_len = sizeof(struct sockaddr_in);
	tdb.tdb_src.sin.sin_addr = sin_src->sin_addr;
	tdb.tdb_dst.sin.sin_family = AF_INET;
	tdb.tdb_dst.sin.sin_len = sizeof(struct sockaddr_in);
	tdb.tdb_dst.sin.sin_addr = sin_dst->sin_addr;
	tdb.tdb_xform = &xfs;
	xfs.xf_type = -1;	/* not XF_IP4 */

	switch (family) {
	case AF_INET:
		if (m->m_len < sizeof(struct ip)) {
			m = m_pullup(m, sizeof(struct ip));
			if (m == NULL)
				return ENOBUFS;
		}
		hlen = (mtod(m, struct ip *)->ip_hl) << 2;
		poff = offsetof(struct ip, ip_p);
		break;
#ifdef INET6
	case AF_INET6:
		hlen = sizeof(struct ip6_hdr);
		poff = offsetof(struct ip6_hdr, ip6_nxt);
		break;
#endif
#if NBRIDGE > 0
	case AF_LINK:
		break;
#endif /* NBRIDGE */
	default:
#ifdef DEBUG
	        printf("in_gif_output: warning: unknown family %d passed\n",
			family);
#endif
		m_freem(m);
		return EAFNOSUPPORT;
	}

#if NBRIDGE > 0
	if (family == AF_LINK) {
	        mp = NULL;
		error = etherip_output(m, &tdb, &mp, 0, 0);
		if (error)
		        return error;
		else if (mp == NULL)
		        return EFAULT;

		m = mp;
		goto sendit;
	}
#endif /* NBRIDGE */

	/* encapsulate into IPv4 packet */
	mp = NULL;
	error = ipip_output(m, &tdb, &mp, hlen, poff, NULL);
	if (error)
		return error;
	else if (mp == NULL)
		return EFAULT;

	m = mp;

#if NBRIDGE > 0
 sendit:
#endif /* NBRIDGE */
	/* ip_output needs host-order length.  it should be nuked */
	m_copydata(m, offsetof(struct ip, ip_len), sizeof(u_int16_t),
	    (caddr_t) &plen);
	NTOHS(plen);
	m_copyback(m, offsetof(struct ip, ip_len), sizeof(u_int16_t),
	    (caddr_t) &plen);

	return ip_output(m, NULL, NULL, 0, NULL, NULL);
#else
	struct gif_softc *sc = (struct gif_softc*)ifp;
	struct sockaddr_in *dst = (struct sockaddr_in *)&sc->gif_ro.ro_dst;
	struct sockaddr_in *sin_src = (struct sockaddr_in *)sc->gif_psrc;
	struct sockaddr_in *sin_dst = (struct sockaddr_in *)sc->gif_pdst;
	struct ip iphdr;	/* capsule IP header, host byte ordered */
	int proto, error;
	u_int8_t tos;

	if (sin_src == NULL || sin_dst == NULL ||
	    sin_src->sin_family != AF_INET ||
	    sin_dst->sin_family != AF_INET) {
		m_freem(m);
		return EAFNOSUPPORT;
	}

	switch (family) {
#ifdef INET
	case AF_INET:
	    {
		struct ip *ip;

		proto = IPPROTO_IPV4;
		if (m->m_len < sizeof(*ip)) {
			m = m_pullup(m, sizeof(*ip));
			if (!m)
				return ENOBUFS;
		}
		ip = mtod(m, struct ip *);
		tos = ip->ip_tos;
		break;
	    }
#endif /*INET*/
#ifdef INET6
	case AF_INET6:
	    {
		struct ip6_hdr *ip6;
		proto = IPPROTO_IPV6;
		if (m->m_len < sizeof(*ip6)) {
			m = m_pullup(m, sizeof(*ip6));
			if (!m)
				return ENOBUFS;
		}
		ip6 = mtod(m, struct ip6_hdr *);
		tos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		break;
	    }
#endif /*INET6*/
#if defined(__NetBSD__) && defined(ISO)
	case AF_ISO:
		proto = IPPROTO_EON;
		tos = 0;
		break;
#endif
	default:
#ifdef DEBUG
		printf("in_gif_output: warning: unknown family %d passed\n",
			family);
#endif
		m_freem(m);
		return EAFNOSUPPORT;
	}

	bzero(&iphdr, sizeof(iphdr));
	iphdr.ip_src = sin_src->sin_addr;
	/* bidirectional configured tunnel mode */
	if (sin_dst->sin_addr.s_addr != INADDR_ANY)
		iphdr.ip_dst = sin_dst->sin_addr;
	else {
		m_freem(m);
		return ENETUNREACH;
	}
	iphdr.ip_p = proto;
	/* version will be set in ip_output() */
	iphdr.ip_ttl = ip_gif_ttl;
	iphdr.ip_len = m->m_pkthdr.len + sizeof(struct ip);
	if (ifp->if_flags & IFF_LINK1)
		ip_ecn_ingress(ECN_ALLOWED, &iphdr.ip_tos, &tos);
	else
		ip_ecn_ingress(ECN_NOCARE, &iphdr.ip_tos, &tos);

	/* prepend new IP header */
	M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
	if (m && m->m_len < sizeof(struct ip))
		m = m_pullup(m, sizeof(struct ip));
	if (m == NULL)
		return ENOBUFS;
	bcopy(&iphdr, mtod(m, struct ip *), sizeof(struct ip));

	if (dst->sin_family != sin_dst->sin_family ||
	    dst->sin_addr.s_addr != sin_dst->sin_addr.s_addr) {
		/* cache route doesn't match */
		dst->sin_family = sin_dst->sin_family;
		dst->sin_len = sizeof(struct sockaddr_in);
		dst->sin_addr = sin_dst->sin_addr;
		if (sc->gif_ro.ro_rt) {
			RTFREE(sc->gif_ro.ro_rt);
			sc->gif_ro.ro_rt = NULL;
		}
	}

	if (sc->gif_ro.ro_rt == NULL) {
		rtalloc(&sc->gif_ro);
		if (sc->gif_ro.ro_rt == NULL) {
			m_freem(m);
			return ENETUNREACH;
		}

		/* if it constitutes infinite encapsulation, punt. */
		if (sc->gif_ro.ro_rt->rt_ifp == ifp) {
			m_freem(m);
			return ENETUNREACH;	/*XXX*/
		}
#if 0
		ifp->if_mtu = sc->gif_ro.ro_rt->rt_ifp->if_mtu
			- sizeof(struct ip);
#endif
	}

	error = ip_output(m, NULL, &sc->gif_ro, 0, NULL);
	return(error);
#endif /*openbsd*/
}

void
#if __STDC__
in_gif_input(struct mbuf *m, ...)
#else
in_gif_input(m, va_alist)
	struct mbuf *m;
	va_dcl
#endif
{
	int off, proto;
	struct ifnet *gifp = NULL;
	struct ip *ip;
	va_list ap;
#ifndef __OpenBSD__
	int af;
	u_int8_t otos;
#endif

	va_start(ap, m);
	off = va_arg(ap, int);
#ifndef __OpenBSD__
	proto = va_arg(ap, int);
#endif
	va_end(ap);

	ip = mtod(m, struct ip *);
#ifdef __OpenBSD__
	proto = ip->ip_p;
#endif

	gifp = (struct ifnet *)encap_getarg(m);

	if (gifp == NULL || (gifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		ipstat.ips_nogif++;
		return;
	}

#ifdef __OpenBSD__
	m->m_pkthdr.rcvif = gifp;
	gifp->if_ipackets++;
	gifp->if_ibytes += m->m_pkthdr.len;
	ipip_input(m, off); /* We have a configured GIF */
	return;
#else
	otos = ip->ip_tos;
	m_adj(m, off);

	switch (proto) {
#ifdef INET
	case IPPROTO_IPV4:
	    {
		struct ip *ip;
		af = AF_INET;
		if (m->m_len < sizeof(*ip)) {
			m = m_pullup(m, sizeof(*ip));
			if (!m)
				return;
		}
		ip = mtod(m, struct ip *);
		if (gifp->if_flags & IFF_LINK1)
			ip_ecn_egress(ECN_ALLOWED, &otos, &ip->ip_tos);
		else
			ip_ecn_egress(ECN_NOCARE, &otos, &ip->ip_tos);
		break;
	    }
#endif
#ifdef INET6
	case IPPROTO_IPV6:
	    {
		struct ip6_hdr *ip6;
		u_int8_t itos;
		af = AF_INET6;
		if (m->m_len < sizeof(*ip6)) {
			m = m_pullup(m, sizeof(*ip6));
			if (!m)
				return;
		}
		ip6 = mtod(m, struct ip6_hdr *);
		itos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		if (gifp->if_flags & IFF_LINK1)
			ip_ecn_egress(ECN_ALLOWED, &otos, &itos);
		else
			ip_ecn_egress(ECN_NOCARE, &otos, &itos);
		ip6->ip6_flow &= ~htonl(0xff << 20);
		ip6->ip6_flow |= htonl((u_int32_t)itos << 20);
		break;
	    }
#endif /* INET6 */
#if defined(__NetBSD__) && defined(ISO)
	case IPPROTO_EON:
		af = AF_ISO;
		break;
#endif
	default:
		ipstat.ips_nogif++;
		m_freem(m);
		return;
	}
	gif_input(m, af, gifp);
	return;
#endif /*openbsd*/
}

/*
 * we know that we are in IFF_UP, outer address available, and outer family
 * matched the physical addr family.  see gif_encapcheck().
 */
int
gif_encapcheck4(m, off, proto, arg)
	const struct mbuf *m;
	int off;
	int proto;
	void *arg;
{
	struct ip ip;
	struct gif_softc *sc;
	struct sockaddr_in *src, *dst;
	int addrmatch;
	struct in_ifaddr *ia4;

	/* sanity check done in caller */
	sc = (struct gif_softc *)arg;
	src = (struct sockaddr_in *)sc->gif_psrc;
	dst = (struct sockaddr_in *)sc->gif_pdst;

	/* LINTED const cast */
	m_copydata((struct mbuf *)m, 0, sizeof(ip), (caddr_t)&ip);

	/* check for address match */
	addrmatch = 0;
	if (src->sin_addr.s_addr == ip.ip_dst.s_addr)
		addrmatch |= 1;
	if (dst->sin_addr.s_addr == ip.ip_src.s_addr)
		addrmatch |= 2;
	if (addrmatch != 3)
		return 0;

	/* martian filters on outer source - NOT done in ip_input! */
#if defined(__OpenBSD__) || defined(__NetBSD__)
	if (IN_MULTICAST(ip.ip_src.s_addr))
#else
	if (IN_MULTICAST(ntohl(ip.ip_src.s_addr)))
#endif
		return 0;
	switch ((ntohl(ip.ip_src.s_addr) & 0xff000000) >> 24) {
	case 0: case 127: case 255:
		return 0;
	}
	/* reject packets with broadcast on source */
#if defined(__OpenBSD__) || defined(__NetBSD__)
	for (ia4 = in_ifaddr.tqh_first; ia4; ia4 = ia4->ia_list.tqe_next)
#elif (defined(__FreeBSD__) && __FreeBSD__ >= 3)
	for (ia4 = TAILQ_FIRST(&in_ifaddrhead); ia4;
	     ia4 = TAILQ_NEXT(ia4, ia_link))
#else
	for (ia4 = in_ifaddr; ia4 != NULL; ia4 = ia4->ia_next)
#endif
	{
		if ((ia4->ia_ifa.ifa_ifp->if_flags & IFF_BROADCAST) == 0)
			continue;
		if (ip.ip_src.s_addr == ia4->ia_broadaddr.sin_addr.s_addr)
			return 0;
	}

	/* ingress filters on outer source */
	if ((sc->gif_if.if_flags & IFF_LINK2) == 0 &&
	    (m->m_flags & M_PKTHDR) != 0 && m->m_pkthdr.rcvif) {
		struct sockaddr_in sin;
		struct rtentry *rt;

		bzero(&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_addr = ip.ip_src;
#ifdef __FreeBSD__
		rt = rtalloc1((struct sockaddr *)&sin, 0, 0UL);
#else
		rt = rtalloc1((struct sockaddr *)&sin, 0);
#endif
		if (!rt || rt->rt_ifp != m->m_pkthdr.rcvif) {
#if 0
			log(LOG_WARNING, "%s: packet from 0x%x dropped "
			    "due to ingress filter\n", if_name(&sc->gif_if),
			    (u_int32_t)ntohl(sin.sin_addr.s_addr));
#endif
			if (rt)
				rtfree(rt);
			return 0;
		}
		rtfree(rt);
	}

	return 32 * 2;
}
