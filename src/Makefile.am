AM_CFLAGS = $(LIBUSB_CFLAGS) \
            $(AO_CFLAGS) \
            $(BLUEZ_CFLAGS)

bin_PROGRAMS=aoaproxy
aoaproxy_SOURCES=aoaproxy.c accessory.c a2spipe.c audio.c bluetooth.c tcp.c
aoaproxy_LDADD = $(LIBUSB_LIBS) $(AO_LIBS) $(BLUEZ_LIBS)
