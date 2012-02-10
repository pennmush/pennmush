Building PennMUSH 1.8.x under windows with OpenSSL
--------------------------------------------------
by Daniel Powell <daniko@M*U*S*H>

Last update: February 07, 2012
Current Version of OpenSSL: 1.0.0g
PennMUSH Version: 1.8.4p9

This file is directed toward those compiling PennMUSH with Visual Studio 2005, 2008
or 2010. README.SSL (located in the PennMUSH root directory) should be read before 
continuing with these instructions.

1. The OpenSSL homepage is located at http://www.openssl.org. However, as a Windows 
   user, it will relay you to http://www.slproweb.com/products/Win32OpenSSL.html.
   
   At the bottom of this page you will notice a few downloads such as Win32 OpenSSL
   Light, Full, 64-bit, and a few different versions. Also offered are links to the
   Visual C++ 2008 Redistributables. 
   
   The "light" version of OpenSSL does NOT include headers or libraries needed to
   compile programs with it enabled. Thus, you will need the non-"light" version
   currently labelled "Win32 OpenSSL v1.0.0g".
   
2. Install Win32OpenSSL-1_0_0g.exe. It is recommended that you select the option
   to copy the executables to the System folder. This will ensure that all
   applications that use SSL, such as Apache or any other servers, will also be
   using the same libraries. 
   
   Besides that, PennMUSH will automatically look in System32 first for these 
   libraries before checking the OpenSSL directory, and if Apache has copied older 
   versions of the required dll's into the system directory, Penn will use those 
   rather than the version you just installed.
   
3. Set up Visual Studio project with the include and lib directories (default is
   C:\OpenSSL-Win32\include and C:\OpenSSL-Win32\lib). In conjunction with README.SSL
   in the PennMUSH root directory, Noltar states that "it requires compiling both 
   OpenSSL and PennMUSH in /MD (multithread dll) mode." Therefore, you will also need
   to include "C:\OpenSSL-Win32\lib\VC" in your project's "Library Directories"
   setting.
   
   You will also need to navigate to the Linker/Input project property and add 
   both libeay32MD.lib and ssleay32MD.lib to the "Additional Dependencies" list. 
   These two libraries are located in C:\OpenSSL-Wini32\lib\VC
   
   If, for some reason, you get uncaught exceptions when trying to start PennMUSH up,
   and you believe it to be related to the integration of OpenSSL, you can change 
   your lib directory in Visual Studio to C:\OpenSSL-Win32\lib\VC\static in order 
   to use the static libraries for debugging purposes.
   
4. Run a rebuild. If all is well, you should have no linker errors and receive a 
   success message and a pennmush.exe file in the game folder.
   
Note: PennMUSH 1.8.4p8 r1333 and up uses certain functions which require OpenSSL 1.0.0g
and up. Subsequently, SSL is no longer an optional library in 1.8.4p9, but is now a 
required and will need to be worked into the compile. This, however, does not mean you
will have to integrate secure sockets into your server, but it does mean that the server
will be compiled with this capability if you wish to.
