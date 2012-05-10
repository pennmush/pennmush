Building PennMUSH 1.8.x under Windows with MySQL
------------------------------------------------
by Ervin Hearn <ehearn@pennmush.org>
updated by Daniel Powell <daniko@M*U*S*H>

Last update: Friday, 14 October 2011
Current Version of MySQL: 5.5.16
MySQL Version referenced: 5.1.57
PennMUSH Version: 1.8.4p7

These directions are oriented toward those using MSVC++ or another Graphical
Windows IDE. Those using MinGW+MSys should install MySQL just as if they were
doing so on Linux.

1. Download and install MySQL for Windows from http://www.mysql.org/

   Be sure and check the "C Include Files/Lib Files" additional component 
   when installing the MySQL engine.
   
2. Make sure that the contents of mysql/lib/opt and mysql/include are located
   within your build environment's path.

3. Add libmysql.lib as an additional dependency within your environment's 
   linker input (file is stored in mysql/lib/opt).
   
4. Now set your active configuration to one of the Win32 MySQL profiles. 

5. Open PennMUSH/Win32/config.h and change #UNDEF HAVE_MYSQL to 
   #DEFINE HAVE_MYSQL. The project should now compile and produce a binary 
   which includes PennMUSH's MySQL functionality.

6. From the top-level pennmush directory, the binary is: game/pennmush.exe


The MySQL server currently installs to the following directory:

C:/Program Files/MySQL/

 MySQL Server 5.1 ----+-> bin
                      +-> include -----> mysql 
                      |
                      +-> lib -------+-> opt
                      |              |   
                      |              \-> plugin
                      |
                      \-> share -----+-> charsets
                                     +-> czech
                                     +-> danish
                                     +-> dutch
                                     +-> english
                                     ...
                                     \->ukranian
