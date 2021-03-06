/*-
 * Copyright (c) 1999 Brian Somers <brian@Awfulhak.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: realhostname.c,v 1.2.2.1 1999/05/02 08:50:52 brian Exp $
 */

#include <sys/param.h>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <string.h>

#include "libutil.h"

int
realhostname(char *host, size_t hsize, const struct in_addr *ip)
{
	int result;
	struct hostent *hp;

	result = HOSTNAME_INVALIDADDR;
	hp = gethostbyaddr((char *)ip, sizeof(*ip), AF_INET);

	if (hp != NULL && strlen(hp->h_name) <= hsize) {
		char lookup[MAXHOSTNAMELEN];

		strncpy(lookup, hp->h_name, sizeof(lookup) - 1);
		lookup[sizeof(lookup) - 1] = '\0';
		hp = gethostbyname(lookup);
		if (hp == NULL)
			result = HOSTNAME_INVALIDNAME;
		else for (; ; hp->h_addr_list++) {
			if (hp->h_addr_list[0] == NULL) {
				result = HOSTNAME_INCORRECTNAME;
				break;
			}
			if (!memcmp(*hp->h_addr_list, ip, sizeof(*ip))) {
				strncpy(host, lookup, hsize);
				return HOSTNAME_FOUND;
			}
		}
	}

	strncpy(host, inet_ntoa(*ip), hsize);

	return result;
}
