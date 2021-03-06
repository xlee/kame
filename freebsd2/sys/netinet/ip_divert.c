/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	$Id: ip_divert.c,v 1.1.2.12 1998/07/06 08:48:38 julian Exp $
 */

#include "opt_ipfw.h"

#ifndef INET
#error "IPDIVERT requires INET."
#endif

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

/*
 * Divert sockets
 */

/*
 * Allocate enough space to hold a full IP packet
 */
#define	DIVSNDQ		(65536 + 100)
#define	DIVRCVQ		(65536 + 100)

/* Global variables */

/*
 * ip_input() and ip_output() set this secret value before calling us to
 * let us know which divert port to divert a packet to; this is done so
 * we can use the existing prototype for struct protosw's pr_input().
 * This is stored in host order.
 */
u_short ip_divert_port;

/*
 * #ifndef IPFW_DIVERT_RESTART
 * We set this value to a non-zero port number when we want the call to
 * ip_fw_chk() in ip_input() or ip_output() to ignore ``divert <port>''
 * chain entries. This is stored in host order.
 * #else
 * A 16 bit cookie is passed to the user process.
 * The user process can send it back to help the caller know something
 * about where the packet came from.
 *
 * If IPFW is the caller then the IN cookie is the rule that sent
 * us here and the OUT cookie is the rule after which processing
 * should continue. Leaving it the same will make processing start
 * at the rule number after that which sent it here. Setting it to
 * 0 will restart processing at the beginning. 
 * #endif 
 */
u_short ip_divert_cookie;

/* Internal variables */

static struct inpcbhead divcb;
static struct inpcbinfo divcbinfo;

static u_long	div_sendspace = DIVSNDQ;	/* XXX sysctl ? */
static u_long	div_recvspace = DIVRCVQ;	/* XXX sysctl ? */

/* Optimization: have this preinitialized */
static struct sockaddr_in divsrc = { sizeof(divsrc), AF_INET };

/* Internal functions */

static int div_output(struct socket *so,
		struct mbuf *m, struct mbuf *addr, struct mbuf *control);

/*
 * Initialize divert connection block queue.
 */
void
div_init(void)
{
	LIST_INIT(&divcb);
	divcbinfo.listhead = &divcb;
	/*
	 * XXX We don't use the hash list for divert IP, but it's easier
	 * to allocate a one entry hash list than it is to check all
	 * over the place for hashbase == NULL.
	 */
	divcbinfo.hashbase = hashinit(1, M_PCB, &divcbinfo.hashmask);
}

/*
 * Setup generic address and protocol structures
 * for div_input routine, then pass them along with
 * mbuf chain. ip->ip_len is assumed to have had
 * the header length (hlen) subtracted out already.
 * We tell whether the packet was incoming or outgoing
 * by seeing if hlen == 0, which is a hack.
 */
void
div_input(struct mbuf *m, int off, int proto)
{
	int hlen = off;
	struct ip *ip;
	struct inpcb *inp;
	struct socket *sa;

	/* Sanity check */
	if (ip_divert_port == 0)
		panic("div_input: port is 0");

	/* Assure header */
	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof(struct ip))) == 0) {
		return;
	}
	ip = mtod(m, struct ip *);

	/* Record divert cookie */
	divsrc.sin_port = ip_divert_cookie;
	ip_divert_cookie = 0;

	/* Restore packet header fields */
	ip->ip_len += hlen;
	HTONS(ip->ip_len);
	HTONS(ip->ip_off);

	/*
	 * Record receive interface address, if any 
	 * But only for incoming packets.
	 */
	divsrc.sin_addr.s_addr = 0;
	if (hlen) {
		struct ifaddr *ifa;

#ifdef DIAGNOSTIC
		/* Sanity check */
		if (!(m->m_flags & M_PKTHDR))
			panic("div_input: no pkt hdr");
#endif

		/* More fields affected by ip_input() */
		HTONS(ip->ip_id);

		/* Find IP address for recieve interface */
		for (ifa = m->m_pkthdr.rcvif->if_addrlist;
		    ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr == NULL)
				continue;
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			divsrc.sin_addr =
			    ((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
			break;
		}
	}
	/*
	 * Record the incoming interface name whenever we have one.
	 */
	if (m->m_pkthdr.rcvif) {
		char	name[32];
		
		/*
		 * Hide the actual interface name in there in the 
		 * sin_zero array. XXX This needs to be moved to a
		 * different sockaddr type for divert, e.g.
		 * sockaddr_div with multiple fields like 
		 * sockaddr_dl. Presently we have only 7 bytes
		 * but that will do for now as most interfaces
		 * are 4 or less + 2 or less bytes for unit.
		 * There is probably a faster way of doing this,
		 * possibly taking it from the sockaddr_dl on the iface.
		 * This solves the problem of a P2P link and a LAN interface
		 * having the same address, which can result in the wrong
		 * interface being assigned to the packet when fed back
		 * into the divert socket. Theoretically if the daemon saves
		 * and re-uses the sockaddr_in as suggested in the man pages,
		 * this iface name will come along for the ride.
		 * (see div_output for the other half of this.)
		 */ 
		sprintf(name, "%s%d",
			m->m_pkthdr.rcvif->if_name, m->m_pkthdr.rcvif->if_unit);
		strncpy(divsrc.sin_zero, name, 7);
	}

	/* Put packet on socket queue, if any */
	sa = NULL;
	for (inp = divcb.lh_first; inp != NULL; inp = inp->inp_list.le_next) {
		if (inp->inp_lport == htons(ip_divert_port))
			sa = inp->inp_socket;
	}
	ip_divert_port = 0;
	if (sa) {
		if (sbappendaddr(&sa->so_rcv, (struct sockaddr *)&divsrc,
				m, (struct mbuf *)0) == 0)
			m_freem(m);
		else
			sorwakeup(sa);
	} else {
		m_freem(m);
		ipstat.ips_noproto++;
		ipstat.ips_delivered--;
        }
}

/*
 * Deliver packet back into the IP processing machinery.
 *
 * If no address specified, or address is 0.0.0.0, send to ip_output();
 * otherwise, send to ip_input() and mark as having been received on
 * the interface with that address.
 *
 * If no address specified, or dest port is 0, allow packet to divert
 * back to this socket; otherwise, don't.
 */
static int
div_output(so, m, addr, control)
	struct socket *so;
	register struct mbuf *m;
	struct mbuf *addr, *control;
{
	register struct inpcb *const inp = sotoinpcb(so);
	register struct ip *const ip = mtod(m, struct ip *);
	struct sockaddr_in *sin = NULL;
	int error = 0;

	if (control)
		m_freem(control);		/* XXX */
	if (addr)
		sin = mtod(addr, struct sockaddr_in *);

	/* Loopback avoidance */
	if (sin) {
		int	len = 0;
		char	*c = sin->sin_zero;

		ip_divert_cookie = sin->sin_port;

		/*
		 * Find receive interface with the given name or IP address.
		 * The name is user supplied data so don't trust it's size or 
		 * that it is zero terminated. The name has priority.
		 * We are presently assuming that the sockaddr_in 
		 * has not been replaced by a sockaddr_div, so we limit it
		 * to 16 bytes in total. the name is stuffed (if it exists)
		 * in the sin_zero[] field.
		 */
		while (*c++ && (len++ < sizeof(sin->sin_zero)));
		if ((len > 0) && (len < sizeof(sin->sin_zero)))
			m->m_pkthdr.rcvif = ifunit(sin->sin_zero);
	} else {
		ip_divert_cookie = 0;
	}

	/* Reinject packet into the system as incoming or outgoing */
	if (!sin || sin->sin_addr.s_addr == 0) {
		/*
		 * Don't allow both user specified and setsockopt options,
		 * and don't allow packet length sizes that will crash
		 */
		if (((ip->ip_hl != (sizeof (*ip) >> 2)) && inp->inp_options) ||
		     ((u_short)ntohs(ip->ip_len) > m->m_pkthdr.len)) {
			error = EINVAL;
			goto cantsend;
		}

		/* Convert fields to host order for ip_output() */
		NTOHS(ip->ip_len);
		NTOHS(ip->ip_off);

		/* Send packet to output processing */
		ipstat.ips_rawout++;			/* XXX */
		error = ip_output(m, inp->inp_options, &inp->inp_route,
			(so->so_options & SO_DONTROUTE) |
			IP_ALLOWBROADCAST | IP_RAWOUTPUT, inp->inp_moptions);
	} else {
		struct	ifaddr *ifa;

		/* If no luck with the name above, check by IP address.  */
		if (m->m_pkthdr.rcvif == NULL) {
			if (!(ifa = ifa_ifwithaddr((struct sockaddr *) sin))) {
				error = EADDRNOTAVAIL;
				goto cantsend;
			}
			m->m_pkthdr.rcvif = ifa->ifa_ifp;
		}

		/* Send packet to input processing */
		ip_input(m);
	}

	/* Reset for next time (and other packets) */
	ip_divert_cookie = 0;
	return error;

cantsend:
	ip_divert_cookie = 0;
	m_freem(m);
	return error;
}

/*ARGSUSED*/
int
div_usrreq(so, req, m, nam, control)
	register struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
{
	register int error = 0;
	register struct inpcb *inp = sotoinpcb(so);
	int s;

	if (inp == NULL && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}
	switch (req) {

	case PRU_ATTACH:
		if (inp)
			panic("div_attach");
		if ((so->so_state & SS_PRIV) == 0) {
			error = EACCES;
			break;
		}
		s = splnet();
		error = in_pcballoc(so, &divcbinfo);
		splx(s);
		if (error)
			break;
		error = soreserve(so, div_sendspace, div_recvspace);
		if (error)
			break;
		inp = (struct inpcb *)so->so_pcb;
		inp->inp_ip_p = (int)nam;	/* XXX */
		inp->inp_flags |= INP_HDRINCL;
		/* The socket is always "connected" because
		   we always know "where" to send the packet */
		so->so_state |= SS_ISCONNECTED;
		break;

	case PRU_DISCONNECT:
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			error = ENOTCONN;
			break;
		}
		/* FALLTHROUGH */
	case PRU_ABORT:
		soisdisconnected(so);
		/* FALLTHROUGH */
	case PRU_DETACH:
		if (inp == 0)
			panic("div_detach");
		in_pcbdetach(inp);
		break;

	case PRU_BIND:
		s = splnet();
		error = in_pcbbind(inp, nam);
		splx(s);
		break;

	/*
	 * Mark the connection as being incapable of further input.
	 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	case PRU_SEND:
		/* Packet must have a header (but that's about it) */
		if (m->m_len < sizeof (struct ip) ||
		    (m = m_pullup(m, sizeof (struct ip))) == 0) {
			ipstat.ips_toosmall++;
			error = EINVAL;
			break;
		}

		/* Send packet */
		error = div_output(so, m, nam, control); 
		m = NULL;
		break;

	case PRU_SOCKADDR:
		in_setsockaddr(inp, nam);
		break;

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize.
		 */
		return (0);

	/*
	 * Not supported.
	 */
	case PRU_CONNECT:
	case PRU_CONNECT2:
	case PRU_CONTROL:
	case PRU_RCVOOB:
	case PRU_RCVD:
	case PRU_LISTEN:
	case PRU_ACCEPT:
	case PRU_SENDOOB:
	case PRU_PEERADDR:
		error = EOPNOTSUPP;
		break;

	default:
		panic("div_usrreq");
	}
release:
	if (m)
		m_freem(m);
	return (error);
}
