/*	$KAME: token.l,v 1.34 2001/09/25 14:15:24 sakane Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
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

%{
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/route.h>
#include <net/pfkeyv2.h>
#include <netkey/keydb.h>
#include <netkey/key_debug.h>
#include <netinet/in.h>
#include <netinet6/ipsec.h>

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>

#include "vchar.h"
#ifdef __NetBSD__
#include "parse.h"
#else
#include "y.tab.h"
#endif

int lineno = 1;

extern u_char m_buf[BUFSIZ];
extern u_int m_len;
extern int f_debug;

int yylex __P((void));
void yyfatal __P((const char *s));
void yyerror __P((const char *s));
extern void parse_init __P((void));
int parse __P((FILE **));
int yyparse __P((void));

/* make the code compile on *BSD-current */
#ifndef SADB_X_AALG_SHA2_256
#define SADB_X_AALG_SHA2_256	(-1)
#endif
#ifndef SADB_X_AALG_SHA2_384
#define SADB_X_AALG_SHA2_384	(-1)
#endif
#ifndef SADB_X_AALG_SHA2_512
#define SADB_X_AALG_SHA2_512	(-1)
#endif
#ifndef SADB_X_EALG_TWOFISHCBC
#define SADB_X_EALG_TWOFISHCBC	(-1)
#endif
%}

/* common section */
nl		\n
ws		[ \t]+
digit		[0-9]
letter		[0-9A-Za-z]
hexdigit	[0-9A-Fa-f]
dot		\.
hyphen		\-
slash		\/
blcl		\[
elcl		\]
semi		\;
comment		\#.*
quotedstring	\"[^"]*\"
decstring	{digit}+
hexstring	0[xX]{hexdigit}+
ipaddress	[a-fA-F0-9:]([a-fA-F0-9:\.]*|[a-fA-F0-9:\.]*%[a-zA-Z0-9]*)
ipaddrmask	{slash}{digit}{1,3}
name		{letter}(({letter}|{digit}|{hyphen})*({letter}|{digit}))*
hostname	{name}(({dot}{name})+{dot}?)?

%s S_PL

%%

add		{ return(ADD); }
delete		{ return(DELETE); }
deleteall	{ return(DELETEALL); }
get		{ return(GET); }
flush		{ return(FLUSH); }
dump		{ return(DUMP); }

	/* for management SPD */
spdadd		{ return(SPDADD); }
spddelete	{ return(SPDDELETE); }
spddump		{ return(SPDDUMP); }
spdflush	{ return(SPDFLUSH); }
{hyphen}P	{ BEGIN S_PL; return(F_POLICY); }
<S_PL>[a-zA-Z0-9:\.\-_/ \n\t][a-zA-Z0-9:\.\-_/ \n\t]* {
			yymore();

			/* count up for nl */
			    {
				char *p;
				for (p = yytext; *p != NULL; p++)
					if (*p == '\n')
						lineno++;
			    }

			yylval.val.len = strlen(yytext);
			yylval.val.buf = strdup(yytext);

			return(PL_REQUESTS);
		}
<S_PL>{semi}	{ BEGIN INITIAL; return(EOT); }

	/* address resolution flags */
{hyphen}[n46][n46]*	{
			yylval.val.len = strlen(yytext);
			yylval.val.buf = strdup(yytext);
			return(F_AIFLAGS);
		}

	/* security protocols */
ah		{ yylval.num = 0; return(PR_AH); }
esp		{ yylval.num = 0; return(PR_ESP); }
ah-old		{ yylval.num = 1; return(PR_AH); }
esp-old		{ yylval.num = 1; return(PR_ESP); }
ipcomp		{ yylval.num = 0; return(PR_IPCOMP); }

	/* authentication alogorithm */
{hyphen}A	{ return(F_AUTH); }
hmac-md5	{ yylval.num = SADB_AALG_MD5HMAC; return(ALG_AUTH); }
hmac-sha1	{ yylval.num = SADB_AALG_SHA1HMAC; return(ALG_AUTH); }
keyed-md5	{ yylval.num = SADB_X_AALG_MD5; return(ALG_AUTH); }
keyed-sha1	{ yylval.num = SADB_X_AALG_SHA; return(ALG_AUTH); }
hmac-sha2-256	{ yylval.num = SADB_X_AALG_SHA2_256; return(ALG_AUTH); }
hmac-sha2-384	{ yylval.num = SADB_X_AALG_SHA2_384; return(ALG_AUTH); }
hmac-sha2-512	{ yylval.num = SADB_X_AALG_SHA2_512; return(ALG_AUTH); }
null		{ yylval.num = SADB_X_AALG_NULL; return(ALG_AUTH); }

	/* encryption alogorithm */
{hyphen}E	{ return(F_ENC); }
des-cbc		{ yylval.num = SADB_EALG_DESCBC; return(ALG_ENC); }
3des-cbc	{ yylval.num = SADB_EALG_3DESCBC; return(ALG_ENC); }
simple		{ yylval.num = SADB_EALG_NULL; return(ALG_ENC); }
blowfish-cbc	{ yylval.num = SADB_X_EALG_BLOWFISHCBC; return(ALG_ENC); }
cast128-cbc	{ yylval.num = SADB_X_EALG_CAST128CBC; return(ALG_ENC); }
des-deriv	{ yylval.num = SADB_EALG_DESCBC; return(ALG_ENC_DESDERIV); }
des-32iv	{ yylval.num = SADB_EALG_DESCBC; return(ALG_ENC_DES32IV); }
twofish-cbc	{ yylval.num = SADB_X_EALG_TWOFISHCBC; return(ALG_ENC); }
rijndael-cbc	{ yylval.num = SADB_X_EALG_RIJNDAELCBC; return(ALG_ENC); }

	/* compression algorithms */
{hyphen}C	{ return(F_COMP); }
oui		{ yylval.num = SADB_X_CALG_OUI; return(ALG_COMP); }
deflate		{ yylval.num = SADB_X_CALG_DEFLATE; return(ALG_COMP); }
lzs		{ yylval.num = SADB_X_CALG_LZS; return(ALG_COMP); }
{hyphen}R	{ return(F_RAWCPI); }

	/* extension */
{hyphen}m	{ return(F_MODE); }
transport	{ yylval.num = IPSEC_MODE_TRANSPORT; return(MODE); }
tunnel		{ yylval.num = IPSEC_MODE_TUNNEL; return(MODE); }
{hyphen}u	{ return(F_REQID); }
{hyphen}f	{ return(F_EXT); }
random-pad	{ yylval.num = SADB_X_EXT_PRAND; return(EXTENSION); }
seq-pad		{ yylval.num = SADB_X_EXT_PSEQ; return(EXTENSION); }
zero-pad	{ yylval.num = SADB_X_EXT_PZERO; return(EXTENSION); }
nocyclic-seq	{ return(NOCYCLICSEQ); }
{hyphen}r	{ return(F_REPLAY); }
{hyphen}lh	{ return(F_LIFETIME_HARD); }
{hyphen}ls	{ return(F_LIFETIME_SOFT); }

	/* ... */
any		{ return(ANY); }
{ws}		{ }
{nl}		{ lineno++; }
{comment}
{semi}		{ return(EOT); }

	/* for address parameters: /prefix, [port] */
{slash}		{ return SLASH; }
{blcl}		{ return BLCL; }
{elcl}		{ return ELCL; }

	/* parameter */
{decstring}	{
			char *bp;

			yylval.ulnum = strtoul(yytext, &bp, 10);
			return(DECSTRING);
		}

{hexstring}	{
			yylval.val.buf = strdup(yytext + 2);
			yylval.val.len = strlen(yylval.val.buf);

			return(HEXSTRING);
		}

{quotedstring}	{
			char *p = yytext;
			while (*++p != '"') ;
			*p = NULL;
			yytext++;
			yylval.val.len = yyleng - 2;
			yylval.val.buf = strdup(yytext);

			return(QUOTEDSTRING);
		}

[A-Za-z0-9:][A-Za-z0-9:%\.-]* {
			yylval.val.len = yyleng;
			yylval.val.buf = strdup(yytext);
			return(STRING);
		}

[0-9,]+ {
			yylval.val.len = yyleng;
			yylval.val.buf = strdup(yytext);
			return(STRING);
		}

.		{
			yyfatal("Syntax error");
			/*NOTREACHED*/
		}

%%

void
yyfatal(s)
	const char *s;
{
	yyerror(s);
	exit(1);
}

void
yyerror(s)
	const char *s;
{
	printf("line %d: %s at [%s]\n", lineno, s, yytext);
}

int
parse(fp)
	FILE **fp;
{
	yyin = *fp;

	parse_init();

	if (yyparse()) {
		printf("parse failed, line %d.\n", lineno);
		return(-1);
	}

	return(0);
}