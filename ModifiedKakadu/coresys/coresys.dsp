# Microsoft Developer Studio Project File - Name="coresys" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=coresys - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "coresys.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "coresys.mak" CFG="coresys - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "coresys - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "coresys - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "coresys - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\v6_generated\coresys\release"
# PROP Intermediate_Dir "..\..\v6_generated\coresys\release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CORESYS_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "common" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CORESYS_EXPORTS" /D "KDU_PENTIUM_MSVC" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /out:"..\..\bin\kdu_v61R.dll" /implib:"..\..\lib\kdu_v61R.lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "coresys - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\v6_generated\coresys\debug"
# PROP Intermediate_Dir "..\..\v6_generated\coresys\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CORESYS_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "common" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "CORESYS_EXPORTS" /D "KDU_PENTIUM_MSVC" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"..\..\bin\kdu_v61D.dll" /implib:"..\..\lib\kdu_v61D.lib" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "coresys - Win32 Release"
# Name "coresys - Win32 Debug"
# Begin Group "common"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\common\kdu_arch.cpp
# End Source File
# Begin Source File

SOURCE=.\common\kdu_arch.h
# End Source File
# Begin Source File

SOURCE=.\common\kdu_block_coding.h
# End Source File
# Begin Source File

SOURCE=.\common\kdu_compressed.h
# End Source File
# Begin Source File

SOURCE=.\common\kdu_elementary.h
# End Source File
# Begin Source File

SOURCE=.\common\kdu_kernels.h
# End Source File
# Begin Source File

SOURCE=.\common\kdu_messaging.h
# End Source File
# Begin Source File

SOURCE=.\common\kdu_params.h
# End Source File
# Begin Source File

SOURCE=.\common\kdu_roi_processing.h
# End Source File
# Begin Source File

SOURCE=.\common\kdu_sample_processing.h
# End Source File
# Begin Source File

SOURCE=.\common\kdu_threads.h
# End Source File
# Begin Source File

SOURCE=.\common\kdu_utils.h
# End Source File
# End Group
# Begin Group "parameters"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\parameters\params.cpp
# End Source File
# Begin Source File

SOURCE=.\parameters\params_local.h
# End Source File
# End Group
# Begin Group "compressed"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\compressed\blocks.cpp
# End Source File
# Begin Source File

SOURCE=.\compressed\codestream.cpp
# End Source File
# Begin Source File

SOURCE=.\compressed\compressed.cpp
# End Source File
# Begin Source File

SOURCE=.\compressed\compressed_local.h
# End Source File
# End Group
# Begin Group "coding"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\coding\block_coding_common.cpp
# End Source File
# Begin Source File

SOURCE=.\coding\block_coding_common.h
# End Source File
# Begin Source File

SOURCE=.\coding\block_decoder.cpp
# End Source File
# Begin Source File

SOURCE=.\coding\block_encoder.cpp
# End Source File
# Begin Source File

SOURCE=.\coding\decoder.cpp
# End Source File
# Begin Source File

SOURCE=.\coding\encoder.cpp
# End Source File
# Begin Source File

SOURCE=.\coding\mq_decoder.cpp
# End Source File
# Begin Source File

SOURCE=.\coding\mq_decoder.h
# End Source File
# Begin Source File

SOURCE=.\coding\mq_encoder.cpp
# End Source File
# Begin Source File

SOURCE=.\coding\mq_encoder.h
# End Source File
# Begin Source File

SOURCE=.\coding\msvc_block_decode_asm.h
# End Source File
# Begin Source File

SOURCE=.\coding\msvc_coder_mmx_local.h
# End Source File
# End Group
# Begin Group "transform"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\transform\analysis.cpp
# End Source File
# Begin Source File

SOURCE=.\transform\analysis_local.h
# End Source File
# Begin Source File

SOURCE=.\transform\colour.cpp
# End Source File
# Begin Source File

SOURCE=.\transform\gcc_colour_altivec_local.h
# End Source File
# Begin Source File

SOURCE=.\transform\gcc_colour_mmx_local.h
# End Source File
# Begin Source File

SOURCE=.\transform\gcc_colour_sparcvis_local.h
# End Source File
# Begin Source File

SOURCE=.\transform\gcc_dwt_altivec_local.h
# End Source File
# Begin Source File

SOURCE=.\transform\gcc_dwt_mmx_local.h
# End Source File
# Begin Source File

SOURCE=.\transform\gcc_dwt_sparcvis_local.h
# End Source File
# Begin Source File

SOURCE=.\transform\msvc_colour_mmx_local.h
# End Source File
# Begin Source File

SOURCE=.\transform\msvc_dwt_mmx_local.h
# End Source File
# Begin Source File

SOURCE=.\transform\multi_transform.cpp
# End Source File
# Begin Source File

SOURCE=.\transform\multi_transform_local.h
# End Source File
# Begin Source File

SOURCE=.\transform\synthesis.cpp
# End Source File
# Begin Source File

SOURCE=.\transform\synthesis_local.h
# End Source File
# Begin Source File

SOURCE=.\transform\transform_local.h
# End Source File
# End Group
# Begin Group "kernels"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\kernels\kernels.cpp
# End Source File
# End Group
# Begin Group "messaging"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\messaging\messaging.cpp
# End Source File
# End Group
# Begin Group "roi"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\roi\roi.cpp
# End Source File
# Begin Source File

SOURCE=.\roi\roi_local.h
# End Source File
# End Group
# Begin Group "Resources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\coresys.rc
# End Source File
# End Group
# Begin Group "threads"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\threads\kdu_threads.cpp
# End Source File
# Begin Source File

SOURCE=.\threads\threads_local.h
# End Source File
# End Group
# End Target
# End Project
