# $FreeBSD$

CONFS+=	${SRCTOP}/usr.sbin/sendmail/mail/access.sample
CONFS+=	${SRCTOP}/usr.sbin/sendmail/mail/aliases
CONFS+=	${SRCTOP}/usr.sbin/sendmail/mail/mailer.conf
CONFS+=	${SRCTOP}/usr.sbin/sendmail/mail/freebsd.mc
CONFS+=	${SRCTOP}/usr.sbin/sendmail/mail/freebsd.submit.mc
CONFS+=	${SRCTOP}/usr.sbin/sendmail/mail/freefall.mc
CONFSDIR=	/etc/mail
CONFSMODE=	644

FILES+=	${SRCTOP}/usr.sbin/sendmail/mailertable.sample
FILES+=	${SRCTOP}/usr.sbin/sendmail/virtusertable.sample
FILES+=	${SRCTOP}/usr.sbin/sendmail/README
FILESDIR=	/etc/mail
FILESMODE=	644

LINKS=	/etc/mail/aliases /etc/aliases

.include <bsd.prog.mk>
