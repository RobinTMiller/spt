#if !defined(SPT_PRINT_H)
#define SPT_PRINT_H

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
#define HEX_DEC_FIELD	"%38.38s: %#x (%u)"
#define FIELD_WIDTH	40		/* The field width (see above).	*/
#define DEFAULT_WIDTH   132			/* tty display width.   */

#define LNUMERIC_FIELD	"%38.38s: " LUF
#define LDEC_HEX_FIELD	"%38.38s: " LUF " (" LXF ")"
#define LHEX_FIELD	"%38.38s: " LXF
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
extern void PrintHexDec(scsi_device_t *sdp, char *field_str, uint32_t numeric_value, int nl_flag);
extern void PrintAscii(scsi_device_t *sdp, char *field_str, char *ascii_str, int nl_flag);
extern void PrintLongLong(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintLongDec(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintLongDecHex(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag);
extern void PrintLongHex(scsi_device_t *sdp, char *field_str, uint64_t numeric_value, int nl_flag);
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

#endif /* !defined(SPT_PRINT_H) */
