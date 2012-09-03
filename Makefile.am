# Automake file for Comfirm DMARC

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

bin_PROGRAMS = comfirm_dmarc
comfirm_dmarc_SOURCES = src/master_server.c src/server.c src/safe.c src/worker.c src/socket.c src/cache.c src/request.c src/response.c src/connection.c src/config_file.c src/lock.c src/events.c src/base64.c src/lib-pq/persisted_queue.c
comfirm_dmarc_LDFLAGS = -Wl,--no-as-needed -lpthread -ljson

dist_noinst_SCRIPTS = autogen.sh

install-data-local:
	mv /usr/local/bin/comfirm_dmarc /usr/local/bin/comfirm-dmarc
	mkdir -p /etc/comfirm-dmarc
	
	gcc -o dmarc-mailhandler src/lib-pq/persisted_queue.c src/mailhandler/dns.c src/mailhandler/base64.c src/mailhandler/string_buffer.c src/mailhandler/mailhandler.c -lfirm-dkim -lresolv -lcrypt -lz -lcurl `xml2-config --cflags --libs`
	cp dmarc-mailhandler /usr/local/bin/dmarc-mailhandler
	
	cp config/comfirm-dmarc.conf /etc/comfirm-dmarc/comfirm-dmarc.conf
	
	cp comfirm-dmarc_startup /etc/init.d/comfirm-dmarc
	chmod +x /etc/init.d/comfirm-dmarc
	
	mkdir -p /var/log/comfirm-dmarc
