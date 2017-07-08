# Microsoft Developer Studio Generated NMAKE File, Based on spt.dsp
!IF "$(CFG)" == ""
CFG=spt - Win32 Debug
!MESSAGE No configuration specified. Defaulting to spt - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "spt - Win32 Release" && "$(CFG)" != "spt - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "spt.mak" CFG="spt - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "spt - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "spt - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "spt - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\spt.exe"


CLEAN :
	-@erase "$(INTDIR)\libscsi.obj"
	-@erase "$(INTDIR)\memory_alloc.obj"
	-@erase "$(INTDIR)\scsidata.obj"
	-@erase "$(INTDIR)\scsilib.obj"
	-@erase "$(INTDIR)\spt.obj"
	-@erase "$(INTDIR)\utilities.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\spt.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\spt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\spt.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\spt.pdb" /machine:I386 /out:"$(OUTDIR)\spt.exe" 
LINK32_OBJS= \
	"$(INTDIR)\libscsi.obj" \
	"$(INTDIR)\memory_alloc.obj" \
	"$(INTDIR)\scsidata.obj" \
	"$(INTDIR)\scsilib.obj" \
	"$(INTDIR)\spt.obj" \
	"$(INTDIR)\utilities.obj"

"$(OUTDIR)\spt.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "spt - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\spt.exe"


CLEAN :
	-@erase "$(INTDIR)\libscsi.obj"
	-@erase "$(INTDIR)\memory_alloc.obj"
	-@erase "$(INTDIR)\scsidata.obj"
	-@erase "$(INTDIR)\scsilib.obj"
	-@erase "$(INTDIR)\spt.obj"
	-@erase "$(INTDIR)\utilities.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\spt.exe"
	-@erase "$(OUTDIR)\spt.ilk"
	-@erase "$(OUTDIR)\spt.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\spt.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\spt.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\spt.pdb" /debug /machine:I386 /out:"$(OUTDIR)\spt.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\libscsi.obj" \
	"$(INTDIR)\memory_alloc.obj" \
	"$(INTDIR)\scsidata.obj" \
	"$(INTDIR)\scsilib.obj" \
	"$(INTDIR)\spt.obj" \
	"$(INTDIR)\utilities.obj"

"$(OUTDIR)\spt.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("spt.dep")
!INCLUDE "spt.dep"
!ELSE 
!MESSAGE Warning: cannot find "spt.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "spt - Win32 Release" || "$(CFG)" == "spt - Win32 Debug"
SOURCE=.\libscsi.c

"$(INTDIR)\libscsi.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\memory_alloc.c

"$(INTDIR)\memory_alloc.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\scsidata.c

"$(INTDIR)\scsidata.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\scsilib.c

"$(INTDIR)\scsilib.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\spt.c

"$(INTDIR)\spt.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\utilities.c

"$(INTDIR)\utilities.obj" : $(SOURCE) "$(INTDIR)"



!ENDIF 

