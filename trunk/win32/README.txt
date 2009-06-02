How to compile PennMUSH 1.8.x under Windows (MSVC++/MS VS.NET)
----------------------------------------------
by Nick Gammon <nick@gammon.com.au> and Javelin and Luuk de Waard
Updated by Ervin Hearn <ehearn@pennmush.org>

Last update: Saturday, 11 November 2006

1. From the top-level pennmush directory,
   Copy the following files        to:

   For MSVC++ 6:
   win32/msvc6/pennmush.dsw        pennmush.dsw
   win32/msvc6/pennmush.dsp        pennmush.dsp
   win32/config.h                  config.h
   win32/confmagic.h               confmagic.h
   win32/options.h                 options.h
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


   For MS VS.NET:
   win32/msvc.net/pennmush.vcproj  pennmush.vcproj
   win32/msvc.net/pennmush.sln     pennmush.sln

   VS.NET has been configured to copy the .dst files automatically if they
   do not exist, and the header files each time the project is compiled. This
   can be disabled or changed by going to Project -> pennmush Properties ->
   Configuration Properties -> Build Events -> Pre-Build Event and Post-Build
   Event.


   If you've already got src/*local.c files that you've modified,
   you'll just have to make sure that there are no new functions
   in src/*local.dst that are missing in your src/*local.c files

2. Use supplied project files in the top-level pennmush directory.

3. Compile

4. From the top-level pennmush directory, the binary is: game/pennmush.exe

*** Note: Windows defaults to a stack size of 1 MB. The supplied project files
set the stack size to 8MB to match the default environment on Linux systems and
resolve several issues of early code termination on Windows. This setting can
also be changed on the precompiled executable by using the 'editbin.exe' tool
supplied by Microsoft in the Visual Studio installation, including the free
Express version. To set the stack size (in KB) run the following from a
command prompt in the game directory: editbin /STACK:<size> pennmush.exe
