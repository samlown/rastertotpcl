
EXEC=rastertotpcl
CFLAGS=-lcupsimage -lcups
PPDPATH=/usr/share/ppd
EXECPATH=/usr/lib/cups/filter

all: rastertotpcl ppd

.PHONY: ppd clean install uninstall

$(EXEC): rastertotpcl.c

ppd:
	ppdc tectpcl2.drv

install:
	if test ! -d $(PPDPATH)/$(EXEC); then mkdir $(PPDPATH)/$(EXEC); fi
	cp ppd/* $(PPDPATH)/$(EXEC)
	cp $(EXEC) $(EXECPATH)/
	

uninstall:
	rm -rf $(PPDPATH)/$(EXEC)
	rm -f $(EXECPATH)/$(EXEC)


clean:
	rm -f rastertotpcl
	rm -rf ppd
