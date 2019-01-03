
CC = cl.exe
LD = link.exe
RM = del /F


!ifdef X86
ARCH  = X86
!else
ARCH  = X64
!endif

CFLAGS  = /nologo /MT /Ox /W4 /sdl /fp:fast /EHsc /Zi /GF /GL
LDFLAGS = /nologo /incremental:no /machine:$(ARCH) /opt:ref /LTCG


OBJS = config.obj app.res app.obj window.obj util.obj \
	pty.obj screen.obj encoding.obj encoding_table.obj


.SUFFIXES: .cpp .obj

all: ck.exe ck.app.dll #ck.con.exe

ck.app.dll: cygwin1_app.lib interface.tlh $(OBJS)
	$(LD) /dll $(LDFLAGS) /out:ck.app.dll /pdb:ck.app.pdb $(OBJS)

ck.con.exe: main_con_gcc.c rsrc.rc
	windres -o main_con_gcc.res.o -D TARGET=1 -i rsrc.rc
	dlltool -d ck_app.def -l ck_app.a
	gcc -o ck.con.exe -pipe -O2 -Wall -s -static-libgcc main_con_gcc.c main_con_gcc.res.o ck_app.a -lkernel32 -lole32 -lshell32

ck.exe: main_launch.res main_launch.obj
	$(LD) $(LDFLAGS) /out:ck.exe /pdb:ck.pdb main_launch.obj main_launch.res

main_launch.res: rsrc.rc ck.ico
	RC.exe /nologo /l 0x409 /d TARGET=0 /fo $@ rsrc.rc
app.res: rsrc.rc ck.ico interface.tlb script_sys.js
	RC.exe /nologo /l 0x409 /d TARGET=2 /fo $@ rsrc.rc


cygwin1_app.lib: cygwin1_app.def
	lib.exe /nologo /machine:$(ARCH) /out:$@ /def:cygwin1_app.def
interface.tlh: interface.tlb
	$(CC) /nologo /c tlbimp.cpp
interface.tlb: interface.idl
	Midl.exe /nologo interface.idl
main_launch.obj: main_launch.cpp
	$(CC) $(CFLAGS) /Fdck.pdb /GS- /c main_launch.cpp
encoding_table.obj: encoding_table.cpp encoding.h
	$(CC) $(CFLAGS) /Fdck.app.pdb /c encoding_table.cpp
config.obj: config.cpp config.h
	$(CC) $(CFLAGS) /Fdck.app.pdb /Ycconfig.h /c config.cpp
.cpp.obj:
	$(CC) $(CFLAGS) /Fdck.app.pdb /Yuconfig.h /c $<


encoding.obj: encoding.cpp config.h interface.tlh encoding.h
screen.obj  : screen.cpp   config.h interface.tlh screen.h
pty.obj     : pty.cpp      config.h interface.tlh util.h encoding.h screen.h
utilobj     : util.cpp     config.h interface.tlh util.h
window.obj  : window.cpp   config.h interface.tlh util.h
app.obj     : app.cpp      config.h interface.tlh util.h


clean:
	$(RM)	ck.exe ck.con.exe ck.app.dll \
		$(OBJS) \
		config.pch tlbimp.obj interface.tlb \
		interface.tlh interface.h interface_i.c interface_p.c dlldata.c \
		main_launch.obj main_launch.res \
		main_con_gcc.res.o ck_app.a \
		ck.pdb ck.con.pdb ck.app.pdb \
		ck.con.exe.stackdump \
		ck.app.exp ck.app.lib ck.app.pdb \
		cygwin1_con.exp cygwin1_con.lib \
		cygwin1_app.exp cygwin1_app.lib \
		*~
