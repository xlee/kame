#	from: @(#)Makefile	2.3 88/08/11 4.0 RPCSRC
#	$Id: Makefile,v 1.23 1998/09/17 19:51:40 obrien Exp $

.SUFFIXES: .x

RPCCOM = rpcgen -C

HDRS= key_prot.h klm_prot.h mount.h nfs_prot.h nlm_prot.h rex.h rnusers.h \
      rquota.h rstat.h rwall.h sm_inter.h spray.h yppasswd.h yp.h \
      ypxfrd.h ypupdate_prot.h nis.h nis_cache.h nis_callback.h \
      bootparam_prot.h crypt.h
XFILES= bootparam_prot.x key_prot.x klm_prot.x mount.x nfs_prot.x nlm_prot.x \
      rex.x rnusers.x rquota.x rstat.x rwall.x sm_inter.x spray.x \
      yppasswd.x yp.x ypxfrd.x ypupdate_prot.x nis.x nis_cache.x nis_object.x \
      nis_callback.x crypt.x
HFILES= yp_prot.h ypclnt.h nis_db.h nis_tags.h nislib.h

CLEANFILES+= ${HDRS}

all: ${HDRS}

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${HFILES:S;^;${.CURDIR}/;} \
		${XFILES:S;^;${.CURDIR}/;} \
		${HDRS} \
		${DESTDIR}/usr/include/rpcsvc
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 key_prot.h \
		${DESTDIR}/usr/include/rpc

.x.h:
	${RPCCOM} -h -DWANT_NFS3 ${.IMPSRC} -o ${.TARGET}

nis.h: nis_object.x

.include <bsd.prog.mk>
