/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2020			    *
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
 * File:	spt.h
 * Author:	Robin T. Miller
 * Date:	September 27th, 2006
 *
 * Description:
 *	This file contains definitions, structures, and declarations for spt.
 *
 * Modification History:
 * 
 * December 27th, 2016 by Robin T. Miller
 *      Increased the dump limit default to 1024 or 1 Kbytes.
 *      Added support for Storage Enclosure Services (SES).
 * 
 * February 25th, 2016 by Robin T. Miller
 * 	Increased the log file buffer size to accomodate very long
 * command lines. What's very long? how about ~45k of pout data!!!
 * Therefore, the log file buffer size is now set to the args buffer
 * size which has been increased from 32k to 64k!
 * 
 * August 27th, 2011 by Robin T. MIller
 *	Add data structures for verifying expected input data.
 *
 * May 9th, 2010 by Robin T. Miller
 * 	Initial creation, moving defintions here from spt.c
 */
#if defined(AIX)
# include "aix_workaround.h"
#endif /* defined(AIX) */

/* I've seen inline, __inline, and __inline__ used! */
#define INLINE		static __inline
#define INLINE_FUNCS	1

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(WIN32)
#  include<io.h>
#  include<direct.h>
#  include<process.h>
#else /* !defined(WIN32) */
#  include <pthread.h>
#  include <unistd.h>
#  include <strings.h>      /* for strcasecmp() */
#  include <termios.h>
#  include <sys/param.h>
#  include <sys/time.h>
#  include <sys/times.h>
#  if !defined(NOSYSLOG)
#    define SYSLOG 1
#    include <syslog.h>
#  endif /* defined(NOSYSLOG) */
#endif /* defined(WIN32) */

#include "libscsi.h"
#include "inquiry.h"
#include "scsi_opcodes.h"
#include "scsi_ses.h"
#if defined(_WIN32)
#  include "spt_win.h"
#else /* !defined(_WIN32) */
#  include "spt_unix.h"
#  include <pthread.h>
#endif /* defined(_WIN32) */

/* Note: spt_win.h or spt_unix.h included in include.h! */
#include "include.h"
#include "spt_mtrand64.h"

//#define DEF_LOG_BUFSIZE	(PATH_BUFFER_SIZE * 2) /* File name + stats!	*/
#define DEF_LOG_BUFSIZE	ARGS_BUFFER_SIZE

#define LOG_BUFSIZE	DEF_LOG_BUFSIZE	/* The log file buffer size.	*/

/*
 * Definitions:
 */
#define MAX_DEVICES	5		/* The max devices per thread.	*/
#define IO_INDEX_BASE	0		/* The base IO parameters.	*/
#define IO_INDEX_DSF	IO_INDEX_BASE	/* The device special file.	*/
#define IO_INDEX_DST	IO_INDEX_DSF	/* The destination is the 1st.	*/
#define IO_INDEX_SRC	(IO_INDEX_DST + 1) /* Start of source devices.	*/
#define IO_INDEX_DSF1	(IO_INDEX_DST + 1) /* Start of second device.	*/
#define MAX_COPY_DEVICES 2		/* The max copy/verify devices.	*/

#define XCOPY_MIN_DEVS	2		/* The minimum xcopy devices.	*/
#define XCOPY_MAX_DEVS	(MAX_DEVICES - 1) /* The maximum xcopy devices.	*/

#define EMIT_STATUS_BUFFER_SIZE	4096

#define TIMEOUT_SECONDS		60	/* Default timeout in seconds.	*/
#undef ScsiDefaultTimeout
#define ScsiDefaultTimeout	(TIMEOUT_SECONDS * MSECS)
					/* Default timeout value (ms).  */
#define AbortDefaultTimeout	1	/* Default abort timoeut (ms).	*/
#define BypassFlagDefault	False
#define CompareFlagDefault	False
#define DeviceOpenFlagDefault	True
#define ErrorsFlagDefault	True
#define ImageModeFlagDefault	False
#define JsonPrettyFlagDefault	True
#define PreWriteFlagDefault	True
#define ReadAfterWriteDefault	False
#define ShowCachingFlagDefault  False
#define UniquePatternDefault	True

#define LogHeaderFlagDefault	False
#define MapDeviceToScsiDefault	False
#define RecoveryDelayDefault	2
#define RecoveryRetriesDefault	60
#define RecoveryFlagDefault	False
#define RepeatCountDefault	1
#define RetryLimitDefault	1
#define RangeCountDefault	1
#define SegmentCountDefault	1
#define ThreadsDefault		1
#define VerboseFlagDefault	True
#define VerifyFlagDefault	False
#define WarningsFlagDefault	True
#define DumpLimitDefault	KBYTE_SIZE
#define ScriptLevels		5
#define IOT_SEED		0x01010101 /* Default IOT pattern seed.	*/

#define SataDeviceFlagDefault	False	/* Controls SATA ASCII decoding.*/
#define SenseFlagDefault	True
#define ScsiInformationDefault	False
#define ScsiReadTypeDefault	scsi_read16_cdb
#define ScsiWriteTypeDefault	scsi_write16_cdb
#define ScsiIoLengthDefault	65536	/* This is 64K (in bytes) folks! */
#define ScsiReadLengthDefault	ScsiIoLengthDefault
#define ScsiWriteLengthDefault	ScsiIoLengthDefault

#define ANY_DATA_LIMITS(ioparmp) ( (ioparmp->ending_lba != 0) || (ioparmp->data_limit != 0) )

/*
 * Macro used with IOT to extract the LBA:
 */
#define get_lbn(bp)	( ((uint32_t)bp[3] << 24) | ((uint32_t)bp[2] << 16) | \
			  ((uint32_t)bp[1] << 8) | (uint32_t)bp[0])

#if !defined(MAXBADBLOCKS)
#  define MAXBADBLOCKS	10
#endif

typedef enum bfmt {DEC_FMT, HEX_FMT} bfmt_t;
typedef enum dfmt {NONE_FMT, BYTE_FMT, SHORT_FMT, WORD_FMT, QUAD_FMT} dfmt_t;
typedef enum ofmt {ASCII_FMT, CVS_FMT, JSON_FMT} ofmt_t;
typedef enum rfmt {REPORT_BRIEF, REPORT_FULL, REPORT_NONE} rfmt_t;

/*
 * Expected Data Definitions: (parameter data)
 */
#define EXP_DATA_ENTRIES	64	/* Match SCSIT simulator (for now). */

typedef struct exp_data {
    uint32_t	exp_byte_index;		/* Index into received data, */
    uint8_t	exp_byte_value;		/* Byte value at this index. */
} exp_data_t;

/*
 * Expected Data Types: (for input parsing)
 */
typedef enum exp_type {
    EXP_CHAR = 0,			/* Character strings. */
    EXP_BYTE,				/* 8-bit data value.  */
    EXP_SHORT,				/* 16-bit data value. */
    EXP_WORD,				/* 32-bit data value. */
    EXP_LONG				/* 64-bit data value. */
} exp_type_t;

/*
 * Error Control Actions:
 */
typedef enum onerr_control {
    ONERR_CONTINUE,			/* Continue execution on errors.*/
    ONERR_STOP				/* Stop execution on errors.    */
} onerr_control_t;

typedef enum thread_state {TS_NotQueued, TS_Queued} tstate_t;

/* Separator between file name and file postfix. */
#define DEFAULT_FILE_SEP	"-"
/* Note: Used for a directory postfix and file postfix. */
#define DEFAULT_FILE_POSTFIX	"j%jobt%thread"
#define DEFAULT_JOBLOG_POSTFIX	"Job%job"

#define DEFAULT_GTOD_LOG_PREFIX		"%tod (%etod) %prog (j:%job t:%thread): " 

#if !defined(MAXHOSTNAMELEN)
#   define MAXHOSTNAMELEN	256
#endif

#if 0
typedef enum job_state {JS_STOPPED, JS_RUNNING, JS_FINISHED, JS_PAUSED, JS_TERMINATING} jstate_t;
typedef enum thread_state {TS_STOPPED, TS_STARTING, TS_RUNNING, TS_FINISHED, TS_JOINED, TS_PAUSED, TS_TERMINATING} tstate_t;
typedef volatile jstate_t vjstate_t;
typedef volatile tstate_t vtstate_t;
#endif /* 0 */

/*
 * Vendor Identification:
 *
 * This identifier is used to control vendor unique commands, as well as
 * looking up the additional sense code/qualifier for error messages.
 */
typedef enum vendor_ident {
    VID_UNKNOWN		= -1,		/* Unknown vendor identity.     */
    VID_ALL		= 0,		/* Support for all vendors.	*/
    VID_CELESTICA,			/* Celestica vendor identity.   */
    VID_HGST,				/* HGST vendor identity.        */
    VID_WDC,				/* Western Digital identity.    */
    VID_NIMBLE				/* HPE/Nimble storage array.	*/
} vendor_id_t;

typedef enum product_ident {
    PID_UNKNOWN		= -1,		/* Unknown product identity.	*/
    PID_ALL		= 0,		/* Support for all products.	*/
    PID_CASTLEPEAK,			/* Castle Peak enclosure.	*/
    PID_MISSIONPEAK,			/* Mission Peak enclosure.	*/
    PID_KEPLER,				/* Kepler enclosure.		*/
    PID_PIKESPEAK,			/* Pikes Peak enclosure.	*/
    PID_OURAY,				/* Ouray enclosure.		*/
    PID_MT_MADONNA                      /* Mount Madonna enclosure.     */
} product_id_t;

typedef enum iomode {
    IOMODE_COPY,
    IOMODE_MIRROR,
    IOMODE_TEST,
    IOMODE_VERIFY
} iomode_t;

/*
 * Type of Command:
 */
typedef enum cmd_type { CMD_TYPE_NONE, CMD_TYPE_CLEAR, CMD_TYPE_GET, CMD_TYPE_SET, CMD_TYPE_RESET } cmd_type_t;

/*
 * Clear/Get/Set Type:
 */
typedef enum cgs_type {
    CGS_TYPE_NONE,
    CGS_TYPE_DEVOFF,
    CGS_TYPE_FAULT,
    CGS_TYPE_IDENT,
    CGS_TYPE_UNLOCK,
} cgs_type_t;

/*
 * Type of Operations:
 */
typedef enum spt_op {
    UNDEFINED_OP = 0,
    ABORT_TASK_SET_OP,
    BUS_RESET_OP,
    TARGET_RESET_OP,
    LUN_RESET_OP,
    CTLR_RESET_OP,
    COLD_RESET_OP,
    WARM_RESET_OP,
    SCSI_CDB_OP,
    SCAN_BUS_OP,
    RESUME_IO_OP,
    SUSPEND_IO_OP,
    SHOW_DEVICES_OP
} spt_op_t;

/*
 * Test Checks Information:
 */
typedef struct scsi_test_checks {
    hbool_t	check_status;		/* Check for expected status.	*/
    hbool_t	wait_for_status;	/* Wait for expected status.	*/
    uint8_t	exp_scsi_status;	/* Expected SCSI status.	*/
    uint8_t	exp_sense_key;		/* Expected sense key.		*/
    uint8_t	exp_sense_asc;		/* Expected sense code.		*/
    uint8_t	exp_sense_asq;		/* Expected sense qualifier.	*/
    hbool_t	check_resid;		/* Check for residual count.	*/
    uint32_t	exp_residual;		/* Expected residual count.	*/
    hbool_t	check_xfer;		/* Check the transfer count.	*/
    uint32_t	exp_transfer;		/* Expected transfer count.	*/
} scsi_test_checks_t;

/*
 * SCSI Specific Information:
 */
typedef struct scsi_information {
    inquiry_t	*si_inquiry;		/* The device Inquiry data.	*/
    idt_t	si_idt;			/* The Inquiry data type.	*/
    char	*si_vendor_id;		/* The Inquiry vendor ID.	*/
    char	*si_product_id;		/* The Inquiry product ID.	*/
    char	*si_revision_level;	/* The Inquiry revision level.	*/
    char	*si_device_id;		/* The device identifier.	*/
    char	*si_serial_number;	/* The device serial number.	*/
    char	*si_mgmt_address;	/* The mgmt network address.	*/
    /* Read Capacity Information */
    //uint64_t	si_device_capacity;	/* The device capacity.		*/
    //uint32_t	si_block_length;	/* The device block length.	*/
} scsi_information_t;

/*
 * Per Device I/O Parameters:
 */ 
typedef struct {
    scsi_opcode_t *sop;			/* The SCSI opcode data.	*/
    vendor_id_t	vendor_id;		/* The vendor ID (internal).	*/
    product_id_t product_id;		/* The product ID (internal).	*/
    uint8_t	device_type;		/* The Inquiry device type.	*/
    hbool_t	user_cdb_size;		/* User specified a CDB size.	*/
    hbool_t	cloned_device;		/* The primary device is cloned.*/
    hbool_t	disable_length_check;	/* Disable length check flag.	*/
    int		scale_count;		/* Scale max CDB blocks value.	*/
    uint64_t	cdb_blocks;		/* The blocks per CDB.		*/
    uint64_t	device_capacity;	/* The device capacity.		*/
    uint32_t	device_size;		/* The device size (block size) */
    uint32_t	data_length;		/* The original data length.	*/
    hbool_t	user_starting_lba;	/* User specified starting lba.	*/
    uint64_t	starting_lba;		/* The starting logical block.	*/
    uint64_t	ending_lba;		/* The ending logical block.	*/
    uint64_t	data_limit;		/* The maximum data to transfer.*/
    uint64_t	step_value;		/* The value to step after IO.	*/
    /* min/max/incr for variable blocks or ranges, etc */
    uint32_t	min_size;		/* The minimum size.		*/
    uint32_t	max_size;		/* The maximum size.		*/
    uint32_t	incr_size;		/* The increment size.		*/
    hbool_t	incr_variable;		/* Variable increment sizes.	*/
    hbool_t	user_min;		/* User specified min value.	*/
    hbool_t	user_max;		/* User specified max value.	*/
    hbool_t	user_increment;		/* User specified variable size	*/
    /* Context for loops: */
    hbool_t	first_time;		/* Flag to mark first time.	*/
    hbool_t	end_of_data;		/* End of data reached flag.	*/
    uint64_t	current_lba;		/* The current block number.	*/
    uint64_t	block_count;		/* The current block count.	*/
    uint64_t	block_limit;		/* Data transfer block limit.	*/
    uint64_t	blocks_transferred;	/* Request blocks transferred.	*/
    uint64_t	operations;		/* The SCSI operations executed.*/
    uint64_t	total_blocks;		/* The total blocks transferred.*/
    uint64_t	total_transferred;	/* Total data bytes transferred.*/
    /* Token based xcopy Information: */
    uint32_t	list_identifier;	/* The list identifier.		*/
    int		range_count;		/* The block range descriptors.	*/
    /* Context for xcopy segment descriptors: (non-token based xcopy) */
    uint64_t	segment_lba;		/* The segment logical block.	*/
    uint32_t	segment_blocks;		/* The segment blocks.		*/
    /* Restore these for looping! */
    uint64_t	saved_block_limit;	/* Data transfer block limit.	*/
    uint32_t	saved_data_length;	/* The original data length.	*/
    uint64_t	saved_cdb_blocks;	/* The original blocks/cdb.	*/
    uint64_t	saved_starting_lba;	/* The starting logical block.	*/
    uint64_t	saved_ending_lba;	/* The ending logical block.	*/
    uint32_t	saved_list_identifier;	/* The list identifier.		*/
    
    /* Slice Information: */
    uint64_t	slice_lba;		/* Slice logical block number.	*/
    uint64_t	slice_length;		/* The length of each slice.	*/
    uint64_t	slice_resid;		/* The residual slice blocks.	*/

    /* Xcopy Specific Parameters: */
    uint8_t	*designator_id;		/* The designator identifier.	*/
    int		designator_length;	/* The designator length.	*/
    int		designator_type;	/* The designator type.		*/
    
    /* Get LBA Status Parameters: */
    uint64_t	deallocated_blocks;	/* Deallocated blocks (holes).	*/
    uint64_t	mapped_blocks;		/* The mapped blocks (data).	*/
    uint64_t	total_lba_blocks;	/* Total LBA blocks processed.	*/

    /* Get Block Limits Parameters: */
    uint32_t	max_unmap_lba_count;	/* The maximum unmap lba count.	*/
    uint32_t	optimal_unmap_granularity; /* Optimal unmap granularity.*/
    uint64_t	max_write_same_len;	/* The maximum write same len.	*/
    uint16_t	max_range_descriptors;	/* The maximum range descriptors*/
    uint16_t	max_lba_per_range;	/* The max LBA's per range desc.*/
    
    /* Logical Block Provisioning Parameters: */
    /* These originate from Read Capacity(16) data. */
    hbool_t	lbpmgmt_valid;		/* Provisioning mgmt is valid.	*/
    hbool_t	lbpme_flag;		/* Provisioning mgmt enabled.	*/
					/*   True = Thin Provisioned	*/
					/*   False = Full Provisioned	*/
    hbool_t	lbprz_flag;		/* Provisioning read zeroes.	*/
    /* This is from Inquiry VPD 0xB2, Logical Block Provisioning Page. */
    uint8_t	provisioning_type;	/* The provisioning type.	*/
    hbool_t	warning_displayed;	/* Multiple warnings flag.	*/

    scsi_information_t *sip;		/* Various SCSI information.	*/

    /* per device SCSI pass-through data */
    scsi_generic_t sg;			/* The SCSI generic data.	*/
    tool_specific_t tool_specific;	/* Tool specific information.	*/
} io_params_t;

/*
 * The SCSI Device Information:
 */
typedef struct scsi_device {
    FILE	*efp;			/* Default error data stream.	*/
    FILE	*ofp;			/* Default output data stream.	*/
    HANDLE	data_fd;		/* The file data handle.	*/
    int		argc;			/* Argument count.              */
    char	**argv;			/* Argument pointer array.      */
    char	*cmdbufptr;		/* The command buffer pointer.	*/
    size_t	cmdbufsiz;		/* The command buffer size.	*/
    /*
     * Script File Information: 
     */
    int		script_level;		/* The script nesting level.	*/
    char	*script_name[ScriptLevels];
				    	/* The script names.		*/
    FILE	*sfp[ScriptLevels];
				    	/* The script file pointers.	*/
    int		script_lineno[ScriptLevels];
				    	/* The script file line number.	*/
    hbool_t	script_verify;		/* Flag to control script echo. */
    /*
     * Test Control Information:
     */
    hbool_t	async;			/* Execute asynchronously.	*/
    hbool_t	bypass;			/* Bypass sanity checks.	*/
    hbool_t	debug_flag;		/* Enable debug output flag.	*/
    hbool_t	DebugFlag;		/* The program debug flag.	*/
    hbool_t	tDebugFlag;		/* Timer (alarm) debug flag.	*/
    hbool_t	xDebugFlag;		/* Extra program debug flag.	*/
    int		status;			/* The SCSI IOCTL status.	*/
    ofmt_t      output_format;          /* The output format type.      */
    rfmt_t      report_format;		/* The output report format.	*/
    iomode_t	iomode;			/* The I/O mode (see above).	*/
    cmd_type_t  cmd_type;               /* The command type.            */
    cgs_type_t  cgs_type;               /* The clear/get/set type.      */
    spt_op_t    op_type;		/* The SCSI operation type.	*/
    hbool_t	tmf_flag;		/* Task Management Function.	*/
    hbool_t	decode_flag;		/* SCSI CDB decode control flag	*/
    hbool_t	encode_flag;		/* SCSI CDB encode control flag	*/
    onerr_control_t onerr;		/* Error control action type.	*/
    char	*din_file;		/* The data (in) file name.	*/
    char	*dout_file;		/* The data (out) file name.	*/
    uint32_t	abort_freq;		/* The abort frequency (per th)	*/
    uint32_t	abort_timeout;		/* The abort timeout to use.	*/
    uint32_t	dump_limit;		/* The amount of data to dump.	*/
    uint32_t	error_count;		/* The current error count.	*/
    uint32_t	retry_count;		/* The current retry count.	*/
    uint32_t	retry_limit;		/* The retry limit (wait for).	*/
    uint32_t	sleep_value;		/* Sleep value (in seconds).	*/
    uint32_t	msleep_value;		/* Millesecond (ms) sleep value	*/
    uint32_t	usleep_value;		/* Microsecond (us) sleep value	*/
    char	*scsi_name;		/* User defined SCSI name.	*/
    hbool_t	compare_data;		/* Compare data pattern.	*/
    hbool_t	genspt_flag;		/* Generate spt command flag.	*/
    hbool_t	image_copy;		/* Image copy flag (strict).	*/
    hbool_t	iot_pattern;		/* IOT test pattern selected.	*/
    hbool_t	json_pretty;		/* JSON pretty output control.	*/
    hbool_t	log_header_flag;	/* The log header control flag.	*/
    hbool_t	prewrite_flag;		/* Prewrite data blocks flag.	*/
    hbool_t	read_after_write;	/* The read after write flag.	*/
    hbool_t	sata_device_flag;	/* The SATA device flag.	*/
    hbool_t	scsi_info_flag;		/* The SCSI information flag.	*/
    hbool_t	sense_flag;		/* Display full sense flag.	*/
    hbool_t	unique_pattern;		/* Unique pattern per process.	*/
    hbool_t	user_data;		/* User defined data.		*/
    hbool_t	user_pattern;		/* User specified pattern.	*/
    uint32_t	iot_seed;		/* The default IOT seed value.  */
    uint32_t	iot_seed_per_pass;	/* The per pass IOT seed value.	*/
    uint32_t	pattern;		/* The 32-bit pattern to use.	*/
    void	*pattern_buffer;	/* The pattern buffer.		*/
    /* Page Control Information: */
    uint8_t     page_code;              /* The command page code.       */
    uint8_t     page_subcode;           /* The command page subcode.    */
    uint8_t	page_control;		/* The page control (Log Sense).*/
    uint8_t	page_control_field;	/* The page control field.	*/
    hbool_t     page_format;            /* Controls page format bit.    */
    hbool_t     page_code_valid;        /* Controls page code valid bit.*/
    hbool_t	page_specified;		/* User specified page code.	*/
    /* Parameter Data In/Out Information: */
    hbool_t	pin_data;		/* Parameter input data flag.	*/
    uint32_t	pin_length;		/* Parameter input data length.	*/
    void	*pin_buffer;		/* Paramater input data buffer.	*/
    hbool_t	user_sname;		/* User specified SCSI name.	*/
    hbool_t	verbose;		/* Controls verbose output.	*/
    hbool_t	verify_data;		/* Verify data after setting.	*/
    hbool_t	warnings_flag;		/* Controls warning messages.	*/
    hbool_t	emit_all;		/* Emit status for all cmds.	*/
    char	*emit_status;		/* The emit status string.	*/
    uint64_t	iterations;		/* The current iteration count.	*/
    uint64_t	random_seed;		/* Seed for random # generator.	*/
    uint64_t	repeat_count;		/* Repeat SCSI command count.	*/
    time_t	runtime;		/* The user specified runtime.	*/
    time_t	start_time;		/* The start time (in seconds).	*/
    time_t	end_time;		/* The end time (in seconds).	*/
    time_t	loop_time;		/* Time for last test loop.	*/
    clock_t	start_ticks;		/* Test start time (in ticks).	*/
    clock_t	end_ticks;		/* Test end time (in ticks).	*/
    struct timeval gtod;		/* Current GTOD information.    */
    struct timeval ptod;		/* Previous GTOD information.   */
    
    /* Keepalive Information: */
    time_t	keepalive_time;		/* Frequesncy of keepalives.	*/
    time_t	last_keepalive;		/* The last keepalive time.	*/
    char	*keepalive;		/* The keepalive string.	*/

    /* IOT Corruption Analysis Information: */
    hbool_t	dumpall_flag;		/* Controls dumping all blocks.	*/
    uint32_t	max_bad_blocks;		/* Maximum bad blocks to dump.	*/
    bfmt_t	boff_format;		/* Buffer offset data format.	*/
    dfmt_t	data_format;		/* Data display format.		*/

    /* Prefix String Information: (Note: Not currently implemented!) */
    char	*fprefix_string;	/* The formatted prefix string.	*/
    int		fprefix_size;		/* The formatted prefix size.	*/

    /* Recovery Parameters: (these get propagated to SCSI generic) */
    hbool_t	recovery_flag;		/* The recovery control flag.	*/
    uint32_t	recovery_delay;		/* The recovery delay (in secs)	*/
    uint32_t	recovery_limit;		/* The recovery retry limit.	*/

    /*
     * Logging Information:
     */
    hbool_t	joblog_inhibit;		/* Inhibit logging to job log.	*/
    hbool_t	logheader_flag;		/* The log file header flag.	*/
    hbool_t	log_opened;		/* Flag to tell log file open.	*/
    hbool_t	syslog_flag;		/* Log errors to syslog.	*/
    hbool_t	timestamp_flag;		/* Timestamp each data block.	*/
    hbool_t	unique_log;		/* Make the log file unique.	*/
    int		log_level;		/* The logging level.		*/
    int		sequence;		/* The sequence number.		*/
    char	*cmd_line;		/* Copy of our command line.	*/
    struct job_info *job;		/* Pointer to job information.	*/
    uint32_t	job_id;			/* The job indentifier.		*/
    char	*job_log;		/* The job log file name.	*/
    char	*job_tag;		/* The user defined job tag.	*/
    char	*log_file;		/* The log file name.		*/
    char	*log_format;		/* The log file format string.	*/
    char	*log_buffer;		/* Pointer to log file buffer.	*/
    char	*log_bufptr;		/* Pointer into log buffer.	*/
    char	*log_prefix;		/* The per line log prefix.	*/
    ssize_t	log_bufsize;		/* The log buffer size.		*/
    char	dir_sep;		/* The directory separator.	*/
    char	*file_sep;		/* The inter-file separator.	*/
    char	*file_postfix;		/* The file name postfix.	*/
    char        *date_sep;              /* The date field separator.    */
    char        *time_sep;              /* The time field separator.    */

    /*
     * Pack and Unpack Information:
     */
    char	*unpack_format;		/* The unpack format string.	*/
    bfmt_t	unpack_data_fmt;	/* The unpack data format.	*/
    
    /* Slice Information: */
    uint32_t	slices;			/* Number of slices to create.	*/
    uint32_t	slice_number;		/* Slice number to operate on.	*/

    /*
     * Thread Related Information:
     */
    int		threads;		/* The number of threads.	*/
    int		threads_active;		/* The number of active threads.*/
    pthread_t	thread_id;		/* The thread ID.		*/
    int		thread_number;		/* The thread number.		*/
    tstate_t	thread_state;		/* Our maintained thread state.	*/
    void	*(*thread_func)(void *arg); /* Thread function.	*/
    /*
     * SCSI Test Checks Information:
     */ 
    scsi_test_checks_t	tci;		/* Test checks information.	*/
    int		exp_radix;		/* The default exp data radix.	*/
    uint32_t	exp_data_count;		/* Current number of entries.	*/
    uint32_t	exp_data_entries;	/* The max number of entries.	*/
    size_t	exp_data_size;		/* Size of allocated data.	*/
    exp_data_t	*exp_data;		/* Expected input data checks.	*/
    /*
     * Extended Copy Information: (for PT/RRTI/WUT method) (used by MS)
     */ 
    char	*rod_token_file;	/* Extended copy token file.	*/
    hbool_t	rod_token_valid;	/* The ROD token is valid.	*/
    hbool_t	zero_rod_flag;		/* Zero ROD token control flag.	*/
    uint8_t	*rod_token_data;	/* Copy of ROD token data.	*/
    uint32_t	rod_token_size;		/* Size of ROD token data.	*/
    uint32_t	rod_inactivity_timeout;	/* The ROD inactivity timeout.	*/
    uint8_t	*rrti_data_buffer;	/* The RRTI data buffer.	*/
    uint32_t	rrti_data_length;	/* The RRTI data length.	*/
    int		segment_count;		/* The number of segments.	*/
    /*
     * Unmap and Punch Hole Information:
     */ 
    int		range_count;		/* The range descriptors.	*/

    /*
     * SCSI Generic Information:
     */
    int		(*sg_func)(scsi_generic_t *sgp); /* sg function.	*/
    /* Note: These are used during copy/verify operations. */
    scsi_io_type_t scsi_read_type;	/* The default read opcode.	*/
    uint32_t	scsi_read_length;	/* The default read length.	*/
    scsi_io_type_t scsi_write_type;	/* The default write opcode.	*/
    uint32_t	scsi_write_length;	/* The default write length.	*/
    
    /*
     * I/O Parameters:
     */ 
    int		io_devices;		/* The number of IO devices.	*/
    hbool_t	io_same_lun;		/* Multiple devices, same LUN.	*/
    hbool_t	io_multiple_sources;	/* Muletiple source devices.	*/
    io_params_t	io_params[MAX_DEVICES];	/* The I/O parameters.		*/

    /*
     * Storage Enclosure Services (SES) Parameters:
     */
    element_status_t ses_element_status;/* The element status.		*/
    element_type_t ses_element_type;	/* The element type.		*/
    hbool_t	ses_element_flag;	/* Element number specified.	*/
    int		ses_element_index;	/* The element index.		*/

    /*
     * Show Device Parameters:
     */
    hbool_t     show_caching_flag;      /* Show device caching flag.    */
    hbool_t	show_header_flag;    	/* Show devices header flag.    */
    scsi_filters_t scsi_filters;        /* The SCSI show filters.       */
    char	*show_fields;   	/* The show brief fields.       */
    char        *show_format;           /* The SCSI show format.        */
    char        *show_paths;            /* The show devices path(s).    */

    /*
     * Shared Library Parameters:
     */
    hbool_t     shared_library;         /* Shared library interface.    */
    struct scsi_device *master_sdp;     /* The master device pointer.   */
    char        *stderr_buffer;         /* The callers' stderr buffer.  */
    char        *stderr_bufptr;         /* Updated stderr buffer ptr.   */
    char        *stdout_buffer;         /* The callers' stdout buffer.  */
    char        *stdout_bufptr;         /* Updated stdout buffer ptr.   */
    char        *emit_status_buffer;    /* The emit status buffer.      */
    char        *emit_status_bufptr;    /* The emit status buffer ptr.  */
    int         stderr_length;          /* The stderr buffer length.    */
    int         stderr_remaining;       /* The stderr bytes remaining.  */
    int         stdout_length;          /* The stdout buffer length.    */
    int         stdout_remaining;       /* The stdout bytes remaining.  */
    int         emit_status_length;     /* The emit status buffer length*/
    int         emit_status_remaining;  /* The emit bytes remaining.    */
} scsi_device_t;

#if 0
typedef struct threads_info {
    int		ti_threads;	/* The number of active threads.	*/
    int		ti_finished;	/* The number of finished threads.	*/
    scsi_device_t **ti_sds;	/* Array of device info pointers.	*/
    int		ti_status;	/* Status from joined threads.		*/
} threads_info_t;

typedef struct job_info {
    struct job_info *ji_flink;	/* Forward link to next entry.  */
    struct job_info *ji_blink;	/* Backward link to prev entry. */
    pthread_mutex_t ji_job_lock;/* Per job lock.		*/
    job_id_t	ji_job_id;	/* The job identifier.		*/
    vjstate_t	ji_job_state;	/* The job state.		*/
    int		ji_job_status;	/* The job status.		*/
    char	*ji_job_tag;	/* The users job tag.		*/
    char	*ji_job_logfile;/* The job log file name.	*/
    FILE	*ji_job_logfp;	/* The job log file pointer.	*/
    time_t	ji_job_start;	/* The job start time.		*/
    time_t	ji_job_end;	/* The job end time.		*/
    pthread_mutex_t ji_print_lock;/* The job print lock.	*/
    threads_info_t *ji_tinfo;	/* The thread(s) information.	*/
    void        *ji_opaque;     /* Test specific opaque data.   */
} job_info_t;
#endif /* 0 */

/* Note: Temporary definition to compile until job control is added! */
typedef struct job_info {
    int		ji_job_id;
    char	*ji_job_tag;
    FILE	*ji_job_logfp;
} job_info_t;

extern FILE *efp, *ofp;
extern scsi_device_t *master_sdp;

extern clock_t hertz;

/*
 * External Declarations:
 */
/* spt.c */
extern char *OurName;
extern hbool_t DebugFlag, InteractiveFlag, mDebugFlag, PipeModeFlag;
extern volatile hbool_t CmdInterruptedFlag;
/* Functions used for parsing. */
extern int HandleExit(scsi_device_t *sdp, int status);
extern hbool_t match(char **sptr, char *s);
extern uint32_t number(scsi_device_t *sdp, char *str, int base);
extern uint64_t large_number(scsi_device_t *sdp, char *str, int base);
extern time_t mstime_value(scsi_device_t *sdp, char *str);
extern time_t time_value(scsi_device_t *sdp, char *str);

/* spt_inquiry.c */
extern int inquiry_encode(void *arg);
extern int inquiry_decode(void *arg);
extern int setup_inquiry(scsi_device_t *sdp, scsi_generic_t *sgp, size_t data_length, uint8_t page);
extern uint8_t find_inquiry_page_code(scsi_device_t *sdp, char *page_name, int *status);
extern char *GetDeviceType(uint8_t device_type, hbool_t full_name);
extern uint8_t GetDeviceTypeCode(scsi_device_t *sdp, char *device_type, int *status);
extern char *GetVendorSerialNumber(scsi_device_t *sdp, inquiry_t *inquiry);

/* scsi_opcodes.c */
extern int setup_rtpg(scsi_device_t *sdp, scsi_generic_t *sgp);
extern int setup_read_capacity10(scsi_device_t *sdp, scsi_generic_t *sgp);
extern int setup_read_capacity16(scsi_device_t *sdp, scsi_generic_t *sgp);
extern int setup_request_sense(scsi_device_t *sdp, scsi_generic_t *sgp);
extern void ShowScsiOpcodes(scsi_device_t *sdp, char *opstr);
extern void init_swapped(      scsi_device_t   *sdp,
			       void            *buffer,
			       size_t          count,
			       uint32_t        pattern );

/* spt_fmt.c */
extern int FmtEmitStatus(scsi_device_t *sdp, io_params_t *uiop, scsi_generic_t *usgp, char *format, char *buffer);
extern char *FmtString(scsi_device_t *sdp, char *format, hbool_t filepath_flag);
extern char *FmtUnpackString(scsi_device_t *sdp, char *format, unsigned char *data, size_t count);
/* Consolidated into one function now. */
#define FmtLogFile	FmtString
#define FmtFilePath	FmtString
#define FmtLogPrefix	FmtString

/* spt_mem.c */
extern void report_nomem(scsi_device_t *sdp, size_t bytes);

/* For memory tracing, need these inline! */
/* But that said, I'm leaving them for better performance! */

#if defined(INLINE_FUNCS)

#define Free(sdp,ptr)		free(ptr)
#define FreeMem(sdp,ptr,size)	\
  if (ptr) { memset(ptr, 0xdd, size); free(ptr); }
#define FreeStr(sdp,ptr)	\
  if (ptr) { memset(ptr, 0xdd, strlen(ptr)); free(ptr); }

INLINE void* Malloc(scsi_device_t *sdp, size_t bytes)
{
    void *ptr = malloc(bytes);
    if (ptr) {
	memset(ptr, '\0', bytes);
    } else {
	report_nomem(sdp, bytes);
    }
    return(ptr);
}

INLINE void *
Realloc(scsi_device_t *sdp, void *ptr, size_t bytes)
{
    ptr = realloc(ptr, bytes);
    if (ptr) {
	memset(ptr, '\0', bytes);
    } else {
	report_nomem(sdp, bytes);
    }
    return(ptr);
}
/* Older compilers require code inline as above! */
//extern __inline void *Malloc(scsi_device_t *sdp, size_t bytes);
//extern __inline void *Realloc(scsi_device_t *sdp, void *ptr, size_t bytes);

#else /* !defined(INLINE_FUNCS) */

extern void Free(scsi_device_t *sdp, void *ptr);
extern void FreeMem(scsi_device_t *sdp, void *ptr, size_t size);
extern void FreeStr(scsi_device_t *sdp, void *ptr);
extern void *Malloc(scsi_device_t *sdp, size_t bytes);
extern void *Realloc(scsi_device_t *sdp, void *bp, size_t bytes);

#endif /* defined(INLINE_FUNCS) */

extern void *malloc_palign(scsi_device_t *sdp, size_t bytes, int offset);
extern void free_palign(scsi_device_t *sdp, void *pa_addr);

/* scsi_opcodes.c */
extern int GetCapacity(scsi_device_t *sdp, io_params_t *iop);
extern int initialize_multiple_devices(scsi_device_t *sdp);
extern int sanity_check_src_dst_devices(scsi_device_t *sdp);
extern int initialize_slices(scsi_device_t *sdp);
extern void initialize_slice(scsi_device_t *sdp, scsi_device_t *tsdp);

/* spt_print.c */
#include "spt_print.h"

/* spt_iot.c */
extern uint32_t	init_iotdata(	scsi_device_t	*sdp,
				io_params_t	*iop,
				void		*buffer,
				uint32_t	count,
				uint32_t	lba,
				uint32_t	iot_seed);
extern void process_iot_data(scsi_device_t *sdp, io_params_t *iop, uint8_t *pbuffer, uint8_t *vbuffer, size_t bcount);
extern void analyze_iot_data(scsi_device_t *sdp, io_params_t *iop, uint8_t *pbuffer, uint8_t *vbuffer, size_t bcount);
extern void display_iot_data(scsi_device_t *sdp, io_params_t *iop, uint8_t *pbuffer, uint8_t *vbuffer, size_t bcount);
extern void display_iot_block(scsi_device_t *sdp, io_params_t *iop, int block, Offset_t block_offset, uint8_t *pptr, uint8_t *vptr, size_t bsize);
extern void report_bad_sequence(scsi_device_t *sdp, io_params_t *iop, int start, int length, Offset_t offset);
extern void report_good_sequence(scsi_device_t *sdp, io_params_t *iop, int start, int length, Offset_t offset);

/* spt_scsi.c */
extern void clone_scsi_information(io_params_t *iop, io_params_t *ciop);
extern void free_scsi_information(io_params_t *iop);
extern int get_scsi_information(scsi_device_t *sdp, io_params_t *iop);
extern int get_standard_scsi_information(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp);
extern int get_inquiry_information(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *csgp);
extern void report_scsi_information(scsi_device_t *sdp);
extern void report_standard_scsi_information(scsi_device_t *sdp, io_params_t *iop);

/* spt_ses.c */
extern int parse_ses_args(char *string, scsi_device_t *sdp);

/* spt_show.c */
extern int parse_show_devices_args(scsi_device_t *sdp, char **argv, int argc, int *arg_index);
extern int show_devices(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp);

/* spt_usage.c */
extern char *version_str;
extern void Usage(scsi_device_t *sdp);
extern void Version(scsi_device_t *sdp);
extern void Help(scsi_device_t *sdp);

/* utilities.c */
extern int DoSystemCommand(scsi_device_t *sdp, char *cmdline);
extern int StartupShell(scsi_device_t *sdp, char *shell);
extern void ReportCdbDeviceInformation(scsi_device_t *sdp, scsi_generic_t *sgp);
extern void ReportCdbScsiInformation(scsi_device_t *sdp, scsi_generic_t *sgp);
extern void ReportDeviceInformation(scsi_device_t *sdp, scsi_generic_t *sgp);
extern void ReportErrorInformation(scsi_device_t *sdp);
extern void ReportErrorMessage(scsi_device_t *sdp, scsi_generic_t *sgp, char *error_msg);
extern void ReportErrorTimeStamp(scsi_device_t *sdp);
extern void DisplayScriptInformation(scsi_device_t *sdp);
extern void CloseScriptFile(scsi_device_t *sdp);
extern void CloseScriptFiles(scsi_device_t *sdp);
extern int OpenScriptFile(scsi_device_t *sdp, char *file);
extern void DumpBuffer(	scsi_device_t	*sdp,
			char		*name,
            		unsigned char	*ptr,
		        size_t		bufr_size,
                        size_t          dump_limit );
extern int VerifyBuffers( scsi_device_t	*sdp,
			  unsigned char	*dbuffer,
			  unsigned char	*vbuffer,
			  size_t	count );
extern void dump_buffer ( scsi_device_t	*sdp,
			  char		*name,
			  unsigned char	*base,
			  unsigned char	*ptr,
			  size_t	dump_size,
			  size_t	bufr_size,
			  hbool_t	expected );
extern int open_file(scsi_device_t *sdp, char *file, open_mode_t open_mode, HANDLE *fd);
extern int close_file(scsi_device_t *sdp, HANDLE *fd);
extern int read_file(scsi_device_t *sdp, char *file, HANDLE fd, unsigned char *buffer, size_t length);
extern int write_file(scsi_device_t *sdp, char *file, HANDLE fd, unsigned char *buffer, size_t length);
extern int FormatElapstedTime(char *buffer, clock_t ticks);
extern int Fputs(char *str, FILE *stream);
extern hbool_t isHexString(char *s);

/* scsidata.c */
extern int parse_show_scsi_args(scsi_device_t *sdp, char **argv, int argc, int *arg_index);

/*
 * OS Specific Functions: (spt_unix.c and spt_win.c)
 * Note: These must be here since they reference the SCSI device structure! :-(
 */
extern char *os_getcwd(void);
extern uint64_t os_get_file_size(char *path, HANDLE handle);
extern hbool_t os_file_exists(char *file);
extern hbool_t os_isdir(char *dirpath);
extern hbool_t os_file_information(char *file, uint64_t *filesize, hbool_t *is_dir);
extern char *os_get_universal_name(char *drive_letter);
extern char *os_get_volume_path_name(char *path);
extern char *os_ctime(time_t *timep, char *time_buffer, int timebuf_size);
extern int os_set_priority(scsi_device_t *sdp, HANDLE hThread, int priority);
extern void tPerror(scsi_device_t *sdp, int error, char *format, ...);
extern void os_perror(scsi_device_t *sdp, char *format, ...);
extern uint64_t	os_create_random_seed(void);
