# $Id$

noinst_HEADERS = include/*.h
EXTRA_DIST = configure doc
SUBDIRS = src
sysconf_DATA =
man8_MANS = doc/man/grossd.8
man5_MANS = doc/man/grossd_conf.5
dist-hook:
	rm -rf `find $(distdir)/doc -type d -name .svn`

install-data-local:
	if test ! -f $(DESTDIR)$(sysconfdir)/grossd.conf; then $(INSTALL_DATA) $(srcdir)/doc/examples/grossd.conf $(DESTDIR)$(sysconfdir); fi
