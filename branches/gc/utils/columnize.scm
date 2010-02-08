#!/usr/local/bin/csi -script
#| !# ; |#
;;; Formats words in columns for inclusion in help files. 

;; Works with chicken or guile

;; Reads from standard input, prints to standard output. Intended to
;; be used from within an editor to replace a table in-place.

;; For emacs:
;; Mark the current table, then C-u M-| utils/columnize.scm
;;
;; For vi:
;; Something like :jfadskjfq423jram utils/columnize.scm
;;
;; (Or, using guile instead of chicken: guile -s utils/columize.scm)
;;
;; Compiled instead of interpeted:
;; csc -o columize -O2 utils/columnize.scm
;;    (display "|")
;; This is similar to column(1) but that doesn't always work the way
;; we need. This does.

(cond-expand
 ((and chicken compiling)
  (declare (block)
	   (fixnum)
	   (usual-integrations)
	   (disable-interrupts)
	   (uses srfi-1 srfi-13 regex extras)))
 ((and chicken csi)
  (use srfi-1 srfi-13 regex extras))
 (guile
  (use-modules (srfi srfi-1) (srfi srfi-13) (ice-9 regex)
	       (ice-9 rdelim))
  (define fx= =)
  (define fx+ +)
  (define fx< <)
  (define fx> >)
  (define fxmax max)
  (define (read-lines)
    (let loop ((line (read-line))
	       (accum '()))
      (if (eof-object? line)
	  (reverse accum)
	  (loop (read-line) (cons line accum)))))
  (define-macro (define-constant sym val)
    `(define ,sym ,val))
  (define (string-split-fields re str)
    (map match:substring (list-matches re str)))))

(define-constant line-width 78)

(define (drop-while pred? lst)
  (cond
   ((null? lst) '())
   ((pred? (car lst)) (drop-while pred? (cdr lst)))
   (else lst)))

(define words
  (drop-while (lambda (w) (fx= (string-length w) 0))
	      (sort
	       (string-split-fields "[A-Za-z0-9_@()-]+"
				    (string-join (read-lines) " "))
	       string-ci<?)))
(define max-word-length (fold (lambda (w len) 
				(fxmax (string-length w) len))
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
	


