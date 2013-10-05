#!/usr/bin/env csi -script
;;; Convert a diff file made with Windows-style \ directory paths to
;;; Unix-style / paths. Works with context and unified diffs. Requires
;;; chicken scheme (http://www.call-with-current-continuation.org).
;;;
;;; Written by Shawn Wagner (Raevnos) and placed in public domain.
;;; No warranty. Use at your own risk. Blah blah blah.
;;;
;;; Usage: ./utils/fixdiff.scm < win32.patch > unix.path
;;; or compile with: csc -O2 -o fixdiff fixdiff.scm

(cond-expand
 ((and chicken compiling)
  ; Boilerplate for turning on optimizations
  (declare
   (block)
   (usual-integrations)
   (fixnum)
   (disable-interrupts)
   (no-procedure-checks-for-usual-bindings)
   (safe-globals)
   (always-bound path-regexp)
   (uses utils)))
 ((and chicken csi)
  ; Load the appropriate libraries in the interpeter
  (use utils)))

(require-extension regex)

(define (for-each-line f)
  (let loop ((line (read-line)))
    (unless (eof-object? line)
	    (f line)
	    (loop (read-line)))))

;;; Two ways of doing it.

;; Some testing suggests the regular expression approach is a little
;; bit faster (Especially when run through csi instead of compiled to
;; a binary)
(define path-regexp (regexp "^(?:\\+\\+\\+|---|\\*\\*\\*|Index:|diff)\\s"))
(define (fix-paths line)
  (if (string-search path-regexp line)
      (string-translate line #\\ #\/)
      line))

;; But this version, using the SRFI-13 string-prefix? function, is a lot
;; more readable when it comes to seeing what marks a line with a path that needs
;; to be converted.
;(define (fix-paths line)
;  (if (or (string-prefix? "+++ " line)
;          (string-prefix? "--- " line)
;          (string-prefix? "*** " line)
;          (string-prefix? "Index: " line)
;          (string-prefix? "diff " line))
;      (string-translate line #\\ #\/)
;      line))

;; The driver, equivalent to perl's behavior when invoked with -p
(for-each-line (compose print fix-paths))

;;; As an exercise, compare with the following idiomatic perl equivalent:

; #!/usr/bin/env perl -p
; s!\\!/!og if m!^(?:\+\+\+|---|\*\*\*|Index:|diff)\s!o;
;
; Without -p, but still using all the things that give perl a bad name
; (Like relying on $_ instead of explict variables) it'd be more like:
;
; while (<>) {
;     s!\\!/!og if m!^(?:\+\+\+|---|\*\*\*|Index:|diff)\s!o;
;     print;
; }
;
; I think the scheme version is far more readable, with only a few
; more lines of actual code (Ignoring all the boilerplate stuff in the
; cond-expand. It's just hints to the compiler and loading libraries).
; Well, now that chicken removed for-each-line, it's longer...
