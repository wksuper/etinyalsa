CROSS_COMPILE ?=

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

OBJECTS = ecap.o
TARGET = ecap
CFLAGS += -I../include -I../prebuilt/include -fPIC -O2 -DVERSION=\"$(VERSION)\"


.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJECTS) ../src/libetinyalsa.a
	$(LD) $^ ../prebuilt/lib/libklogging.a.0.1 -lstdc++ -lpthread -ltinyalsa -ldl -o $@

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
	-rm -f $(OBJECTS) $(OBJECTS:.o=.d) $(TARGET)


.PHONY: install
install:
	install -d $(BINDIR)/
	install $(TARGET) $(BINDIR)/


.PHONY: uninstall
uninstall:
	-rm -f $(BINDIR)/$(TARGET)
