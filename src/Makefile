
EXEC=rastertotpcl
CFLAGS != cups-config --cflags
LDFLAGS != cups-config --ldflags
LDLIBS != cups-config --image --libs
CUPSDATADIR != cups-config --datadir
PPDPATH=${CUPSDATADIR}/model
CUPSDIR !=cups-config --serverbin

all: rastertotpcl ppd

.PHONY: ppd clean install uninstall

$(EXEC): rastertotpcl.c

ppd:
	ppdc tectpcl2.drv

install:
	install -s $(EXEC) $(CUPSDIR)/filter/
	install -m 644 tectpcl2.drv $(CUPSDATADIR)/drv/
	install -m 644 labelmedia.h $(CUPSDATADIR)/ppdc/
	

uninstall:
	rm -f $(CUPSDIR)/filter/$(EXEC)
	rm -f $(CUPSDATADIR)/drv/tectpcl2.drv
	rm -f $(CUPSDATADIR)/ppdc/labelmedia.h


clean:
	rm -f rastertotpcl
	rm -rf ppd
