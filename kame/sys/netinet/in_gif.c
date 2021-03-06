/*	$KAME: in_gif.c,v 1.101 2007/06/14 13:51:33 itojun Exp $	*/

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
#include "opt_inet.h"
#include "opt_inet6.h"
#endif
#ifdef __NetBSD__
#include "opt_inet.h"
#include "opt_iso.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif
#ifndef __FreeBSD__
#include <sys/ioctl.h>
#endif
#include <sys/syslog.h>
#include <sys/protosw.h>

#ifdef __FreeBSD__
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

#include <net/if_gif.h>

#include "gif.h"
#ifdef __OpenBSD__
#include "bridge.h"
#endif

#ifndef __OpenBSD__	/* version??? */
#include <machine/stdarg.h>
#endif

#include <net/net_osdep.h>

static int gif_validate4(const struct ip *, struct gif_softc *,
	struct ifnet *);

#if NGIF > 0
int ip_gif_ttl = GIF_TTL;
#else
int ip_gif_ttl = 0;
#endif
#ifndef __OpenBSD__
static int in_gif_rtcachettl = 300; /* XXX appropriate value? configurable? */
#endif
#ifdef __FreeBSD__
SYSCTL_INT(_net_inet_ip, IPCTL_GIF_TTL, gifttl, CTLFLAG_RW,
	&ip_gif_ttl,	0, "");
#endif

extern struct domain inetdomain;
struct protosw in_gif_protosw =
{ SOCK_RAW,	&inetdomain,	0/* IPPROTO_IPV[46] */,	PR_ATOMIC|PR_ADDR,
  in_gif_input, 
#ifdef __FreeBSD__
  (pr_output_t*)rip_output, 
#else
  rip_output,
#endif
  0,		rip_ctloutput,
#ifdef __FreeBSD__
  0,
#else
  rip_usrreq,
#endif
  0,            0,              0,              0,
#ifdef __FreeBSD__
  &rip_usrreqs
#endif
};

#ifndef offsetof
#define offsetof(s, e) ((int)&((s *)0)->e)
#endif

int
in_gif_output(struct ifnet *ifp, int family, struct mbuf *m)
{
#ifdef __OpenBSD__
	struct gif_softc *sc = (struct gif_softc*)ifp;
	struct sockaddr_in *sin_src = (struct sockaddr_in *)sc->gif_psrc;
	struct sockaddr_in *sin_dst = (struct sockaddr_in *)sc->gif_pdst;
	struct tdb tdb;
	struct xformsw xfs;
	int error;
	int hlen, poff;
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
	error = ipip_output(m, &tdb, &mp, hlen, poff);
	if (error)
		return error;
	else if (mp == NULL)
		return EFAULT;

	m = mp;

#if NBRIDGE > 0
 sendit:
#endif /* NBRIDGE */

	return ip_output(m, NULL, NULL, 0, NULL, NULL);
#else  /* !OpenBSD */
	struct gif_softc *sc = (struct gif_softc*)ifp;
	struct sockaddr_in *dst = (struct sockaddr_in *)&sc->gif_ro.ro_dst;
	struct sockaddr_in *sin_src = (struct sockaddr_in *)sc->gif_psrc;
	struct sockaddr_in *sin_dst = (struct sockaddr_in *)sc->gif_pdst;
	struct ip iphdr;	/* capsule IP header, host byte ordered */
	int proto, error;
	u_int8_t tos;
#ifdef __FreeBSD__
	struct timeval mono_time;
#endif

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
#endif /* INET */
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
#endif /* INET6 */
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
#ifdef __NetBSD__
	iphdr.ip_len = htons(m->m_pkthdr.len + sizeof(struct ip));
#else
	iphdr.ip_len = m->m_pkthdr.len + sizeof(struct ip);
#endif
	ip_ecn_ingress((ifp->if_flags & IFF_LINK1) ? ECN_ALLOWED : ECN_NOCARE,
		       &iphdr.ip_tos, &tos);

	/* prepend new IP header */
	M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
	if (m && m->m_len < sizeof(struct ip))
		m = m_pullup(m, sizeof(struct ip));
	if (m == NULL)
		return ENOBUFS;
	bcopy(&iphdr, mtod(m, struct ip *), sizeof(struct ip));

#ifdef __FreeBSD__
	microtime(&mono_time);
#endif

	/*
	 * Check if the cached route is still valid.  If not, create another
	 * one.
	 * XXX: we should be able to let ip_output() to validate the cache
	 * and to make a new route (see in6_gif_output()).  However, FreeBSD's
	 * ip_output() does not make a clone for the new route, while we'd
	 * like do so in case of route changes.  Thus, we reluctantly put
	 * almost duplicated logic here.
	 */
	if (sc->gif_ro.ro_rt && (!(sc->gif_ro.ro_rt->rt_flags & RTF_UP) ||
	    dst->sin_family != sin_dst->sin_family ||
	    dst->sin_addr.s_addr != sin_dst->sin_addr.s_addr ||
	    sc->rtcache_expire == 0 || mono_time.tv_sec >= sc->rtcache_expire)) {
		/*
		 * The cache route doesn't match, has been invalidated, or has
		 * expired.
		 */
		RTFREE(sc->gif_ro.ro_rt);
		sc->gif_ro.ro_rt = NULL;
	}

	if (sc->gif_ro.ro_rt == NULL) {
		bzero(dst, sizeof(*dst));
		dst->sin_family = sin_dst->sin_family;
		dst->sin_len = sizeof(struct sockaddr_in);
		dst->sin_addr = sin_dst->sin_addr;

		rtalloc(&sc->gif_ro);
		if (sc->gif_ro.ro_rt == NULL) {
			m_freem(m);
			return ENETUNREACH;
		}

		/* if it constitutes infinite encapsulation, punt. */
		if (sc->gif_ro.ro_rt->rt_ifp == ifp) {
			RTFREE(sc->gif_ro.ro_rt);
			sc->gif_ro.ro_rt = NULL;
			m_freem(m);
			return ENETUNREACH;	/* XXX */
		}

		sc->rtcache_expire = mono_time.tv_sec + in_gif_rtcachettl;
	}

	error = ip_output(m, NULL, &sc->gif_ro, 0, NULL
#if defined(__FreeBSD__) || defined(__NetBSD__)
			 , NULL
#endif
			 );
	return (error);
#endif /* OpenBSD */
}

void
#ifdef __FreeBSD__
in_gif_input(struct mbuf *m, int off)
#else
#if __STDC__
in_gif_input(struct mbuf *m, ...)
#else
in_gif_input(struct mbuf *m, va_alist)
#endif
#endif
{
#ifndef __FreeBSD__
	int off, proto;
	va_list ap;
#else
	int proto;
#endif
	struct ifnet *gifp = NULL;
	struct ip *ip;
#ifndef __OpenBSD__
	int af;
	u_int8_t otos;
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
	va_start(ap, m);
	off = va_arg(ap, int);
#if !defined(__OpenBSD__)
	proto = va_arg(ap, int);
#endif
	va_end(ap);
#endif

	ip = mtod(m, struct ip *);
#if defined(__OpenBSD__) || defined(__FreeBSD__)
	proto = ip->ip_p;
#endif

	gifp = (struct ifnet *)encap_getarg(m);

	if (gifp == NULL || (gifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		ipstat.ips_nogif++;
		return;
	}
#ifndef GIF_ENCAPCHECK
	if (!gif_validate4(ip, (struct gif_softc *)gifp, m->m_pkthdr.rcvif)) {
		m_freem(m);
		ipstat.ips_nogif++;
		return;
	}
#endif

#ifdef __OpenBSD__
	m->m_pkthdr.rcvif = gifp;
	gifp->if_ipackets++;
	gifp->if_ibytes += m->m_pkthdr.len;
	ipip_input(m, off, gifp); /* We have a configured GIF */
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
		if (ip_ecn_egress((gifp->if_flags & IFF_LINK1) ?
				  ECN_ALLOWED : ECN_NOCARE,
				  &otos, &ip->ip_tos) == 0) {
			m_freem(m);
			return;
		}
		break;
	    }
#endif
#ifdef INET6
	case IPPROTO_IPV6:
	    {
		struct ip6_hdr *ip6;
		u_int8_t itos, oitos;

		af = AF_INET6;
		if (m->m_len < sizeof(*ip6)) {
			m = m_pullup(m, sizeof(*ip6));
			if (!m)
				return;
		}
		ip6 = mtod(m, struct ip6_hdr *);
		itos = oitos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
		if (ip_ecn_egress((gifp->if_flags & IFF_LINK1) ?
				  ECN_ALLOWED : ECN_NOCARE,
				  &otos, &itos) == 0) {
			m_freem(m);
			return;
		}
		if (itos != oitos) {
			ip6->ip6_flow &= ~htonl(0xff << 20);
			ip6->ip6_flow |= htonl((u_int32_t)itos << 20);
		}
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
#endif /* OpenBSD */
}

/*
 * validate outer address.
 */
static int
gif_validate4(const struct ip *ip, struct gif_softc *sc, struct ifnet *ifp)
{
	struct sockaddr_in *src, *dst;
	struct in_ifaddr *ia4;

	src = (struct sockaddr_in *)sc->gif_psrc;
	dst = (struct sockaddr_in *)sc->gif_pdst;

	/* check for address match */
	if (src->sin_addr.s_addr != ip->ip_dst.s_addr ||
	    dst->sin_addr.s_addr != ip->ip_src.s_addr)
		return 0;

	/* martian filters on outer source - NOT done in ip_input! */
#if defined(__OpenBSD__) || defined(__NetBSD__)
	if (IN_MULTICAST(ip->ip_src.s_addr))
#else
	if (IN_MULTICAST(ntohl(ip->ip_src.s_addr)))
#endif
		return 0;
	switch ((ntohl(ip->ip_src.s_addr) & 0xff000000) >> 24) {
	case 0: case 127: case 255:
		return 0;
	}
	/* reject packets with broadcast on source */
#if defined(__OpenBSD__)
	for (ia4 = in_ifaddr.tqh_first; ia4; ia4 = ia4->ia_list.tqe_next)
#elif defined(__NetBSD__)
	TAILQ_FOREACH(ia4, &in_ifaddrhead, ia_list)
#elif defined(__FreeBSD__)
	for (ia4 = TAILQ_FIRST(&in_ifaddrhead); ia4;
	     ia4 = TAILQ_NEXT(ia4, ia_link))
#else
	for (ia4 = in_ifaddr; ia4 != NULL; ia4 = ia4->ia_next)
#endif
	{
		if ((ia4->ia_ifa.ifa_ifp->if_flags & IFF_BROADCAST) == 0)
			continue;
		if (ip->ip_src.s_addr == ia4->ia_broadaddr.sin_addr.s_addr)
			return 0;
	}

	/* ingress filters on outer source */
	if ((sc->gif_if.if_flags & IFF_LINK2) == 0 && ifp) {
		struct sockaddr_in sin;
		struct rtentry *rt;

		bzero(&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_addr = ip->ip_src;
#ifdef __FreeBSD__
		rt = rtalloc1((struct sockaddr *)&sin, 0, 0UL);
#else
		rt = rtalloc1((struct sockaddr *)&sin, 0);
#endif
		if (!rt || rt->rt_ifp != ifp) {
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

#ifdef GIF_ENCAPCHECK
/*
 * we know that we are in IFF_UP, outer address available, and outer family
 * matched the physical addr family.  see gif_encapcheck().
 */
int
gif_encapcheck4(const struct mbuf *m, int off, int proto, void *arg)
{
	struct ip ip;
	struct gif_softc *sc;
	struct ifnet *ifp;

	/* sanity check done in caller */
	sc = (struct gif_softc *)arg;

	/* LINTED const cast */
	m_copydata((struct mbuf *)m, 0, sizeof(ip), (caddr_t)&ip);
	ifp = ((m->m_flags & M_PKTHDR) != 0) ? m->m_pkthdr.rcvif : NULL;

	return gif_validate4(&ip, sc, ifp);
}
#endif

int
in_gif_attach(struct gif_softc *sc)
{
#ifndef GIF_ENCAPCHECK
	struct sockaddr_in mask4;

	bzero(&mask4, sizeof(mask4));
	mask4.sin_len = sizeof(struct sockaddr_in);
	mask4.sin_addr.s_addr = ~0;

	if (!sc->gif_psrc || !sc->gif_pdst)
		return EINVAL;
	sc->encap_cookie4 = encap_attach(AF_INET, -1, sc->gif_psrc,
	    (struct sockaddr *)&mask4, sc->gif_pdst, (struct sockaddr *)&mask4,
	    (struct protosw *)&in_gif_protosw, sc);
#else
	sc->encap_cookie4 = encap_attach_func(AF_INET, -1, gif_encapcheck,
	    &in_gif_protosw, sc);
#endif
	if (sc->encap_cookie4 == NULL)
		return EEXIST;
	return 0;
}

int
in_gif_detach(struct gif_softc *sc)
{
	int error;

	error = encap_detach(sc->encap_cookie4);
	if (error == 0)
		sc->encap_cookie4 = NULL;
	return error;
}
