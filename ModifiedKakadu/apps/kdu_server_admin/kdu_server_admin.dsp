# Microsoft Developer Studio Project File - Name="kdu_server_admin" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=kdu_server_admin - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "kdu_server_admin.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "kdu_server_admin.mak" CFG="kdu_server_admin - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "kdu_server_admin - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "kdu_server_admin - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "kdu_server_admin - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\v6_generated\server\release"
# PROP Intermediate_Dir "..\..\..\v6_generated\server\release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\coresys\common" /I "..\kdu_server" /I "..\args" /I "..\caching_sources" /D "NDEBUG" /D "_CONSOLE" /D "WIN32" /D "_MBCS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib /nologo /subsystem:console /machine:I386 /out:"..\..\..\bin\kdu_server_admin.exe"

!ELSEIF  "$(CFG)" == "kdu_server_admin - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\v6_generated\server\debug"
# PROP Intermediate_Dir "..\..\..\v6_generated\server\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\coresys\common" /I "..\kdu_server" /I "..\args" /I "..\caching_sources" /D "_DEBUG" /D "_CONSOLE" /D "WIN32" /D "_MBCS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib /nologo /subsystem:console /debug /machine:I386 /out:"..\..\..\bin\kdu_server_admin.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "kdu_server_admin - Win32 Release"
# Name "kdu_server_admin - Win32 Debug"
# Begin Group "main"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\kdu_server_admin.cpp
# End Source File
# End Group
# Begin Group "args"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\args\args.cpp
# End Source File
# Begin Source File

SOURCE=..\args\kdu_args.h
# End Source File
# End Group
# Begin Group "coresys"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\coresys\common\kdu_elementary.h
# End Source File
# Begin Source File

SOURCE=..\..\coresys\common\kdu_messaging.h
# End Source File
# Begin Source File

SOURCE=..\..\coresys\messaging\messaging.cpp
# End Source File
# End Group
# Begin Group "client_server_common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\caching_sources\client_server_comms.cpp
# End Source File
# Begin Source File

SOURCE=..\caching_sources\client_server_comms.h
# End Source File
# Begin Source File

SOURCE=..\kdu_server\kdu_security.cpp
# End Source File
# Begin Source File

SOURCE=..\kdu_server\kdu_security.h
# End Source File
# End Group
# End Target
# End Project
