# New ports collection makefile for:	mnews
# Version required:		1.22
# Date created:			23 Sep 1996
# Whom:				Masafumi NAKANE <max@FreeBSD.org>
#
# $FreeBSD: ports/japanese/mnews/Makefile,v 1.17 1999/12/26 14:58:46 sada Exp $
#

DISTNAME=	mnews122
PKGNAME?=	ja-mnews-1.22
CATEGORIES=	japanese news
MASTER_SITES=	ftp://ftp.leo.chubu.ac.jp/pub/mnews/

MAINTAINER=	seiken@ARGV.AC

WRKSRC=		${WRKDIR}/mnews-1.22/build
MAN1=		mnews.1
MANLANG=	ja

DOCS=		FAQ.doc board.doc gnspool.doc mnews.doc prerel.doc	\
		present.doc read1ST.doc readme.doc update.doc youbin.doc
SAMPLES=	_mnews.smp jnames.fj jnames.jp jnames.smp jnames.tnn

GNSPOOL?=	0
VERSION=	1.22

do-configure:
.for f in Makefile site_dep.h
	${SED} -e s+@PREFIX@+${PREFIX}+ -e s+@GNSPOOL@+${GNSPOOL}+ \
		-e s+@VERSION@+${VERSION}+ \
		${FILESDIR}/$f.in > ${WRKSRC}/$f
.endfor

do-install:
	${INSTALL_PROGRAM} ${WRKSRC}/mnews ${PREFIX}/bin
	${MKDIR} ${PREFIX}/lib/mnews
	${INSTALL_DATA} ${WRKSRC}/../etc/jnames.fj ${PREFIX}/lib/mnews/jnames
	${MKDIR} ${PREFIX}/share/examples/mnews
.for f in ${SAMPLES}
	${INSTALL_DATA} ${WRKSRC}/../etc/$f ${PREFIX}/share/examples/mnews
.endfor
	${INSTALL_MAN} ${WRKSRC}/../doc/mnews.1 ${PREFIX}/man/ja/man1
.if !defined(NOPORTDOCS)
	${MKDIR} ${PREFIX}/share/doc/mnews
.for f in ${DOCS}
	${INSTALL_DATA} ${WRKSRC}/../doc/$f ${PREFIX}/share/doc/mnews
.endfor
.endif

post-install:
.if !defined(BATCH)
	@( \
		while [ X$$domain = X ]; do \
			read -p "Enter the domain name of this host>" domain; \
		done; \
		${ECHO} $$domain > ${PREFIX}/lib/mnews/default-domain \
	)
.else
	@${ECHO} "Now, you need to make sure ${PREFIX}/lib/mnews/default-domain"
	@${ECHO} "contains correct domain name."
	@${ECHO} my.domain.com > ${PREFIX}/lib/mnews/default-domain
.endif
	@${ECHO} "Please refer to the files in:"
	@${ECHO} "${PREFIX}/share/doc/mnews & ${PREFIX}/share/examples/mnews."

.include <bsd.port.mk>
