#	$NetBSD: Makefile.inc,v 1.1 2002/06/05 01:04:24 fredette Exp $

#	$OpenBSD: Makefile.inc,v 1.4 2001/03/29 03:58:17 mickey Exp $
#
#	NOTE:	$S must correspond to the top of the 'sys' tree
#		$HPPA must correspond to the top of hppa tree

SPMATHDIR=	${HPPA}/spmath

SPMATHDST?=	${.OBJDIR}/lib/spmath

SPMATH=		${SPMATHDST}/spmath.o
SPMATH_PROF=	${SPMATHDST}/spmath.po

SPMATHMAKE= \
	cd ${SPMATHDIR} && MAKEOBJDIR=${SPMATHDST} ${MAKE} \
	    CC='${CC}' CFLAGS='${CFLAGS}' \
	    CPPFLAGS='${CPPFLAGS:S@^-I.@-I../../.@g}'\
	    AS='${AS}' AFLAGS='${AFLAGS}' \
	    LD='${LD}' STRIP='${STRIP}' \
	    CPP='${CPP}' STRIP='${STRIP}' AR='${AR}' \
	    NM='${NM}' LORDER='${LORDER}' \
	    XMACHINE='${MACHINE}' XMACHINE_ARCH='${MACHINE_ARCH}' \

${SPMATH}:	.NOTMAIN __always_make_spmath
	@echo making sure the spmath library is up to date...
	@${SPMATHMAKE} spmath.o

${SPMATH_PROF}:	.NOTMAIN __always_make_spmath
	@echo making sure the profiled spmath library is up to date...
	@${SPMATHMAKE} spmath.po

__CLEANSPMATH:	.NOTMAIN __always_make_spmath
	@echo cleaning the spmath library objects
	@${SPMATHMAKE} clean

clean:	__CLEANSPMATH

__DEPENDSPMATH:	.NOTMAIN __always_make_spmath assym.h
	@echo depending the spmath library objects
	@${SPMATHMAKE} depend

depend:	__DEPENDSPMATH

__always_make_spmath:	.NOTMAIN
	-mkdir -p ${SPMATHDST}

