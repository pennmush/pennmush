Note: This is an old document and the steps in it have not been tested for some
time and it relies on long-obsolete software. The current reccomended ways to
use Penn on Windows is MinGW installed through MSYS2 (See README-MSYS2.txt) or
on the Windows 10 Linux subsystem (README-Windows10.txt). Raevnos hopes to
eventually supported Visual Studio 2017, though.

How to compile PennMUSH 1.8.x under Windows using Visual Studio
---------------------------------------------------------------
by Nick Gammon <nick@gammon.com.au> and Javelin and Luuk de Waard
Updated by Ervin Hearn <ehearn@pennmush.org>

Last update: Monday, 20 August 2012

For information on how to build the dependencies (OpenSSL and PCRE), please
refer to 
https://github.com/pennmush/pennmush/wiki/Installing-PennMUSH-on-Windows

1. From the top-level pennmush directory,

   For Visual Studio 2005 and later:
   No copying required!

   Visual Studio has been configured to copy the .dst files automatically if they
   do not exist, and the header files each time the project is compiled. This can
   be disabled or changed by going to Project -> pennmush Properties ->
   Configuration Properties -> Build Events -> Pre-Build Event and Post-Build
   Event.

   For MSVC++ 6 (not supported):
   Copy the following files        to:
   win32/config.h                  config.h
   win32/confmagic.h               confmagic.h
   win32/cmds.h                    hdrs/cmds.h
   win32/funs.h                    hdrs/funs.h
   win32/patches.h                 hdrs/patches.h
   src/cmdlocal.dst                src/cmdlocal.c
   src/flaglocal.dst               src/flaglocal.c
   src/funlocal.dst                src/funlocal.c
   src/local.dst                   src/local.c
   game/mushcnf.dst                game/mush.cnf
   game/aliascnf.dst               game/alias.cnf
   game/namescnf.dst               game/names.cnf
   game/restrictcnf.dst            game/restrict.cnf

   If you've already got src/*local.c files that you've modified,
   you'll just have to make sure that there are no new functions
   in src/*local.dst that are missing in your src/*local.c files

2. Open the appropriate Visual Studio project from the win32 subdirectory.

3. Compile.

4. From the top-level pennmush directory, the binary is: game/pennmush.exe

*** Note: Windows defaults to a stack size of 1 MB. The supplied project files
set the stack size to 8MB to match the default environment on Linux systems and
resolve several issues of early code termination on Windows. This setting can
also be changed on the precompiled executable by using the 'editbin.exe' tool
supplied by Microsoft in the Visual Studio installation, including the free
Express version. To set the stack size (in KB) run the following from a
command prompt in the game directory: editbin /STACK:<size> pennmush.exe
