#/****************************************************************************
# *                                                                          *
# *                       COPYRIGHT (c) 2006 - 2020                          *
# *                        This Software Provided                            *
# *                                  By                                      *
# *                       Robin's Nest Software Inc.                         *
# *                                                                          *
# * Permission to use, copy, modify, distribute and sell this software and   *
# * its documentation for any purpose and without fee is hereby granted      *
# * provided that the above copyright notice appear in all copies and that   *
# * both that copyright notice and this permission notice appear in the      *
# * supporting documentation, and that the name of the author not be used    *
# * in advertising or publicity pertaining to distribution of the software   *
# * without specific, written prior permission.                              *
# *                                                                          *
# * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,        *
# * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN      *
# * NO EVENT SHALL HE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL   *
# * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR    *
# * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS  *
# * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF   *
# * THIS SOFTWARE.                                                           *
# *                                                                          *
# ****************************************************************************/
#
# Makefile -- Makefile for building SCSI Tools.
#	

### MKMF:DEFINITIONS ###


# System makefile definitions for program makefiles

.SUFFIXES:	.ln

.c.ln:
#		@lint -i $(LINTFLAGS) $<
		@lint -c $(LINTFLAGS) $<

#.c~.ln:
#		@echo $<
#		@$(GET) -G$*.c $(GFLAGS) $<
#		@lint -i $(LINTFLAGS) $*.c
#		@lint -c $(LINTFLAGS) $*.c
#		@rm -f $*.c

# Libraries the program links to which are considered volatile

LIBS=

# Libraries considered static

EXTLIBS= -lpthread

LINTLIBS=

# P or G flag ( override on command line by invoking make PORG=-g )

VPATH	= ..
PORG	= -O
#PORG	= -g

AWK=	awk

# Note: /usr/bin/cc should link to 'gcc' to use the local GNU compiler.
#	If this is *not* true, then update makespt to define CC=gcc
#CC=	gcc
# Note: Solaris needs "-D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT"
# Also Note: Linux does NOT define O_DIRECT without -D_GNU_SOURCE! (WTF!)
CFLAGS= $(PORG) -I.. -D_ALL_SOURCE -D_GNU_SOURCE -D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT -DNimble
CPP=	/usr/local/bin/cpp
CPPOPTS= -M ${CFLAGS}
LDFLAGS= 
LINTFLAGS=

# end of system makefile definitions

HDRS=	spt.h spt_devices.h spt_print.h spt_ses.h include.h inquiry.h libscsi.h scsilib.h \
	scsi_cdbs.h scsi_diag.h scsi_log.h scsi_opcodes.h scsi_ses.h spt_version.h parson.h

### MKMF:SOURCES ###

CFILES=

SPT_CFILES=	spt.c		\
		spt_fmt.c	\
		spt_inquiry.c	\
		spt_iot.c	\
		spt_log.c	\
		spt_mem.c	\
		spt_print.c	\
		spt_scsi.c	\
		spt_ses.c	\
		spt_show.c	\
		spt_unix.c	\
		spt_usage.c	\
		scsi_opcodes.c

#
# Common C Files:
#
COMMON_CFILES=	libscsi.c	\
		scsidata.c	\
		scsilib.c	\
		utilities.c	\
		spt_mtrand64.c	\
		parson.c

ALL_CFILES=	${CFILES} ${SPT_CFILES} ${COMMON_CFILES}

### MKMF:OBJECTS ###

OBJS=		${CFILES:.c=.o}
COMMON_OBJS=	${COMMON_CFILES:.c=.o}

SPT_OBJS=	${SPT_CFILES:.c=.o}

### MKMF:TARGETS ###

PROGRAMS=	spt
#PROGRAMS=	devid spt mon_vdisk

# system targets for program makefile

all:	$(PROGRAMS)
	@echo done!

devid:	devid.o $(COMMON_OBJS) $(XOBJS)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $@.o $(COMMON_OBJS) $(LIBS) $(EXTLIBS)

mon_vdisk:	mon_vdisk.o $(COMMON_OBJS) $(XOBJS)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $@.o $(COMMON_OBJS) $(LIBS) $(EXTLIBS)

spt:	$(SPT_OBJS) $(COMMON_OBJS) $(XOBJS)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(SPT_OBJS) $(COMMON_OBJS) $(LIBS) $(EXTLIBS)

scsilib.c:
	@if [ "$(OS)" = "" ] ; then \
           echo "Please specify OS={aix,linux,hpux,solaris,windows}"; \
           exit 1; \
        fi; \
        ln -sf ../scsilib-$(OS).c scsilib.c
		
lint:	$(LINTOBJS)
	lint $(LINTFLAGS) $(LINTOBJS) $(LINTLIBS)
	touch lint

clean:;
	@rm -f $(OBJS) $(COMMON_OBJS) $(SPT_OBJS) $(PROGRAMS) scsilib.c

tags:	$(CFILES) $(HDRS) $(COMMON_CFILES)
	ctags -wt $(CFILES) $(HDRS)

# end of system targets for program makefile


depend: makedep
	echo '/^# DO NOT DELETE THIS LINE/+1,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	cp Makefile Makefile.bak
	ex - Makefile < eddep
	rm eddep makedep makedep1 makedeperrs

# Kinda Micky Mouse, but it's better than nothing (IMHO) :-)
makedep: ${CFILES}
	@cat /dev/null >makedep
	-(for i in ${ALL_CFILES} ; do \
		${CPP} ${CPPOPTS} $$i >> makedep; done) \
		2>makedeperrs
	sed \
		-e 's,\/local\/lib\/gcc-lib/\(.*\)\/include,\/include,' \
		-e 's,^\(.*\)\.o:,\1.o \1.ln:,' makedep > makedep1
	${AWK} ' { if ($$1 != prev) { print rec; rec = $$0; prev = $$1; } \
		else { if (length(rec $$3) > 78) { print rec; rec = $$0; } \
		       else rec = rec " " $$3 } } \
	      END { print rec } ' makedep1 > makedep
	@cat makedeperrs
	@(if [ -s makedeperrs ]; then false; fi)


# Note: Hand edited, don't want OS specific from "make depend"!
# DO NOT DELETE THIS LINE

devid.o devid.ln: devid.c \
 include.h libscsi.h scsilib.h inquiry.h
mon_vdisk.o mon_vdisk.ln: mon_vdisk.c \
 include.h libscsi.h scsilib.h netapp_vdisk.h
parson.o parson.ln: parson.c parson.h
spt.o spt.ln: spt.c $(HDRS) spt_version.h
spt_fmt.o spt_fmt.ln: spt_fmt.c $(HDRS)
spt_inquiry.o spt_inquiry.ln: spt_inquiry.c $(HDRS)
spt_iot.o spt_iot.ln: spt_iot.c $(HDRS)
spt_log.o spt_log.ln: spt_log.c $(HDRS)
spt_mtrand64.o spt_mtrand64.ln: spt_mtrand64.c spt_mtrand64.h
spt_print.o spt_print.ln: spt_print.c $(HDRS)
spt_scsi.o spt_scsi.ln: spt_scsi.c $(HDRS)
spt_ses.o spt_ses.ln: spt_ses.c $(HDRS)
spt_show.o spt_show.ln: spt_show.c $(HDRS)
spt_usage.o spt_usage.ln: spt_usage.c \
 include.h libscsi.h scsilib.h spt.h scsi_opcodes.h spt_version.h
libscsi.o libscsi.ln: libscsi.c $(HDRS)
scsidata.o scsidata.ln: scsidata.c $(HDRS)
scsilib.o scsilib.ln: scsilib.c $(HDRS)
scsi_opcodes.o scsi_opcodes.ln: scsi_opcodes.c $(HDRS) 
spt_mem.o spt_mem.ln: spt_mem.c $(HDRS)
spt_unix.o spt_unix.ln: spt_unix.c $(HDRS)
utilities.o utilities.ln: utilities.c $(HDRS)
