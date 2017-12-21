.PHONY: all

all: *.html

%.html: ../%.md
	pandoc -t html5 -o $@ -s --toc $<
