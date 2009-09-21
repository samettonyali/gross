VERSION=1.0.2
TARPKG=gross-${VERSION}.tar.gz

distinfo:
	( cd /tmp && sha256 ${TARPKG} ) > gross/distinfo
	( cd /tmp && md5 ${TARPKG} ) >> gross/distinfo
	( cd /tmp && stat -f "SIZE (%N) = %z" ${TARPKG} ) >> gross/distinfo

dist:
	cd gross && make clean
	if [ -d /tmp/port-gross-TMP ]; then rm -r /tmp/port-gross-TMP; fi
	mkdir /tmp/port-gross-TMP
	tar cf - gross | ( cd /tmp/port-gross-TMP && tar xf - )
	find /tmp/port-gross-TMP -name .svn | xargs rm -fr
	cd /tmp/port-gross-TMP \
		&& tar cf freebsd-port-gross-${VERSION}.tar gross \
		&& gzip freebsd-port-gross-${VERSION}.tar \
		&& mv freebsd-port-gross-${VERSION}.tar.gz /tmp
	cd /tmp
	rm -r /tmp/port-gross-TMP
