/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/kame/kame/kame/kame/tcpdump/print-ppp.c,v 1.2 1999/08/17 12:14:25 itojun Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <ctype.h>
#include <netdb.h>
#include <pcap.h>
#include <stdio.h>
#ifdef __bsdi__
#include <net/slcompress.h>
#include <net/if_ppp.h>
#endif

#include "interface.h"
#include "addrtoname.h"
#include "ppp.h"

/* XXX This goes somewhere else. */
#define PPP_HDRLEN 4

/* LCP */

#define LCP_CONF_REQ	1
#define LCP_CONF_ACK	2
#define LCP_CONF_NAK	3
#define LCP_CONF_REJ	4
#define LCP_TERM_REQ	5
#define LCP_TERM_ACK	6
#define LCP_CODE_REJ	7
#define LCP_PROT_REJ	8
#define LCP_ECHO_REQ	9
#define LCP_ECHO_RPL	10
#define LCP_DISC_REQ	11

#define LCP_MIN	LCP_CONF_REQ
#define LCP_MAX LCP_DISC_REQ

static char *lcpcodes[] = {
	/*
	 * LCP code values (RFC1661, pp26)
	 */
	"Configure-Request",
	"Configure-Ack",
	"Configure-Nak",
	"Configure-Reject",
	"Terminate-Request",
	"Terminate-Ack",
 	"Code-Reject",
	"Protocol-Reject",
	"Echo-Request",
	"Echo-Reply",
	"Discard-Request",
};

#define LCPOPT_VEXT	0
#define LCPOPT_MRU	1
#define LCPOPT_ACCM	2
#define LCPOPT_AP	3
#define LCPOPT_QP	4
#define LCPOPT_MN	5
#define LCPOPT_PFC	7
#define LCPOPT_ACFC	8

#define LCPOPT_MIN 0
#define LCPOPT_MAX 24

static char *lcpconfopts[] = {
	"Vendor-Ext",
	"Max-Rx-Unit",
	"Async-Ctrl-Char-Map",
	"Auth-Prot",
	"Quality-Prot",
	"Magic-Number",
	"unassigned (6)",	
	"Prot-Field-Compr",
	"Add-Ctrl-Field-Compr",
	"FCS-Alternatives",
	"Self-Describing-Pad",
	"Numbered-Mode",
	"Multi-Link-Procedure",
	"Call-Back",
	"Connect-Time"
	"Compund-Frames",
	"Nominal-Data-Encap",
	"Multilink-MRRU",
	"Multilink-SSNHF",
	"Multilink-ED",
	"Proprietary",
	"DCE-Identifier",
	"Multilink-Plus-Proc",
	"Link-Discriminator",
	"LCP-Auth-Option",
};

/* CHAP */

#define CHAP_CHAL	1
#define CHAP_RESP	2
#define CHAP_SUCC	3
#define CHAP_FAIL	4

#define CHAP_CODEMIN 1
#define CHAP_CODEMAX 4

static char *chapcode[] = {
	"Challenge",
	"Response",
	"Success",
	"Failure",	
};

/* PAP */

#define PAP_AREQ	1
#define PAP_AACK	2
#define PAP_ANAK	3

#define PAP_CODEMIN	1
#define PAP_CODEMAX	3

static char *papcode[] = {
	"Authenticate-Request",
	"Authenticate-Ack",
	"Authenticate-Nak",
};

/* IPCP */

#define IPCP_2ADDR	1
#define IPCP_CP		2
#define IPCP_ADDR	3

static const char *ppp_protoname __P((int proto));
static void handle_lcp __P((const u_char *p, int length));
static int print_lcp_config_options __P((const u_char *p));
static void handle_chap __P((const u_char *p, int length));
static void handle_ipcp __P((const u_char *p, int length));
static void handle_pap __P((const u_char *p, int length));

static const char *
ppp_protoname(int proto)
{
	static char buf[20];

	switch (proto) {
	case PPP_IP:	return "IP";
#ifdef PPP_XNS
	case PPP_XNS:	return "XNS";
#endif
#ifdef PPP_IPX
	case PPP_IPX:	return "IPX";
#endif
#ifdef PPP_COMP
	case PPP_COMP:	return "COMP";
#endif
#ifdef PPP_IPCP
	case PPP_IPCP:	return "IPCP";
#endif
#ifdef PPP_IPV6CP
	case PPP_IPV6CP: return "IPV6CP";
#endif
#ifdef PPP_IPXCP
	case PPP_IPXCP:	return "IPXCP";
#endif
#ifdef PPP_CCP
	case PPP_CCP:	return "CCP";
#endif
#ifdef PPP_LCP
	case PPP_LCP:	return "LCP";
#endif
#ifdef PPP_PAP
	case PPP_PAP:	return "PAP";
#endif
#ifdef PPP_LQR
	case PPP_LQR:	return "LQR";
#endif
#ifdef PPP_CHAP
	case PPP_CHAP:	return "CHAP";
#endif
	default:
		snprintf(buf, sizeof(buf), "unknown-0x%04x\n", proto);
		return buf;
	}
}

/* print LCP frame */
static void
handle_lcp(const u_char *p, int length)
{
	int x, j;
	const u_char *ptr;

	x = p[4];

	if ((x >= LCP_MIN) && (x <= LCP_MAX))
		printf("%s", lcpcodes[x - 1]);
	else {
		printf("0x%02x", x);
		return;
	}

	length -= 4;
	
	switch (x) {
	case LCP_CONF_REQ:
	case LCP_CONF_ACK:
	case LCP_CONF_NAK:
	case LCP_CONF_REJ:
		x = length;
		ptr = p + 8;
		do {
			if ((j = print_lcp_config_options(ptr)) == 0)
				break;
			x -= j;
			ptr += j;
		} while (x > 0);
		break;

	case LCP_ECHO_REQ:
	case LCP_ECHO_RPL:
		printf(", Magic-Number=%u",
			(u_int32_t)ntohl(*(u_int32_t *)(p + 8)));
		break;
	case LCP_TERM_REQ:
	case LCP_TERM_ACK:
	case LCP_CODE_REJ:
	case LCP_PROT_REJ:
	case LCP_DISC_REQ:
	default:
		break;
	}
}

/* LCP config options */
static int
print_lcp_config_options(const u_char *p)
{
	int len	= p[1];
	int opt = p[0];
	
	if ((opt >= LCPOPT_MIN) && (opt <= LCPOPT_MAX))
		printf(", %s", lcpconfopts[opt]);

	switch (opt) {
	case LCPOPT_MRU:
		if (len == 4)
			printf("=%d", (*(p+2) << 8) + *(p+3));
		break;
	case LCPOPT_AP:
		if (len >= 4) {
			if (p[2] == 0xc0 && p[3] == 0x23)
				printf(" PAP");
			else if (p[2] == 0xc2 && p[3] == 0x23) {
				printf(" CHAP/");
				switch (p[4]) {
				default:
					printf("unknown-algorithm-%u", p[4]);
					break;
				case 5:
					printf("MD5");
					break;
				case 0x80:
					printf("Microsoft");
					break;
				}
			}
			else if (p[2] == 0xc2 && p[3] == 0x27)
				printf(" EAP");
			else if (p[2] == 0xc0 && p[3] == 0x27)
				printf(" SPAP");
			else if (p[2] == 0xc1 && p[3] == 0x23)
				printf(" Old-SPAP");
			else
				printf("unknown");
		}
		break;
	case LCPOPT_QP:
		if (len >= 4) {
			if (p[2] == 0xc0 && p[3] == 0x25)
				printf(" LQR");
			else
				printf(" unknown");
		}
		break;
	case LCPOPT_MN:
		if (len == 6)
			printf("=%u", (u_int32_t)ntohl(*(u_int32_t *)(p + 2)));
		break;
	case LCPOPT_PFC:
		printf(" PFC");
		break;
	case LCPOPT_ACFC:
		printf(" ACFC");
		break;
	}
	return len;
}

/* CHAP */
static void
handle_chap(const u_char *p, int length)
{
	int x;
	const u_char *ptr;

	if ((x >= CHAP_CODEMIN) && (x <= CHAP_CODEMAX))
		printf("%s", chapcode[x - 1]);
	else {
		printf("0x%02x", x);
		return;
	}

	length -= 4;
	
	switch (p[4]) {
	case CHAP_CHAL:
	case CHAP_RESP:
		printf(", Value=");
		x = p[8];	/* value size */
		ptr = p + 9;
		while (--x >= 0)
			printf("%02x", *ptr++);
		x = length - p[8] - 1;
		printf(", Name=");
		while (--x >= 0) {
			if (isprint(*ptr))
				printf("%c", *ptr);
			else
				printf("\\%03o", *ptr);
			ptr++;
		}
		break;
	}
}

/* PAP */
static void
handle_pap(const u_char *p, int length)
{
	int x;
	const u_char *ptr;

	x = p[4];

	if ((x >= PAP_CODEMIN) && (x <= PAP_CODEMAX))
		printf("%s", papcode[x - 1]);
	else {
		printf("0x%02x", x);
		return;
	}

	length -= 4;
	
	switch (x) {
	case PAP_AREQ:
		printf(", Peer-Id=");
		x = p[8];	/* peerid size */
		ptr = p + 9;
		while (--x >= 0) {
			if (isprint(*ptr))
				printf("%c", *ptr);
			else
				printf("\\%03o", *ptr);
			ptr++;
		}
		x = *ptr++;
		printf(", Passwd=");
		while (--x >= 0) {
			if (isprint(*ptr))
				printf("%c", *ptr);
			else
				printf("\\%03o", *ptr);
			ptr++;
		}
		break;
	case PAP_AACK:
	case PAP_ANAK:
		break;
	}
}

/* IPCP */
static void
handle_ipcp(const u_char *p, int length)
{
	length -= 4;
	
	switch (p[8]) {
	case IPCP_2ADDR:
		printf("IP-Addresses");
		printf(", src=%s", ipaddr_string(p + 10));
		printf(", drc=%s", ipaddr_string(p + 14));
		break;
		
	case IPCP_CP:
		printf("IP-Compression-Protocol");
		break;

	case IPCP_ADDR:
		printf("IP-Address=%s", ipaddr_string(p + 10));
		break;
	}
}
	
/* Standard PPP printer */
void
ppp_if_print(u_char *user, const struct pcap_pkthdr *h,
	     register const u_char *p)
{
	register u_int length = h->len;
	register u_int caplen = h->caplen;
	const struct ip *ip;
	u_int proto;

	ts_print(&h->ts);

	if (caplen < PPP_HDRLEN) {
		printf("[|ppp]");
		goto out;
	}

	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	proto = ntohs(*(u_short *)&p[2]);
	packetp = p;
	snapend = p + caplen;

	if (eflag)
		printf("%c %4d %02x %s: ", p[0] ? 'O' : 'I', length,
		       p[1], ppp_protoname(proto));

	length -= PPP_HDRLEN;
	ip = (struct ip *)(p + PPP_HDRLEN);
	switch (proto) {
	case PPP_LCP:
		handle_lcp(p, length);
		break;
	case PPP_CHAP:
		handle_chap(p, length);
		break;
	case PPP_PAP:
		handle_pap(p, length);
		break;
	case PPP_IPCP:
		handle_ipcp(p, length);
		break;
	case ETHERTYPE_IP:	/*XXX*/
	case PPP_IP:
		ip_print((const u_char *)ip, length);
		break;
#ifdef INET6
	case ETHERTYPE_IPV6:	/*XXX*/
#ifdef PPP_IPV6
	case PPP_IPV6:
#endif
		ip6_print((const u_char *)ip, length);
		break;
#endif
	}
	if (xflag)
		default_print((const u_char *)ip, caplen - PPP_HDRLEN);
out:
	putchar('\n');
}

#ifdef __bsdi__
/* proto type to string mapping */
static struct tok ptype2str[] = {
	{ PPP_VJC,	"VJC" },
	{ PPP_VJNC,	"VJNC" },
	{ PPP_OSI,	"OSI" },
	{ PPP_LCP,	"LCP" },
	{ PPP_IPCP,	"IPCP" },
	{ 0,		NULL }
};
#endif

#define PPP_BSDI_HDRLEN 24

/* BSD/OS specific PPP printer */
void
ppp_bsdos_if_print(u_char *user, const struct pcap_pkthdr *h,
	     register const u_char *p)
{
#ifdef __bsdi__
	register u_int length = h->len;
	register u_int caplen = h->caplen;
	register int hdrlength;
	u_short ptype;
	const u_char *q;
	int i;

	ts_print(&h->ts);

	if (caplen < PPP_BSDI_HDRLEN) {
		printf("[|ppp]");
		goto out;
	}

	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;
	hdrlength = 0;

#if 0
	if (p[0] == PPP_ADDRESS && p[1] == PPP_CONTROL) {
		if (eflag) 
			printf("%02x %02x ", p[0], p[1]);
		p += 2;
		hdrlength = 2;
	}

	if (eflag) 
		printf("%d ", length);
	/* Retrieve the protocol type */
	if (*p & 01) {
		/* Compressed protocol field */
		ptype = *p;
		if (eflag) 
			printf("%02x ", ptype);
		p++;
		hdrlength += 1;
	} else {
		/* Un-compressed protocol field */
		ptype = ntohs(*(u_short *)p);
		if (eflag) 
			printf("%04x ", ptype);
		p += 2;
		hdrlength += 2;
	}
#else
	ptype = 0;	/*XXX*/
	if (eflag)
		printf("%c ", p[SLC_DIR] ? 'O' : 'I');
	if (p[SLC_LLHL]) {
		/* link level header */
		struct ppp_header *ph;

		q = p + SLC_BPFHDRLEN;
		ph = (struct ppp_header *)q;
		if (ph->phdr_addr == PPP_ADDRESS
		 && ph->phdr_ctl == PPP_CONTROL) {
			if (eflag) 
				printf("%02x %02x ", q[0], q[1]);
			ptype = ntohs(ph->phdr_type);
			if (eflag && (ptype == PPP_VJC || ptype == PPP_VJNC)) {
				printf("%s ", tok2str(ptype2str,
						"proto-#%d", ptype));
			}
		} else {
			if (eflag) {
				printf("LLH=[");
				for (i = 0; i < p[SLC_LLHL]; i++)
					printf("%02x", q[i]);
				printf("] ");
			}
		}
		if (eflag) 
			printf("%d ", length);
	}
	if (p[SLC_CHL]) {
		q = p + SLC_BPFHDRLEN + p[SLC_LLHL];

		switch (ptype) {
		case PPP_VJC:
			ptype = vjc_print(q, length - (q - p), ptype);
			hdrlength = PPP_BSDI_HDRLEN;
			p += hdrlength;
			if (ptype == PPP_IP)
				ip_print(p, length);
			goto printx;
		case PPP_VJNC:
			ptype = vjc_print(q, length - (q - p), ptype);
			hdrlength = PPP_BSDI_HDRLEN;
			p += hdrlength;
			if (ptype == PPP_IP)
				ip_print(p, length);
			goto printx;
		default:
			if (eflag) {
				printf("CH=[");
				for (i = 0; i < p[SLC_LLHL]; i++)
					printf("%02x", q[i]);
				printf("] ");
			}
			break;
		}
	}

	hdrlength = PPP_BSDI_HDRLEN;
#endif

	length -= hdrlength;
	p += hdrlength;

	if (ptype == PPP_IP)
		ip_print(p, length);
	else
		printf("%s ", tok2str(ptype2str, "proto-#%d", ptype));

printx:
	if (xflag)
		default_print((const u_char *)p, caplen - hdrlength);
out:
	putchar('\n');
#endif /* __bsdi__ */
}