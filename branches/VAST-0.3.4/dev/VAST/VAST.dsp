# Microsoft Developer Studio Project File - Name="VAST" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=VAST - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "VAST.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "VAST.mak" CFG="VAST - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "VAST - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "VAST - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "VAST - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\lib"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "VAST_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\include" /I "..\ACE_wrappers" /I "..\zlib\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "VAST_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x404 /d "NDEBUG"
# ADD RSC /l 0x404 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 ace.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib zdll.lib /nologo /dll /machine:I386 /libpath:"..\ACE_wrappers\lib.vc6" /libpath:"..\zlib\lib"
# SUBTRACT LINK32 /incremental:yes

!ELSEIF  "$(CFG)" == "VAST - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\lib"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "VAST_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\include" /I "..\ACE_wrappers" /I "..\zlib\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "VAST_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x404 /d "_DEBUG"
# ADD RSC /l 0x404 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 aced.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib zdll.lib /nologo /dll /debug /machine:I386 /out:"..\lib/VASTd.dll" /pdbtype:sept /libpath:"..\ACE_wrappers\lib.vc6" /libpath:"..\zlib\lib"

!ENDIF 

# Begin Target

# Name "VAST - Win32 Release"
# Name "VAST - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\net_ace.cpp
# End Source File
# Begin Source File

SOURCE=.\net_ace_handler.cpp
# End Source File
# Begin Source File

SOURCE=.\net_emu.cpp
# End Source File
# Begin Source File

SOURCE=.\net_emubridge.cpp
# End Source File
# Begin Source File

SOURCE=.\vast_dc.cpp
# End Source File
# Begin Source File

SOURCE=.\vast_fo.cpp
# End Source File
# Begin Source File

SOURCE=.\vastverse.cpp
# End Source File
# Begin Source File

SOURCE=.\vor_SF.cpp
# End Source File
# Begin Source File

SOURCE=.\vor_SF_algorithm.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\include\config.h
# End Source File
# Begin Source File

SOURCE=..\include\msghandler.h
# End Source File
# Begin Source File

SOURCE=.\net_ace.h
# End Source File
# Begin Source File

SOURCE=.\net_ace_acceptor.h
# End Source File
# Begin Source File

SOURCE=.\net_ace_handler.h
# End Source File
# Begin Source File

SOURCE=.\net_emu.h
# End Source File
# Begin Source File

SOURCE=.\net_emubridge.h
# End Source File
# Begin Source File

SOURCE=.\net_msg.h
# End Source File
# Begin Source File

SOURCE=..\include\network.h
# End Source File
# Begin Source File

SOURCE=..\include\typedef.h
# End Source File
# Begin Source File

SOURCE=..\include\vast.h
# End Source File
# Begin Source File

SOURCE=.\vast_dc.h
# End Source File
# Begin Source File

SOURCE=.\vast_fo.h
# End Source File
# Begin Source File

SOURCE=..\include\vastverse.h
# End Source File
# Begin Source File

SOURCE=.\vor_SF.h
# End Source File
# Begin Source File

SOURCE=.\vor_SF_algorithm.h
# End Source File
# Begin Source File

SOURCE=..\include\voronoi.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
