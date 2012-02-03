#!/usr/local/bin/csi -script
#| !# ; |#

; Print out a list of all typedefs in the src in a format suitable for
; using in the indent rule for src/Makefile. Requires chicken scheme.
; http://call-with-current-continuation.org or your package manager.
;
; Written by Raevnos <shawnw@speakeasy.org> for PennMUSH.
;
; Version 1.0
;
; Usage:
; % csc -O2 -heap-initial-size 768k -o typedefs utils/typedefs.scm
; % make etags
; % ./typedefs < src/TAGS > indent.defs
; % emacs src/Makefile.in indent.defs
;    edit Makefile.in to modify the indent typedef section
; % ./config.status
; % make
;
; You can also use it as an interpreted script:
; % csi -script utils/typedefs.scm < src/TAGS > indent.defs
; or
; % chmod +x typedefs.scm
; % ./typedefs.scm < TAGS
;
; This is just slower to run, thus good for occasional use.
; Some rough time trials suggest that the compiled version runs
; twice as fast. Of course, it also takes a while to compile it...
;
;
; TODO
; Take command-line arguments: The TAGS file and whether or not to
; generate output to be included into a Makefile (Lines continued by \)
; or into an .indent.pro file. # of columns, tab stops, etc.

; Optimization and module directives. 
(cond-expand
 ((and chicken compiling)
  (declare
   (fixnum)
   (block)
   (usual-integrations)
   (disable-interrupts)
   (no-procedure-checks-for-usual-bindings)
   (safe-globals)
   (uses utils srfi-1 srfi-13 srfi-69)))
 ((and chicken csi)
  (require-extension utils srfi-1 srfi-13 srfi-69)))

(define (for-each-line f)
  (let loop ((line (read-line)))
    (unless (eof-object? line)
	    (f line)
	    (loop (read-line)))))

; Return what's between the first occurance of fc and the last of lc in
; a string. Raises an error if index(fc) > index(lc) or one of the two
; doesn't exist.
(define-syntax define-typename-extractor
  (syntax-rules ()
    ((_ (name) fif lif)
     (define (name str fc lc)
       (let ((first-index (fif str fc))
	     (last-index (lif str lc)))
	 (if (and (integer? first-index)
		  (integer? last-index)
		  (fx< first-index last-index))
	     (substring/shared str (fx+ first-index 1) last-index)
	     (signal 'out-of-range)))))))

(define-typename-extractor (string-between/shared-ctags)
  string-index string-index-right)
(define-typename-extractor (string-between/shared-emacs)
  string-index-right string-index-right)

; The special characters that mark the start and end of an identifier
(define-constant type-start (integer->char 127))
(define-constant type-end (integer->char 1))

; Either return a typedef name or a symbol : 'line-did-not-match or
; 'read-more-lines
(define process-line
  (let*
      ((in-struct-typedef? #f)
       (in-enum-typedef? #f)
       (copy-typedef-ctags
	(cut string-between/shared-ctags <> type-start type-end))
       (copy-typedef-emacs 
	(cut string-between/shared-emacs <> #\space #\;))
       (copy-typedef
	(lambda (str)
	  (if (string-index str type-end) 
	      (copy-typedef-ctags str)
	      (copy-typedef-emacs str)))))
    (lambda (line)
      (handle-exceptions
       exn (if (eq? exn 'out-of-range)
	       (begin
		 (format (current-error-port) "Unable to extract typedef name from line: ~A\n"
			 line)
		 'line-did-not-match)
	       (abort exn))
       (cond
	(in-struct-typedef?
	 (if (char=? (string-ref line 0) #\})
	     (begin
	       (set! in-struct-typedef? #f)
	       (copy-typedef line))
	     'read-more-lines))
	(in-enum-typedef?
	 (if (char=? (string-ref line 0) #\})
	     (begin
	       (set! in-enum-typedef? #f)
	       (copy-typedef line))
	     'read-more-lines))
	((string-prefix? "typedef struct " line)		     
	 (if (string-index line #\;)
	     (copy-typedef line)
	     (begin
	       ;; If the struct is defined here the typedef name
	       ;; is on the next line starting with }. There are 
	       ;; optional structure member lines between.
	       (set! in-struct-typedef? #t)
	       'read-more-lines)))
	((string-prefix? "typedef enum " line)
	 (if (string-index line #\;)
	     (copy-typedef line)
	     (begin
	       ;; Skip enum values
	       (set! in-enum-typedef? #t)
	       'read-more-lines)))
	((string-prefix? "typedef " line)
	 (copy-typedef line))
	((string-prefix? "} " line)
	 ;; We get this with a typedef of an anonymous struct.
	 ;; If it then starts an array, some versions of etags
	 ;; won't record the typedef name and you'll get a warning.
	 (copy-typedef line))
	(else 'line-did-not-match))))))

; Read all typedefs from an inchannel
(define (read-typedefs)
  (let*
      ((typedefs (make-hash-table #:hash string-hash #:test string=?))
       (fl-proc (lambda (line) 
		  (let ((res (process-line line)))
		    (if (string? res) (hash-table-set! typedefs res #t))))))
    (for-each-line fl-proc)
    typedefs))


; Control pretty-printing of the typedefs.
(define-constant max-column-width 75)
(define-constant tab-stop 8)

; Print out one typedef to stdout, formated as indent args.
(define emit-typedef 
  (let
      ((column tab-stop))
    (lambda (typedef)
      (let* ((start-of-line? (fx= column tab-stop))
	     (len (fx+ (string-length typedef)
		     (if start-of-line? 3 4))))
	(if (fx>= (fx+ column len) max-column-width)
	    (begin 
	      (display " \\\n\t")
	      (set! column tab-stop)
	      (set! start-of-line? #t)))
	(if start-of-line?
	    (printf "-T ~A" typedef)
	    (printf " -T ~A" typedef))
	(set! column (fx+ column len))))))


(define (list->hash-table lst . args)
  (let ((pairs (map (cut cons <> #t) lst)))
    (apply alist->hash-table pairs args)))

; Predefined typedefs
(define *standard-typedefs* 
  (list->hash-table
	'("int8_t" "uint8_t" "int16_t" "uint16_t"
	  "int32_t" "uint32_t" "int64_t" "uint64_t"
	  "intmax_t" "uintmax_t"
	  "size_t" "ssize_t" "sig_atomic_t" "time_t"
	  "pid_t" "socklen_t" "ptrdiff_t" "sigset_t"
	  "ldiv_t" "bool" "BOOL"
	  "BIO" "EVP_MD" "SSL_CTX" "SSL_METHOD" "X509_STORE_CTX"
	  "X509" "DH" "SSL"
	  "MYSQL_RES" "MYSQL_FIELD" "PGresult" "sqlite3_stmt"
	  "pcre" "pcre_extra")
   #:test string=? #:hash string-hash))
   
; structs that contain anonymous structs or unions sometimes confuse things.
(define *false-positives* '("str" "val" "data" "sub" "sw" "memo" "hooks" "handle"))
					     
; main
(define typedef-table (read-typedefs))
(for-each (cut hash-table-delete! typedef-table <>) *false-positives*)
(define typedefs
  (sort (hash-table-keys (hash-table-merge typedef-table *standard-typedefs*)) string-ci<))

(write-char #\tab)
(for-each emit-typedef typedefs)
(newline)
