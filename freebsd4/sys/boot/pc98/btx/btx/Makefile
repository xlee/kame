# $FreeBSD: src/sys/boot/pc98/btx/btx/Makefile,v 1.3 1999/10/13 10:55:56 kato Exp $

M4?=	m4

.if defined(PAGING)
M4FLAGS+=	-DPAGING
.endif

ORG=	0x9000

AFLAGS+=	--defsym PC98=1

all: btx

btx: btx.o
.if ${OBJFORMAT} == aout
	${LD} -nostdlib -N -s -T ${ORG} -o btx.out btx.o
	dd if=btx.out of=${.TARGET} ibs=32 skip=1
.else
	${LD} -N -e start -Ttext ${ORG} -o btx.out btx.o
	objcopy -S -O binary btx.out ${.TARGET}
.endif

btx.o: btx.m4 btx.s
	(cd ${.CURDIR}; ${M4} ${M4FLAGS} btx.m4 btx.s) | \
	   ${AS} ${AFLAGS} -o ${.TARGET}

CLEANFILES+= btx btx.out btx.o

.include <bsd.prog.mk>
