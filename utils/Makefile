CROSS_COMPILE ?=

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar


OBJECTS_ECAP = ecap.o
OBJECTS_EPLAY = eplay.o
OBJECTS = $(OBJECTS_ECAP) $(OBJECTS_EPLAY)
TARGET_ECAP = ecap
TARGET_EPLAY = eplay
CFLAGS += -I../include -I../prebuilt/include -fPIC -O2 -DVERSION=\"$(VERSION)\"


.PHONY: all
all: $(TARGET_ECAP) $(TARGET_EPLAY)

$(TARGET_ECAP): $(OBJECTS_ECAP) ../src/libetinyalsa.a ../prebuilt/lib/libklogging.a.0.2
	$(LD) $^ -lstdc++ -lpthread -ltinyalsa -ldl -o $@

$(TARGET_EPLAY): $(OBJECTS_EPLAY) ../src/libetinyalsa.a ../prebuilt/lib/libklogging.a.0.2
	$(LD) $^ -lstdc++ -lpthread -ltinyalsa -ldl -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

-include $(OBJECTS:.o=.d)

%.d: %.c
	@set -e; \
	rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$


.PHONY: clean
clean:
	-rm -f $(OBJECTS) $(OBJECTS:.o=.d) $(TARGET_ECAP) $(TARGET_EPLAY)


.PHONY: install
install:
	install -d $(BINDIR)/
	install $(TARGET_ECAP) $(BINDIR)/
	install $(TARGET_EPLAY) $(BINDIR)/


.PHONY: uninstall
uninstall:
	-rm -f $(BINDIR)/$(TARGET_ECAP)
	-rm -f $(BINDIR)/$(TARGET_EPLAY)
