
DIRS = src

all:
	- for d in $(DIRS); do (cd $$d; $(MAKE) all); done

install:
	- for d in $(DIRS); do (cd $$d; $(MAKE) install); done

uninstall:
	- for d in $(DIRS); do (cd $$d; $(MAKE) uninstall); done

clean:
	- for d in $(DIRS); do (cd $$d; $(MAKE) clean); done

