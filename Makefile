# version info
MAINVERSION = 0
SUBVERSION = 1
PATCHLEVEL =

export VERSION = $(MAINVERSION).$(SUBVERSION)$(if $(PATCHLEVEL),.$(PATCHLEVEL))


# compile
export DESTDIR ?=
export PREFIX ?= /usr/local
export INCDIR ?= $(DESTDIR)$(PREFIX)/include
export LIBDIR ?= $(DESTDIR)$(PREFIX)/lib
export BINDIR ?= $(DESTDIR)$(PREFIX)/bin


HEADER = easoundlib.h


.PHONY: all
all:
	$(MAKE) -C src
	$(MAKE) -C utils


.PHONY: clean
clean:
	$(MAKE) -C src clean
	$(MAKE) -C utils clean


.PHONY: install
install:
	install -d $(INCDIR)/
	install include/$(HEADER) $(INCDIR)/
	$(MAKE) -C src install
	$(MAKE) -C utils install


.PHONY: uninstall
uninstall:
	-rm -f $(INCDIR)/$(HEADER)
	$(MAKE) -C src uninstall
	$(MAKE) -C utils uninstall
