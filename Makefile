# Makefile wrapper for waf

all:
	./waf

configure:
	./waf configure --enable-examples -d debug -o build

build:
	./waf build

install:
	./waf install

clean:
	./waf clean

distclean:
	./waf distclean
