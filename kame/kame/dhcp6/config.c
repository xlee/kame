/*	$KAME: config.c,v 1.7 2002/05/08 10:36:16 jinmei Exp $	*/

/*
 * Copyright (C) 2002 WIDE Project.
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if_dl.h>

#include <netinet/in.h>

#include <syslog.h>
#include <stdio.h>
#include <string.h>
#include <ifaddrs.h>

#include <dhcp6.h>
#include <config.h>

extern int errno;

struct dhcp6_if *dhcp6_if;

static struct dhcp6_ifconf *dhcp6_ifconflist;
static struct prefix_ifconf *prefix_ifconflist0, *prefix_ifconflist;

enum { DHCPOPTCODE_SEND, DHCPOPTCODE_REQUEST, DHCPOPTCODE_ALLOW };

static int add_options __P((int, struct dhcp6_ifconf *, struct cf_list *));
static void clear_ifconf __P((struct dhcp6_ifconf *));
static void clear_prefixifconf __P((struct prefix_ifconf *));
static void clear_options __P((struct dhcp6_optconf *));
static int get_default_ifid __P((struct prefix_ifconf *));

void
ifinit(ifname)
	char *ifname;
{
	struct dhcp6_if *ifp;

	if ((ifp = find_ifconf(ifname)) != NULL) {
		dprintf(LOG_NOTICE, "duplicated interface: %s", ifname);
		return;
	}

	if ((ifp = (struct dhcp6_if *)malloc(sizeof(*ifp))) == NULL) {
		dprintf(LOG_ERR, "malloc failed");
		goto die;
	}
	memset(ifp, 0, sizeof(*ifp));

	ifp->state = DHCP6S_INIT;
	
	if ((ifp->ifname = strdup(ifname)) == NULL) {
		dprintf(LOG_ERR, "failed to copy ifname");
		goto die;
	}

	if ((ifp->ifid = if_nametoindex(ifname)) == 0) {
		dprintf(LOG_ERR, "invalid interface(%s): %s",
			ifname, strerror(errno));
		goto die;
	}
#ifdef HAVE_SCOPELIB
	if (inet_zoneid(AF_INET6, 2, ifname, &ifp->linkid)) {
		dprintf(LOG_ERR, "failed to get link ID for %s", ifname);
		goto die;
	}
#else
	ifp->linkid = ifp->ifid; /* XXX */
#endif

	ifp->next = dhcp6_if;
	dhcp6_if = ifp;
	return;

  die:
	exit(1);
}

int
configure_interface(iflist)
	struct cf_iflist *iflist;
{
	struct cf_iflist *ifp;
	struct dhcp6_ifconf *ifc;

	for (ifp = iflist; ifp; ifp = ifp->next) {
		struct cf_list *cfl;

		if ((ifc = (struct dhcp6_ifconf *)malloc(sizeof(*ifc)))
		    == NULL) {
			dprintf(LOG_ERR, "malloc failed");
			goto bad;
		}
		memset(ifc, 0, sizeof(*ifc));
		ifc->next = dhcp6_ifconflist;
		dhcp6_ifconflist = ifc;

		if ((ifc->ifname = strdup(ifp->ifname)) == NULL) {
			dprintf(LOG_ERR, "failed to copy ifname");
			goto bad;
		}

		for (cfl = ifp->params; cfl; cfl = cfl->next) {
			switch(cfl->type) {
			case DECL_REQUEST:
				if (add_options(DHCPOPTCODE_REQUEST,
						ifc, cfl->list)) {
					goto bad;
				}
				break;
			case DECL_SEND:
				if (add_options(DHCPOPTCODE_SEND,
						ifc, cfl->list)) {
					goto bad;
				}
				break;
			case DECL_ALLOW:
				if (add_options(DHCPOPTCODE_ALLOW,
						ifc, cfl->list)) {
					goto bad;
				}
				break;
			case DECL_INFO_ONLY:
				ifc->send_flags |= DHCIFF_INFO_ONLY;
				break;
			default:
				dprintf(LOG_ERR,
					"invalid interface configuration");
				goto bad;
			}
		}
	}
	
	return(0);

  bad:
	clear_ifconf(dhcp6_ifconflist);
	dhcp6_ifconflist = NULL;
	return(-1);
}

int
configure_prefix_interface(iflist)
	struct cf_iflist *iflist;
{
	struct cf_iflist *ifp;
	struct prefix_ifconf *pif;

	for (ifp = iflist; ifp; ifp = ifp->next) {
		struct cf_list *cfl;

		if ((pif = (struct prefix_ifconf *)malloc(sizeof(*pif)))
		    == NULL) {
			dprintf(LOG_ERR, "malloc failed");
			goto bad;
		}
		memset(pif, 0, sizeof(*pif));
		pif->next = prefix_ifconflist0;
		prefix_ifconflist0 = pif;

		/* validate and copy ifname */
		if (if_nametoindex(ifp->ifname) == 0) {
			dprintf(LOG_ERR, "invalid interface (%s): %s",
				ifp->ifname, strerror(errno));
			goto bad;
		}
		if ((pif->ifname = strdup(ifp->ifname)) == NULL) {
			dprintf(LOG_ERR, "failed to copy ifname");
			goto bad;
		}

		pif->ifid_len = IFID_LEN_DEFAULT;
		if (get_default_ifid(pif))
			goto bad;

		for (cfl = ifp->params; cfl; cfl = cfl->next) {
			switch(cfl->type) {
			case IFPARAM_SLA_ID:
				pif->sla_id = (u_int32_t)cfl->num;
				break;
			default:
				dprintf(LOG_ERR, "invalid prefix "
					"interface configuration");
				goto bad;
			}
		}
	}
	
	return(0);

  bad:
	clear_prefixifconf(prefix_ifconflist);
	prefix_ifconflist = NULL;
	return(-1);
}

/* we currently only construct EUI-64 based interface ID */
static int
get_default_ifid(pif)
	struct prefix_ifconf *pif;
{
	struct ifaddrs *ifa, *ifap;
	struct sockaddr_dl *sdl;

	if (pif->ifid_len < 64) {
		dprintf(LOG_NOTICE, "get_default_ifid: ID length too short");
		return -1;
	}

	if (getifaddrs(&ifap) < 0)
		return -1;
	
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		char *cp;

		if (strcmp(ifa->ifa_name, pif->ifname) != 0)
			continue;

		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		if (sdl->sdl_alen < 6) {
			dprintf(LOG_NOTICE, "get_default_ifid: "
				"link layer address is too short (%s)",
				pif->ifname);
			goto fail;
		}

		memset(pif->ifid, 0, sizeof(pif->ifid));
		cp = (char *)(sdl->sdl_data + sdl->sdl_nlen);
		pif->ifid[8] = cp[0];
		pif->ifid[8] ^= 0x02; /* reverse the u/l bit*/
		pif->ifid[9] = cp[1];
		pif->ifid[10] = cp[2];
		pif->ifid[11] = 0xff;
		pif->ifid[12] = 0xfe;
		pif->ifid[13] = cp[3];
		pif->ifid[14] = cp[4];
		pif->ifid[15] = cp[5];
	}

	freeifaddrs(ifap);
	return(0);

  fail:
	freeifaddrs(ifap);
	return(-1);
}

void
configure_cleanup()
{
	clear_ifconf(dhcp6_ifconflist);
	dhcp6_ifconflist = NULL;
}

void
configure_commit()
{
	struct dhcp6_ifconf *ifc;
	struct dhcp6_if *ifp;

	/* commit interface configuration */
	for (ifc = dhcp6_ifconflist; ifc; ifc = ifc->next) {
		if ((ifp = find_ifconf(ifc->ifname)) != NULL) {
			ifp->send_flags = ifc->send_flags;
			ifp->allow_flags = ifc->allow_flags;
			clear_options(ifp->send_options);
			ifp->send_options = ifc->send_options;
			clear_options(ifp->request_options);
			ifp->request_options = ifc->request_options;
			ifc->send_options = NULL;
		}
	}
	clear_ifconf(dhcp6_ifconflist);

	/* commit prefix configuration */
	if (prefix_ifconflist) {
		/* clear previous configuration. (need more work?) */
		clear_prefixifconf(prefix_ifconflist);
	}
	prefix_ifconflist = prefix_ifconflist0;
	prefix_ifconflist0 = NULL;
}

static void
clear_ifconf(iflist)
	struct dhcp6_ifconf *iflist;
{
	struct dhcp6_ifconf *ifc, *ifc_next;

	for (ifc = iflist; ifc; ifc = ifc_next) {
		ifc_next = ifc->next;

		free(ifc->ifname);
		clear_options(ifc->send_options);

		free(ifc);
	}
}

static void
clear_prefixifconf(iflist)
	struct prefix_ifconf *iflist;
{
	struct prefix_ifconf *pif, *pif_next;

	for (pif = iflist; pif; pif = pif_next) {
		pif_next = pif->next;

		free(pif->ifname);
		free(pif);
	}
}

static void
clear_options(opt0)
	struct dhcp6_optconf *opt0;
{
	struct dhcp6_optconf *opt, *opt_next;

	for (opt = opt0; opt; opt = opt_next) {
		opt_next = opt->next;

		free(opt->val);
		free(opt);
	}
}

static int
add_options(opcode, ifc, cfl0)
	int opcode;
	struct dhcp6_ifconf *ifc;
	struct cf_list *cfl0;
{
	struct dhcp6_optconf *opt;
	struct cf_list *cfl;

	for (cfl = cfl0; cfl; cfl = cfl->next) {
		switch(cfl->type) {
		case DHCPOPT_RAPID_COMMIT:
			switch(opcode) {
			case DHCPOPTCODE_SEND:
				ifc->send_flags |= DHCIFF_RAPID_COMMIT;
				break;
			case DHCPOPTCODE_ALLOW:
				ifc->allow_flags |= DHCIFF_RAPID_COMMIT;
				break;
			default:
				dprintf(LOG_ERR, "invalid operation (%d) "
					"for option type (%d)",
					opcode, cfl->type);
				return(-1);
			}
			break;
		case DHCPOPT_PREFIX_DELEGATION:
			switch(opcode) {
			case DHCPOPTCODE_REQUEST:
				if ((opt = (struct dhcp6_optconf *)
				     malloc(sizeof(*opt))) == NULL) {
					dprintf(LOG_ERR, "add_options: "
						"memory allocation failed");
					return(-1);
				}
				memset(opt, 0, sizeof(*opt));
				opt->type = DH6OPT_PREFIX_DELEGATION;
				opt->next = ifc->request_options;
				ifc->request_options = opt;
				break;
			default:
				dprintf(LOG_ERR, "invalid operation (%d) "
					"for option type (%d)",
					opcode, cfl->type);
				break;
			}
			break;
		default:
			dprintf(LOG_ERR, "unknown option type: %d", cfl->type);
				return(-1);
		}
	}

	return(0);
}

struct dhcp6_if *
find_ifconf(ifname)
	char *ifname;
{
	struct dhcp6_if *ifp;

	for (ifp = dhcp6_if; ifp; ifp = ifp->next) {
		if (strcmp(ifp->ifname, ifname) == NULL)
			return(ifp);
	}

	return(NULL);
}

struct prefix_ifconf *
find_prefixifconf(ifname)
	char *ifname;
{
	struct prefix_ifconf *ifp;

	for (ifp = prefix_ifconflist; ifp; ifp = ifp->next) {
		if (strcmp(ifp->ifname, ifname) == NULL)
			return(ifp);
	}

	return(NULL);
}