# Microsoft Developer Studio Project File - Name="kdu_text_extractor" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=kdu_text_extractor - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "kdu_text_extractor.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "kdu_text_extractor.mak" CFG="kdu_text_extractor - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "kdu_text_extractor - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "kdu_text_extractor - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "kdu_text_extractor - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\v6_generated\text_extractor\release"
# PROP Intermediate_Dir "..\..\..\v6_generated\text_extractor\release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\coresys\common" /I "..\args" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386 /out:"..\..\..\bin\kdu_text_extractor.exe"
# Begin Special Build Tool
TargetPath=\Software\Kakadu\bin\kdu_text_extractor.exe
SOURCE="$(InputPath)"
PostBuild_Cmds=cd ../../language	"$(TargetPath)" -s coresys.src -quiet	"$(TargetPath)" -s jp2.src -quiet	"$(TargetPath)" -s jpx.src -quiet	"$(TargetPath)" -s mj2.src -quiet	"$(TargetPath)" -s client.src -quiet	"$(TargetPath)" -s misc.src -quiet
# End Special Build Tool

!ELSEIF  "$(CFG)" == "kdu_text_extractor - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\v6_generated\text_extractor\debug"
# PROP Intermediate_Dir "..\..\..\v6_generated\text_extractor\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\coresys\common" /I "..\args" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"..\..\..\bin\kdu_text_extractor.exe" /pdbtype:sept
# Begin Special Build Tool
TargetPath=\Software\Kakadu\bin\kdu_text_extractor.exe
SOURCE="$(InputPath)"
PostBuild_Cmds=cd ../../language	"$(TargetPath)" -s coresys.src -quiet	"$(TargetPath)" -s jp2.src -quiet	"$(TargetPath)" -s jpx.src -quiet	"$(TargetPath)" -s mj2.src -quiet	"$(TargetPath)" -s client.src -quiet	"$(TargetPath)" -s misc.src -quiet
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "kdu_text_extractor - Win32 Release"
# Name "kdu_text_extractor - Win32 Debug"
# Begin Group "main"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\kdu_text_extractor.cpp
# End Source File
# Begin Source File

SOURCE=.\text_extractor_local.h
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
# Begin Group "args"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\args\args.cpp
# End Source File
# Begin Source File

SOURCE=..\args\kdu_args.h
# End Source File
# End Group
# End Target
# End Project
