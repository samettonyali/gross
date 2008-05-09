# New ports collection makefile for:   gross
# Date created:        7 Juni 2007
# Whom: eino@utu.fi
#
# $FreeBSD$
#

PORTNAME=	gross
DISTVERSION=	1.0rc5
CATEGORIES=	mail
MASTER_SITES=	${MASTER_SITE_GOOGLE_CODE}

MAINTAINER=	eino@utu.fi
COMMENT=	Implements greylisting for postfix, as a policy daemon

OPTIONS= \
        DNSBL "With DNSBL check (depends on c-ares)" On \
	MILTER "Include Milter support" Off 


.include <bsd.port.pre.mk>

.if defined(WITH_DNSBL)
LIB_DEPENDS=	cares.1:${PORTSDIR}/dns/c-ares
.else
CONFIGURE_ARGS+= --disable-dnsbl
.endif

.if defined(WITH_MILTER)
CONFIGURE_ARGS+= --enable-milter
.if !exists(/usr/lib/libmilter.a)
BUILD_DEPENDS+= ${LOCALBASE}/lib/libmilter.a:${PORTSDIR}/mail/sendmail
.endif
.endif

GNU_CONFIGURE=	yes
CONFIGURE_TARGET=--build=${MACHINE_ARCH}-portbld-freebsd${OSREL}

USE_AUTOTOOLS=	libtool:15
USE_RC_SUBR=	grossd

MAN5=		grossd.conf.5
MAN8=		grossd.8

post-patch:
	@${REINPLACE_CMD} -e 's|-pthread|${PTHREAD_LIBS}|' ${WRKSRC}/configure

.include <bsd.port.post.mk>
