# Microsoft Developer Studio Project File - Name="kdu_show" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=kdu_show - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "kdu_show.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "kdu_show.mak" CFG="kdu_show - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "kdu_show - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "kdu_show - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "kdu_show - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\v6_generated\show\release"
# PROP Intermediate_Dir "..\..\..\v6_generated\show\release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\..\coresys\common" /I "..\support" /I "..\image" /I "..\compressed_io" /I "..\jp2" /D "_WINDOWS" /D "_AFXDLL" /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "CORESYS_IMPORTS" /D "KDU_PENTIUM_MSVC" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 ws2_32.lib ..\..\..\lib\kdu_v61R.lib /nologo /subsystem:windows /machine:I386 /out:"..\..\..\bin\kdu_show.exe"

!ELSEIF  "$(CFG)" == "kdu_show - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\v6_generated\show\debug"
# PROP Intermediate_Dir "..\..\..\v6_generated\show\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\..\coresys\common" /I "..\support" /I "..\image" /I "..\compressed_io" /I "..\jp2" /D "_WINDOWS" /D "_AFXDLL" /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "CORESYS_IMPORTS" /D "KDU_PENTIUM_MSVC" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ws2_32.lib ..\..\..\lib\kdu_v61D.lib /nologo /subsystem:windows /debug /machine:I386 /out:"..\..\..\bin\kdu_show.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "kdu_show - Win32 Release"
# Name "kdu_show - Win32 Debug"
# Begin Group "main"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ChildView.cpp
# End Source File
# Begin Source File

SOURCE=.\ChildView.h
# End Source File
# Begin Source File

SOURCE=.\kd_metadata_editor.cpp
# End Source File
# Begin Source File

SOURCE=.\kd_metadata_editor.h
# End Source File
# Begin Source File

SOURCE=.\kd_metashow.cpp
# End Source File
# Begin Source File

SOURCE=.\kd_metashow.h
# End Source File
# Begin Source File

SOURCE=.\kd_playcontrol.cpp
# End Source File
# Begin Source File

SOURCE=.\kd_playcontrol.h
# End Source File
# Begin Source File

SOURCE=.\kdu_show.cpp
# End Source File
# Begin Source File

SOURCE=.\kdu_show.h
# End Source File
# Begin Source File

SOURCE=.\MainFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\MainFrm.h
# End Source File
# End Group
# Begin Group "coresys"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\coresys\common\kdu_arch.h
# End Source File
# Begin Source File

SOURCE=..\..\coresys\common\kdu_block_coding.h
# End Source File
# Begin Source File

SOURCE=..\..\coresys\common\kdu_compressed.h
# End Source File
# Begin Source File

SOURCE=..\..\coresys\common\kdu_elementary.h
# End Source File
# Begin Source File

SOURCE=..\..\coresys\common\kdu_kernels.h
# End Source File
# Begin Source File

SOURCE=..\..\coresys\common\kdu_messaging.h
# End Source File
# Begin Source File

SOURCE=..\..\coresys\common\kdu_params.h
# End Source File
# Begin Source File

SOURCE=..\..\coresys\common\kdu_sample_processing.h
# End Source File
# Begin Source File

SOURCE=..\..\coresys\common\kdu_threads.h
# End Source File
# Begin Source File

SOURCE=..\..\coresys\common\kdu_utils.h
# End Source File
# End Group
# Begin Group "compressed_io"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\compressed_io\jp2.h
# End Source File
# Begin Source File

SOURCE=..\compressed_io\jpx.h
# End Source File
# Begin Source File

SOURCE=..\compressed_io\kdu_cache.h
# End Source File
# Begin Source File

SOURCE=..\compressed_io\kdu_client.h
# End Source File
# Begin Source File

SOURCE=..\compressed_io\kdu_clientx.h
# End Source File
# Begin Source File

SOURCE=..\compressed_io\kdu_file_io.h
# End Source File
# Begin Source File

SOURCE=..\compressed_io\kdu_video_io.h
# End Source File
# Begin Source File

SOURCE=..\compressed_io\mj2.h
# End Source File
# End Group
# Begin Group "jp2"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\jp2\jp2.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\jp2\jp2_local.h
# End Source File
# Begin Source File

SOURCE=..\jp2\jp2_shared.h
# End Source File
# Begin Source File

SOURCE=..\jp2\jpx.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\jp2\jpx_local.h
# End Source File
# Begin Source File

SOURCE=..\jp2\mj2.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\jp2\mj2_local.h
# End Source File
# End Group
# Begin Group "caching_sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\caching_sources\cache_local.h
# End Source File
# Begin Source File

SOURCE=..\caching_sources\client_local.h
# End Source File
# Begin Source File

SOURCE=..\caching_sources\clientx_local.h
# End Source File
# Begin Source File

SOURCE=..\caching_sources\kdu_cache.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\caching_sources\kdu_client.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\caching_sources\kdu_clientx.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# End Group
# Begin Group "mfc_elements"

# PROP Default_Filter ""
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\cursor1.cur
# End Source File
# Begin Source File

SOURCE=.\res\kdu_show.ico
# End Source File
# Begin Source File

SOURCE=.\res\kdu_show.rc2
# End Source File
# End Group
# Begin Source File

SOURCE="..\..\..\..\..\Program Files\Microsoft Visual Studio\VC98\Include\BASETSD.H"
# End Source File
# Begin Source File

SOURCE=.\kdu_show.rc
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\res\toolbar1.bmp
# End Source File
# Begin Source File

SOURCE=.\res\toolbar2.bmp
# End Source File
# End Group
# Begin Group "client_server_common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\caching_sources\client_server.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\caching_sources\client_server_comms.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\caching_sources\client_server_comms.h
# End Source File
# Begin Source File

SOURCE=..\compressed_io\kdu_client_window.h
# End Source File
# End Group
# Begin Group "support"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\support\kdu_region_compositor.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\support\kdu_region_compositor.h
# End Source File
# Begin Source File

SOURCE=..\support\kdu_region_decompressor.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\support\kdu_region_decompressor.h
# End Source File
# Begin Source File

SOURCE=..\support\msvc_region_compositor_local.h
# End Source File
# Begin Source File

SOURCE=..\support\msvc_region_decompressor_local.h
# End Source File
# Begin Source File

SOURCE=..\support\region_compositor_local.h
# End Source File
# Begin Source File

SOURCE=..\support\region_decompressor_local.h
# End Source File
# End Group
# End Target
# End Project
