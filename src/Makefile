
EXEC=rastertotpcl
CFLAGS=`cups-config --cflags`
LDFLAGS=`cups-config --ldflags`
LDLIBS=`cups-config --image --libs`
PPDPATH=/usr/local/share/ppd
EXECPATH=/usr/local/libexec/cups/filter

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
