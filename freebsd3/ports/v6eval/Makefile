# New ports collection makefile for:	v6eval
# Date created:		26 Mar 1999 23:00 JST
# Whom:			TANAKA Takashi <tanaka@tahi.org>
#
# $KAME: Makefile,v 1.6 2000/08/21 05:17:36 sumikawa Exp $
#

PORTNAME=	v6eval
PORTVERSION=	1.1
CATEGORIES=	net
MASTER_SITES=	ftp://ftp.tahi.org/pub/TAHI/v6eval/
# you might need debugging, it's a developer release !
#CFLAGS+=	-g

MAINTAINER=	tanaka@tahi.org

BUILD_DEPENDS=	openssl:${PORTSDIR}/security/openssl
RUN_DEPENDS=	$(PERLLIBDIR)/Expect.pm:${PORTSDIR}/lang/p5-Expect \
		$(PERLLIBDIR)/IO/Stty.pm:${PORTSDIR}/devel/p5-IO-Stty \
		$(PERLLIBDIR)/i386-freebsd/IO/Tty.pm:${PORTSDIR}/devel/p5-IO-Tty \

USE_PERL5=	yes
PERLLIBDIR=	${PREFIX}/lib/perl5/site_perl/${PERL_VER}

post-install:
	@${SED} -e 's#/usr/local#${PREFIX}#' ${PKGDIR}/MESSAGE

.include <bsd.port.mk>
