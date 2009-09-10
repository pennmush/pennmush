# Microsoft Developer Studio Project File - Name="pennmush" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=pennmush - Win32 MySQL Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pennmush.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pennmush.mak" CFG="pennmush - Win32 MySQL Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pennmush - Win32 MySQL SSL Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "pennmush - Win32 MySQL SSL Release" (based on "Win32 (x86) Console Application")
!MESSAGE "pennmush - Win32 MySQL Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "pennmush - Win32 MySQL Release" (based on "Win32 (x86) Console Application")
!MESSAGE "pennmush - Win32 SSL Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "pennmush - Win32 SSL Release" (based on "Win32 (x86) Console Application")
!MESSAGE "pennmush - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "pennmush - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pennmush - Win32 MySQL SSL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "pennmush___Win32_MySQL_SSL_Debug"
# PROP BASE Intermediate_Dir "pennmush___Win32_MySQL_SSL_Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../../game"
# PROP Intermediate_Dir "../../obj"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /ZI /I "../../win32" /I "../../hdrs" /I "../../" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# SUBTRACT BASE CPP /Ox /Ot /Os
# ADD CPP /nologo /MD /W3 /GX /ZI /I "../../win32" /I "../../hdrs" /I "../../" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# SUBTRACT CPP /Ox /Ot /Os
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib user32.lib winmm.lib wsock32.lib ssleay32.lib libeay32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib user32.lib winmm.lib wsock32.lib ssleay32.lib libeay32.lib libmysql.lib /nologo /stack:0x1fec /subsystem:console /debug /machine:I386 /pdbtype:sept

!ELSEIF  "$(CFG)" == "pennmush - Win32 MySQL SSL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "pennmush___Win32_MySQL_SSL_Release"
# PROP BASE Intermediate_Dir "pennmush___Win32_MySQL_SSL_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "../../game"
# PROP Intermediate_Dir "../../obj"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /Ob0 /I "../../hdrs" /I "../../" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /I /Win32" " /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob0 /I "../../hdrs" /I "../../" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /I /Win32" " /c
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 user32.lib winmm.lib wsock32.lib ssleay32.lib libeay32.lib /nologo /subsystem:console /pdb:"pennmush.pdb" /machine:I386
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 user32.lib winmm.lib wsock32.lib ssleay32.lib libeay32.lib libmysql.lib /nologo /stack:0x1fec /subsystem:console /pdb:"pennmush.pdb" /machine:I386
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "pennmush - Win32 MySQL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "pennmush___Win32_MySQL_Debug"
# PROP BASE Intermediate_Dir "pennmush___Win32_MySQL_Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../../game"
# PROP Intermediate_Dir "../../obj"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /ZI /I "../../win32" /I "../../hdrs" /I "../../" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# SUBTRACT BASE CPP /Ox /Ot /Os
# ADD CPP /nologo /MD /W3 /GX /ZI /I "../../win32" /I "../../hdrs" /I "../../" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# SUBTRACT CPP /Ox /Ot /Os
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib user32.lib winmm.lib wsock32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib user32.lib winmm.lib wsock32.lib libmysql.lib /nologo /stack:0x1fec /subsystem:console /debug /machine:I386 /pdbtype:sept

!ELSEIF  "$(CFG)" == "pennmush - Win32 MySQL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "pennmush___Win32_MySQL_Release"
# PROP BASE Intermediate_Dir "pennmush___Win32_MySQL_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "../../game"
# PROP Intermediate_Dir "../../obj"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /Ob0 /I "../../hdrs" /I "../../" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /I /Win32" " /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob0 /I "../../hdrs" /I "../../" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /I /Win32" " /c
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 user32.lib winmm.lib wsock32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 user32.lib winmm.lib wsock32.lib libmysql.lib /nologo /stack:0x1fec /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "pennmush - Win32 SSL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "pennmush___Win32_SSL_Debug"
# PROP BASE Intermediate_Dir "pennmush___Win32_SSL_Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../../game"
# PROP Intermediate_Dir "../../obj"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /ZI /I "../../win32" /I "../../hdrs" /I "../../" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# SUBTRACT BASE CPP /Ox /Ot /Os
# ADD CPP /nologo /MD /W3 /GX /ZI /I "../../win32" /I "../../hdrs" /I "../../" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# SUBTRACT CPP /Ox /Ot /Os
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib user32.lib winmm.lib wsock32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib user32.lib winmm.lib wsock32.lib ssleay32.lib libeay32.lib /nologo /stack:0x1fec /subsystem:console /debug /machine:I386 /pdbtype:sept

!ELSEIF  "$(CFG)" == "pennmush - Win32 SSL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "pennmush___Win32_SSL_Release"
# PROP BASE Intermediate_Dir "pennmush___Win32_SSL_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "../../game"
# PROP Intermediate_Dir "../../obj"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /Ob0 /I "../../hdrs" /I "../../" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /I /Win32" " /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob0 /I "../../hdrs" /I "../../" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /I /Win32" " /c
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 user32.lib winmm.lib wsock32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 user32.lib winmm.lib wsock32.lib ssleay32.lib libeay32.lib /nologo /stack:0x1fec /subsystem:console /pdb:"pennmush.pdb" /machine:I386
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "pennmush - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../../game"
# PROP Intermediate_Dir "../../obj"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /ZI /I "../../win32" /I "../../hdrs" /I "../../" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# SUBTRACT CPP /Ox /Ot /Os
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib user32.lib winmm.lib wsock32.lib /nologo /stack:0x1fec /subsystem:console /debug /machine:I386 /pdbtype:sept

!ELSEIF  "$(CFG)" == "pennmush - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "../../game"
# PROP Intermediate_Dir "../../obj"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob0 /I "../../hdrs" /I "../../" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /I /Win32" " /c
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 user32.lib winmm.lib wsock32.lib /nologo /stack:0x1fec /subsystem:console /machine:I386

!ENDIF 

# Begin Target

# Name "pennmush - Win32 MySQL SSL Debug"
# Name "pennmush - Win32 MySQL SSL Release"
# Name "pennmush - Win32 MySQL Debug"
# Name "pennmush - Win32 MySQL Release"
# Name "pennmush - Win32 SSL Debug"
# Name "pennmush - Win32 SSL Release"
# Name "pennmush - Win32 Debug"
# Name "pennmush - Win32 Release"
# Begin Source File

SOURCE=..\..\src\access.c
# End Source File
# Begin Source File

SOURCE=..\..\src\atr_tab.c
# End Source File
# Begin Source File

SOURCE=..\..\src\attrib.c
# End Source File
# Begin Source File

SOURCE=..\..\src\boolexp.c
# End Source File
# Begin Source File

SOURCE=..\..\src\bsd.c
# End Source File
# Begin Source File

SOURCE=..\..\src\bufferq.c
# End Source File
# Begin Source File

SOURCE=..\..\src\chunk.c
# End Source File
# Begin Source File

SOURCE=..\..\src\cmdlocal.c
# End Source File
# Begin Source File

SOURCE=..\..\src\cmds.c
# End Source File
# Begin Source File

SOURCE=..\..\src\command.c
# End Source File
# Begin Source File

SOURCE=..\..\src\compress.c
# End Source File
# Begin Source File

SOURCE=..\..\src\conf.c
# End Source File
# Begin Source File

SOURCE=..\..\src\cque.c
# End Source File
# Begin Source File

SOURCE=..\..\src\create.c
# End Source File
# Begin Source File

SOURCE=..\..\src\db.c
# End Source File
# Begin Source File

SOURCE=..\..\src\destroy.c
# End Source File
# Begin Source File

SOURCE=..\..\src\extchat.c
# End Source File
# Begin Source File

SOURCE=..\..\src\extmail.c
# End Source File
# Begin Source File

SOURCE=..\..\src\filecopy.c
# End Source File
# Begin Source File

SOURCE=..\..\src\flaglocal.c
# End Source File
# Begin Source File

SOURCE=..\..\src\flags.c
# End Source File
# Begin Source File

SOURCE=..\..\src\funcrypt.c
# End Source File
# Begin Source File

SOURCE=..\..\src\function.c
# End Source File
# Begin Source File

SOURCE=..\..\src\fundb.c
# End Source File
# Begin Source File

SOURCE=..\..\src\funlist.c
# End Source File
# Begin Source File

SOURCE=..\..\src\funlocal.c
# End Source File
# Begin Source File

SOURCE=..\..\src\funmath.c
# End Source File
# Begin Source File

SOURCE=..\..\src\funmisc.c
# End Source File
# Begin Source File

SOURCE=..\..\src\funstr.c
# End Source File
# Begin Source File

SOURCE=..\..\src\funtime.c
# End Source File
# Begin Source File

SOURCE=..\..\src\funufun.c
# End Source File
# Begin Source File

SOURCE=..\..\src\game.c
# End Source File
# Begin Source File

SOURCE=..\..\src\help.c
# End Source File
# Begin Source File

SOURCE=..\..\src\htab.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ident.c
# End Source File
# Begin Source File

SOURCE=..\..\src\local.c
# End Source File
# Begin Source File

SOURCE=..\..\src\lock.c
# End Source File
# Begin Source File

SOURCE=..\..\src\log.c
# End Source File
# Begin Source File

SOURCE=..\..\src\look.c
# End Source File
# Begin Source File

SOURCE=..\..\src\malias.c
# End Source File
# Begin Source File

SOURCE=..\..\src\match.c
# End Source File
# Begin Source File

SOURCE=..\..\src\memcheck.c
# End Source File
# Begin Source File

SOURCE=..\..\src\move.c
# End Source File
# Begin Source File

SOURCE=..\..\src\mycrypt.c
# End Source File
# Begin Source File

SOURCE=..\..\src\mymalloc.c
# End Source File
# Begin Source File

SOURCE=..\..\src\myrlimit.c
# End Source File
# Begin Source File

SOURCE=..\..\src\mysocket.c
# End Source File
# Begin Source File

SOURCE=..\..\src\myssl.c
# End Source File
# Begin Source File

SOURCE=..\..\src\notify.c
# End Source File
# Begin Source File

SOURCE=..\..\src\parse.c
# End Source File
# Begin Source File

SOURCE=..\..\src\pcre.c
# End Source File
# Begin Source File

SOURCE=..\..\src\player.c
# End Source File
# Begin Source File

SOURCE=..\..\src\plyrlist.c
# End Source File
# Begin Source File

SOURCE=..\..\src\predicat.c
# End Source File
# Begin Source File

SOURCE=..\..\src\privtab.c
# End Source File
# Begin Source File

SOURCE=..\..\src\ptab.c
# End Source File
# Begin Source File

SOURCE=..\..\src\rob.c
# End Source File
# Begin Source File

SOURCE=..\..\src\services.c
# End Source File
# Begin Source File

SOURCE=..\..\src\set.c
# End Source File
# Begin Source File

SOURCE=..\..\src\SFMT.c
# End Source File
# Begin Source File

SOURCE=..\..\src\shs.c
# End Source File
# Begin Source File

SOURCE=..\..\src\sig.c
# End Source File
# Begin Source File

SOURCE=..\..\src\speech.c
# End Source File
# Begin Source File

SOURCE=..\..\src\sql.c
# End Source File
# Begin Source File

SOURCE=..\..\src\strdup.c
# End Source File
# Begin Source File

SOURCE=..\..\src\strtree.c
# End Source File
# Begin Source File

SOURCE=..\..\src\strutil.c
# End Source File
# Begin Source File

SOURCE=..\..\src\tables.c
# End Source File
# Begin Source File

SOURCE=..\..\src\timer.c
# End Source File
# Begin Source File

SOURCE=..\..\src\unparse.c
# End Source File
# Begin Source File

SOURCE=..\..\src\utils.c
# End Source File
# Begin Source File

SOURCE=..\..\src\version.c
# End Source File
# Begin Source File

SOURCE=..\..\src\warnings.c
# End Source File
# Begin Source File

SOURCE=..\..\src\wild.c
# End Source File
# Begin Source File

SOURCE=..\..\src\wiz.c
# End Source File
# End Target
# End Project
