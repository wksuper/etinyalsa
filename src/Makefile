CROSS_COMPILE ?=

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar
CFLAGS += -I../include -I../prebuilt/include -fPIC -O2 -DVERSION=\"$(VERSION)\"

OBJECTS = epcm.o queue.o resampler.o
SHARED_LIB_TARGET = libetinyalsa.so
STATIC_LIB_TARGET = libetinyalsa.a

.PHONY: all
all: $(SHARED_LIB_TARGET) $(STATIC_LIB_TARGET)

$(SHARED_LIB_TARGET): $(OBJECTS)
	$(LD) -shared $^ -o $@

$(STATIC_LIB_TARGET): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

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
	-rm -f $(OBJECTS) $(OBJECTS:.o=.d) $(SHARED_LIB_TARGET) $(STATIC_LIB_TARGET)


.PHONY: install
install:
	install -d $(LIBDIR)/
	install $(SHARED_LIB_TARGET) $(LIBDIR)/
	install $(STATIC_LIB_TARGET) $(LIBDIR)/


.PHONY: uninstall
uninstall:
	-rm -f $(LIBDIR)/$(SHARED_LIB_TARGET)
	-rm -f $(LIBDIR)/$(STATIC_LIB_TARGET)
