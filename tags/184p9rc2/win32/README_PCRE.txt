Building PennMUSH 1.8.x under Windows with PCRE
-----------------------------------------------
Written by Daniel Powell <daniko@M*U*S*H>

Last update: Thursday, Nov 17, 2011
Current Version of PCRE: 8.20
Current Precompiled Version of PCRE: 7.0
PennMUSH Version: 1.8.4p8

These directions are oriented toward those using MSVC++ or another Graphical
Windows IDE. Those using MinGW+MSys should install PCRE just as if they were
doing so on Linux.

*PCRE can be found at http://www.pcre.org/. The most current version can be 
pulled manually from: ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/
   
*Pre-compiled libraries have also been released, though are a bit outdated.
They can be found at: http://gnuwin32.sourceforge.net/packages/pcre.htm

Building PCRE from the Source
-----------------------------
1. Download the PCRE source from http://ftp.csx.cam.ac.uk via the link above.

2. Download the Visual Studio 200x file located at:
   ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre/Contrib/pcre-vsbuild.zip
   
3. Unzip both archives into C:\PCRE. You will then want to merge the contents
   of the new pcre-vsbuild directory with the main source directory titled 
   pcre-<version> (currently pcre-8.20).
   
4. Navigate into the sourcecode directory (pcre-2.80 for our example here).
   Rename config.h.generic to config.h. Also rename pcre_chartables.c.dist to
   pcre_chartables.c. Ensure that ucp.h and pcre_internal.h both exist. Also
   ensure that pcre_printint.src is present. You can view the config.h file
   to verify that all the desired options are accounted for. I myself didn't
   have to do anything to it.

5. Navigate into the build directory and open either vc2005 or vc2008,
   depending on which version of Visual Studio you are using. If you are using
   Visual Studio 2010, the Conversion Wizard will convert the 2008 project via
   a prompt when you open it.

6. Attempt to build dftables, since I heard that you should build that before
   anything else. You may then build the pcre project.
   
7. Navigate into the Debug directory. The two files which we are interested in
   are pcre78d.dll and pcre78.lib. Copy these two files out above the pcre-8.20
   folder into C:\PCRE. You may distribute these into respective lib and bin
   subfolders if you wish.
   
   Note: I'm sure that somewhere in the Visual Studio project there is an 
   option to change the output filenames of the lib and dll. The project itself
   is not kept up to date with the latest release, but it still DOES use the
   latest source and header files, so the functionality will still be there
   even if the filenames are wrong.
   
   C:/PCRE/
   
   +--> bin
   |
   +--> pcre-8.20 +-> build +-> vc2005 -> Debug
   |              |         +-> vc2008 -> Debug 
   |              |         \-> vcres
   |              +-> cmake
   |              +-> doc -> html
   |              +-> sljit
   |              \-> testdata
   \--> lib
   
 8. You can now open your PennMUSH project and edit it as follows: In the
    project properties, navigate to the VC++ Directories listing. Add 
    C:\PCRE\pcre-8.20 onto the tail end of the Include Directories list. Also
    add C:\PCRE\lib into the Library Directories list.
 
 9. Navigate to Linker > Input > Additional Dependencies and add the filename
    for the lib file you copied out in step 7. Specify the correct directory
    and file which if the recommendations were followed to the letter should be
    C:\PCRE\lib\pcre78.lib. 
    
 9. In the PCRE for Win32 main page, they also state that it is important to
    have msvcrt.dll installed on your computer (dependent on iexplorer 4).
    However, this dll has to be explicitly pulled into the project as well with
    its respective lib file. Thus, you will need to also add msvcrt.lib to the
    Linker's Additional Dependencies list. If this is omitted, you will receive
    Debug Assertion Failed errors when trying to start pennmush.exe.
    
    During linking, Visual Studio will also warn you of a conflict between
    msvcrt.lib and libcmtd.lib. However, PCRE _requires_ msvcrt.lib, so you
    will need to add libcmtd.lib to the "Ignore Specific Default Libraries"
    list in the Linker Input.
    
10. You may now attempt a build of PennMUSH. If all goes well, you should end
    up with the executable in the Games directory. However, if you run it now,
    you will receive an error that it cannot find the PCRE dll. You should now
    either copy pcre78d.lib into your PennMUSH game directory, or into a folder
    that is in your computer's environment path, OR add C:\PCRE\bin (where we 
    put pcre78d.dll) to your environment path.
    
    PennMUSH should now run and continue running without any stack overflow or
    debug errors.

Building with Pre-Compiled Libraries
------------------------------------
1. Download the PCRE Developer Files from http://gnuwin32.sourceforge.net via
   the link above, or get everything at: 
   http://sourceforge.net/projects/gnuwin32/files/pcre/
   
   Decompress these packages into a new folder. It is recommended this be
   somewhere easy to work with such as C:\PCRE. You MUST at least have the lib
   and includes folder. Everything else is optional.
   
   If EVERYTHING is downloaded, you should end up with a file structure similar
   to the following:
   
   C:\PCRE\  
    
    +-bin
    +-contrib -> pcre -> 7.0 +-> pcre-7.0
    |                        |
    |                        \-> pcre-7.0-src
    |
    +-doc +-> pcre -> 7.0 -> pcre-7.0
    |     |
    |     \-> perl -> 5.88
    |
    +-include
    +-lib -> pkgconfig
    |
    +-man +-> cat1
    |     +-> cat3
    |     +-> html
    |     +-> man1
    |     +-> man3
    |     +-> pdf
    |     \-> ps
    |
    +-manifest
    +-share
    +-src -> pcre -> 7.0 +-> pcre-7.0 +-> patches
    |                    |            +-> resources
    |                    |            \-> testdata
    |                    |
    |                    \-> pcre-7.0-src +-> doc -> html
    |                                     \-> testdata
    \-uninstall

2. Open your PennMUSH project and add C:\PCRE\include to the Includes 
   Additional Directory as well as C:\PCRE\lib to the Libraries Additional 
   Directory. These options can be found in the Project properties under VC++
   Directories.
   
3. Navigate into the Project's Properties>Linker>Additional Dependencies. To 
   the end of this field, add pcre.lib and pcreposix.lib (the two main 
   libraries found in C:\PCRE\lib.
   
4. You should now be able to build/rebuild the project successfully. And when
   these libraries are updated on the GnuWin website, you should be able to 
   grab the new headers and libraries and recompile

Note: If you run into any compile errors where the "pcre" identifier is somehow
      undefined, the problem I discovered was missing HAVE_PCRE and
      HAVE_PCRE_H definitions in win32/config.h. You can temporarily add these
      definitions in order to force mypcre.h to verify that they do indeed
      exist.
     
Note 2: PCRE 7.0 causes PennMUSH to return a Debug Assertion Failure when it is
        started. This may be due to a stack overflow error discovered in one of
        the PCRE functions. Thus, it is recommended that you build your own via 
        the source instead of using the pre-compiled libraries.