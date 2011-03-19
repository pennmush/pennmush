Building PennMUSH 1.8.x under Windows with MySQL
------------------------------------------------
by Ervin Hearn <ehearn@pennmush.org>

Last update: Saturday, 11 November 2006
Current Version of MySQL: 5.0.27

These directions are oriented toward those using MSVC++ or another Graphical
Windows IDE. Those using MinGW+MSys should install MySQL just as if they were
doing so on Linux.

1. Download and install MySQL for Windows from http://www.mysql.org/

2. Make sure that the contents of mysql/lib/opt and mysql/include are located
   within your build environment's path.

   I recommend copying the contents of the directories listed below into your
   compiler's lib and include directories under directories named 'mysql'. A
   representation of this is:

   C:\Program Files\Microsoft Visual Studio\VC98\lib\mysql -> C:\mysql\lib\opt
   C:\Program Files\Microsoft Visual Studio\VC98\include\mysql -> C:\mysql\include

3. Now set your active configuration to one of the Win32 MySQL profiles.
   The project should now compile and produce a binary which includes PennMUSH's
   MySQL functionality.

4. Place libmysql.dll in your C:\Windows\System32 directory and set the sql
   directives in your mush.cnf.

5. From the top-level pennmush directory, the binary is: game/pennmush.exe
