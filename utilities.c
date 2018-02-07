/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2018			    *
 *			   This Software Provided			    *
 *				     By					    *
 *			  Robin's Nest Software Inc.			    *
 *									    *
 * Permission to use, copy, modify, distribute and sell this software and   *
 * its documentation for any purpose and without fee is hereby granted,	    *
 * provided that the above copyright notice appear in all copies and that   *
 * both that copyright notice and this permission notice appear in the	    *
 * supporting documentation, and that the name of the author not be used    *
 * in advertising or publicity pertaining to distribution of the software   *
 * without specific, written prior permission.				    *
 *									    *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 	    *
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN	    *
 * NO EVENT SHALL HE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL   *
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR    *
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS  *
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF   *
 * THIS SOFTWARE.							    *
 *									    *
 ****************************************************************************/
/*
 * Module:	utilities.c
 * Author:	Robin T. Miller
 * Date:	September 27th, 2006
 *
 * Description:
 *      Common utility functions.
 * 
 * Modification History:
 * 
 * June 15th, 2014 by Robin T. Miller
 * 	When reporting CDB information, display the CDB, dir, and length.
 * 
 *  For Windows, convert large numbers via _strtoui64() to avoid overflowing
 *  32-bits, which basically returns 0xffffffff (or 4294967295) when too big!
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#if !defined(_WIN32)
#  include <unistd.h>
#endif /* defined(_WIN32) */

#include "spt.h"

char *expected_str =    	"Expected";
char *received_str =	        "Received";

static char *compare_error_str =	"Data compare error at byte";

int
DoSystemCommand(scsi_device_t *sdp, char *cmdline)
{
    if ( (cmdline == NULL) || (strlen(cmdline) == 0) ) return(WARNING);
    return( system(cmdline) );
}

int
StartupShell(scsi_device_t *sdp, char *shell)
{
    static char *Shell;
#if defined(WIN32)
    Shell = "cmd.exe";
    /* The above should be in the users' path! */
    //Shell = "c:\\windows\\system32\\cmd.exe";
#endif /* !defined(WIN32) */

    if (shell) Shell = shell;
#if !defined(WIN32)
    if (Shell == NULL) {
	if ( (Shell = getenv("SHELL")) == NULL) {
	    if (access("/bin/ksh", X_OK) == SUCCESS) {
		Shell = "/bin/ksh";	/* My preference... */
	    } else {
		Shell = "/bin/sh";	/* System shell. */
	    }
	}
    }
#endif /* !defined(WIN32) */
    /*
     * Startup the shell.
     */
    return( system(Shell) );
}

/*
 * CvtStrtoValue() - Converts ASCII String into Numeric Value.
 *
 * Inputs:
 * 	nstr = String to convert.
 *	eptr = Pointer for terminating character pointer.
 *	base = The base used for the conversion.
 *
 * Outputs:
 *	eptr = Points to terminating character or nstr if an
 *		invalid if numeric value cannot be formed.
 *
 * Return Value:
 *	Returns converted number or -1 for FAILURE.
 */
uint32_t
CvtStrtoValue(char *nstr, char **eptr, int base)
{
    uint32_t n = 0, val;

    if ( (n = strtoul (nstr, eptr, base)) == 0L) {
        if (nstr == *eptr) {
            n++;
        }
    }
#ifdef notdef
    if (nstr == *eptr) {
        return(n);
    }
#endif /* notdef */
    nstr = *eptr;
    for (;;) {

        switch (*nstr++) {
            
            case 'k':
            case 'K':           /* Kilobytes */
                n *= KBYTE_SIZE;
                continue;

            case 'g':
            case 'G':           /* Gigibytes */
                n *= GBYTE_SIZE;
                continue;

            case 'm':
            case 'M':           /* Megabytes */
                n *= MBYTE_SIZE;
                continue;

#if defined(QuadIsLong) && !defined(_WIN64)
            case 't':
            case 'T':
                n *= TBYTE_SIZE;
                continue;
#endif /* defined(QuadIsLong) && !defined(_WIN64) */

            case 'w':
            case 'W':           /* Word count. */
                n *= sizeof(int);
                continue;

            case 'q':
            case 'Q':           /* Quadword count. */
                n *= sizeof(uint64_t);
                continue;

            case 'b':
            case 'B':           /* Block count. */
                n *= BLOCK_SIZE;
                continue;

            case 'i':
            case 'I':
                if ( ( ( nstr[0] == 'N' ) || ( nstr[0] == 'n' ) ) &&
                     ( ( nstr[1] == 'F' ) || ( nstr[1] == 'f' ) )) {
                    nstr += 2;
                    n = (uint32_t)MY_INFINITY;
                    continue;
                } else {
                    goto error;
                }

            case '+':
                n += CvtStrtoValue (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '-':
                n -= CvtStrtoValue (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '*':
            case 'x':
            case 'X':
                n *= CvtStrtoValue (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '/':
                val = CvtStrtoValue (nstr, eptr, base);
                if (val) n /= val;
                nstr = *eptr;
                continue;

            case '%':
                val = CvtStrtoValue (nstr, eptr, base);
                if (val) n %= val;
                nstr = *eptr;
                continue;

            case '~':
                n = ~CvtStrtoValue (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '|':
                n |= CvtStrtoValue (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '&':
                n &= CvtStrtoValue (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '^':
                n ^= CvtStrtoValue (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '<':
                if (*nstr++ != '<') goto error;
                n <<= CvtStrtoValue (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '>':
                if (*nstr++ != '>') goto error;
                n >>= CvtStrtoValue (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case ' ':
            case '\t':
                continue;

            case '\0':
                *eptr = --nstr;
                break;

            default:
error:
                n = 0L;
                *eptr = --nstr;
                break;
        }
        return(n);
    }
}

/*
 * CvtStrtoLarge() - Converts ASCII String into Large Value.
 *
 * Inputs:
 *	nstr = String to convert.
 *	eptr = Pointer for terminating character pointer.
 *	base = The base used for the conversion.
 *
 * Outputs:
 *	eptr = Points to terminating character or nstr if an
 *		invalid if numeric value cannot be formed.
 *
 * Return Value:
 *	Returns converted number or -1 for FAILURE.
 */
uint64_t
CvtStrtoLarge(char *nstr, char **eptr, int base)
{
    uint64_t n = 0, val;

#if defined(QuadIsLong)
    if ( (n = strtoul (nstr, eptr, base)) == (uint64_t) 0) {
#elif defined(QuadIsLongLong)
# if defined(__sun) || defined(_AIX) || defined(__hpux)
    if ( (n = strtoull (nstr, eptr, base)) == (uint64_t) 0) {
# elif defined(_WIN32)
    if ( (n = _strtoui64 (nstr, eptr, base)) == (uint64_t) 0) {
# else /* !defined(WIN32) */
    if ( (n = strtouq (nstr, eptr, base)) == (uint64_t) 0) {
# endif /* if defined(__sun) || defined(_AIX) */
#else /* !defined(QuadIsLong) && !defined(QuadIsLongLong) */
# error Need to define Quad definition!
        if ( (n = strtoul (nstr, eptr, base)) == (uint64_t) 0) {
#endif /* defined(QuadIsLong) || defined(__hpux) */
        if (nstr == *eptr) {
            n++;
        }
    }
#ifdef notdef
    if (nstr == *eptr) {
        return(n);
    }
#endif /* notdef */
    nstr = *eptr;
    for (;;) {

        switch (*nstr++) {
            
            case 'k':
            case 'K':           /* Kilobytes */
                n *= KBYTE_SIZE;
                continue;

            case 'g':
            case 'G':           /* Gigibytes */
                n *= GBYTE_SIZE;
                continue;

            case 'm':
            case 'M':           /* Megabytes */
                n *= MBYTE_SIZE;
                continue;

            case 't':
            case 'T':
                n *= TBYTE_SIZE;
                continue;

            case 'w':
            case 'W':           /* Word count. */
                n *= sizeof(int);
                continue;

            case 'q':
            case 'Q':           /* Quadword count. */
                n *= sizeof(uint64_t);
                continue;

            case 'b':
            case 'B':           /* Block count. */
                n *= BLOCK_SIZE;
                continue;

            case 'i':
            case 'I':
                if ( ( ( nstr[0] == 'N' ) || ( nstr[0] == 'n' ) ) &&
                     ( ( nstr[1] == 'F' ) || ( nstr[1] == 'f' ) )) {
                    nstr += 2;
                    n = MY_INFINITY;
                    continue;
                } else {
                    goto error;
                }

            case '+':
                n += CvtStrtoLarge (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '-':
                n -= CvtStrtoLarge (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '*':
            case 'x':
            case 'X':
                n *= CvtStrtoLarge (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '/':
                val = CvtStrtoLarge (nstr, eptr, base);
                if (val) n /= val;
                nstr = *eptr;
                continue;

            case '%':
                val = CvtStrtoLarge (nstr, eptr, base);
                if (val) n %= val;
                nstr = *eptr;
                continue;

            case '~':
                n = ~CvtStrtoLarge (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '|':
                n |= CvtStrtoLarge (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '&':
                n &= CvtStrtoLarge (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '^':
                n ^= CvtStrtoLarge (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '<':
                if (*nstr++ != '<') goto error;
                n <<= CvtStrtoLarge (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case '>':
                if (*nstr++ != '>') goto error;
                n >>= CvtStrtoLarge (nstr, eptr, base);
                nstr = *eptr;
                continue;

            case ' ':
            case '\t':
                continue;

            case '\0':
                *eptr = --nstr;
                break;

            default:
error:
                n = 0;
                *eptr = --nstr;
                break;
        }
        return(n);
    }
}

/*
 * CvtTimetoValue() - Converts ASCII Time String to Numeric Value.
 *
 * Inputs:
 *	nstr = String to convert.
 *	eptr = Pointer for terminating character pointer.
 *
 * Outputs:
 *	eptr = Points to terminating character or nstr if an
 *		invalid if numeric value cannot be formed.
 *
 * Return Value:
 *	Returns converted number in seconds or -1 for FAILURE.
 */
time_t
CvtTimetoValue (char *nstr, char **eptr)
{
    time_t n = 0;
    int base = ANY_RADIX;

    if ( (n = strtoul (nstr, eptr, base)) == 0L) {
	if (nstr == *eptr) {
	    n++;
	}
    }
#ifdef notdef
    if (nstr == *eptr) {
	return(n);
    }
#endif /* notdef */
    nstr = *eptr;
    for (;;) {

	switch (*nstr++) {
	    
	    case 'd':
	    case 'D':		/* Days */
		n *= SECS_PER_DAY;
		continue;

	    case 'h':
	    case 'H':		/* Hours */
		n *= SECS_PER_HOUR;
		continue;

	    case 'm':
	    case 'M':		/* Minutes */
		n *= SECS_PER_MIN;
		continue;

	    case 's':
	    case 'S':		/* Seconds */
		continue;	/* default */

	    case '+':
		n += CvtTimetoValue (nstr, eptr);
		nstr = *eptr;
		continue;

	    case '-':
		n -= CvtTimetoValue (nstr, eptr);
		nstr = *eptr;
		continue;

	    case '*':
	    case 'x':
	    case 'X':
		n *= CvtTimetoValue (nstr, eptr);
		nstr = *eptr;
		continue;

	    case '/':
		n /= CvtTimetoValue (nstr, eptr);
		nstr = *eptr;
		continue;

	    case '%':
		n %= CvtTimetoValue (nstr, eptr);
		nstr = *eptr;
		continue;

	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
		nstr--;
		n += CvtTimetoValue (nstr, eptr);
		nstr = *eptr;
		continue;

	    case ' ':
	    case '\t':
		continue;

	    case '\0':
		*eptr = --nstr;
		break;
	    default:
		n = 0L;
		*eptr = --nstr;
		break;
	}
	return(n);
    }
}

/*
 * CvtTimetoMsValue() - Converts ASCII Time String to Numeric Value.
 *
 * Inputs:
 *	nstr = String to convert.
 *	eptr = Pointer for terminating character pointer.
 *
 * Outputs:
 *	eptr = Points to terminating character or nstr if an
 *		invalid if numeric value cannot be formed.
 *
 * Return Value:
 *	Returns converted number in microseconds or -1 for FAILURE.
 */
time_t
CvtTimetoMsValue(char *nstr, char **eptr)
{
    time_t n = 0;
    int base = ANY_RADIX;

    if ( (n = strtoul (nstr, eptr, base)) == 0L) {
        if (nstr == *eptr) {
            n++;
        }
    }
#ifdef notdef
    if (nstr == *eptr) {
        return(n);
    }
#endif /* notdef */
    nstr = *eptr;
    for (;;) {

        switch (*nstr++) {
            
            case 'd':
            case 'D':           /* Days */
                n *= MSECS_PER_DAY;
                continue;

            case 'h':
            case 'H':           /* Hours */
                n *= MSECS_PER_HOUR;
                continue;

            case 'm':
            case 'M':           /* Minutes */
                n *= MSECS_PER_MIN;
                continue;

            case 's':
            case 'S':           /* Seconds */
                n *= MSECS_PER_SEC;
                continue;

            case '+':
                n += CvtTimetoValue (nstr, eptr);
                nstr = *eptr;
                continue;

            case '-':
                n -= CvtTimetoValue (nstr, eptr);
                nstr = *eptr;
                continue;

            case '*':
            case 'x':
            case 'X':
                n *= CvtTimetoValue (nstr, eptr);
                nstr = *eptr;
                continue;

            case '/':
                n /= CvtTimetoValue (nstr, eptr);
                nstr = *eptr;
                continue;

            case '%':
                n %= CvtTimetoValue (nstr, eptr);
                nstr = *eptr;
                continue;

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                nstr--;
                n += CvtTimetoValue (nstr, eptr);
                nstr = *eptr;
                continue;

            case ' ':
            case '\t':
                continue;

            case '\0':
                *eptr = --nstr;
                break;
            default:
                n = 0L;
                *eptr = --nstr;
                break;
        }
        return(n);
    }
}

/*
 * DumpBuffer() - Dump data buffer in hex bytes.
 *
 * Inputs:
 *	name = The buffer name being dumped.
 *	ptr  = Pointer into buffer being dumped.
 *	bufr_size = The maximum size of this buffer.
 *	dump_limit = The limit of data to dump.
 *
 * Return Value:
 *	Void.
 */
void
DumpBuffer(	scsi_device_t	*sdp,
                char		*name,
		uint8_t		*ptr,
		size_t		bufr_size,
                size_t          dump_limit )
{
    size_t i, limit;
    uint32_t field_width = 16;
    uint8_t *base = ptr;
    uint8_t *bptr = base;

    /*
     * Limit data dumped (if desired).
     */
    limit = (bufr_size < dump_limit) ? bufr_size : dump_limit;

    Fprintf(sdp, "Dumping %s Buffer (base = %#lx, limit = %u bytes):\n",
						name, base, limit);
    Fprintf(sdp, "\n");

    for (i = 0; i < limit; i++, bptr++) {
	if ((i % field_width) == (size_t) 0) {
	    if (i) Fprint(sdp, "\n");
	    Fprint(sdp, "%p ", bptr);
	}
	Fprint(sdp, " %02x", *bptr);
    }
    if (i) Fprint(sdp, "\n");
    (void)fflush(stderr);
}

/*
 * InitBuffer() - Initialize Buffer with a Data Pattern.
 *
 * Inputs:
 *      buffer = Pointer to buffer to init.
 *      count = Number of bytes to initialize.
 *      pattern = Data pattern to init buffer with.
 *
 * Return Value:
 *	Void.
 */
void
InitBuffer( unsigned char   *buffer,
            size_t	    count,
            uint32_t	    pattern )
{
    register unsigned char *bptr;
    union {
        unsigned char pat[sizeof(uint32_t)];
        uint32_t pattern;
    } p;
    register size_t i;

    /*
     * Initialize the buffer with a data pattern.
     */
    p.pattern = pattern;
    bptr = buffer;
    for (i = 0; i < count; i++) {
        *bptr++ = p.pat[i & (sizeof(uint32_t) - 1)];
    }
    return;
}

/*
 * VerifyBuffers() - Verify Data Buffers.
 *
 * Description:
 *	Simple verification of two data buffers.
 *
 * Inputs:
 *      sdp = The SCSI device pointer.
 *	dbuffer = Data buffer to verify with.
 *	vbuffer = Verification buffer to use.
 *	count = The number of bytes to compare.
 *
 * Outputs:
 *      Returns SUCCESS/FAILURE = Data Ok/Compare Error.
 */
int
VerifyBuffers(  scsi_device_t   *sdp,
                unsigned char	*dbuffer,
		unsigned char	*vbuffer,
		size_t		count )
{
    /* Note: This needs to go! */
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    register unsigned char *dptr = dbuffer;
    register unsigned char *vptr = vbuffer;
    register uint32_t i;

    if (memcmp(dptr, vptr, count) == 0) {
        return (SUCCESS);
    }
    for (i = 0; (i < count); i++, dptr++, vptr++) {
	if (*dptr != *vptr) {
            time_t error_time = time((time_t *) 0);
	    size_t dump_size = min(sdp->dump_limit, count);
            if (sdp->verbose) Fprint(sdp, "\n");
	    DisplayScriptInformation(sdp);
            Fprintf(sdp, "ERROR: Error number %u occurred on %s", ++sdp->error_count, ctime(&error_time));
            Fprintf(sdp, "Data Compare Error on device %s (thread %d)\n", sgp->dsf, sdp->thread_number);
            if (iop->block_limit) {
                Fprintf(sdp, "The current logical block is " LUF " (" LXF "), length is %u blocks\n", 
                        iop->current_lba, iop->current_lba, (sgp->data_length / iop->device_size));
            }
	    /* expected */
	    dump_buffer(sdp, expected_str, vbuffer, vptr, dump_size, count, True);
	    /* received */
	    dump_buffer(sdp, received_str, dbuffer, dptr, dump_size, count, False);
	    return (FAILURE);
	}
    }
    return (SUCCESS);
}

/*
 * dump_buffer() Dump data buffer in hex bytes.
 *
 * Inputs:
 *      sdp = The SCSI device pointer.
 *      name = The buffer name being dumped.
 *	base = Base pointer of buffer to dump.
 *	ptr  = Pointer into buffer being dumped.
 *	dump_size = The size of the buffer to dump.
 *	bufr_size = The maximum size of this buffer.
 *	expected = Boolean flag (True = Expected).
 *
 * Return Value:
 *	Void
 */
void
dump_buffer (   scsi_device_t   *sdp,
                char		*name,
		unsigned char	*base,
		unsigned char	*ptr,
		size_t		dump_size,
		size_t		bufr_size,
		hbool_t		expected )
{
    size_t boff, coff, limit, offset;
    u_int  field_width = 16;
    unsigned char *bend = (base + bufr_size);
    unsigned char *bptr;
    unsigned char data;
    unsigned char *abufp, *abp;

    abufp = abp = (unsigned char *) Malloc(sdp, field_width + 1);
    /*
     * Since many requests do large transfers, limit data dumped.
     */
    //limit = (dump_size < dump_limit) ? dump_size : dump_limit;
    limit = dump_size;  /* Expect to be limited already! */

    /*
     * Now to provide context, attempt to dump data on both sides of
     * the corrupted data, ensuring buffer limits are not exceeded.
     */
    if ((size_t)(ptr - base) <= limit) {
        bptr = base;                    /* Dump the entire buffer. */
    } else {
        bptr = (ptr - (limit >> 1));
    }
    if (bptr < base) bptr = base;
    if ( (bptr + limit) > bend) {
	limit = (bend - bptr);		/* Dump to end of buffer. */
    }
    offset = (ptr - base);		/* Offset to failing data. */
    coff = (ptr - bptr);                /* Offset from dump start. */

    Fprintf(sdp, "The %scorrect data starts at address %#lx (marked by asterisk '*')\n",
							(expected) ? "" : "in", ptr);
    Fprintf(sdp, "Dumping %s Data Buffer (base = %#lx, offset = %u, limit = %u bytes):\n",
							name, base, offset, limit);

#if defined(MACHINE_64BITS)
    LogMsg(sdp, sdp->efp, logLevelError, (PRT_NOFLUSH | PRT_NOLEVEL), "          Address / Offset\n");
#else /* !defined(MACHINE_64BITS) */
    LogMsg(sdp, sdp->efp, logLevelError, (PRT_NOFLUSH | PRT_NOLEVEL), "  Address / Offset\n");
#endif /* defined(MACHINE_64BITS) */

    /*
     * TODO: Rewrite this to provide side by side comparision, and also flag
     * each failing byte, NOT just the first! Awaiting corruption analysis!
     */
    for (boff = 0; boff < limit; boff++, bptr++) {
	if ((boff % field_width) == (size_t) 0) {
	    if (boff) Fprint(sdp, " \"%s\"\n", abufp); abp = abufp;
	    #if defined(MACHINE_64BITS)
            LogMsg(sdp, sdp->efp, logLevelError, (PRT_NOFLUSH | PRT_NOLEVEL), "%#018lx/%6d |",
#else /* !defined(MACHINE_64BITS) */
            LogMsg(sdp, sdp->efp, logLevelError, (PRT_NOFLUSH | PRT_NOLEVEL), "%#010lx/%6d |",
#endif /* defined(MACHINE_64BITS) */
		   bptr, (boff + (offset - coff)));
	}
	data = *bptr;
	Fprint(sdp, "%c%02x", (bptr == ptr) ? '*' : ' ', data);
        abp += Sprintf ((char *)abp, "%c", isprint((int)data) ? data : ' ');
    }
    if (abp != abufp) {
        while (boff++ % field_width) Fprint(sdp, "   ");
        Fprint(sdp, " \"%s\"\n", abufp);
    }
    Free(sdp, abufp);
    if (expected) Fprintf(sdp, "\n");
    (void)fflush(sdp->efp);
}

int
Fputs(char *str, FILE *stream)
{
    int status = SUCCESS;

    (void)fputs((char *)str, stream);
    if (ferror (stream) != 0) {
        clearerr (stream);
        status = FAILURE;
    }
    return (status);
}

/*
 * open_file - Generic Open File Function.
 * 
 * Inputs:
 * 	file = The file name.
 * 	open_mode = The read/write open mode.
 * 	*fd = Pointer to store handle.
 * 
 * Return Value:
 * 	SUCCESS / FAILURE = File open'ed / Open Failed.
 */ 
int
open_file(scsi_device_t *sdp, char *file, open_mode_t open_mode, HANDLE *fd)
{
    /*
     * When reading data, open a file for writing (if requested).
     */
    if (open_mode == OPEN_FOR_READING) {
	/* Open a file to read data from. */
	if (*file == '-') {
#if defined(_WIN32)
	    *fd = GetStdHandle(STD_INPUT_HANDLE);
#else /* !defined(_WIN32) */
	    *fd = dup(fileno(stdin));
#endif /* defined(_WIN32) */
	} else {
#if defined(_WIN32)
	    *fd = CreateFile(file, GENERIC_READ, FILE_SHARE_READ,
			     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#else /* !defined(_WIN32) */
	    *fd = open(file, O_RDONLY);
#endif /* defined(_WIN32) */
	}
	if (*fd == INVALID_HANDLE_VALUE) {
	    os_perror(sdp, "Couldn't open '%s' for reading!", file);
	    return (FAILURE);
	}
    } else { /* !reading */
	if (*file == '-') {
#if defined(_WIN32)
	    *fd = GetStdHandle(STD_OUTPUT_HANDLE);
#else /* !defined(_WIN32) */
	    *fd = dup(fileno(stdout));
#endif /* defined(_WIN32) */
	} else {
	    /* Create a file to write data to. */
#if defined(_WIN32)
	    *fd = CreateFile(file, GENERIC_WRITE, FILE_SHARE_WRITE,
			     NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#else /* !defined(_WIN32) */
	    *fd = open(file, (O_CREAT|O_RDWR), (S_IRWXU|S_IRWXG|S_IRWXO));
#endif /* defined(_WIN32) */
	}
	if (*fd == INVALID_HANDLE_VALUE) {
	    os_perror(sdp, "Couldn't open '%s' for writing!", file);
	    return(FAILURE);
	}
    }
    return(SUCCESS);
}

int
close_file(scsi_device_t *sdp, HANDLE *fd)
{
#if defined(_WIN32)
    (void)CloseHandle(*fd);
#else /* !defined(_WIN32) */
    (void)close(*fd);
#endif /* defined(_WIN32) */
    *fd = INVALID_HANDLE_VALUE;
    return (SUCCESS);
}

int
read_file(scsi_device_t *sdp, char *file, HANDLE fd, unsigned char *buffer, size_t length)
{
    ssize_t count;
#if defined(_WIN32)
    if (!ReadFile(fd, buffer, (DWORD)length, (LPDWORD)&count, NULL)) count = -1;
#else /* !defined(_WIN32) */
    count = read(fd, buffer, length);
#endif /* defined(_WIN32) */
    if ((size_t)count != length) {
        os_perror(sdp, "Read failed while reading %u bytes from file!", length, file);
	return (FAILURE);
    }
    return (SUCCESS);
}

int
write_file(scsi_device_t *sdp, char *file, HANDLE fd, unsigned char *buffer, size_t length)
{
    ssize_t count;
#if defined(_WIN32)
    if (!WriteFile(fd, buffer, (DWORD)length, (LPDWORD)&count, NULL)) count = -1;
#else /* !defined(_WIN32) */
    count = write(fd, buffer, length);
#endif /* defined(_WIN32) */
    if ((size_t)count != length) {
        os_perror(sdp, "Write failed while writing %u bytes to file!", length, file);
	return (FAILURE);
    }
    return (SUCCESS);
}

/*
 * Simple Error Reporting Functions (for consistency):
 */
void
ReportCdbDeviceInformation(scsi_device_t *sdp, scsi_generic_t *sgp)
{
    ReportErrorInformation(sdp);
    Fprintf(sdp, "%s failed on device %s (thread %d)\n", sgp->cdb_name, sgp->dsf, sdp->thread_number);
    ReportCdbScsiInformation(sdp, sgp);
    return;
}

void
ReportCdbScsiInformation(scsi_device_t *sdp, scsi_generic_t *sgp)
{
    char efmt_buffer[EMIT_STATUS_BUFFER_SIZE];
    if (sdp == NULL) sdp = master_sdp;
    efmt_buffer[0] = '\0';
    (void)FmtEmitStatus(sdp, NULL, sgp, "SCSI CDB: %cdb, dir=%dir, length=%length", efmt_buffer);
    Fprintf(sdp, "%s\n", efmt_buffer);
    return;
}

void
ReportDeviceInformation(scsi_device_t *sdp, scsi_generic_t *sgp)
{
    ReportErrorInformation(sdp);
    Fprintf(sdp, "Device %s (thread %d)\n", sgp->dsf, sdp->thread_number);
    return;
}

void
ReportErrorInformation(scsi_device_t *sdp)
{
    DisplayScriptInformation(sdp);
    ReportErrorTimeStamp(sdp);
    return;
}

void
ReportErrorMessage(scsi_device_t *sdp, scsi_generic_t *sgp, char *error_msg)
{
    DisplayScriptInformation(sdp);
    ReportErrorTimeStamp(sdp);
    Fprintf(sdp, "%s on device %s (thread %d)\n", error_msg, sgp->dsf, sdp->thread_number);
    return;
}

void
ReportErrorTimeStamp(scsi_device_t *sdp)
{
    time_t error_time = time((time_t *) 0);
    /* Note: Counts the error also! */
    Fprintf(sdp, "ERROR: Error number %u occurred on %s", ++sdp->error_count, ctime(&error_time));
    return;
}

void
DisplayScriptInformation(scsi_device_t *sdp)
{
    if (sdp->script_level) {
	int level = (sdp->script_level - 1);
	Fprintf(sdp, "Script '%s', line number %d\n",
		sdp->script_name[level], sdp->script_lineno[level]);
    }
    return;
}

void
CloseScriptFile(scsi_device_t *sdp)
{
    int level = --sdp->script_level;
    (void)fclose(sdp->sfp[level]);
    sdp->sfp[level] = (FILE *)0;
    free(sdp->script_name[level]);
    sdp->script_name[level] = NULL;
    return;
}

void
CloseScriptFiles(scsi_device_t *sdp)
{
    while (sdp->script_level) {
        CloseScriptFile(sdp);
    }
    return;
}
int
OpenScriptFile(scsi_device_t *sdp, char *file)
{
    char filename_buffer[KBYTE_SIZE];
    struct stat statbuf;
    struct stat *sb = &statbuf;
    char *fnp = filename_buffer;
    char *scriptfile = file;
    char *mode = "r";
    int level = sdp->script_level;
    FILE **fp = &sdp->sfp[level];
    int status;

    if ( strlen(file) == 0) {
        Fprintf(sdp, "Please specify a script file name!\n");
        return (FAILURE);
    }
    if ( (level + 1) > ScriptLevels) {
        Fprintf(sdp, "The maximum script level is %d!\n", ScriptLevels);
        return (FAILURE);
    }
    /*
     * Logic:
     *   o If default extension was specified, then attempt to locate
     *	   the specified script file.
     *   o If default extension was NOT specified, then attempt to
     *	   locate the file with default extension first, and if that
     *	   fails, attempt to locate the file without default extension.
     */
    if ( strstr (scriptfile, ScriptExtension) ) {
        (void) strcpy (fnp, scriptfile);
        status = stat (fnp, sb);
    } else {
        (void) sprintf (fnp, "%s%s", scriptfile, ScriptExtension);
        if ( (status = stat (fnp, sb)) == FAILURE) {
            (void) strcpy (fnp, scriptfile);
            status = stat (fnp, sb);
        }
    }

    if (status == FAILURE) {
        Perror(sdp, "Unable to access script file '%s'", fnp);
        return (status);
    }

    if ( (*fp = fopen (fnp, mode)) == (FILE *) 0) {
        Perror(sdp, "Unable to open script file '%s', mode '%s'", file, mode);
        return (FAILURE);
    }
    sdp->script_name[level] = strdup(fnp);
    sdp->script_lineno[level] = 0;
    sdp->script_level++;
    return (SUCCESS);
}

/*
 * Format the elapsed time.
 */
int
FormatElapstedTime(char *buffer, clock_t ticks)
{
    char *bp = buffer;
    unsigned int hr, min, sec, frac;

    frac = (ticks % hertz);
    frac = (frac * 100) / hertz;
    ticks /= hertz;
    sec = ticks % 60; ticks /= 60;
    min = ticks % 60;
    if (hr = ticks / 60) {
	bp += Sprintf(bp, "%dh", hr);
    }
    bp += Sprintf(bp, "%02dm", min);
    bp += Sprintf(bp, "%02d.", sec);
    bp += Sprintf(bp, "%02ds", frac);
    return( (int)(bp - buffer) );
}

/*
 * Check for all hex characters in string.
 */
hbool_t
isHexString(char *s)
{
    if ( (*s == '0') &&
         ((*(s+1) == 'x') || (*(s+1) == 'X')) ) {
        s += 2; /* Skip over "0x" or "0X" */
    }
    while (*s) {
        if ( !isxdigit((int)*s++) ) return(False);
    }
    return(True);
}
