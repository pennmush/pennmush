% Installation Guide to PennMUSH 1.8.x

Introduction
============

This file explains how to install PennMUSH. It comes in three parts:

1. Important background
2. Installation from source (recommended)
3. Installation of precompiled binaries (only for Windows platforms)

If you are upgrading from a previous PennMUSH release, this is
probably not the file you want to start with. Read the
[UPGRADING.md](UPGRADING.html) file first.

**DISCLAIMER**: Before attempting to run a MUD of any sort, you should
have some reasonable knowledge of UNIX and C.  If you do not, it is
_strongly_ suggested that you learn UNIX and C to some reasonable
level of competency before attempting to set up a MUSH.  (Note that
even people using the Windows ports are encouraged to know UNIX,
because that's the paradigm that PennMUSH was built with, and most
resources will be written with UNIX is mind.)

You may also want to take a look at the Managing PennMUSH book at
<http://community.pennmush.org> and at Javelin's Guide for PennMUSH
Gods, at <https://download.pennmush.org/Guide/guide-single.html>
 
Important background
====================

Here's a quick picture of the organization of the MUSH directory tree.
The "src" directory contains C source code.  The "hdrs" directory
contains header files for the source code.  The files used by a
running MUSH are in the "game" directory, which includes
subdirectories "data" (current databases), "txt" (text files and
directories for building them), "log" (log files), and "save" (backup
databases).  Finally, the "hints" directory is used during the
installation process, the "test" directory contains an automated
regression testing suite, and the "po" directory holds translation
message files.

    pennmush--+-> src
               +-> hdrs 
               +-> game ------+-> data 
               |              |   
               |              +-> txt -------+-> nws 
               |              |              +-> evt 
               |              |              \-> hlp 
               |              |                  
               |              +-> log 
               |              \-> save 
               +-> hints 
               +-> po
               +-> utils 
               +-> test
               \-> win32
               

PennMUSH has been tested on a fairly wide variety of machines and
operating systems including at least:

* GNU/Linux
* NetBSD
* OpenBSD
* Mac OS X
* Microsoft Windows
         
There's no real reason why PennMUSH shouldn't compile on any 32-bit or
better BSD, System V, or POSIX operating system.  Development is
primarily done on GNU/Linux and NetBSD systems.

Mac OS 9 and earlier ("Classic"), and OS/2 are not supported.

Installation from source
========================

The quickstart version of the installation is:

1. On win32 only, install proper tools or read win32/README*.
2. On Unix systems, you need: A C compiler, perl, the minimum
    development packages required to compile programs (Linux
    distributions that don't come with gcc in the base install often
    need a package named glibc-dev).
3. A version of the OpenSSL library; most OSes these days come with
    it out of the box, but some might require a development package as
    well (libssl-dev, openssl-dev or some such name).
4. Development versions of a number of optional libraries are nice to have:
    * A sql client library for MySQL/MariaDB or Postgresql.
    * ICU (For enhanced Unicode support)
    * libevent (For ssl_slave)
    * libcurl (For `@http`)
5. Run ./configure with any desired options (See ./configure --help or
   read below)
6. make update
7. make install
8. possibly make customize
9. Read game/README and follow those instructions

Here's the process in detail:

1. If you're running on win32, read one of the win32/README* files
   for information on how to compile with various compilers.

2. On Unix systems, unpack the code and:

        % cd pennmush
        % ./configure 

   Useful arguments to configure:
   
   `--disable-sql`
   
   :    Don't compile in SQL support. See [README.SQL](README.SQL.html) for
        more sql-related config options.

   `--disable-nls`
   
   :    Turn off translation support if you don't need it.
   
   `--disable-info_slave`
   
   :    Don't use an external process to do hostname lookups. This option
        is required on Windows.

   `--help`
   
   :    See all options.

   Environment variables to customize search paths:

   * CPPFLAGS=-I/path/to/extra/headers
   * LDFLAGS=-L/path/to/extra/libraries
   * CFLAGS=-Optimization and -Warning options.

   See hints/your-os.txt and hints/your-processor if present for more
   details and system-specific help, and [README.SQL](README.SQL.html)
   for help with detecting a SQL server.
	

3. Run `make update`, and answer all the questions about which MUSH
options you want. This will also create several config files in game/
which you may wish to edit, particularly game/mush.cnf, and will create
the game/restart script that's used to start your MUSH.

4. Do a `make install`. This will build all the necessary files, and
set up some symbolic links for the restart script.  You will probably
receive a few compilation warnings, which can generally be ignored.

5. If you plan to run multiple MUSHes, you may want to do a "make
customize" which will run a script to help set up a separate
customized game subdirectory for each MUSH (run it once per MUSH you
plan to run).  Files in these subdirectories will already be
customized in many ways, so what follows may be slightly different. :)
This is probably broken.

6. Read game/README and follow those instructions. 

A final thing you may want to think about is compiling portmsg.c
(`make portmsg`). This is a port announcer; if your MUSH ever goes
down, you can set one up, and a message will be given to a person
attempting to connect to that port.  Read the comments in
src/portmsg.c for details. It is not an official MUSH piece of code;
rather, it is a freely distributable program available via anonymous
FTP that is included in this code because it happens to be fairly
useful.

