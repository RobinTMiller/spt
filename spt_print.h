#if !defined(SPT_PRINT_H)
#define SPT_PRINT_H
/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2018			    *
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

typedef enum logLevel {
	logLevelCrit = LOG_CRIT,
	logLevelError = LOG_ERR,
	logLevelInfo = LOG_INFO,
        logLevelDiag = LOG_INFO,
    	logLevelLog  = LOG_INFO,
	logLevelWarn = LOG_WARNING
} logLevel_t;

/*
 * Flags to control print behaviour:
 */
#define PRT_NOFLUSH	0x01
#define PRT_NOIDENT	0x02
#define PRT_NOLEVEL	0x04
#define PRT_NOLOG	0x08
#define PRT_SYSLOG	0x10

/*
 * Strings used by Common Printing Functions.
 */
#define ASCII_FIELD	"%38.38s: %s"
#define EMPTY_FIELD	"%40.40s%s"
#define NUMERIC_FIELD	"%38.38s: %u"
#define DEC_HEX_FIELD	"%38.38s: %u (%#lx)"
#define HEX_FIELD	"%38.38s: %#x"
#define HEXP_FIELD	"%38.38s: " LLHXFMT
#define HEX_DEC_FIELD	"%38.38s: %#x (%u)"
#define FIELD_WIDTH	40		/* The field width (see above).	*/
#define DEFAULT_WIDTH   132			/* tty display width.   */

#define LNUMERIC_FIELD	"%38.38s: " LUF
#define LDEC_HEX_FIELD	"%38.38s: " LUF " (" LXF ")"
#define LHEX_FIELD	"%38.38s: " LXF
#define LHEXP_FIELD	"%38.38s: " LLFXFMT
#define LHEX_DEC_FIELD	"%38.38s: " LXF " (" LUF ")"

#define DNL		0			/* Disable newline.	*/
#define PNL		1			/* Print newline.	*/

/* dtprint.c */
extern int DisplayWidth;
extern int initialize_print_lock(scsi_device_t *sdp);
extern int acquire_print_lock(void);
extern int release_print_lock(void);
extern int AcquirePrintLock(scsi_device_t *sdp);
extern int ReleasePrintLock(scsi_device_t *sdp);

extern int PrintLogs(scsi_device_t *sdp, FILE *fp, char *buffer);
extern char *fmtmsg_prefix(scsi_device_t *sdp, char *bp, int flags, logLevel_t level);
extern void LogMsg(scsi_device_t *sdp, FILE *fp, enum logLevel level, int flags, char *fmtstr, ...);
extern void SystemLog(scsi_device_t *sdp, int priority, char *format, ...);

extern void Eprintf(scsi_device_t *sdp, char *format, ...);
extern void Fprintf(scsi_device_t *sdp, char *format, ...);
extern void Fprint(scsi_device_t *sdp, char *format, ...);
extern void Fprintnl(scsi_device_t *sdp);
extern void Lprintf(scsi_device_t *sdp, char *format, ...);
extern void Printf(scsi_device_t *sdp, char *format, ...);
extern void Print(scsi_device_t *sdp, char *format, ...);
extern void Printnl(scsi_device_t *sdp);
extern void Wprintf(scsi_device_t *sdp, char *format, ...);
extern void Perror(scsi_device_t *sdp, char *format, ...);
extern void Lflush(scsi_device_t *sdp);
extern int Sprintf(char *bufptr, char *msg, ...);
extern int vSprintf(char *bufptr, const char *msg, va_list ap);

extern void DumpFieldsOffset(scsi_device_t *sdp, uint8_t *bptr, int length);
extern void PrintFields(scsi_device_t *sdp, u_char *bptr, int length);
extern void PrintHAFields(scsi_device_t *sdp, unsigned char *bptr, int length);

extern void PrintLines(scsi_device_t *sdp, char *buffer);
extern void PrintHeader(scsi_device_t *sdp, char *header);
extern void PrintNumeric(scsi_device_t *sdp, char *field_str, uint32_t numeric_value, int nl_flag);
#define PrintDec PrintDecimal
extern void PrintDecimal(scsi_device_t *sdp, char *field_str, uint32_t numeric_value, int nl_flag);
extern void PrintDecHex(scsi_device_t *sdp, char *field_str, uint32_t numeric_value, int nl_flag);
extern void PrintHex(scsi_device_t *sdp, char *field_str, uint32_t numeric_value, int nl_flag);
extern void PrintHexP(scsi_device_t *sdp, char *field_str, uint32_t numeric_value, int nl_flag);
extern void PrintHexDec(scsi_device_t *sdp, char *field_str, uint32_t numeric_value, int nl_flag);
extern void PrintAscii(scsi_device_t *sdp, char *field_str, char *ascii_str, int nl_flag);
extern void PrintLongLong(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintLongDec(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintLongDecHex(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintLongHex(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintLongHexP(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintLongHexDec(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintBoolean( scsi_device_t	*sdp,
			  hbool_t	numeric,
			  char		*field_str,
			  hbool_t	boolean_flag,
			  hbool_t	nl_flag);
extern void PrintEnDis( scsi_device_t	*sdp,
			hbool_t		numeric,
			char		*field_str,
			hbool_t		boolean_flag,
			hbool_t		nl_flag);
extern void PrintOnOff( scsi_device_t	*sdp,
			hbool_t		numeric,
			char		*field_str,
			hbool_t		boolean_flag,
			hbool_t		nl_flag);
extern void PrintYesNo( scsi_device_t	*sdp,
			hbool_t		numeric,
			char		*field_str,
			hbool_t		boolean_flag,
			hbool_t		nl_flag);

extern int FormatAsciiData(char *text, int offset, uint8_t *bptr, int length);
extern int PrintAsciiData(scsi_device_t *sdp, int offset, uint8_t *bptr, int length);
extern void PrintFieldData(scsi_device_t *sdp, char *field, int offset, char *data);
extern int FormatHexData(char *text, int offset, uint8_t *bptr, int length);
extern int PrintHexData(scsi_device_t *sdp, int offset, uint8_t *bptr, int length);
#if defined(INLINE_FUNCS)
INLINE int
PrintHexDebug(scsi_device_t *sdp, int offset, uint8_t *ucp, int length)
{
    if (sdp->DebugFlag) {
	offset = PrintHexData(sdp, offset, ucp, length);
    } else {
	offset += length;
    }
    return(offset);
}
#else /* !defined(INLINE_FUNCS) */
extern int PrintHexDebug(scsi_device_t *sdp, int offset, uint8_t *bptr, int length);
#endif /* defined(INLINE_FUNCS) */
extern int FormatHexBytes(char *text, int offset, uint8_t *bptr, int length);
extern char *FormatQuotedText(char *text, char *tp, int length);

#endif /* !defined(SPT_PRINT_H) */
