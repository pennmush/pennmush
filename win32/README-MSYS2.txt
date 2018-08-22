PennMUSH for MSYS2 Instructions.

Minimum OS requirements: Windows 7 or Server 2008 or better.

Setting up environment and prereqs:

1. Install MSYS2 and update it per the instructions on the web site.
2. Open a MSYS2 MinGW 64-bit shell (From the MSYS2 64-bit folder in the
   start menu).
3. Install the following packages with pacman:
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-openssl perl make
3.a. Optionally also install the following packages:
   pacman -S cmake git gperf ed mingw-w64-x86_64-gdb mingw-w64-x86_64-icu
3.b Optionally instally a sql library if needed (Use the MinGW version,
     not the plain MSYS2 version).

   
Building Penn:   
1. Navigate to the PennMUSH folder (Or clone the repository with git).
2. Run configure:
   ./configure --disable-info_slave --disable-ssl_slave
3. Build the game:
   make
   make install
4. Run the game.
   cd game
   sh restart
   (Note that the mush currently runs in the foreground. Closing the terminal
    window will cause it to exit.)
