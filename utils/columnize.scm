#!/usr/local/bin/csi -script
#| !# ; |#
;;; Formats words in columns for inclusion in help files. 

;; Works with chicken scheme

;; Reads from standard input, prints to standard output. Intended to
;; be used from within an editor to replace a table in-place.

;; For emacs:
;; Mark the current table, then C-u M-| utils/columnize.scm
;;
;; For vi:
;; Something like :jfadskjfq423jram utils/columnize.scm
;;
;;
;; Compiled instead of interpeted:
;; csc -o columize -O2 utils/columnize.scm
;;
;; This is similar to column(1) but that doesn't always work the way
;; we need. This does.

(cond-expand
 ((and chicken compiling)
  (declare (block)
	   (fixnum)
	   (usual-integrations)
	   (no-procedure-checks-for-usual-bindings)
	   (safe-globals)
	   (disable-interrupts)))
 ((and chicken csi)))

(require-extension srfi-1 srfi-13 extras regex)

(define-constant line-width 78)

(define words
  (drop-while string-null?
	      (sort
	       (string-split-fields (regexp "[A-Za-z0-9_@()-]+")
				    (string-join (read-lines) " "))
	       string-ci<?)))
(define max-word-length (fold (lambda (w len) (fxmax (string-length w) len))
			      0 words))
(define column-width (fx+ max-word-length 3))
(define ncolumns (quotient line-width column-width))

(define (print-word word column)
  (cond 
   ((fx= column 1)
    (display "  ") 
    (display (string-pad-right word column-width))
    2)
   ((fx< column ncolumns)
    (display (string-pad-right word column-width))
    (fx+ column 1))
   (else
    (display word)
    (newline)
    1)))

(if (fx> (fold print-word 1 words) 1) (newline))
	


