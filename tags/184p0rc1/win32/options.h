/* options.h */

#ifndef __OPTIONS_H
#define __OPTIONS_H

/* *********** READ THIS BEFORE YOU MODIFY ANYTHING IN THIS FILE *********** */
/* WARNING:  All options in this file have the ability to significantly change
 * the look and feel and sometimes even internal behavior of the program.
 * The ones shipped as the default have been extensively tested.  Others have
 * been tested to a (usually) lesser degree, and therefore might still have
 * latent bugs.  If you change any of them from the default, PLEASE check
 * to make sure that you know the full effects of what you are changing. And
 * if you encounter any errors or compile time problems with any options
 * other than the default settings, PLEASE inform 
 * pennmush-bugs@pennmush.org
 * immediately, so that they can be fixed.  The same goes for any other bug
 * you might find in using this software.  All efforts will be made to fix
 * errors encountered, but unless given a FULL description of the error,
 * (IE telling me that logging in doesn't work is insufficient.  telling
 * me that logging in with WCREAT undefined still gives you the registration
 * message is a lot better.  MOST effective would be a full dbx trace, or a
 * patch for the bug.)  Enjoy using the program.
 */
/***************************************************************************/

/*---------------- Internals with many options ------------------------*/

/* Malloc package options */
/* malloc() is the routine that allocates memory while the MUSH is
 * running. Because mallocs vary a lot from operating system to operating
 * system, you can choose to use one of the mallocs we provide instead of
 * your operating system's malloc. 
 * Set the value of MALLOC_PACKAGE  to one of these values:
 *  0 -- Use my system's malloc. Required for Win32 systems.
 *       Recommended for FreeBSD, Linux, Mac OS X/Darwin, and other OS's
 *       where you think the malloc routines are efficient and debugged.
 *       Otherwise, use only as a last resort.
 *  1 -- Use the CSRI malloc package in normal mode. 
 *       Recommended for most operating systems where system malloc is
 *       suspect. Known to work well on SunOS 4.1.x.
 *  2 -- Use the CSRI malloc package in debug mode. 
 *       Only use this if you're tracking down memory leaks. Don't use
 *       for a production MUSH - it's slow.
 *  5 -- Use the GNU malloc (gmalloc) package. 
 *       Doesn't work on Alpha processors or FreeBSD systems, and
 *       reportedly flaky on Linux. Requires an ANSI compiler.
 *       Otherwise, similar to CSRI malloc.
 *  3, 4, 6 -- Same as 0, kept for compatibility. 
 */
#define MALLOC_PACKAGE 0

/* What type of attribute compression should the MUSH use?
 * Your options are:
 * 1 - the default Huffman compression which has been in use for a
 *     long time. In theory, this should be the best compression,
 *     possibly at the cost of some speed. It is also 8-bit clean,
 *     and thus suitable for locales that use extended character sets.
 *     Sometimes has trouble on some linux systems for some reason.
 * 2 - Same as 1, for backwards compability.
 * 3 - Nick Gammon's word-based compression algorithm.
 *     In theory, this should be considerably faster than Huffman
 *     when decompressing, and considerably slower when compressing.
 *     (But you decompress a lot more often). Compression ratio
 *     is worse than Huffman for small dbs (<1.5Mb of text), but
 *     better for larger dbs. Win32 systems must use this.
 * 4 - Raevnos's almost 8-bit clean version of the word-based algorithm.
 *     Prefer 3 unless you need extended characters. This algorithm
 *     can encode all characters except 0x06.
 * 0 - No compression at all. Very fast, but your db in memory
 *     will be big - at least as large as your on-disk db.
 *     Possibly suitable for the building stages of a small MUSH.
 *     This should be 8-bit clean, too.
 * You can change this at any time, with no worries. It only affects
 * the in-memory compression of attribute/mail text, not the disk
 * db compression. Recommend to keep it at 1. When in doubt, try them
 * all, and check @uptime's memory usage stats for the most efficient
 * choice among those that are stable for you. When using word-based
 * compression, you can also #define COMP_STATS to get some detailed
 * information in @stats/tables.
 */
#define COMPRESSION_TYPE 4


/*------------------------- Other internals ----------------------*/

/* If defined, use the info_slave to get information from identd,
 * instead of having the MUSH do it directly.  This may help reduce lag
 * from new logins.  This does _not_ work under Win32.
 */
/* #define INFO_SLAVE /* */

/*------------------------- MUSH Features ----------------------*/

/* Many MUSHes want to change the +channels to =channels. That's
 * annoying. So we've got this CHAT_TOKEN_ALIAS, which allows + as well
 * as = (or whatever) channels. If you want this, define it to be
 * the character you want to use in addition to +, enclosed in
 * single quotes, as in '=' or '.' or whatever. Don't define it to '+'!
 */
#define CHAT_TOKEN_ALIAS '=' /* */


/*------------------------- Cosmetic Features --------------------*/

/* If you're using the email registration feature, but want to 
 * use a mailer other than sendmail, put the full path to the mailer
 * program here. The mailer must accept the -t command-line
 * argument ("get the recipient address from the message header To:").
 * If it doesn't, you could probably write a wrapper for it.
 * Example: #define MAILER "/full/path/to/other/mailer"
/* #define MAILER /* */


#endif
