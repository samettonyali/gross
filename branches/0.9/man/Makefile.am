man_MANS = grossd.8 grossd.conf.5

EXTRA_DIST = grossd.8.pod grossd.conf.5.pod grossd.8.in grossd.conf.5.in

CLEANFILES = $(man_MANS)

if HAVE_POD2MAN
grossd.8.in: grossd.8.pod
        $(POD2MAN) -c "" -r -s 8 -n grossd $< $@

grossd.conf.5.in: grossd.conf.5.pod
        $(POD2MAN) -c "" -r -s 5 -n grossd.conf $< $@
endif

grossd.8: grossd.8.in
	@SED@ \
          -e 's!@''sysconfdir''@!@sysconfdir@!' \
          -e 's!@''sbindir''@!@sbindir@!' \
          -e 's!@''libdir''@!@libdir@!' \
          -e 's!@''VERSION''@!@VERSION@!' \
	$< > $@

grossd.conf.5: grossd.conf.5.in
	@SED@ \
          -e 's!@''sysconfdir''@!@sysconfdir@!' \
          -e 's!@''sbindir''@!@sbindir@!' \
          -e 's!@''libdir''@!@libdir@!' \
          -e 's!@''VERSION''@!@VERSION@!' \
	$< > $@
