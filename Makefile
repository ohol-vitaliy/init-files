ETC_FILES =  ./inittab
ETC_FILES += ./udhcpc.script
ETC_FILES += ./mdev.conf

SBIN_FILES  = ./custom-init-script
SBIN_FILES += ./syncdate.sh
SBIN_FILES += ./auto-getty.sh
SBIN_FILES += ./power-manager

A_SCRIPTS := $(wildcard scripts/*)
A_BINLINKS := $(patsubst scripts/%,/usr/bin/_%,$(A_SCRIPTS))

.PHONY: install
install: $(SBIN_FILES) scripts
	/usr/bin/install -m 0755 $(ETC_FILES) /etc/
	/usr/bin/install -m 0755 $(SBIN_FILES) /sbin/
	mkdir -p /etc/X11/xorg.conf.d /etc/mdev
	/usr/bin/install -m 0755 -D ./00-input.conf /etc/X11/xorg.conf.d/
	/usr/bin/install -m 0755 -D ./input-link.sh /etc/mdev/

.PHONY: scripts
scripts: $(A_BINLINKS)

/usr/bin/_%: scripts/%
	ln -sf $(abspath $<) $@

%: %.cpp
	g++ -std=c++20 -O2 -Wall -o $@ $<
	strip $@

.PHONY: clean
clean:
	rm  $(foreach f,$(SBIN_FILES),/sbin/$(f))
	rm  $(foreach f,$(ETC_FILES),/etc/$(f))
	rm /etc/mdev/input-link.sh /etc/X11/xorg.conf.d/00-input.conf
