/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2016			    *
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
 * Module:	spt_print.c
 * Author:	Robin T. Miller
 * Date:	December 16th, 2013
 *
 * Description:
 *	Various printing functions.
 *
 * Modification History:
 * 
 * February 25th, 2016 by Robin T. Miller
 *  Increase print format buffers to accomodate very long command lines.
 * 
 * December 16th, 2013 by Robin T. Miller
 *	Integrating dt's print functions into spt.
 */
#include "spt.h"

#include <ctype.h>
#include <string.h>
#include <stdarg.h>

/*
 * Local Definitions:
 */ 
int DisplayWidth = DEFAULT_WIDTH;

/*
 * Enabled/Disabled Table.
 */
char *endis_table[] = {
	"Disabled",
	"Enabled"
};

/*
 * On/Off Table.
 */
char *onoff_table[] = {
	"Off",
	"On"
};

/*
 * True/False Table:
 */
char *boolean_table[] = {
	"False",
	"True"
};

/*
 * Yes/No Table.
 */
char *yesno_table[] = {
	"No",
	"Yes"
};

pthread_mutex_t print_lock;		/* Printing lock (sync output). */

int
initialize_print_lock(scsi_device_t *sdp)
{
    int status;

    if ( (status = pthread_mutex_init(&print_lock, NULL)) != SUCCESS) {
        tPerror(sdp, status, "pthread_mutex_init() of print lock failed!");
    }
    return(status);
}

int
acquire_print_lock(void)
{
    int status = pthread_mutex_lock(&print_lock);
    if (status != SUCCESS) {
	tPerror(NULL, status, "Failed to acquire print mutex!\n");
    }
    return(status);
}

int
release_print_lock(void)
{
    int status = pthread_mutex_unlock(&print_lock);
    if (status != SUCCESS) {
	tPerror(NULL, status, "Failed to unlock print mutex!\n");
    }
    return(status);
}

/*
 * fmtmsg_prefix() - Common function to format the prefix for messages.
 *
 * Inputs:
 *	sdp = The device information pointer.
 * 	bp = Pointer to the message buffer.
 * 	flags = The format control flags.
 * 	level = The logging level.
 *
 * Return Value:
 *	The updated buffer pointer.
 */
char *
fmtmsg_prefix(scsi_device_t *sdp, char *bp, int flags, logLevel_t level)
{
    char *log_prefix;

    if (sdp == NULL) sdp = master_sdp;
    /*
     * The logging prefix can be user define or standard.
     */ 
    if (sdp->log_prefix) {
        log_prefix = FmtLogPrefix(sdp, sdp->log_prefix, False);
    } else {
        if (sdp->debug_flag || sdp->tDebugFlag) {
            log_prefix = FmtLogPrefix(sdp, "%prog (tid:%tid j:%job t:%thread): ", False);
        } else {
            log_prefix = FmtLogPrefix(sdp, "%prog (j:%job t:%thread): ", False);
        }
    }
    bp += sprintf(bp, "%s", log_prefix);
    free(log_prefix);

    /*
     * Add an ERROR: prefix to clearly indicate error/critical issues.
     */
    if ( !(flags & PRT_NOLEVEL) ) {
	if ( (level == logLevelCrit) || (level == logLevelError) ) {
	    bp += sprintf(bp, "ERROR: ");
	} else if (level == logLevelWarn) {
	    bp += sprintf(bp, "Warning: ");
	}
    }
    sdp->sequence++;
    return (bp);
}

/*
 * Display failure message to file pointer and flush output.
 */
/*VARARGS*/
void
LogMsg(scsi_device_t *sdp, FILE *fp, enum logLevel level, int flags, char *fmtstr, ...)
{
    va_list ap;
    char buffer[LOG_BUFSIZE];
    char *bp = buffer;

    if (sdp == NULL) sdp = master_sdp;
    /* Note: The user conrols this with "%level" during formatting! */
    sdp->log_level = level;
    if ( !(flags & PRT_NOIDENT) ) {
	bp = fmtmsg_prefix(sdp, bp, flags, level);
    }
    va_start(ap, fmtstr);
    bp += vsprintf(bp, fmtstr, ap);
    va_end(ap);
    /* Note: Not currently used, but allows syslog only. */
    if ( !(flags & PRT_NOLOG) ) {
        (void)PrintLogs(sdp, fp, buffer);
	if ( !(flags & PRT_NOFLUSH) ) {
	    (void)fflush(fp);
	}
    }
    if ( sdp->syslog_flag && (flags & PRT_SYSLOG) ) {
	syslog(level, buffer);
    }
    return;
}

void
SystemLog(scsi_device_t *sdp, int priority, char *format, ...)
{
    va_list ap;
    char buffer[LOG_BUFSIZE];
    char *bp = buffer;
    int flags = PRT_NOLEVEL;
    logLevel_t level = logLevelInfo;

    if (sdp == NULL) sdp = master_sdp;
    bp = fmtmsg_prefix(sdp, bp, flags, level);
    va_start(ap, format);
    bp += vsprintf(bp, format, ap);
    va_end(ap);
    syslog(priority, buffer);
    return;
}

int
AcquirePrintLock(scsi_device_t *sdp)
{
    hbool_t job_log_flag;
    int status = WARNING;

    job_log_flag = (sdp->job && sdp->job->ji_job_logfp);
    /*
     * Locking logic:
     *  o if job log, acquire the job lock
     *    Note: This syncs all thread output to job log.
     *  o if no thread log, acquire global print lock
     *    Note: This syncs all thread output to the terminal.
     *  o otherwise, thread log we don't take any locks.
     *    Note: No locks necessary since only thread writing.
     */
    if (job_log_flag) {
        ; // status = acquire_job_print_lock(sdp, sdp->job);
    } else if (sdp->log_file == NULL) {
        status = acquire_print_lock();
    }
    return(status);
}

int
ReleasePrintLock(scsi_device_t *sdp)
{
    hbool_t job_log_flag;
    int status = WARNING;

    job_log_flag = (sdp->job && sdp->job->ji_job_logfp);
    /*
     * Locking logic:
     *  o if job log, acquire the job lock
     *    Note: This syncs all thread output to job log.
     *  o if no thread log, acquire global print lock
     *    Note: This syncs all thread output to the terminal.
     *  o otherwise, thread log we don't take any locks.
     *    Note: No locks necessary since only thread writing.
     */
    if (job_log_flag) {
        ; // status = release_job_print_lock(sdp, sdp->job);
    } else if (sdp->log_file == NULL) {
        status = release_print_lock();
    }
    return(status);
}

int
PrintLogs(scsi_device_t *sdp, FILE *fp, char *buffer)
{
    hbool_t job_log_flag;
    int status;

    job_log_flag = (sdp->job && sdp->job->ji_job_logfp);
    /*
     * Here's our printing logic:
     *  o if job and thread log is open, write to both.
     *  o if job log is open, write to this.
     *  o if no logs are open, write to specified fp.
     *    Note: Generally, this fp will be stdout/stderr.
     */
    if ( sdp->log_opened && job_log_flag) {
        status = Fputs(buffer, fp);
        if (sdp->joblog_inhibit == False) {
            status = Fputs(buffer, sdp->job->ji_job_logfp);
        }
    } else if (job_log_flag) {
        status = Fputs(buffer, sdp->job->ji_job_logfp);
    } else {
        status = Fputs(buffer, fp);
    }
    return(status);
}

/*
 * Function to print error message (with ERROR: prefix).
 */
void
Eprintf(scsi_device_t *sdp, char *format, ...)
{
    char buffer[LOG_BUFSIZE];
    va_list ap;
    char *bp = buffer;
    FILE *fp;

    (void)fflush(sdp->ofp);
    ReportErrorTimeStamp(sdp);
    fp = sdp->efp;
    bp = fmtmsg_prefix(sdp, bp, 0, logLevelError);
    va_start(ap, format);
    bp += vsprintf(bp, format, ap);
    va_end(ap);
    (void)PrintLogs(sdp, fp, buffer);
    (void)fflush(fp);
    return;
}

void
Fprintf(scsi_device_t *sdp, char *format, ...)
{
    char buffer[LOG_BUFSIZE];
    va_list ap;
    char *bp = buffer;
    FILE *fp;

    if (sdp == NULL) sdp = master_sdp;
    fp = sdp->efp;
    bp = fmtmsg_prefix(sdp, bp, 0, logLevelInfo);
    va_start(ap, format);
    bp += vsprintf(bp, format, ap);
    va_end(ap);
    (void)PrintLogs(sdp, fp, buffer);
    (void)fflush(fp);
    return;
}

/*
 * Same as Fprintf except no identifier or log prefix.
 */
/*VARARGS*/
void
Fprint(scsi_device_t *sdp, char *format, ...)
{
    char buffer[LOG_BUFSIZE];
    va_list ap;
    FILE *fp;

    if (sdp == NULL) sdp = master_sdp;
    fp = sdp->efp;
    va_start(ap, format);
    (void)vsprintf(buffer, format, ap);
    va_end(ap);
    (void)PrintLogs(sdp, fp, buffer);
    return;
}

void
Fprintnl(scsi_device_t *sdp)
{
    if (sdp == NULL) sdp = master_sdp;
    Fprint(sdp, "\n");
    (void)fflush(sdp->efp);
}


/*
 * Format & append string to log file buffer.
 */
/*VARARGS*/
void
Lprintf(scsi_device_t *sdp, char *format, ...)
{
    va_list ap;
    char *bp = sdp->log_bufptr;

    va_start(ap, format);
    vsprintf(bp, format, ap);
    va_end(ap);
    bp += strlen(bp);
    sdp->log_bufptr = bp;
    return;
}

/*
 * Flush the log buffer and reset the buffer pointer.
 */
void
Lflush(scsi_device_t *sdp)
{
    size_t size;
    if ((size = (sdp->log_bufptr - sdp->log_buffer)) > LOG_BUFSIZE) {
	Fprintf(sdp, "Oops, we've exceeded the log buffer size, %d > %d!\n", 
		size,  LOG_BUFSIZE);
    }
    PrintLines(sdp, sdp->log_buffer);
    sdp->log_bufptr = sdp->log_buffer;
    *sdp->log_bufptr = '\0';
    return;
}

/*
 * Display message to stdout & flush output.
 */
/*VARARGS*/
void
Printf(scsi_device_t *sdp, char *format, ...)
{
    char buffer[LOG_BUFSIZE];
    va_list ap;
    FILE *fp;
    char *bp = buffer;

    if (sdp == NULL) sdp = master_sdp;
    fp = sdp->ofp;
    bp = fmtmsg_prefix(sdp, bp, 0, logLevelInfo);
    va_start(ap, format);
    bp += vsprintf(bp, format, ap);
    va_end(ap);
    (void)PrintLogs(sdp, fp, buffer);
    (void)fflush(fp);
    return;
}

/*
 * Same as Printf except no program name identifier.
 */
/*VARARGS*/
void
Print(scsi_device_t *sdp, char *format, ...)
{
    char buffer[LOG_BUFSIZE];
    va_list ap;
    FILE *fp;

    if (sdp == NULL) sdp = master_sdp;
    fp = sdp->ofp;
    va_start(ap, format);
    (void)vsprintf(buffer, format, ap);
    va_end(ap);
    (void)PrintLogs(sdp, fp, buffer);
    return;
}

void
Printnl(scsi_device_t *sdp)
{
    if (sdp == NULL) sdp = master_sdp;
    Print(sdp, "\n");
    (void)fflush(sdp->ofp);
}

/*
 * Simple funtion to print warning messages (prefixed with "Warning: ")
 */
void
Wprintf(scsi_device_t *sdp, char *format, ...)
{
    char buffer[LOG_BUFSIZE];
    va_list ap;
    FILE *fp;
    char *bp = buffer;

    fp = sdp->ofp;
    bp = fmtmsg_prefix(sdp, bp, 0, logLevelWarn);
    va_start(ap, format);
    bp += vsprintf(bp, format, ap);
    va_end(ap);
    (void)PrintLogs(sdp, fp, buffer);
    (void)fflush(fp);
    return;
}

/*
 * Perror() - Common Function to Print Error Messages.
 *
 * Description:
 *	This reports POSIX style errors only.
 *
 * Implicit Inputs:
 *      format = Pointer to format control string.
 *      ... = Variable argument list.
 *
 * Return Value:
 *      void
 */
/*VARARGS*/
void
Perror(scsi_device_t *sdp, char *format, ...)
{
    char msgbuf[LOG_BUFSIZE];
    va_list ap;
    FILE *fp;

    if (sdp == NULL) sdp = master_sdp;
    fp = (sdp) ? sdp->efp : efp;
    va_start(ap, format);
    (void)vsprintf(msgbuf, format, ap);
    va_end(ap);
    LogMsg(sdp, fp, logLevelError, 0,
	   "%s, errno = %d - %s\n", msgbuf, errno, strerror(errno));
    return;
}

/*VARARGS*/
int
Sprintf(char *bufptr, char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    (void)vsprintf(bufptr, msg, ap);
    va_end(ap);
    return( (int)strlen(bufptr) );
}

int	
vSprintf(char *bufptr, const char *msg, va_list ap)
{
    (void)vsprintf(bufptr, msg, ap);
    return( (int)strlen(bufptr) );
}

void
PrintLines(scsi_device_t *sdp, char *buffer)
{
    char *bp = buffer, *p;
    char *end = (buffer + strlen(buffer));
    int status;
    
    status = AcquirePrintLock(sdp);

    do {
        if (p = strchr(bp, '\n')) {
            p++;
            Printf(sdp, "%.*s", (p - bp), bp);
            bp = p;
        } else {
            Printf(sdp, "%s", bp);
            break;
        }
    } while (bp < end);

    if (status == SUCCESS) {
        status = ReleasePrintLock(sdp);
    }
    return;
}

/*
 * PrintHeader() - Function to Displays Header Message.
 *
 * Inputs:
 *	header = The header strings to display.
 *
 * Return Value:
 *	void
 */
void
PrintHeader(scsi_device_t *sdp, char *header)
{
    Printf(sdp, "\n");
    Printf(sdp, "%s:\n", header);
    Printf(sdp, "\n");
}

/*
 * PrintAscii() - Function to Print an ASCII Field.
 * PrintNumeric() - Function to Print a Numeric Field.
 * PrintDecimal() - Function to Print a Numeric Field in Decimal.
 * PrintHex() - Function to Print a Numeric Field in Hexadecimal.	*
 *
 * Inputs:
 *	field_str = The field name string.
 *	ascii_str = The ASCII string to print.
 *	nl_flag = Flag to control printing newline.
 *
 * Return Value:
 *	void
 */
void
PrintNumeric(scsi_device_t *sdp, char *field_str, uint32_t numeric_value, int nl_flag)
{
    char *printf_str = NUMERIC_FIELD;

    Printf(sdp, printf_str, field_str, numeric_value);
    if (nl_flag) Print(sdp, "\n");
}

void
PrintDecimal(scsi_device_t *sdp, char *field_str, uint32_t numeric_value, int nl_flag)
{
    char *printf_str = NUMERIC_FIELD;

    Printf(sdp, printf_str, field_str, numeric_value);
    if (nl_flag) Print(sdp, "\n");
}

void
PrintDecHex(scsi_device_t *sdp, char *field_str, uint32_t numeric_value, int nl_flag)
{
    char *printf_str = DEC_HEX_FIELD;

    Printf(sdp, printf_str, field_str, numeric_value, numeric_value);
    if (nl_flag) Print(sdp, "\n");
}

void
PrintHex(scsi_device_t *sdp, char *field_str, uint32_t numeric_value, int nl_flag)
{
    char *printf_str = HEX_FIELD;

    Printf(sdp, printf_str, field_str, numeric_value);
    if (nl_flag) Print(sdp, "\n");
}

void
PrintHexDec(scsi_device_t *sdp, char *field_str, uint32_t numeric_value, int nl_flag)
{
    char *printf_str = HEX_DEC_FIELD;

    Printf(sdp, printf_str, field_str, numeric_value, numeric_value);
    if (nl_flag) Print(sdp, "\n");
}

void
PrintAscii(scsi_device_t *sdp, char *field_str, char *ascii_str, int nl_flag)
{
    size_t length = strlen(field_str);
    char *printf_str = ((length) ? ASCII_FIELD : EMPTY_FIELD);

    Printf(sdp, printf_str, field_str, ascii_str);
    if (nl_flag) Print(sdp, "\n");
}

/*
 * Functions to display quad (64-bit) values.
 */
void
PrintLongLong(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag)
{
    char *printf_str = LNUMERIC_FIELD;

    Printf(sdp, printf_str, field_str, numeric_value);
    if (nl_flag) Print(sdp, "\n");
}

void
PrintLongDec(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag)
{
    char *printf_str = LNUMERIC_FIELD;

    Printf(sdp, printf_str, field_str, numeric_value);
    if (nl_flag) Print(sdp, "\n");
}

void
PrintLongDecHex(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag)
{
    char *printf_str = LDEC_HEX_FIELD;

    Printf(sdp, printf_str, field_str, numeric_value, numeric_value);
    if (nl_flag) Print(sdp, "\n");
}

void
PrintLongHex(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag)
{
    char *printf_str = LHEX_FIELD;

    Printf(sdp, printf_str, field_str, numeric_value);
    if (nl_flag) Print(sdp, "\n");
}

void
PrintLongHexDec(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag)
{
    char *printf_str = LHEX_DEC_FIELD;

    Printf(sdp, printf_str, field_str, numeric_value, numeric_value);
    if (nl_flag) Print(sdp, "\n");
}

/*
 * Printxxx() - Functions to Print Fields with Context.
 *
 * Inputs:
 *	numeric = Print field as numeric.
 *	field_str = The field name string.
 *	ascii_str = The ASCII string to print.
 *	nl_flag = Flag to control printing newline.
 *
 * Return Value:
 *	void
 */
void
PrintBoolean(
    scsi_device_t *sdp,
    hbool_t	numeric,
    char	*field_str,
    hbool_t	boolean_flag,
    hbool_t	nl_flag)
{
    if (numeric) {
	PrintNumeric(sdp, field_str, boolean_flag, nl_flag);
    } else {
	PrintAscii(sdp, field_str, boolean_table[(int)boolean_flag], nl_flag);
    }
}

void
PrintEnDis(
    scsi_device_t   *sdp,
    hbool_t	    numeric,
    char	    *field_str,
    hbool_t	    boolean_flag,
    hbool_t         nl_flag)
{
    if (numeric) {
	PrintNumeric(sdp, field_str, boolean_flag, nl_flag);
    } else {
	PrintAscii(sdp, field_str, endis_table[(int)boolean_flag], nl_flag);
    }
}

void
PrintOnOff(
    scsi_device_t   *sdp,
    hbool_t         numeric,
    char	    *field_str,
    hbool_t	    boolean_flag,
    hbool_t	    nl_flag)
{
    if (numeric) {
	PrintNumeric(sdp, field_str, boolean_flag, nl_flag);
    } else {
	PrintAscii(sdp, field_str, onoff_table[(int)boolean_flag], nl_flag);
    }
}

void
PrintYesNo(
    scsi_device_t   *sdp,
    hbool_t	    numeric,
    char	    *field_str,
    hbool_t	    boolean_flag,
    hbool_t	    nl_flag)
{
    if (numeric) {
	PrintNumeric(sdp, field_str, boolean_flag, nl_flag);
    } else {
	PrintAscii(sdp, field_str, yesno_table[(int)boolean_flag], nl_flag);
    }
}

void
PrintFields(scsi_device_t *sdp, uint8_t *bptr, int length)
{
    int field_entrys = ((DisplayWidth - FIELD_WIDTH) / 3) - 1;
    int count = 0;

    while (count < length) {
	if (CmdInterruptedFlag == True)	break;
	if ((++count % field_entrys) == 0) {
	    Print(sdp, "%02x\n", *bptr++);
	    if (count < length)	PrintAscii(sdp, "", "", DNL);
	} else {
	    Print(sdp, "%02x ", *bptr++);
	}               
    }                       
    if (count % field_entrys) Print(sdp, "\n");
}                                     

void
PrintHAFields(scsi_device_t *sdp, uint8_t *bptr, int length)
{
    int field_entrys;
    int count = 0;
    u_char data;
    u_char *abufp, *abp;

    field_entrys = ((DisplayWidth - FIELD_WIDTH) / 3) - 1;
    field_entrys -= (field_entrys / 3);	/* For ASCII */
    abufp = abp = (u_char *)Malloc(sdp, (field_entrys + 1));
    if (abufp == NULL) return;
    while (count < length) {
	if (CmdInterruptedFlag == True)	break;
	data = *bptr++;
	Print(sdp, "%02x ", data);
	abp += Sprintf((char *)abp, "%c", isprint((int)data) ? data : ' ');
	*abp = '\0';
	if ((++count % field_entrys) == 0) {
	    Print(sdp, "\"%s\"\n", abufp); abp = abufp;
	    if (count < length)	PrintAscii(sdp, "", "", DNL);
	}
    }
    if (abp != abufp) {
	while (count++ % field_entrys) Print(sdp, "   ");
	Print(sdp, "\"%s\"\n", abufp);
    }
    Free(sdp, abufp);
    return;
}

void
DumpFieldsOffset(scsi_device_t *sdp, uint8_t *bptr, int length)
{
    int field_entrys = 16;
    int count = 0;
    uint8_t data;
    uint8_t *abufp, *abp;
    hbool_t first = True;

    if (length == 0) return;
    abufp = abp = (uint8_t *)Malloc(sdp, field_entrys + 1);
    if (abufp == NULL) return;
    /*
     * Print offset followed by 'n' hex bytes and ASCII text.
     */
    Printf(sdp, "Offset  Data\n");
    while (count < length) {
        if (first) {
            Printf(sdp, "%06d  ", count);
            first = False;
        }
        data = *bptr++;
        Print(sdp, "%02x ", data);
        abp += sprintf((char *)abp, "%c", isprint((int)data) ? data : ' ');
        *abp = '\0';
        if ((++count % field_entrys) == 0) {
            Print(sdp, "\"%s\"\n", abufp);
            first = True;
            abp = abufp;
        }
    }
    if (abp != abufp) {
        while (count++ % field_entrys) Print(sdp, "   ");
        Print(sdp, "\"%s\"\n", abufp);
    }
    Free(sdp, abufp);
    return;
}
