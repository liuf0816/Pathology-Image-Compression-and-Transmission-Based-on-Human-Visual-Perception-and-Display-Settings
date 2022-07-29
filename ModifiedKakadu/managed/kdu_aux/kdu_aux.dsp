# Microsoft Developer Studio Project File - Name="kdu_aux" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=kdu_aux - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "kdu_aux.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "kdu_aux.mak" CFG="kdu_aux - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "kdu_aux - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "kdu_aux - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "kdu_aux - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\..\..\..\v6_generated\kdu_aux\release"
# PROP Intermediate_Dir ".\..\..\..\v6_generated\kdu_aux\release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "KDU_AUX_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\all_includes" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CORESYS_IMPORTS" /D "KDU_AUX_EXPORTS" /D "KDU_PENTIUM_MSVC" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "NDEBUG"
# ADD RSC /l 0xc09 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 ws2_32.lib ..\..\..\lib\kdu_v61R.lib /nologo /dll /machine:I386 /out:"..\..\..\bin\kdu_a60R.dll" /implib:"..\..\..\lib\kdu_a60R.lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "kdu_aux - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\..\..\..\v6_generated\kdu_aux\debug"
# PROP Intermediate_Dir ".\..\..\..\v6_generated\kdu_aux\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "KDU_AUX_EXPORTS" /YX /FD /GZ  /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\all_includes" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CORESYS_IMPORTS" /D "KDU_AUX_EXPORTS" /D "KDU_PENTIUM_MSVC" /FD /GZ  /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0xc09 /d "_DEBUG"
# ADD RSC /l 0xc09 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ws2_32.lib ..\..\..\lib\kdu_v61D.lib /nologo /dll /debug /machine:I386 /out:"..\..\..\bin\kdu_a60D.dll" /implib:"..\..\..\lib\kdu_a60D.lib" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "kdu_aux - Win32 Release"
# Name "kdu_aux - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\kdu_aux.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\kdu_aux.h
# End Source File
# End Group
# Begin Group "Aux Source Files"

# PROP Default_Filter ""
# Begin Group "jp2"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\apps\jp2\jp2.cpp
# End Source File
# Begin Source File

SOURCE=..\..\apps\jp2\jpx.cpp
# End Source File
# Begin Source File

SOURCE=..\..\apps\jp2\mj2.cpp
# End Source File
# End Group
# Begin Group "image"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\apps\image\kdu_tiff.cpp
# End Source File
# End Group
# Begin Group "support"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\apps\support\kdu_region_compositor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\apps\support\kdu_region_decompressor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\apps\support\kdu_stripe_compressor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\apps\support\kdu_stripe_decompressor.cpp
# End Source File
# End Group
# Begin Group "caching_sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\apps\caching_sources\client_server.cpp
# End Source File
# Begin Source File

SOURCE=..\..\apps\caching_sources\client_server_comms.cpp
# End Source File
# Begin Source File

SOURCE=..\..\apps\caching_sources\kdu_cache.cpp
# End Source File
# Begin Source File

SOURCE=..\..\apps\caching_sources\kdu_client.cpp
# End Source File
# Begin Source File

SOURCE=..\..\apps\caching_sources\kdu_clientx.cpp
# End Source File
# End Group
# Begin Group "server"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\apps\kdu_server\kdu_serve.cpp
# End Source File
# Begin Source File

SOURCE=..\..\apps\kdu_server\kdu_servex.cpp
# End Source File
# End Group
# End Group
# End Target
# End Project
