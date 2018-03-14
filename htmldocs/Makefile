.PHONY: all

all: index.html *.html

index.html: index.md
	pandoc -t html5 -c mushdoc.css -o index.html -s index.md

%.html: ../%.md
	pandoc -f markdown-smart -t html5 -c mushdoc.css -o $@ -s --toc --toc-depth=2 $<
