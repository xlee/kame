# $FreeBSD: src/sys/modules/if_ef/Makefile,v 1.2 2000/01/23 03:35:10 bp Exp $

.PATH:  ${.CURDIR}/../../net

KMOD=   if_ef
SRCS=   if_ef.c opt_ipx.h opt_inet.h opt_ef.h
MAN4=	ef.4

# If you need only limited number of frames comment out unneeded ones
# this will reduce number of visible devices
#ETHER_II=
ETHER_8023=
ETHER_8022=
ETHER_SNAP=

NBPF?=	1

CFLAGS+= ${PROTOS}

.if defined(EFDEBUG)
CFLAGS+= -DEF_DEBUG
.endif

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_ipx.h:
	echo "#define IPX 1" > opt_ipx.h

opt_ef.h:
.for frame in ETHER_II ETHER_8023 ETHER_8022 ETHER_SNAP
.if defined(${frame})
	echo "#define ${frame} 1" >> opt_ef.h
.endif
.endfor

load: all
	/sbin/kldload ./${PROG}

unload:
	/sbin/kldunload ${PROG}

.include <bsd.kmod.mk>
