TEXFILE=doc

all: $(TEXFILE).pdf

$(TEXFILE).pdf: $(TEXFILE).tex
	latex $(TEXFILE).tex
	latex $(TEXFILE).tex
	dvips -t a4 $(TEXFILE).dvi
	ps2pdf -sPAPERSIZE=a4 $(TEXFILE).ps

gen-clean: all clean

clean:
	rm -f $(TEXFILE).aux $(TEXFILE).dvi $(TEXFILE).log $(TEXFILE).ps $(TEXFILE).out

clean-all: clean
	rm -f $(TEXFILE).pdf
