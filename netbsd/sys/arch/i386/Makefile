#	$NetBSD: Makefile,v 1.16 1998/11/15 20:36:30 perry Exp $

# Makefile for i386 tags file and boot blocks

SYSDIR?=	/sys

TI386=	${SYSDIR}/arch/i386/tags
SI386=	${SYSDIR}/arch/i386/i386/*.[ch] ${SYSDIR}/arch/i386/include/*.h \
	${SYSDIR}/arch/i386/eisa/*.[ch] ${SYSDIR}/arch/i386/isa/*.[ch] \
	${SYSDIR}/arch/i386/pci/*.[ch]
AI386=	${SYSDIR}/arch/i386/i386/*.s ${SYSDIR}/arch/i386/isa/*.s

# Directories in which to place tags links
DI386=	i386 eisa isa include pci

.include "../../kern/Make.tags.inc"

tags:
	-ctags -wdtf ${TI386} ${SI386} ${COMM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AI386} | \
	    sed "s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3 \1 /^\2(\3\4$$/;" \
		>> ${TI386}
	sort -o ${TI386} ${TI386}

links:
	-for i in ${DI386}; do \
	    (cd $$i && rm -f tags; ln -s ../tags tags); done


SUBDIR=	include stand isa/pcvt

.include <bsd.subdir.mk>
