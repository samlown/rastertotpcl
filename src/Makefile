# default install paths
EXEC        = rastertotpcl
CUPSDIR     = $(shell cups-config --serverbin)
CUPSDATADIR = $(shell cups-config --datadir)

# MacOS system integrity protection prevents us from writing to $(cups-config --datadir)
# As a fallback, we copy the PPDs to $(cups-config --serverroot)/ppd
UNAME_S     = $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
CUPSDATADIR = $(shell cups-config --serverroot)
endif

CFLAGS  += $(shell cups-config --cflags)
CFLAGS  += -lm				# link math.h
CFLAGS  += -Wall			# enable all compiler warning messages
CFLAGS  += -Wno-deprecated-declarations	# do not warn about deprecated CUPS api

LDFLAGS += $(shell cups-config --ldflags)
LDLIBS  += $(shell cups-config --image --libs)

all: rastertotpcl ppd

.PHONY: all ppd install uninstall clean

rastertotpcl:
	gcc $(CFLAGS) $(LDFLAGS) $(LDLIBS) rastertotpcl.c -o $(EXEC)

ppd:
	ppdc tectpcl2.drv

install:
	install -s $(EXEC) $(CUPSDIR)/filter/
ifeq ($(UNAME_S),Darwin)
	if test ! -d $(CUPSDATADIR)/ppd/$(EXEC); then mkdir $(CUPSDATADIR)/ppd/$(EXEC); fi
	install -m 644 ppd/* $(CUPSDATADIR)/ppd/$(EXEC)
else
	install -m 644 tectpcl2.drv $(CUPSDATADIR)/drv/
	install -m 644 labelmedia.h $(CUPSDATADIR)/ppdc/
endif


uninstall:
	rm -f $(CUPSDIR)/filter/$(EXEC)
ifeq ($(UNAME_S),Darwin)
	rm -rf $(CUPSDATADIR)/ppd/$(EXEC)
else
	rm -f $(CUPSDATADIR)/drv/tectpcl2.drv
	rm -f $(CUPSDATADIR)/ppdc/labelmedia.h
endif

clean:
	rm -f $(EXEC)
	rm -rf ppd
