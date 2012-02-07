Building PennMUSH 1.8.x under windows with OpenSSL
--------------------------------------------------
by Daniel Powell <daniko@M*U*S*H>

Last update: Friday, 11 November 2011
Current Version of OpenSSL: 1.0.0e
PennMUSH Version: 1.8.4p8

This file is directed toward those compiling PennMUSH with Visual Studio 2005, 2008
or 2010. README.SSL (located in the PennMUSH root director) should be read before 
continuing with these instructions.

1. The OpenSSL homepage is located at http://www.openssl.org. However, as a Windows 
   user, it will relay you to http://www.slproweb.com/products/Win32OpenSSL.html.
   
   At the bottom of this page you will notice a few downloads such as Win32 OpenSSL
   Light, Full, 64-bit, and a few different versions. Also offered are links to the
   Visual C++ 2008 Redistributables. 
   
   The "light" version of OpenSSL does NOT include headers or libraries needed to
   compile programs with it enabled. Thus, you will need the non-"light" version
   currently labelled "Win32 OpenSSL v1.0.0e".
   
2. Install Win32OpenSSL-1_0_0e.exe. It is also recommended that you select the option
   to copy the executables to the OpenSSL/bin folder rather than Windows/System32 and 
   edit your environment path afterward.
   
3. Set up Visual Studio project with the include and lib directories (default is
   C:\OpenSSL-Win32\include and C:\OpenSSL-Win32\lib). In conjunction with README.SSL
   in the PennMUSH root directory, Noltar states that "it requires compiling both 
   OpenSSL and PennMUSH in /MD (multithread dll) mode." Therefore, you will also need
   to include "C:\OpenSSL-Win32\lib\VC" in your project's "Library Directories"
   setting.
   
   You will also need to navigate to the Linker/Input project property and add 
   both libeay32md.lib and ssleay32md.lib to the "Additional Dependencies" list. 
   These two libraries are located in C:\OpenSSL-Wini32\lib\VC
   
   Remember to enable SSL support in PennMUSH: (change "#undef HAVE_SSL" to 
   "#define HAVE_SSL" in config.h).
   
4. Run a rebuild. If all is well, you should have no linker errors and receive a 
   success message and a pennmush.exe file in the game folder.