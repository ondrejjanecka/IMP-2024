.PHONY: all doc clean clean-all copy-pdf pack

all: doc copy-pdf

doc:
	make -C doc

copy-pdf:
	cp doc/doc.pdf ./doc.pdf

clean:
	make -C doc clean

clean-all:
	make -C doc clean-all
	rm -f ./doc.pdf

pack:
	zip -r xjanec33.zip doc SmartWatering Makefile README.md secrets-example.cpp
