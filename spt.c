/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2016			    *
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
 * Command:	spt.c
 * Author:	Robin T. Miller
 * Date:	September 27th, 2006
 *
 * Description:
 *	This utility permits sending any SCSI command desired.
 *
 * Modification History:
 * 
 * March 24th, 2016 by Robin T. Miller
 * 	Fix bug in close_devices(), where the thread specific parameters
 * were no being setup properly. The master device was used instead, which
 * later caused us to blowup trying to print debug information when exiting.
 * 
 * March 7th, 2016 by Robin T. Miller
 * 	Move the tool specific information from the SCSI device structure to
 * to the per device data structure, so statistics are accurate for each device.
 * 
 * January 9th, 2016 by Robin T. Miller
 * 	Fix issue cloning the per device SCSI information.
 * 	Copy cdb string before using strtok() to avoid overwrites.
 * 
 * October 3rd, 2014 by Robin T. Miller
 * 	Modified SCSI generic opaque pointer to tool_specific information,
 * so we can pass more tool specific information (e.g. dt versus spt).
 * 
 * June 18th, 2014 by Robin T. Miller
 * 	Add default emit status strings, choose by emit={default|multi}
 * 	This default is being added to help with automation, since it's
 * difficult to define long emit status strings and/or the SPT_EMIT_STATUS
 * environment variable from remote requests.
 *
 * June 16th, 2014 by Robin T. Miller
 *	Allow a runtime of -1 to run forever! (popular request! :-)
 * 	Added IOT Corruption Analysis (CA) support and various options.
 * 	Added enable/disable={raw|read_after_write|read_immed} flag, to
 * enable read after write and optional verify, controlled by compare flag.
 * Note: The compare flag is enabled automatically with pattern/ptype opts.
 *
 * June 15th, 2014 by Robin T. Miller
 * 	Added parsing of pattern=iot, iotpass=, and iotseed= to match dt!
 * 	Ensure the slices= value is reset to 0 before parsing next command.
 * 	Ensure cdb_blocks is reset to 0, or subsequent commands may fail.
 *
 * June 12th, 2024 by Robin T. Miller
 * 	Ensure the recovery parameters get propagated to all devices/threads.
 * Previously the parameters were only set for the selected device, so when
 * doing copy operations with compare, recovery didn't occur for all devices.
 * Basically a false error occured, since a recoverable error was not retried!
 * 
 * January 14th, 2014 by Robin T. Miller
 * 	Add keepalive options and execution to thread loops.
 *
 * December 16th, 2013 by Robin T. Miller
 * 	Add thread log file support.
 * 	Don't to selective buffer duplication, based on async flag.
 * 	This trying to share read-only data is too difficult to manage!
 *
 * October 31st, 1013 by Robin T. Miller
 * 	Remove bypass check iwhen setting up the data length based on
 * the block size (bs=) option. Otherwise, an illegal request requests
 * if they have not specifed the data length.
 * Note: Previously, this bypass check existed for negative tests, but
 * that said, many folks are now using I/O parameters and bumping into
 * issues with certain bypass checks. Let's hope we don't break folks.
 * 
 * January 25th, 2013 by Robin T. Miller
 * 	Moved recovery parameters to the generic SCSI device structure
 * and moved the is retriable function to the common SCSI library. This
 * allows us to enable recovery for both SPT CDB's and library functions.
 * The recovery logic is a must for Linux so we can force path failovers.
 * 
 * December 17th, 2012 by Robin T. Miller
 * 	Added support for automatic error recovery.
 * 	The recovery can be OS specific, or common for SCSI Busy and
 * Unit Attention sense key. These are hardcoded (at present).
 * 
 * November 19th, 2012 by Robin T. Miller
 * 	Replace sleep/msleep/usleep with OS specific macros.
 * 	For *nix, the OS macros invoke poll() to be thread safe!
 *
 * November 13th, 2012 by Robin T. Miller
 * 	Added support for continuation lines and expanding env variables.
 * 	This enhancement is mainly for helping with script files.
 * 	Handle script and interactive end-of-file better (don't loop).
 *
 * November 9th, 2012 by Robin T. Miller
 * 	Added support for iomode={copy,mirror,test,verify} options.
 *
 * October 23rd, 2012 by Robin T. Miller
 * 	Added support for non-token based extended copy (xcopy) support.
 * 	This included major restructuring to support multiple device per thread.
 *
 * February 15th, 2012 by Robin T. Miller
 *	Ensure the proper exit status is maintained with multiple commands.
 * 	When executing script commands, on failures, close all script files.
 * 	Previously, we would continue executing script commands on errors.
 *
 * February 6th, 2012 by Robin T. Miller
 * 	When formatting the emit status string, allocate sufficient memory for
 * the max sense data length. Don't know this is a problem, but we could exceed
 * the previous length of 256 characters!
 * 
 * January 28th, 2012 by Robin T. Miller
 * 	When verifying expected data, provide a more user friendly format, and
 * verify all bytes, rather than stopping at the first mismatched data. Also,
 * when a failure is detected, provide a summary of the good and bad entries.
 * 
 * January 23rd, 2012 by Robin T. Miller
 * 	When expanding the %sense_data, include the sense length byte, otherwise
 * we are short (off by 1) byte in the emitted sense data.
 * 
 * January 19th, 2012 by Robin T. Miller
 * 	Dynamically adjust the expected data bytes during parsing, when we exceed
 * the initially allocated 64 entries. Also add exp_radix, to override the default
 * any radix, so one can set it to hex (for example) and avoid many 0x prefixes.
 * 
 * April 28th, 2011 by Robin T. Miller
 * 	Fix pthread*() API error handling. I assumed (wrongly) that these API's
 * returned -1 for an error, and set errno, but this is not the case. To be
 * thread safe, pthread API's return 0 on success, and the errno on errors!
 * 
 * May 7th, 2010 by Robin T. Miller
 * 	Restructure code in prepartion for multi-threading. Move global data
 * to either scsi_device or scsi_generic data structures.
 * 
 * May 16th, 2008 by Robin T. Miller
 *	Updated ReadDataFile() to do multiple reads in attempts to read
 * the data size specified, since data can get broken up reading pipes.
 *
 * September 11th, 2007 by Robin T. Miller
 *	Add native Windows support (see _WIN32 conditionalization).
 *	When doing a SCSI write, if a data in file is not specified, then
 * we allocate a null filled buffer to write (for ease of use).
 *
 * August 14th, 2007 by Robin T. Miller.
 *	Added support for Solaris head of HA queue type.
 *	Changed queue type default from noq to simple queue.
 *	Added reporting of OS specific host and driver status.
 *
 * August 10th, 2007 by Robin T. Miller
 *	If the dsf= is specified without an operation, enable interactive
 * mode (similar to pipe mode, except w/prompt and no emit status string).
 *
 * August 5th, 2007 by Robin T. Miller
 *	Added support for pipe mode for use with test scripts.
 *
 * August 1st, 2007 by Robin T. Miller
 *	Add support for user defined emit status string.
 *
 * March 21st, 2007 by Robin T. Miller.
 *	Fix improper free() in ExecuteCdb() detected on Linux.
 *
 * February 10th, 2007 by Robin T. Miller
 *      Added support for queue tag message types, and abort task set.
 *
 * November 18th, 2006 by Robin T. Miller
 *      Added support for pout='hh hh ...' option to specify parameter
 * data to send device. Note: When using this option, the data direction
 * is automatically set to "write" and the length is set from the number
 * of hex bytes specified.
 *
 */
#if defined(AIX)
# include "aix_workaround.h"
#endif /* defined(AIX) */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#if !defined(_WIN32)
#  include <limits.h>
#  include <pthread.h>
#  include <unistd.h>
#endif /* defined(_WIN32) */

#include "include.h"
#include "libscsi.h"
#include "inquiry.h"
#include "spt.h"

/*
 * Local Definitions:
 */
#define PROGRAM_DEBUG	"SPT_DEBUG"	/* Define this to enable debug.	*/
#define DEVICE_ENVNAME  "SPT_DEVICE"    /* SPT eevice env variable.     */
#define EMIT_STATUS_ENV	"SPT_EMIT_STATUS" /* The emit status env var.	*/
#define THREAD_STACK_ENV "SPT_THREAD_STACK_SIZE" /* Thread stack size.	*/
#define DEFAULT_MAX_THREADS	1024	/* The default maximum threads.	*/
/* Note: May switch to sysconf() API later, this is OS specific! */
#if !defined(PTHREAD_STACK_MIN)
#  define PTHREAD_STACK_MIN 16384
#endif
/* Increasing min stack required to avoid Linux seg fault in vfprintf()!*/
/* The pthread stack size is the minimum stack require to run a thread. */
/* Note: On 64-bit systems doubling the per thread size is required!    */
/* Although we know how much stack spt uses, the runtime libs we don't! */
//#define THREAD_STACK_SIZE	(PTHREAD_STACK_MIN*4) /* Smaller stacks	*/
                                                      /*  more threads! */
//                              /* Reduce TLS to avoid wasting swap/memory! */
/* Note: The value above is too small on some OS's, esp. 64-bit machines! */
#define THREAD_STACK_SIZE       MBYTE_SIZE      /* Same default as Windows! */

typedef struct threads_info {
    int		active_threads;	/* The number of active threads.	*/
    scsi_device_t *sds;		/* Array of SCSI device structs.	*/
    int		status;		/* Status from joined threads.		*/
} threads_info_t;

/*
 * Local Declarations:
 */
char    *OurName = NULL;                /* Our program name.            */
char	*sptpath;			/* Path to our executable.	*/
volatile hbool_t CmdInterruptedFlag;	/* User interrupted command.	*/ 
hbool_t DebugFlag = False;		/* The program debug flag.	*/
hbool_t	ExitFlag = False;		/* In pipe mode, exit flag.	*/
hbool_t InteractiveFlag = False;	/* Stay in interactive mode.	*/
hbool_t	mDebugFlag = False;		/* Memory related debug flag.	*/
hbool_t StdinIsAtty = TRUE;		/* Standard input isatty flag.	*/
hbool_t StdoutIsAtty = TRUE;		/* Standard output isatty flag.	*/
hbool_t	PipeModeFlag = False;		/* Pipe mode control flag.	*/

/* Note: These will become unique per thread w/log option! */
FILE	*efp;				/* Default error data stream.	*/
FILE	*ofp;				/* Default output data stream.	*/
uint32_t jobid = 1;			/* The job identifier.		*/

clock_t hertz;

scsi_device_t *master_sdp;		/* The parents' information.	*/

/*
 * Default keepalive message, if none is specified (user can override).
 */
char    *keepalive = "%dsf: %scsi_name, %operations ops, %total_bytes bytes, %iops iops, elapsed %elapsed_time" ;

char *pipe_emit = "%progname> ? %status %scsi_status %sense_code "
              "%sense_key %ascq %resid %xfer %host_status %driver_status";

/*
 * Default Emit Status Strings:
 */
char *emit_status_default="\n"
"                       Thread: %thread\n"
"                  Device Name: %dsf\n"
"                  Device Info: Block Length=%device_size, Capacity=%capacity\n"
"                  Block Range: %starting - %ending\n"
"                    SCSI Name: %scsi_name\n"
"                     SCSI CDB: %cdb\n"
"               Data Direction: %dir\n"
"                  Data Length: %length\n"
"                  Exit Status: %status = %status_msg\n"
"                  Host Status: %host_status = %host_msg\n"
"                Driver Status: %driver_status = %driver_msg\n"
"                  SCSI Status: %scsi_status = %scsi_msg\n"
"                   Sense Code: %sense_code = %sense_msg\n"
"                    Sense Key: %sense_key = %skey_msg\n"
"                      asc/asq: (%asc, %asq) = %ascq_msg\n"
"            Bytes Transferred: %xfer (data bytes transferred)\n"
"                     Residual: %resid (bytes not transferred)\n"
"                   Iterations: %iterations\n"
"                  Total Bytes: %total_bytes\n"
"                 Total Blocks: %total_blocks\n"
"             Total Operations: %total_operations\n"
"                   Sense Data: %sense_data\n"
"                 Elapsed Time: %elapsed_time\n"
"                Starting Time: %start_time\n"
"                  Ending Time: %end_time\n";

/*
 * Choose this string for xcopy and/or iomode=copy,verify etc.
 */
char *emit_status_multiple="\n"
"                       Thread: %thread\n"
"                Source Device: %src\n"
"                  Source Info: Block Length=%device_size, Capacity=%capacity\n"
"                  Block Range: %starting - %ending\n"
"           Destination Device: %dst\n"
"             Destination Info: Block Length=%device_size, Capacity=%capacity\n"
"                  Block Range: %starting - %ending\n"
"                    SCSI Name: %scsi_name\n"
"                     SCSI CDB: %cdb\n"
"               Data Direction: %dir\n"
"                  Data Length: %length\n"
"                  Exit Status: %status = %status_msg\n"
"                  Host Status: %host_status = %host_msg\n"
"                Driver Status: %driver_status = %driver_msg\n"
"                  SCSI Status: %scsi_status = %scsi_msg\n"
"                   Sense Code: %sense_code = %sense_msg\n"
"                    Sense Key: %sense_key = %skey_msg\n"
"                      asc/asq: (%asc, %asq) = %ascq_msg\n"
"            Bytes Transferred: %xfer (data bytes transferred)\n"
"                     Residual: %resid (bytes not transferred)\n"
"                   Iterations: %iterations\n"
"                  Total Bytes: %total_bytes\n"
"                 Total Blocks: %total_blocks\n"
"             Total Operations: %total_operations\n"
"                   Sense Data: %sense_data\n"
"                 Elapsed Time: %elapsed_time\n"
"                Starting Time: %start_time\n"
"                  Ending Time: %end_time\n";

/*
 * Forward References:
 */
void *a_job(void *arg);
int wait_for_threads(threads_info_t *tip);
void *a_cdb(void *arg);
void *a_tmf(void *arg);
int process_cdb_params(scsi_device_t *sdp);
int process_input_file(scsi_device_t *sdp);
int process_output_file(scsi_device_t *sdp);
hbool_t	is_retriable(scsi_generic_t *sgp);
int ExecuteCdb(scsi_device_t *sdp, scsi_generic_t *sgp);
static int VerifyExpectedData(scsi_device_t *sdp, unsigned char *buffer, size_t count);
static hbool_t check_expected_status(scsi_device_t *sdp, hbool_t report);
static int parse_args(scsi_device_t *sdp, int argc, char **argv);
static int parse_exp_data(char *str, scsi_device_t *sdp);
static int expand_exp_data(scsi_device_t *sdp);
static hbool_t match(char **sptr, char *s);
char *concatenate_args(scsi_device_t *sdp, int argc, char **argv, int arg_index);
void show_expression(scsi_device_t *sdp, uint64_t value);
static uint32_t number(scsi_device_t *sdp, char *str, int base);
static uint64_t large_number(scsi_device_t *sdp, char *str, int base);
static time_t mstime_value(scsi_device_t *sdp, char *str);
static time_t time_value(scsi_device_t *sdp, char *str);
int sptGetCommandLine(scsi_device_t *sdp);
int ExpandEnvironmentVariables(scsi_device_t *sdp, char *bufptr, size_t bufsiz);
int MakeArgList(scsi_device_t *sdp, char **argv, char *s);
int ReadDataFile(scsi_device_t *sdp, HANDLE fd, void **bufptr, size_t *lenptr);
int DoErrorControl(scsi_device_t *sdp, int status);

void init_devices(scsi_device_t *sdp);
void cleanup_devices(scsi_device_t *sdp, hbool_t master);
int open_devices(scsi_device_t *sdp);
int close_devices(scsi_device_t *sdp, int starting_index);
int clone_devices(scsi_device_t *sdp, scsi_device_t *tsdp);
#if defined(_WIN32)
void mark_devices_closed(scsi_device_t *sdp);
#endif /* defined(WIN32) */
void EmitStatus(scsi_device_t *sdp, char *status_string);
int HandleExit(scsi_device_t *sdp, int status);
void MyExit(scsi_device_t *sdp, int status);
void SignalHandler(int sig);
int do_post_processing(scsi_device_t *sdp, int status);
void do_sleeps(scsi_device_t *sdp);

int do_common_thread_startup(scsi_device_t *sdp);
int create_unique_thread_log(scsi_device_t *sdp);
int do_logfile_open(scsi_device_t *sdp);
void log_header(scsi_device_t *sdp);
char *make_options_string(scsi_device_t *sdp, int argc, char **argv, hbool_t quoting);
void save_cmdline(scsi_device_t *sdp);
static scsi_device_t *init_device_information(void);

void
init_devices(scsi_device_t *sdp)
{
    scsi_generic_t 	*sgp;
    io_params_t		*iop;
    tool_specific_t	*tsp;
    int			device_index;

    /*
     * Initialize initial device information.
     */
    for (device_index = 0; device_index < MAX_DEVICES; device_index++) {
	iop = &sdp->io_params[device_index];
	tsp = &iop->tool_specific;
	sgp = &iop->sg;

	iop->first_time		= True;
	sgp->fd			= INVALID_HANDLE_VALUE;
	sgp->afd		= INVALID_HANDLE_VALUE;
	sgp->debug		= False;
	sgp->dopen		= True;
	tsp->opaque		= sdp;
	tsp->params		= iop;
	tsp->execute_cdb	= (int (*)(void *, scsi_generic_t *))&ExecuteCdb;
	sgp->tsp		= tsp;
	sgp->qtag_type		= SG_SIMPLE_Q;
	sgp->data_dir		= scsi_data_none;
	sdp->dump_limit		= DumpLimitDefault;	/* Data dumped during miscompares. */
	sgp->errlog		= ErrorsFlagDefault;
	sgp->timeout		= ScsiDefaultTimeout;
	sgp->data_dump_limit	= sdp->dump_limit;	/* Data dumped during CDB errors.  */
	sgp->sense_length	= RequestSenseDataLength;
	/*
	 * Recovery Parameters:
	 */ 
	sdp->recovery_flag	= RecoveryFlagDefault;
	sdp->recovery_delay	= RecoveryDelayDefault;
	sdp->recovery_limit	= RecoveryRetriesDefault;
	/*
	 * Note: These need to be gleaned from Read Capacity!
	 */ 
	iop->device_type	= DTYPE_DIRECT;		/* Note: Should come from Inquiry! */
	iop->device_size	= BLOCK_SIZE;		/* Note: Should come from Get Capacity! */
    }
    return;
}

int
open_devices(scsi_device_t *sdp)
{
    scsi_generic_t 	*sgp, *bsgp = NULL;
    io_params_t		*iop;
    int			device_index;
    int			status = SUCCESS;

    for (device_index = 0; device_index < sdp->io_devices; device_index++) {
	iop = &sdp->io_params[device_index];
	sgp = &iop->sg;

	iop->first_time = True;
	if (bsgp) {
	    /* Propgate (some) base info to all devices. */
	    sgp->debug = bsgp->debug;
	    sgp->errlog = bsgp->errlog;
	}

	if (sgp->dsf && (sgp->fd == INVALID_HANDLE_VALUE)) {
	    if ( (status = os_open_device(sgp)) == FAILURE) {
		free(sgp->dsf);
		sgp->dsf = NULL; /* Avoid trying to open again! */
		break;
	    }
	}
	if (device_index == IO_INDEX_BASE) {
	    /* Save to propagate base information. */
	    bsgp = sgp;
	}
	if ( (sgp->sense_data == NULL) && sgp->sense_length) {
	    sgp->sense_data = malloc_palign(sdp, sgp->sense_length, 0);
	}
	if (sdp->scsi_info_flag == True) {
	    (void)get_scsi_information(sdp, iop);
	}
    }
    return (status);
}

int
close_devices(scsi_device_t *sdp, int starting_index)
{
    scsi_generic_t 	*sgp;
    io_params_t		*iop;
    int			device_index = starting_index;
    int			status = SUCCESS;

    for (; device_index < sdp->io_devices; device_index++) {
	iop = &sdp->io_params[device_index];
	sgp = &iop->sg;
	if (sgp->dsf && (sgp->fd != INVALID_HANDLE_VALUE)) {
	    if (iop->cloned_device) {
		sgp->fd = INVALID_HANDLE_VALUE;
	    } else {
		if (os_close_device(sgp) == FAILURE) {
		    status = FAILURE;
		}
	    }
	}
    }
    return (status);
}

void
cleanup_devices(scsi_device_t *sdp, hbool_t master)
{
    scsi_generic_t 	*sgp;
    io_params_t		*iop;
    int			device_index;

    /*
     * Free per device resource allocated.
     */
    for (device_index = 0; device_index < sdp->io_devices; device_index++) {
	iop = &sdp->io_params[device_index];
	sgp = &iop->sg;

	/* Initialize data, so we don't have stale for next command! */
	iop->sop		= NULL;
	iop->cdb_blocks		= 0;
	iop->device_capacity	= 0;
	//iop->device_size	= BLOCK_SIZE;
	iop->starting_lba	= 0;
	iop->ending_lba		= 0;
	iop->data_limit		= 0;
	iop->step_value		= 0;
	iop->min_size		= 0;
	iop->max_size		= 0;
	iop->incr_size		= 0;
	iop->incr_variable	= False;
	iop->user_min		= False;
	iop->user_max		= False;
	iop->user_increment	= False;
	iop->first_time		= True;
	iop->end_of_data	= False, 
	iop->current_lba	= 0;
	iop->block_count	= 0;
	iop->block_limit	= 0;
	iop->total_blocks	= 0;
	iop->total_transferred	= 0;
	iop->list_identifier	= 0;
	iop->range_count	= RangeCountDefault;
	iop->segment_lba	= 0;
	iop->segment_blocks	= 0;
	iop->slice_lba		= 0;
	iop->slice_length	= 0;
	iop->slice_resid	= 0;
	iop->naa_identifier	= 0;
	iop->naa_identifier_len	= 0;
	iop->deallocated_blocks	= 0;
	iop->mapped_blocks	= 0;
	iop->total_lba_blocks	= 0;
	iop->max_unmap_lba_count= 0;
	iop->max_write_same_len = 0;

	sgp->cdb_name		= "SCSI_CDB";
	sgp->cdb_size		= 0;
	sdp->iterations		= 0;
	sgp->data_dir		= scsi_data_none;
	sgp->data_length	= 0;
	sgp->errlog		= ErrorsFlagDefault;
	sgp->timeout		= ScsiDefaultTimeout;
	(void)memset(sgp->cdb, 0, MAX_CDB);

	/* Cloned means we don't need to release these resources. */
	if (iop->cloned_device) {
	    sgp->sense_data = NULL;
	    sgp->data_buffer = NULL;
	    iop->naa_identifier = NULL;
	    sgp->dsf = NULL;
	    sgp->adsf = NULL;
	}

	if (sgp->data_buffer) {
	    free_palign(sdp, sgp->data_buffer);
	    sgp->data_buffer = NULL;
	}
	if (iop->naa_identifier) {
	    free(iop->naa_identifier);
	    iop->naa_identifier = NULL;
	}
	if (master == False) {
	    if (sgp->dsf) {
		free(sgp->dsf);
		sgp->dsf = NULL;
	    }
	    if (sgp->adsf) {
		free(sgp->adsf);
		sgp->adsf = NULL;
	    }
	    if (sgp->sense_data) {
		free_palign(sdp, sgp->sense_data);
		sgp->sense_data = NULL;
	    }
	}
	if (iop->sip) {
	    free_scsi_information(iop);
	}
    } /* end of for (device_index = 0;... */

    /*
     * Free resources duplicated for all threads.
     */
    /* Note: Special handling for master, since we keep the device open! */
    if (master == False) {
	if (sdp->cmd_line) {
	    free(sdp->cmd_line);
	    sdp->cmd_line = NULL;
	}
	if (sdp->exp_data) {
	    free(sdp->exp_data);
	    sdp->exp_data = NULL;
	}
	if (sdp->emit_status) {
	    free(sdp->emit_status);
	    sdp->emit_status = NULL;
	}
	if (sdp->keepalive) {
	    free(sdp->keepalive);
	    sdp->keepalive = NULL;
	}
	if (sdp->log_prefix) {
	    free(sdp->log_prefix);
	    sdp->log_prefix = NULL;
	}
    }
    if (sdp->pin_buffer) {
	free_palign(sdp, sdp->pin_buffer);
	sdp->pin_buffer = NULL;
    }
    if (sdp->user_sname) {
	free(sdp->scsi_name);
	sdp->scsi_name = NULL;
    }
    sdp->scsi_name = NULL;
    if (sdp->pattern_buffer) {
	free_palign(sdp, sdp->pattern_buffer);
	sdp->pattern_buffer = NULL;
    }
    if (sdp->rrti_data_buffer) {
	free_palign(sdp, sdp->rrti_data_buffer);
	sdp->rrti_data_buffer = NULL;
    }
    if (sdp->rod_token_data) {
	free(sdp->rod_token_data);
	sdp->rod_token_data = NULL;
	sdp->rod_token_valid = False;
    }
    /* Note: Close the log file *after* freeing everything else. */
    if (sdp->log_file) {
	if (sdp->log_opened) {
	    (void)fclose(sdp->efp);
	    sdp->log_opened = False;
	}
	Free(sdp, sdp->log_file);
	sdp->log_file = NULL;
    }
    return;
}

/*
 * clone_devices() - Clone master SCSI device for thread devices.
 * 
 * Inputs:
 * 	sdp = The master SCSI device to be cloned.
 * 	tsdp = The thread SCSI device being created.
 *
 * Note: A structure copy has already been done before calling us!
 * 
 * Return Value:
 * 	Returns SUCCESS / FAILURE.
 */
int
clone_devices(scsi_device_t *sdp, scsi_device_t *tsdp)
{
    scsi_generic_t 	*sgp, *tsgp;
    io_params_t		*iop, *tiop, *biop = &sdp->io_params[IO_INDEX_BASE];
    tool_specific_t	*tsp;
    int			device_index;
    int			status = SUCCESS;

    /*
     * Clone the per device information first.
     */
    for (device_index = 0; device_index < sdp->io_devices; device_index++) {
	iop = &sdp->io_params[device_index];
	sgp = &iop->sg;
	tiop = &tsdp->io_params[device_index];
	tsp = &tiop->tool_specific;
	tsgp = &tiop->sg;

	tsp->opaque	    = tsdp;
	tsp->params	    = tiop;
	tsp->execute_cdb    = (int (*)(void *, scsi_generic_t *))&ExecuteCdb;
	tsgp->tsp	    = tsp;
	
	/* Propagate recovery parameters to all devices/threads. */
	tsgp->recovery_flag  = tsdp->recovery_flag;
	tsgp->recovery_delay = tsdp->recovery_delay;
	tsgp->recovery_limit = tsdp->recovery_limit;

	/* Propogate base SCSI operation to all devices, esp. for xcopy. */
	/* Note: For iomode w/slices, destination sop is already setup! */
	if (!tiop->sop) {
	    tiop->sop = biop->sop;
	}

	/*
	 * Ensure each thread gets its' own data/sense buffers.
	 */
	if (tsgp->data_length) {
	    tsgp->data_buffer = malloc_palign(sdp, tsgp->data_length, 0);
	    if ( (sgp->data_dir == scsi_data_write) && sgp->data_buffer) {
		memcpy(tsgp->data_buffer, sgp->data_buffer, (size_t)sgp->data_length);
	    }
	}
	tsgp->sense_data = malloc_palign(sdp, tsgp->sense_length, 0);

	tsgp->dsf = strdup(sgp->dsf);
	if (sgp->adsf) {
	    tsgp->adsf = strdup(sgp->adsf);
	}
	if (sgp->dsf && (sgp->fd != INVALID_HANDLE_VALUE)) {
	    /* TODO: To cleanup, consider open(s) in each thread! */
#if defined(WIN32)
	    /* This handle won't work from a thread, need to research why! */
	    //tsgp->fd = win32_dup(sgp->fd);
	    tsgp->fd = INVALID_HANDLE_VALUE;	/* Force new open in thread. */
#else /* !defined(WIN32) */
	    tsgp->fd = dup(sgp->fd);		/* Can we share the same fd? */
	    if (tsgp->fd == INVALID_HANDLE_VALUE) {
		status = FAILURE;
	    }
#endif /* defined(WIN32) */
	}
	/*
	 * If we clone the base device, we must copy its' SCSI information.
	 */
	if (biop->cloned_device == True) {
	    clone_scsi_information(biop, tiop);
	} else {
	    clone_scsi_information(iop, tiop);
	}
    } /* end for (device_index = 0; ... */
    
    /* 
     * Clone the SCSI device information shared by threads.
     */
    if (sdp->cmd_line) {
	tsdp->cmd_line = strdup(sdp->cmd_line);
    }
    if (sdp->exp_data) {
	tsdp->exp_data = Malloc(sdp, tsdp->exp_data_size);
	memcpy(tsdp->exp_data, sdp->exp_data, sdp->exp_data_size);
    }
    if (sdp->pin_length) {
	tsdp->pin_buffer = malloc_palign(sdp, tsdp->pin_length, 0);
	memcpy(tsdp->pin_buffer, sdp->pin_buffer, sdp->pin_length);
    }
    if (sdp->emit_status) {
	tsdp->emit_status = strdup(sdp->emit_status);
    }
    if (sdp->keepalive) {
	tsdp->keepalive = strdup(sdp->keepalive);
    }
    if (sdp->user_sname) {
	tsdp->scsi_name = strdup(sdp->scsi_name);
    }
    if (sdp->log_file) {
	tsdp->log_file = strdup(sdp->log_file);
    }
    if (sdp->log_prefix) {
	tsdp->log_prefix = strdup(sdp->log_prefix);
    }

    /*
     * Duplicate any other device specific information.
     */
    iop = &sdp->io_params[IO_INDEX_BASE];
    sgp = &iop->sg;
    if ( (sgp->data_dir == scsi_data_read) && sgp->data_length && (sdp->compare_data || sdp->user_pattern)) {
	/* Note: Not used in the case of pin= option, but simplifies logic! */
	tsdp->pattern_buffer = malloc_palign(sdp, tsgp->data_length, 0);
	InitBuffer(tsdp->pattern_buffer, (size_t)tsgp->data_length, tsdp->pattern);
    }
    return (status);
}

#if defined(_WIN32)

void
mark_devices_closed(scsi_device_t *sdp)
{
    scsi_generic_t 	*sgp;
    io_params_t		*iop;
    int			device_index;

    for (device_index = 0; device_index < sdp->io_devices; device_index++) {
	iop = &sdp->io_params[device_index];
	sgp = &iop->sg;
	if (sgp->dsf && (sgp->fd != INVALID_HANDLE_VALUE)) {
	    sgp->fd = INVALID_HANDLE_VALUE;
	}
    }
    return;
}
#endif /* defined(WIN32) */
    
/*
 * Note: This function is used for emit status and keepalive format/display.
 */
void
EmitStatus(scsi_device_t *sdp, char *status_string)
{
    /*
     * If an emit statusi/keepalive string was specified, format and display it.
     */
    if (status_string) {
	char *efmt_buffer = Malloc(sdp, EMIT_STATUS_BUFFER_SIZE);
	if (efmt_buffer == NULL) return;
	(void)FmtEmitStatus(sdp, NULL, NULL, status_string, efmt_buffer);
	strcat(efmt_buffer, "\n");
	PrintLines(sdp, efmt_buffer);
	(void)fflush(stdout);
	free(efmt_buffer);
    }
    return;
}

void
MyExit(scsi_device_t *sdp, int status)
{
    if (sdp && ( (sdp->debug_flag || sdp->DebugFlag) && (status != SUCCESS) ) ) {
	Printf(sdp, "Exiting with status code %d...\n", status);
    }
    exit(status);
}

int
HandleExit(scsi_device_t *sdp, int status)
{
    /*
     * Commands like "help" or "version" will cause scripts to exit,
     * but we don't wish to continue on fatal errors, so...
     */
    //if (InteractiveFlag || PipeModeFlag) {
    if (InteractiveFlag || PipeModeFlag || sdp->script_level) {
	if (sdp->script_level && (status == FAILURE)) {
	    MyExit(sdp, status);	/* Rethink this, it's too messy! */
	}
    } else {
	MyExit(sdp, status);
    }
    return (status);
}

void
SignalHandler(int sig)
{
    /*
     * If we've already set this flag and we're coming through here
     * again, then exit this time.  This is necessary, since some
     * syscalls restart on an interrupt, so we don't wish to hang.
     */
    if (CmdInterruptedFlag) {
	MyExit(master_sdp, FATAL_ERROR);
    }
    CmdInterruptedFlag = True;
    return;
}

/*
 * do_post_processing() - Do Command Post Processing.
 *
 * Inputs:
 *	sdp = The SCSI device information.
 *	status = The last command status.
 *
 * Return Value:
 *	Returns 1 / -1 = Continue / Stop.
 */
int
do_post_processing(scsi_device_t *sdp, int status)
{
    int estatus = DoErrorControl(sdp, status);
    if (estatus != CONTINUE) return (estatus);
    do_sleeps(sdp);
    return (estatus);
}

void
do_sleeps(scsi_device_t *sdp)
{
    if (sdp->sleep_value) (void)os_sleep(sdp->sleep_value);
    if (sdp->msleep_value) (void)os_msleep(sdp->msleep_value);
    if (sdp->usleep_value) (void)os_usleep(sdp->usleep_value);
    return;
}

/*
 * main() - We Start Here!
 *
 * TODO:
 *  Add multiple threads.				(done)
 *  Add SCSI device type (default to direct access).	(done)
 *  - detect Inquiry, set device type accordingly.	(todo)
 *  Add SCSI opcode lookup table.			(done)
 *  - don't forget CDB's requiring subcodes!		(partial)
 *  Add pattern= option to initialize I/O buffers.	(done)
 *  - once added, add pattern verification (compare).	(done)
 *  Add expect/wait support like any_scsi tool has.	(done)
 *  - SCSI status, Sense Key, asc/ascq values.		(done)
 *  - residual count, transfer count			(done)
 *  Add async CDB's					(done)
 *  - consider adding "jobs" tracking/killing control.
 *  Add pin= option to verify parameter input data.	(done)
 *  Add expect= option to verify data with offset/bytes.(done)
 *  Add alias command for CDB short hands (tur, etc).
 *  Add startup script (.sptrc) file.
 *  When Ctrl/C typed to interrupt, cancel/kill threads.
 *  Allow multiple DSF's.				(done)
 *  Add script input.					(done)
 *  Add timing flag.
 *  Add support for CDB layout so keywords can be specified.
 *  Add support for output data layout for formatting data.
 *  Add time options (similar to Scu commands).
 *  Consider RPC API so server/GUI can send spt commands.
 *  - RPC to spt, MPI between spt's on each test client.
 *  Handle ENOMEM from memory allocation functions!
 * 
 *  Consider any_iscsi code, to test without host DSF's.
 *  - this also provides TMF capability, tag control, etc
 */
int
main(int argc, char **argv)
{
    char        	*p;
    pthread_attr_t	attr;
    pthread_attr_t	*attrp = &attr;
    threads_info_t	*tip;
    scsi_device_t 	*sdp, *tsdp;
    scsi_device_t 	*sds;
    scsi_generic_t 	*sgp, *tsgp;
    scsi_addr_t		*sap;
    io_params_t		*iop, *tiop;
    hbool_t		FirstTime = True;
    int         	thread, pstatus, status = SUCCESS;
#if !defined(_WIN32)
    size_t		currentStackSize = 0;
    size_t		desiredStackSize = THREAD_STACK_SIZE;
#endif

    efp = stderr;
    ofp = stdout;
    
    /* TODO: Parameterize and move to OS specific file! */
#if defined(__unix)
    hertz = sysconf(_SC_CLK_TCK);
#else /* !defined(__unix) */
    hertz = CLK_TCK;			/* Note: May be a libc function. */
#endif /* defined(__unix) */
    
    sptpath = argv[0];
    p = strrchr(argv[0], '/');
    OurName = p ? &(p[1]) : argv[0];

    argc--; argv++; /* Skip our program name. */

    if (argc == 0) {
	InteractiveFlag = True;
    }

    sdp = master_sdp = init_device_information();

    iop = &sdp->io_params[IO_INDEX_BASE];
    sgp = &iop->sg;
    sap = &sgp->scsi_addr;

    /*
     * strdup() used to copy so free() is possible later (if necessary).
     */
    if (p = getenv(DEVICE_ENVNAME)) {
	sgp->dsf = strdup(p);
    }
    if (p = getenv(EMIT_STATUS_ENV)) {
	sdp->emit_status = strdup(p);
    }
    if (p = getenv(PROGRAM_DEBUG)) {
	DebugFlag = True;
    }
    if (p = getenv(THREAD_STACK_ENV)) {
	char *string = p;
#if !defined(_WIN32)
	desiredStackSize = number(sdp, string, ANY_RADIX);
#endif
    }

    /*
     * Allow user to specify path to send the SCSI command to.
     * Note: Only supported on AIX with MPIO (at present time).
     */
    sap->scsi_path	  = -1;    /* Indicates no path specified. (for AIX) */

    sdp->argc		  = argc;
    sdp->argv		  = argv;
    sdp->bypass		  = BypassFlagDefault;
    sdp->data_fd	  = INVALID_HANDLE_VALUE;
    iop->device_type	  = DTYPE_DIRECT;	/* Note: Should come from Inquiry! */
    iop->device_size	  = BLOCK_SIZE;		/* Note: Should come from Get Capacity! */
    sdp->dump_limit	  = DumpLimitDefault;	/* Data dumped during miscompares. */
    sgp->data_dump_limit  = sdp->dump_limit;	/* Data dumped during CDB errors.  */
    sdp->exp_radix	  = ANY_RADIX;
    sdp->exp_data_entries = EXP_DATA_ENTRIES;
    sdp->exp_data_size	  = (sizeof(exp_data_t) * sdp->exp_data_entries);
    sdp->log_header_flag  = LogHeaderFlagDefault;
    sdp->read_after_write = ReadAfterWriteDefault;
    sdp->prewrite_flag	  = PreWriteFlagDefault; /* Controls CAW data prewrites. */
    sdp->scsi_info_flag   = ScsiInformationDefault;
    sdp->sense_flag 	  = SenseFlagDefault;
    sdp->verbose	  = VerboseFlagDefault;
    sdp->warnings_flag	  = WarningsFlagDefault;
    /* IOT Corruption Analysis Defaults: */
    sdp->dumpall_flag	  = False;
    sdp->max_bad_blocks	  = MAXBADBLOCKS;
    sdp->boff_format	  = HEX_FMT;
    sdp->data_format	  = NONE_FMT;

    /*
     * SCSI Read Verify Information: (used for xcopy and copy/mirror/verify ops)
     */ 
    sdp->scsi_read_type	   = ScsiReadTypeDefault;
    sdp->scsi_read_length  = ScsiReadLengthDefault;
    sdp->scsi_write_type   = ScsiWriteTypeDefault;
    sdp->scsi_write_length = ScsiWriteLengthDefault;
    
    init_devices(sdp);
    tsgp = NULL;

    (void)signal(SIGINT, SignalHandler);
    (void)signal(SIGTERM, SignalHandler);

    StdinIsAtty = isatty(fileno(stdin));
    StdoutIsAtty = isatty(fileno(stdout));

    status = pthread_attr_init(attrp);
    if (status != SUCCESS) {
	errno = status;
	Perror(sdp, "pthread_attr_init() failed");
	MyExit(sdp, FATAL_ERROR);
    }

    status = pthread_attr_setscope(attrp, PTHREAD_SCOPE_SYSTEM);
    if (status != SUCCESS) {
	errno = status;
	Perror(sdp, "pthread_attr_setscope() failed setting PTHREAD_SCOPE_SYSTEM");
	MyExit(sdp, FATAL_ERROR);
    }

#if !defined(WIN32)
    status = pthread_attr_getstacksize(attrp, &currentStackSize);
    if (status == SUCCESS) {
	if (DebugFlag) {
	    Printf(sdp, "Current thread stack size is %u\n", currentStackSize);
	}
    } else {
	if (DebugFlag) {
	    errno = status;
	    Perror(sdp, "pthread_attr_getstacksize() failed!");
	}
    }

    /*
     * The default stack size on Linux is 10M, which limits the threads created! 
     */
    if (currentStackSize && desiredStackSize && (currentStackSize > desiredStackSize) ) {
	/* Too small and we seg fault, too large limits the number of threads. */
	status = pthread_attr_setstacksize(attrp, desiredStackSize);
	if (status == SUCCESS) {
	    if (DebugFlag) {
		Printf(sdp, "Thread stack size set to %u bytes\n", desiredStackSize);
	    }
	} else {
	    errno = status;
	    Perror(sdp, "pthread_attr_setstacksize() failed setting stack size %u",
		   desiredStackSize);
	}
    }
#endif /* defined(_WIN32) */

    (void)initialize_print_lock(sdp);

    /*
     * Loop once or many times in pipe mode.
     */
    do {
	sdp->abort_freq		= 0;
	sdp->abort_timeout	= AbortDefaultTimeout;
	sdp->async		= False;
	sdp->emit_all		= False;
	sdp->decode_flag	= False;
	sdp->encode_flag	= False;
	sdp->onerr		= ONERR_STOP;
	sdp->sleep_value	= 0;
	sdp->msleep_value	= 0;
	sdp->usleep_value	= 0;
	sdp->error_count	= 0;
	sdp->repeat_count	= RepeatCountDefault;
	sdp->retry_count	= 0;
	sdp->retry_limit	= RetryLimitDefault;
	sdp->zero_rod_flag	= False;
	sdp->runtime		= 0;
	sdp->din_file		= NULL;
	sdp->dout_file		= NULL;
	sdp->rod_token_file	= NULL;
	sdp->iomode		= IOMODE_TEST;
	sdp->op_type		= UNDEFINED_OP;
	memset(&sdp->tci, 0, sizeof(sdp->tci));
	sdp->tci.exp_scsi_status = SCSI_GOOD;
	sdp->exp_data_count	= 0;
	sdp->pin_data		= False;
	sdp->pin_length		= 0;
	sdp->slices		= 0;
	sdp->threads		= ThreadsDefault;
	sdp->user_data		= False;
	sdp->user_pattern	= False;
	sdp->compare_data	= CompareFlagDefault;
	sdp->image_copy		= ImageModeFlagDefault;
	sdp->iot_seed		= IOT_SEED;
	sdp->iot_pattern	= False;
	sdp->range_count	= RangeCountDefault;
	sdp->segment_count	= SegmentCountDefault;
	sdp->unique_pattern	= UniquePatternDefault;
	sdp->io_devices		= 1;
	sdp->io_same_lun	= False;
	sdp->io_multiple_sources = False;
        CmdInterruptedFlag 	= False;

	if (FirstTime) {
	    /* Parse command line options first! */
	    FirstTime = False;
	} else {
	    if ( (pstatus = sptGetCommandLine(sdp)) != SUCCESS) {
		if (pstatus == END_OF_FILE) {
		    ExitFlag = True;
		} else if (pstatus == FAILURE) {
		    status = pstatus;
		}
		continue;
	    }
	    if (sdp->argc <= 0) continue;
	}

	/*
	 * Parse the arguments.
	 */
	if ( (pstatus = parse_args(sdp, sdp->argc, sdp->argv)) != SUCCESS) {
	    HandleExit(sdp, pstatus);
	    continue;
	}

	/*
	 * Allow sleeps without an operation, until job control implemented. 
	 */
	if ( (sdp->op_type == UNDEFINED_OP) &&
	     (sdp->sleep_value || sdp->msleep_value || sdp->usleep_value) ) {
	    do_sleeps(sdp);
	    continue; /* reprompt! */
	}

	/*
	 * Interactive or pipe mode, prompt for more options if device
	 * or operation type not specified.  Allows "spt enable=pipes"
	 */
	if ( ((sgp->dsf == NULL) && (sdp->op_type == UNDEFINED_OP)) &&
	     (InteractiveFlag || PipeModeFlag || sdp->script_level) ) {
	    continue; /* reprompt! */
	}

	if (sgp->dsf == NULL) {
	    Wprintf(sdp, "Please specify a device special file via dsf= option!\n");
	    (void)HandleExit(sdp, WARNING);
	    continue;
	}
	
	/* Propagate recovery parameters to SCSI generic fields. */
	sgp->recovery_flag = sdp->recovery_flag;
	sgp->recovery_delay = sdp->recovery_delay;
	sgp->recovery_limit = sdp->recovery_limit;

#if defined(_AIX)
	/*
	 * Task Management Functions are issued through the adapter.
	 *
	 * Note: We open the hdisk even when enable=adapter is specified,
	 * otherwise all commands complete with Unit Attention (PRLOGIN
	 * is the same as a reset).  May wish to change for interactive!
	 */
	if ( /*AdapterFlag ||*/ (sdp->op_type != SCSI_CDB_OP) ) {
	    ;
	} else if ( sgp->dopen ) {
#endif /* defined(_AIX) */
	    /* Open all devices. */
	    if (open_devices(sdp) == FAILURE) {
		(void)HandleExit(sdp, FAILURE);
		continue;
	    }
#if defined(_AIX)
	}
#endif /* defined(_AIX) */

	switch (sdp->op_type) {
	    
	    case UNDEFINED_OP:
		if (InteractiveFlag || PipeModeFlag || sdp->script_level) {
		    continue;
		} else {
		    Wprintf(sdp, "Please specify an operation to perform!\n");
		    (void)HandleExit(sdp, WARNING);
		    continue;
		}
		break;

	    case ABORT_TASK_SET_OP:
		sdp->tmf_flag = True;
		sdp->thread_func = &a_tmf;
		sdp->sg_func = &os_abort_task_set;
		break;

	    case BUS_RESET_OP:
		sdp->tmf_flag = True;
		sdp->thread_func = &a_tmf;
		sdp->sg_func = &os_reset_bus;
		break;

	    case LUN_RESET_OP:
		sdp->tmf_flag = True;
		sdp->thread_func = &a_tmf;
		sdp->sg_func = &os_reset_lun;
		break;

	    case TARGET_RESET_OP:
		sdp->tmf_flag = True;
		sdp->thread_func = &a_tmf;
		sdp->sg_func = &os_reset_device;
		break;

	    case SCSI_CDB_OP:
		sdp->tmf_flag = False;
		sdp->thread_func = &a_cdb;
		pstatus = process_cdb_params(sdp);
		if (pstatus != SUCCESS) {
		    status = pstatus;
		    continue;
		}
		break;

	    default:
		Eprintf(sdp, "Unsupported operation type %d!\n", sdp->op_type);
		(void)HandleExit(sdp, FAILURE);
		continue;
	}

	save_cmdline(sdp);

	pstatus = pthread_attr_setdetachstate(attrp, PTHREAD_CREATE_JOINABLE);
	if (pstatus != SUCCESS) {
	    errno = status;
	    Perror(sdp, "pthread_attr_setdetachstate() failed setting PTHREAD_CREATE_JOINABLE");
	    MyExit(sdp, FATAL_ERROR);
	}
	
	/*
	 * When doing extended copy, ensure sanity check the src/dst LUNs.
	 */ 
	if ( (sdp->op_type == SCSI_CDB_OP) &&
	     ((sgp->cdb[0] == SOPC_EXTENDED_COPY) && (sdp->io_devices == XCOPY_MIN_DEVS)) ) {
	    status = sanity_check_src_dst_devices(sdp);
	    if (status != SUCCESS) {
		(void)HandleExit(sdp, FAILURE);
		continue;
	    }
	} else if ( (sdp->iomode != IOMODE_TEST) && (sdp->io_devices > 1) ) {
	    status = initialize_multiple_devices(sdp);
	    if (status != SUCCESS) {
		(void)HandleExit(sdp, FAILURE);
		continue;
	    }
	}

	if (sdp->slices && sdp->encode_flag) {
	    status = initialize_slices(sdp);
	    if (status != SUCCESS) {
		(void)HandleExit(sdp, FAILURE);
		continue;
	    }
	    if (sdp->threads > 1) {
		Wprintf(sdp, "The slices option (%u) overrides the threads (%d) specified!\n",
		       sdp->slices, sdp->threads);
	    }
	    sdp->threads = sdp->slices;
	}

	/*
	 * Ok, execute the command via thread(s), and wait for their results.
	 */
	sdp->threads_active = 0;
	sdp->job_id = jobid++;	/* Note: Temporary, until full job control! */
	sds = (scsi_device_t *)Malloc(sdp, (sizeof(*sdp) * sdp->threads) );

	for (thread = 0; (thread < sdp->threads); thread++) {
	    tsdp = &sds[thread];
	    /*
	     * Copy original information for each thread instance.
	     */
	    *tsdp = *sdp;
	    tiop = &tsdp->io_params[IO_INDEX_BASE];
	    tsgp = &tiop->sg;
	    /*
	     * Attempt SCSI aborts via a short timeout (if enabled).
	     */ 
	    if (sdp->abort_freq) {
		if (thread % sdp->abort_freq) {
		    tsgp->timeout = sdp->abort_timeout;
		}
	    }

	    (void)clone_devices(sdp, tsdp);
	    tsdp->thread_number = (thread + 1);

	    if (sdp->slices) {
		initialize_slice(sdp, tsdp);
	    }

	    pstatus = pthread_create( &tsdp->thread_id, attrp, sdp->thread_func, tsdp );
	    /*
	     * Expected Failure:
	     * EAGAIN Insufficient resources to create another thread, or a
	     * system-imposed limit on the number of threads was encountered.
	     * Linux: The kernel's system-wide limit on the number of threads,
	     * /proc/sys/kernel/threads-max, was reached. Mine reports 32478.
	     */
	    if (pstatus != SUCCESS) {
		errno = pstatus;
		Perror (sdp, "pthread_create() failed");
		MyExit(sdp, FATAL_ERROR);
	    }
	    if (tsdp->thread_id == 0) {
		/* Why wasn't EAGAIN returned as described above? */
		Wprintf(sdp, "No thread ID returned, breaking after %d threads!\n", thread);
		break;
	    }
	    sdp->threads_active++;
	    tsdp->thread_state = TS_Queued;

	} /* End of setting up for and creating threads! */

	tip = Malloc(sdp, sizeof(threads_info_t));
	tip->active_threads = sdp->threads_active;
	tip->sds = sds;

	/*
	 * All commands are executed by thread(s).
	 */
	if (sdp->async) {
	    pstatus = pthread_attr_setdetachstate(attrp, PTHREAD_CREATE_DETACHED);
	    if (pstatus != SUCCESS) {
		errno = pstatus;
		Perror(sdp, "pthread_attr_setdetachstate() failed setting PTHREAD_CREATE_DETACHED");
		MyExit(sdp, FATAL_ERROR);
	    }
	    pstatus = pthread_create( &sdp->thread_id, attrp, a_job, tip );
	    if (pstatus != SUCCESS) {
		errno = pstatus;
		Perror (sdp, "pthread_create() failed");
		MyExit(sdp, FATAL_ERROR);
	    }
	} else {
	    int estatus;
	    pstatus = wait_for_threads(tip);
	    /* Honor the users' error control! */
	    estatus = DoErrorControl(sdp, pstatus);
	    if (estatus == FAILURE) {
		if (sdp->script_level) {
		    CloseScriptFiles(sdp);
		}
	    } else { /* Continue... */
		estatus = SUCCESS;
	    }
	    status = estatus;
	    /* Note: tip free'd by above! */
	}
	cleanup_devices(sdp, True);
	/* Source devices are *not* sticky! */
	(void)close_devices(sdp, IO_INDEX_SRC);

    } while ( (InteractiveFlag || PipeModeFlag || sdp->script_level) && (ExitFlag == False) );

    if (sgp->sense_data) {
	free_palign(sdp, sgp->sense_data);
	sgp->sense_data = NULL;
    }
    /* May already be closed, if not, let OS close things down! */
    (void)close_devices(sdp, IO_INDEX_BASE);
    MyExit(sdp, status);
    /*NOTREACHED*/
    return(status);
}

void *
a_job(void *arg)
{
    threads_info_t *tip = arg;

    tip->status = wait_for_threads(tip);
    pthread_exit(tip);
    return(NULL);
}

int
wait_for_threads(threads_info_t *tip)
{
    scsi_device_t 	*sdp;
    scsi_generic_t	*sgp;
    io_params_t		*iop;
    void		*thread_status = NULL;
    int			i, pstatus, status = SUCCESS;

    /*
     * Now, wait for each thread to complete.
     */
    for (i = 0; (i < tip->active_threads); i++) {
	sdp = &tip->sds[i];
	iop = &sdp->io_params[IO_INDEX_BASE];
	sgp = &iop->sg;
	pstatus = pthread_join( sdp->thread_id, &thread_status );
	if (pstatus != SUCCESS) {
	    errno = pstatus;
	    Perror(sdp, "pthread_join() failed");
	    /* continue waiting for other threads */
	} else {
	    sdp->thread_state = TS_NotQueued;
#if !defined(WIN32)
            /* Note: Thread status is 0 (NULL) on Windows! */
            if ( (thread_status == NULL) || (long)thread_status == -1 ) {
		Fprintf(sdp, "Sanity check of thread status failed for device %s!\n", sgp->dsf);
		Fprintf(sdp, "Thread status is NULL or -1, assuming cancelled, setting FAILURE status!\n");
		status = FAILURE;   /* Assumed canceled, etc. */
	    } else {
		if (sdp != (scsi_device_t *)thread_status) {
		    Fprintf(sdp, "Sanity check of thread status failed for device %s!\n", sgp->dsf);
		    Fprintf(sdp, "Expected sdp = %p, Received: %p\n", sdp, thread_status);
		    abort(); /* sanity */
		}
	    }
#endif /* !defined(WIN32) */
	    if (sdp->status == FAILURE) {
		status = sdp->status;
	    }
	    cleanup_devices(sdp, False);
	}
    }
    free(tip->sds);
    free(tip);
    return (status);
}

void *
a_cdb(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    hbool_t expected_found, file_open = False;
    hbool_t opened_devices = False;
    struct tms end_times;
    int status;
 
    sdp->status = do_common_thread_startup(sdp);
    if (sdp->status == FAILURE) goto finish;

    if (sdp->dout_file && (sgp->data_dir == scsi_data_read) ) {
	sdp->status = process_output_file(sdp);
	if (sdp->status == FAILURE) {
	    goto finish;
	} else if (sdp->status == SUCCESS) {
	    file_open = True;
	}
    }
    /* Generally only true for async threads (or Windows). */
    if (sgp->fd == INVALID_HANDLE_VALUE) {
	/* Open all devices (as necessary). */
	if ( (sdp->status = open_devices(sdp)) == FAILURE) {
	    goto finish;
	}
	opened_devices = True;
    }

    sdp->start_time = time((time_t *) 0);
    if (sdp->runtime > 0) {
	sdp->end_time = (sdp->start_time + sdp->runtime);
    }
    if (sdp->keepalive_time && sdp->keepalive) {
	sdp->last_keepalive = time((time_t *) 0); /* Prime it! */
    }
    sdp->start_ticks = times(&end_times);

    if (sdp->iot_pattern) {
	sdp->iot_seed_per_pass = sdp->iot_seed;
    }

    /*
     * Execute the SCSI command for repeat or runtime.
     */
    do {
top:
	/*
	 * Do encoding of CDB or paramater data, if enabled and supported.
	 */ 
	if ( sdp->encode_flag && iop->sop && iop->sop->encode) {
	    sdp->status = (*iop->sop->encode)(sdp);
	    if (sdp->status == END_OF_DATA) {
		sdp->status = SUCCESS;
		iop->first_time = True; /* For looping. */
		/* Alter the IOT data on each write iteration (pass). */
		if ( sdp->iot_pattern && sdp->unique_pattern &&
		     (iop->sop->data_dir == scsi_data_write) ) {
		    /* Note: iterations is bumped after the continue below! */
		    sdp->iot_seed_per_pass = (uint32_t)(sdp->iot_seed * (sdp->iterations + 2));
		}
		continue;
	    } else if (sdp->status == FAILURE) {
		/* Honor the users' onerr= control! */
		if (do_post_processing(sdp, sdp->status) != CONTINUE) {
		    goto finish;
		}
		continue;
	    }
	}
	/*
	 * Execute the SCSI command.
	 */
	status = sdp->status = ExecuteCdb(sdp, sgp);
	if (status == RESTART) continue;
	if (status == SUCCESS) {
	    if (iop->cdb_blocks) {
		iop->blocks_transferred = iop->cdb_blocks;
	    } else if (iop->device_size) {
		iop->blocks_transferred = howmany(sgp->data_transferred, iop->device_size);
	    }
	    iop->total_blocks += iop->blocks_transferred;
	    iop->total_transferred += sgp->data_transferred;
	}

	if (sdp->tci.check_status) {
	    if (sdp->tci.wait_for_status) {
		hbool_t reached_limit = ((sdp->retry_count + 1) == sdp->retry_limit);
		/*
		 * Wait for the expected status.
		 */
		expected_found = check_expected_status(sdp, reached_limit);
		if (expected_found) {
		    sdp->status = SUCCESS;	/* We matched, that's success! */
		    /* continue below */
		} else if (reached_limit) {
		    Eprintf(sdp, "Retry limit of " LUF" reached for SCSI opcode 0x%x (%s)\n",
			    (sdp->iterations + 1), sgp->cdb[0], sgp->cdb_name);
		    sdp->status = FAILURE;
		    break;
		} else {
		    sdp->retry_count++;
		    do_sleeps(sdp);
		    continue;
		}
	    } else {
		expected_found = check_expected_status(sdp, True);
		if (expected_found) {
		    sdp->status = SUCCESS;	/* We matched, that's success! */
		    /* continue below */
		} else {
		    Eprintf(sdp, "Unexpected response for SCSI opcode 0x%x (%s)\n",
			    sgp->cdb[0], sgp->cdb_name);
		    sdp->status = FAILURE;
		    break;
		}
	    }
	}

	if ( (status == SUCCESS) &&
	     (sdp->tci.check_resid && (sdp->tci.exp_residual != sgp->data_resid)) ) {
	    Eprintf(sdp, "Unexpected response for SCSI opcode 0x%x (%s)\n",
		    sgp->cdb[0], sgp->cdb_name);
	    Fprintf(sdp, "Residual value mismatch: expected=%u, actual=%u\n",
		    sdp->tci.exp_residual, sgp->data_resid);
	    sdp->status = FAILURE;
	    break;
	} 
	if ( (status == SUCCESS) &&
	     (sdp->tci.check_xfer && (sdp->tci.exp_transfer != sgp->data_transferred)) ) {
	    Eprintf(sdp, "Unexpected response for SCSI opcode 0x%x (%s)\n",
		    sgp->cdb[0], sgp->cdb_name);
	    Fprintf(sdp, "Transfer value mismatch: expected=%u, actual=%u\n",
		    sdp->tci.exp_transfer, sgp->data_transferred);
	    sdp->status = FAILURE;
	    break;
	} 

	if ( (status == SUCCESS) && sdp->decode_flag &&
	     ((sgp->data_dir == scsi_data_read) && sgp->data_transferred) ) {
	    if (iop->sop && iop->sop->decode) {
		sdp->status = (*iop->sop->decode)(sdp);
	    }
	}
	
	if ( (status == SUCCESS) &&
	     ((sgp->data_dir == scsi_data_read) && sgp->data_transferred) ) {
	    if (file_open == True) {
		ssize_t count;
#if defined(_WIN32)
		if (!WriteFile(sdp->data_fd, sgp->data_buffer, sgp->data_transferred, &count, NULL)) count = -1;
#else /* !defined(_WIN32) */
		count = write(sdp->data_fd, sgp->data_buffer, sgp->data_transferred);
#endif /* defined(_WIN32) */
		if ((size_t)count != sgp->data_transferred) {
		    os_perror(sdp, "File write failed while writing %u bytes!", sgp->data_transferred);
		}
	    } else if (sdp->verbose) {
		uint32_t dlimit;
		if (sdp->scsi_name) {
		    Printf(sdp, "Data Read for SCSI opcode 0x%x (%s), %d bytes: (thread %d)\n",
			   sgp->cdb[0], sdp->scsi_name, sgp->data_transferred, sdp->thread_number);
		    Printf(sdp, "\n");
		} else {
		    Printf(sdp, "Data Read for SCSI opcode 0x%x, %d bytes: (thread %d)\n",
			   sgp->cdb[0], sgp->data_transferred, sdp->thread_number);
		    Printf(sdp, "\n");
		}
		if (sdp->dump_limit) {
		    dlimit = min(sdp->dump_limit, sgp->data_transferred);
		} else {
		    dlimit = sgp->data_transferred;
		}
		DumpFieldsOffset(sdp, sgp->data_buffer, dlimit);
	    }
	    if (sdp->iomode == IOMODE_TEST) {
		/*
		 * Now compare the data, if requested. 
		 */
		if (sdp->compare_data && sdp->pin_data) {
		    sdp->status = VerifyBuffers(sdp, sgp->data_buffer, sdp->pin_buffer,
						min(sdp->pin_length,sgp->data_transferred));
		    if (sdp->status == FAILURE) break;
		} else if (sdp->compare_data && sdp->pattern_buffer) {
		    sdp->status = VerifyBuffers(sdp, sgp->data_buffer,
						sdp->pattern_buffer, sgp->data_transferred);
		    if (sdp->status == FAILURE) {
			if (sdp->iot_pattern) {
			    process_iot_data(sdp, iop, sdp->pattern_buffer,
					     sgp->data_buffer, sgp->data_transferred);
			}
			break;
		    }
		} else if (sdp->exp_data_count) {
		    sdp->status = VerifyExpectedData(sdp, sgp->data_buffer, sgp->data_transferred);
		    if (sdp->status == FAILURE) break;
		}
	    }
	}
	if (sdp->emit_all) {
	    EmitStatus(sdp, sdp->emit_status);
	}
	if (sdp->keepalive_time && sdp->keepalive) {
	    time_t current_time = time((time_t *) 0);
	    if ( (current_time - sdp->last_keepalive) >= sdp->keepalive_time) {
		EmitStatus(sdp, sdp->keepalive);
		sdp->last_keepalive = current_time;
	    }
	}
	if (do_post_processing(sdp, sdp->status) != CONTINUE) {
	    break;
	}
	/* 
	 * Special check, so we can repeat this sequence up to the runtime. 
	 */
	if (iop->block_limit) {
	    if ( (CmdInterruptedFlag == True) ||
		( (sdp->runtime > 0) && (time(&sdp->loop_time) >= sdp->end_time)) ) {
		break;
	    }
	    goto top;
	}
    } while ( (CmdInterruptedFlag == False)			&&
	      (++sdp->iterations < sdp->repeat_count)		||
	      (iop->block_limit && (iop->end_of_data == False))	||
	      (sdp->runtime < 0)				||
	      (sdp->runtime && (time(&sdp->loop_time) < sdp->end_time)) );

finish:
    sdp->end_ticks = times(&end_times);
    sdp->end_time = time((time_t *) 0);
    if (sdp->data_fd) {
	(void)os_close_file(sdp->data_fd);
	sdp->data_fd = INVALID_HANDLE_VALUE;
    }
    (void)close_devices(sdp, IO_INDEX_BASE);
    if (!PipeModeFlag && !sdp->emit_all) {
	EmitStatus(sdp, sdp->emit_status);
    }
    pthread_exit(sdp);
    return(NULL);
}

void *
a_tmf(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    hbool_t opened_devices = False;
    struct tms end_times;

    sdp->status = do_common_thread_startup(sdp);
    if (sdp->status == FAILURE) goto finish;

#if defined(_AIX)
  if (sgp->dopen)
#else /* !defined(_AIX) */
      /* Generally only true for async threads (or Windows). */
    if (sgp->fd == INVALID_HANDLE_VALUE) {
	/* Open all devices (as necessary). */
	if ( (sdp->status = open_devices(sdp)) == FAILURE) {
	    goto finish;
	}
	opened_devices = True;
    }
#endif /* defined(_AIX) */

    sdp->start_time = time((time_t *) 0);
    if (sdp->runtime > 0) {
	sdp->end_time = (sdp->start_time + sdp->runtime);
    }
    if (sdp->keepalive_time && sdp->keepalive) {
	sdp->last_keepalive = time((time_t *) 0); /* Prime it! */
    }
    sdp->start_ticks = times(&end_times);
    
    /*
     * Execute the SCSI command for repeat or runtime.
     */
    do {
	/*
	 * Execute the TMF command:
	 */
	sdp->status = (*sdp->sg_func)(sgp);
	if (sdp->emit_all) {
	    EmitStatus(sdp, sdp->emit_status);
	}
	if (sdp->keepalive_time && sdp->keepalive) {
	    time_t current_time = time((time_t *) 0);
	    if ( (current_time - sdp->last_keepalive) >= sdp->keepalive_time) {
		EmitStatus(sdp, sdp->keepalive);
		sdp->last_keepalive = current_time;
	    }
	}
	if (do_post_processing(sdp, sdp->status) != CONTINUE) {
	    break;
	}
    } while ( !CmdInterruptedFlag				&&
	      (++sdp->iterations < sdp->repeat_count)		||
	      (sdp->runtime < 0)				||
	      (sdp->runtime && (time(&sdp->loop_time) < sdp->end_time)) );

finish:
    sdp->end_ticks = times(&end_times);
    sdp->end_time = time((time_t *) 0);
    (void)close_devices(sdp, IO_INDEX_BASE);
    if (!PipeModeFlag && !sdp->emit_all) {
	EmitStatus(sdp, sdp->emit_status);
    }
    pthread_exit(sdp);
    return(NULL);
}

int
process_cdb_params(scsi_device_t *sdp)
{
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    int status;

    /*
     * Calculate the CDB size (as needed).
     */
    if (iop->user_cdb_size == False) {
	/* Note: Bypass previousily skipped this, but caused issues! */
	/*   But, changing this breaks compatibility, so small risk! */
	if ( (sgp->cdb_size != 6)   && (sgp->cdb_size != 10) &&
	     (sgp->cdb_size != 12)  && (sgp->cdb_size != 16) ) {
	    sgp->cdb_size = GetCdbLength(sgp->cdb[0]);
	} else if (sgp->cdb_size == 0) { /* Note: Should be set! */
	    /* Even with bypass, we need a CDB size! */
	    sgp->cdb_size = GetCdbLength(sgp->cdb[0]);
	}
    }
    
    iop->sop = ScsiOpcodeEntry(sgp->cdb, iop->device_type);
    /*
     * For I/O parameters, do special setup to avoid bypass issues.
     * 
     * Note: This only sets the data length for CDB's without default
     * blocks, so normal read/write CDB's (excludes verify, xcopy, etc).
     * Please Note: For CDB's with encode functions (I/O parameters), the
     * majority of this setup is done in initialize_io_parameters().
     */
    if (iop->sop) {
        if ( (sgp->data_dir != scsi_data_none)  &&
             (sgp->data_length == 0)            &&
             iop->cdb_blocks                    &&
             (iop->sop->default_blocks == 0) ) {
            sgp->data_length = (uint32_t)(iop->cdb_blocks * iop->device_size);
        }
	/* TODO: Cleanup, name is in too many places now! */
	if (!sdp->user_sname && iop->sop->opname) {
	    sdp->scsi_name = iop->sop->opname;
	}
    }
    /* Historic, honor the users' set SCSI name! */
    if (sdp->scsi_name && !sdp->user_sname) {
	sgp->cdb_name = sdp->scsi_name;
    }

    /*
     * Sanity checks:
     */
    if ( (sdp->bypass == False) &&
	 (sgp->data_length && (sgp->data_dir == scsi_data_none)) ) {
	Eprintf(sdp, "Please specify a data direction with a data length!\n");
	(void)HandleExit(sdp, FAILURE);
	return (FAILURE);
    }

    if ( (status = process_input_file(sdp)) != SUCCESS) {
	return (status);
    }

    if ( (sgp->data_dir != scsi_data_none) && (sgp->data_length == 0) ) {
	Wprintf(sdp, "Please specify a data length for reads and writes!\n");
	(void)HandleExit(sdp, WARNING);
	return (WARNING);
    }
    return (SUCCESS);
}

int
process_input_file(scsi_device_t *sdp)
{
    scsi_generic_t *sgp = &sdp->io_params[IO_INDEX_BASE].sg;
    int status;

    /*
     * When writing data, open a file for reading (if requested).
     */
    if (sdp->din_file && (sgp->data_dir == scsi_data_write) ) {
	/* Open a file to read data from. */
	if (*sdp->din_file == '-') {
#if defined(_WIN32)
	    sdp->data_fd = GetStdHandle(STD_INPUT_HANDLE);
#else /* !defined(_WIN32) */
	    sdp->data_fd = dup(fileno(stdin));
#endif /* defined(_WIN32) */
	    if (sgp->data_length == 0) {
		Wprintf(sdp, "Please specify a data length to read from stdin!\n");
		(void)HandleExit(sdp, WARNING);
		return (WARNING);
	    }
	} else {
#if defined(_WIN32)
	    sdp->data_fd = CreateFile(sdp->din_file, GENERIC_READ, FILE_SHARE_READ,
				      NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#else /* !defined(_WIN32) */
	    sdp->data_fd = open(sdp->din_file, O_RDONLY);
#endif /* defined(_WIN32) */
	}
	if (sdp->data_fd == INVALID_HANDLE_VALUE) {
	    os_perror(sdp, "Couldn't open '%s' for reading!", sdp->din_file);
	    (void)HandleExit(sdp, FAILURE);
	    return (FAILURE);
	}
	/*
	 * Read the data (from a file or stdin) to be written to SCSI device.
	 */
	status = ReadDataFile(sdp, sdp->data_fd, &sgp->data_buffer, (size_t *)&sgp->data_length);
	if (status == FAILURE) {
	    (void)HandleExit(sdp, status);
	    return (status);
	}
	(void)os_close_file(sdp->data_fd);
	sdp->user_data = True;
	sdp->data_fd = INVALID_HANDLE_VALUE;
    } else if (sgp->data_dir == scsi_data_write) {
	/*
	 * If we're writing and a data file is not specified, then we'll
	 * allocate a buffer and initialize it with the pattern to write.
	 */
	if (sgp->data_length == 0) {
	    Eprintf(sdp, "Please specify a data length to write!\n");
	    (void)HandleExit(sdp, FAILURE);
	    return (FAILURE);
	}
	/*
	 * Only allocate buffer for user defined pattern, otherwise each
	 * thread gets its' own data buffer later.
	 */
	if (!sdp->user_data && sdp->user_pattern) {
	    sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
	    InitBuffer(sgp->data_buffer, (size_t)sgp->data_length, sdp->pattern);
	}
    }
    return (SUCCESS);
}

int
process_output_file(scsi_device_t *sdp)
{
    scsi_generic_t *sgp = &sdp->io_params[IO_INDEX_BASE].sg;

    /*
     * When reading data, open a file for writing (if requested).
     */
    if (sdp->dout_file && (sgp->data_dir == scsi_data_read) ) {
	if (*sdp->dout_file == '-') {
#if defined(_WIN32)
	    sdp->data_fd = GetStdHandle(STD_OUTPUT_HANDLE);
#else /* !defined(_WIN32) */
	    sdp->data_fd = dup(fileno(stdout));
#endif /* defined(_WIN32) */
	} else {
	    /* Create a file to write data to. */
#if defined(_WIN32)
	    sdp->data_fd = CreateFile(sdp->dout_file, GENERIC_WRITE, FILE_SHARE_WRITE,
				      NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#else /* !defined(_WIN32) */
	    sdp->data_fd = open(sdp->dout_file, (O_CREAT|O_RDWR), (S_IRWXU|S_IRWXG|S_IRWXO));
#endif /* defined(_WIN32) */
	}
	if (sdp->data_fd == INVALID_HANDLE_VALUE) {
	    os_perror(sdp, "Couldn't open '%s' for writing!", sdp->dout_file);
	    (void)HandleExit(sdp, FAILURE);
	    return(FAILURE);
	}
	return(SUCCESS);
    }
    return (WARNING);
}

/*
 * ExecuteCdb() = Execute a SCSI Command Descriptor Block (CDB).
 *
 * Inputs:
 *  sdp = The device information pointer.
 *  sgp = Pointer to SCSI generic pointer.
 *
 * Return Value:
 *      Returns 0/-1/253 for Success/Failure/Restart.
 */
int
ExecuteCdb(scsi_device_t *sdp, scsi_generic_t *sgp)
{
    tool_specific_t *tsp = sgp->tsp;
    io_params_t *iop = (tsp) ? (io_params_t *)tsp->params : NULL;
    hbool_t retriable;
    int error;

    if (iop == NULL) {
	iop = &sdp->io_params[IO_INDEX_BASE];
    }
    if (sdp->genspt_flag == True) {
	GenerateSptCmd(sgp);
    }

    sgp->recovery_retries = 0;
    do {
	retriable = False;
	/*
	 * Ensure the sense data is cleared for emiting status.
	 */
	memset(sgp->sense_data, '\0', sgp->sense_length);
	/* Clear these too, since IOCTL may fail so never get updated! */
	sgp->os_error = 0;		/* The system call error (if any). */
	sgp->scsi_status = sgp->driver_status = sgp->host_status = sgp->data_resid = 0;

	/*
	 * Call OS dependent SCSI Pass-Through (spt) function.
	 */
	error = os_spt(sgp);
	if (iop) iop->operations++;
	if ( !CmdInterruptedFlag &&
	     ((error == FAILURE) || (sgp->error == True)) && sgp->recovery_flag) {
	    if (sgp->recovery_retries == sgp->recovery_limit) {
		Eprintf(sdp, "Exceeded retry limit (%u) for this request!\n", sgp->recovery_limit);
	    } else {
		retriable = libIsRetriable(sgp);
		if (retriable == True) {
		    (void)os_sleep(sgp->recovery_delay);
		    if (sgp->errlog == True) {
			/* Folks wish to see the actual error too! */
			if (error == FAILURE) {		/* The system call failed! */
			    libReportIoctlError(sgp, True);
			} else {
			    libReportScsiError(sgp, True);
			}
			if (sgp->restart_flag == True) {
			    Wprintf(sdp, "Restarting %s after detecting retriable error...\n", sgp->cdb_name);
			    return(RESTART);
			}
			Wprintf(sdp, "Retrying %s after %u second delay, retry #%u...\n",
			       sgp->cdb_name, sgp->recovery_delay, sgp->recovery_retries);
		    }
		}
	    }
	}
    } while (retriable == True);
   
    if (error == FAILURE) {		/* The system call failed! */
        if (sgp->errlog == True) {
	    ReportCdbDeviceInformation(sdp, sgp);
        }
    } else if ( (sgp->error == True) && (sgp->errlog == True) ) { /* SCSI error. */
	char *host_msg = os_host_status_msg(sgp);
	char *driver_msg = os_driver_status_msg(sgp);
        scsi_sense_t *ssp = sgp->sense_data;
        char *ascq_msg = ScsiAscqMsg(ssp->asc, ssp->asq);

	ReportCdbDeviceInformation(sdp, sgp);
	Fprintf(sdp, "SCSI Status = %#x (%s)\n", sgp->scsi_status, ScsiStatus(sgp->scsi_status));
	if (host_msg && driver_msg) {
	    Fprintf(sdp, "Host Status = %#x (%s), Driver Status = %#x (%s)\n",
		    sgp->host_status, host_msg, sgp->driver_status, driver_msg);
	} else if (host_msg || driver_msg) {
	    if (host_msg) {
		Fprintf(sdp, "Host Status = %#x (%s)\n", sgp->host_status, host_msg);
	    }
	    if (driver_msg) {
		Fprintf(sdp, "Driver Status = %#x (%s)\n", sgp->driver_status, driver_msg);
	    }
	} else if (sgp->host_status || sgp->driver_status) {
	    Fprintf(sdp, "Host Status = %#x, Driver Status = %#x\n",
		   sgp->host_status, sgp->driver_status);
	}
	Fprintf(sdp, "Sense Key = %d = %s, Sense Code/Qualifier = (%#x, %#x)",
		ssp->sense_key, SenseKeyMsg(ssp->sense_key),
		ssp->asc, ssp->asq);
	if (ascq_msg) {
	  Fprint(sdp, " - %s\n", ascq_msg);
	} else {
	  Fprint(sdp, "\n");
	}
	fflush(stderr);
        if ( ssp->error_code && (sgp->debug || (sdp && sdp->sense_flag)) ) {
            DumpSenseData(sgp, False, ssp);
        }
    }

    if (sgp->error == True) {
        error = FAILURE;      /* Tell caller we've had an error! */
    }
    return (error);
}

static int
VerifyExpectedData(scsi_device_t *sdp, unsigned char *buffer, size_t count)
{
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    exp_data_t	*edp;
    uint32_t	i;
    int		correct = 0, incorrect = 0;
    hbool_t	header_printed = False;
    int		status = SUCCESS;

    for (i = 0; i < sdp->exp_data_count; i++) {
	edp = &sdp->exp_data[i];
	if (edp->exp_byte_index > count) continue;
	if (edp->exp_byte_value != buffer[edp->exp_byte_index]) {
	    if (header_printed == False) {
		header_printed = True;
		ReportErrorInformation(sdp);
		Fprintf(sdp, "Failure while verifying %s data on device %s (thread %d)\n",
			sgp->cdb_name, sgp->dsf, sdp->thread_number);
	    }
	    Fprint(sdp, "  -> Offset %06u:  Expected Data: %3u (0x%02x, '%c')  Received Data: %3u (0x%02x, '%c')\n",
		   edp->exp_byte_index,
		   edp->exp_byte_value, edp->exp_byte_value,
		   isprint((int)edp->exp_byte_value) ? edp->exp_byte_value : ' ',
		   buffer[edp->exp_byte_index], buffer[edp->exp_byte_index],
		   isprint((int)buffer[edp->exp_byte_index]) ? buffer[edp->exp_byte_index] : ' ');
	    incorrect++;
	    status = FAILURE;
	} else {
	    correct++;
	}
    }
    if (incorrect) {
	Fprintf(sdp, "Data Bytes: %u, Expect Entries: %u, Correct Entries: %d, Incorrect Entries: %d\n",
		count, sdp->exp_data_count, correct, incorrect);
    }
    return (status);
}

static hbool_t
check_expected_status(scsi_device_t *sdp, hbool_t report)
{
    scsi_generic_t *sgp = &sdp->io_params[IO_INDEX_BASE].sg;
    scsi_sense_t *ssp = sgp->sense_data;
    hbool_t expected_found;

    if ( (sdp->tci.exp_scsi_status != sgp->scsi_status)	 ||
	 ((sdp->tci.exp_scsi_status == SCSI_CHECK_CONDITION) &&
	  ((sdp->tci.exp_sense_key != ssp->sense_key)	 ||
	   (sdp->tci.exp_sense_asc != ssp->asc)		 ||
	   (sdp->tci.exp_sense_asq != ssp->asq))) ) {
	if (sgp->debug || report) {
	    Fprint(sdp, "Result for %s\n", sgp->cdb_name);
	    Fprint(sdp, "Expected:\n");
	    print_scsi_status(sgp, sdp->tci.exp_scsi_status, sdp->tci.exp_sense_key,
			      sdp->tci.exp_sense_asc, sdp->tci.exp_sense_asq);
	    Fprint(sdp, "Actual:\n");
	    print_scsi_status(sgp, sgp->scsi_status, ssp->sense_key, ssp->asc, ssp->asq);
	}
	expected_found = False;
    } else {
	expected_found = True;
    }
    return (expected_found);
}

/*
 * parse_args() - Parse 'spt Program Arguments.
 *
 * Inputs:
 *	sdp = The SCSI device information.
 *	argc = The number of arguments.
 * 	argv = Array pointer to arguments.
 *
 * Return Value;
 *	Returns Success/Failure = Parsed Ok/Parse Error.
 */
static int
parse_args(scsi_device_t *sdp, int argc, char **argv)
{
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    /* Note: These get updated for source devices! */
    io_params_t *siop = iop;
    scsi_generic_t *ssgp = sgp;
    char *p, *string;
    int i;

    /*
     * Beware: Can't use pointers into string now that we reuse the
     * buffer space in interactive or pipe modes! (gets overwritten)
     */
    for (i = 0; i < argc; i++) {
        string = argv[i];
	if ( match(&string, "bg") || match(&string, "&") ) {
	    sdp->async = True;
	    sdp->verbose = False;
	    continue;
	}
        if (match (&string, "cdb=")) {
	    uint32_t value;
            char *str, *token;
            sgp->cdb_size = 0;
	    str = strdup(string);
            token = strtok(str, " ");
            while (token != NULL) {
		value = number(sdp, token, HEX_RADIX);
		if (value > 0xFF) {
		    Eprintf(sdp, "CDB byte value %#x is too large!\n", value);
		    Free(sdp, str);
		    return ( HandleExit(sdp, FATAL_ERROR) );
		}
                sgp->cdb[sgp->cdb_size++] = (uint8_t)value;
                token = strtok(NULL, " ");
                if (sgp->cdb_size >= MAX_CDB) {
                    Eprintf(sdp, "Maximum CDB size is %d bytes!\n", MAX_CDB);
		    Free(sdp, str);
		    return ( HandleExit(sdp, FATAL_ERROR) );
                }
            }
	    Free(sdp, str);
	    sdp->op_type = SCSI_CDB_OP;
	    /*
	     * Note: Disabled for fear of breaking existing scripts! (negative testing)
	     */ 
#if 0
	    /* Setup the CDB size and direction, if known. */
	    if (sdp->bypass == False) {
		if (sgp->cdb_size == 0) {
		    sgp->cdb_size = GetCdbLength(sgp->cdb[0]);
		}
		iop->sop = ScsiOpcodeEntry(sgp->cdb, iop->device_type);
		if (iop->sop && (sgp->data_dir != iop->sop->data_dir) ) {
		    sgp->data_dir = iop->sop->data_dir;
		}
	    }
#endif
	    continue;
	}
	/* An option to help with negative testing! */
        if (match (&string, "cdbsize=")) {
            sgp->cdb_size = number(sdp, string, ANY_RADIX);
	    if (sgp->cdb_size == 0) {
		sgp->cdb_size = GetCdbLength(sgp->cdb[0]);
	    }
	    if (sgp->cdb_size >= MAX_CDB) {
		Eprintf(sdp, "Maximum CDB size is %d bytes!\n", MAX_CDB);
		return ( HandleExit(sdp, FATAL_ERROR) );
	    }
	    iop->user_cdb_size = True;
	    continue;
	}
        if (match (&string, "din=")) {
	    if (sdp->din_file) free(sdp->din_file);
            sdp->din_file = strdup(string);
            continue;
        }
        if (match (&string, "dout=")) {
	    if (sdp->dout_file) free(sdp->dout_file);
            sdp->dout_file = strdup(string);
            continue;
        }
        if ( match(&string, "dsf=") || match(&string, "dst=") ) {
	    /* Switch to base (destination) device. */
	    siop = iop;	ssgp = sgp;
	    if (sgp->fd != INVALID_HANDLE_VALUE) {
		(void)os_close_device(sgp);
	    }
	    if (sgp->dsf) {
		free(sgp->dsf);
		sgp->dsf = NULL;
	    }
	    if ( !strlen(string) ) continue;
            sgp->dsf = strdup(string);
	    iop->device_capacity = 0;
            continue;
        }
	if ( match(&string, "dsf1=") || match(&string, "src=") ) {
	    if (sdp->io_devices == MAX_DEVICES) {
                Eprintf(sdp, "The maximum devices of %d exceeded!\n", sdp->io_devices);
		return ( HandleExit(sdp, FATAL_ERROR) );
	    }
	    siop = &sdp->io_params[sdp->io_devices];
	    ssgp = &siop->sg;
	    if (ssgp->fd != INVALID_HANDLE_VALUE) {
		(void)os_close_device(ssgp);
	    }
	    if (ssgp->dsf) {
		free(ssgp->dsf);
		ssgp->dsf = NULL;
	    }
	    if ( !strlen(string) ) continue;
	    ssgp->dsf = strdup(string);
	    siop->device_capacity = 0;
	    sdp->encode_flag = True;
	    sdp->io_devices++;
	    continue;
	}
        if ( match(&string, "len=") || match(&string, "length=") ) {
            sgp->data_length = number(sdp, string, ANY_RADIX);
            continue;
        }
        if (match (&string, "dir=")) {
            if (match (&string, "none")) {
                sgp->data_dir = scsi_data_none;
            } else if (match (&string, "read")) {
                sgp->data_dir = scsi_data_read;
            } else if (match (&string, "write")) {
                sgp->data_dir = scsi_data_write;
            } else {
                Eprintf(sdp, "Valid I/O directions are: 'none', 'read' or 'write'.\n");
		return ( HandleExit(sdp, FATAL_ERROR) );
            }
            continue;
        }
	if (match (&string, "aborts=")) {
	    sdp->abort_freq = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if (match (&string, "abort_timeout=")) {
	    sdp->abort_timeout = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if (match (&string, "dlimit=")) {
	    sdp->dump_limit = number(sdp, string, ANY_RADIX);
	    sgp->data_dump_limit = sdp->dump_limit;
	    continue;
	}
	if (match (&string, "max=")) {
	    iop->user_max = True;
	    iop->max_size = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if (match (&string, "min=")) {
	    iop->user_min = True;
	    iop->min_size = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if (match (&string, "incr=")) {
	    iop->user_increment = True;
	    if (match (&string, "var")) {
		sdp->random_seed = os_create_random_seed();
		init_genrand64(sdp->random_seed);
		iop->incr_variable = True;
	    } else {
		iop->incr_variable = False;
		iop->incr_size = number(sdp, string, ANY_RADIX);
	    }
	    continue;
	}
	if (match (&string, "emit=")) {
	    if (sdp->emit_status) free(sdp->emit_status);
	    if (match (&string, "default")) {
		sdp->emit_status = strdup(emit_status_default);
	    } else if (match (&string, "multi")) {
		sdp->emit_status = strdup(emit_status_multiple);
	    } else {
		sdp->emit_status = strdup(string);
	    }
	    continue;
	}
	if (match (&string, "exp=") || match (&string, "expect=")) {
	    if (parse_exp_data(string, sdp) != SUCCESS) {
		return ( HandleExit(sdp, FATAL_ERROR) );
	    }
	    continue;
	}
	if (match (&string, "exp_radix=")) {
	    if (match (&string,  "any")) {
		sdp->exp_radix = ANY_RADIX;
	    } else if (match (&string,  "dec")) {
		sdp->exp_radix = DEC_RADIX;
	    } else if (match (&string,  "hex")) {
		sdp->exp_radix = HEX_RADIX;
	    } else {
		Eprintf(sdp, "Unsupported radix specified '%s', valid radix is: any, dec, or hex\n", string);
		return ( HandleExit(sdp, FATAL_ERROR) );
	    }
	    continue;
	}
        if (match (&string, "enable=")) {
eloop:
            if (match(&string, ",")) {
                goto eloop;
	    }
            if (*string == '\0') {
                continue;
	    }
            if (match(&string, "adapter")) {
		sgp->flags = SG_ADAPTER;
                goto eloop;
            }
            if (match(&string, "async")) {
                sdp->async = True;
		sdp->verbose = False;
                goto eloop;
            }
	    if (match(&string, "bypass")) {
		sdp->bypass = True;
		goto eloop;
	    }
	    if (match(&string, "compare")) {
		sdp->compare_data = True;
		goto eloop;
	    }
	    if (match(&string, "image")) {
		sdp->image_copy = True;
		goto eloop;
	    }
            if (match(&string, "debug")) {
                sgp->debug = True;
                goto eloop;
            }
            if (match(&string, "Debug")) {
                DebugFlag = sdp->DebugFlag = True;
                goto eloop;
            }
            if (match(&string, "mdebug")) {
                mDebugFlag = True;
                goto eloop;
            }
	    if (match(&string, "header")) {
		sdp->logheader_flag = True;
		goto eloop;
	    }
            if (match(&string, "xdebug")) {
                sdp->xDebugFlag = True;
                goto eloop;
            }
            if (match(&string, "decode")) {
                sdp->decode_flag = True;
                goto eloop;
            }
            if (match(&string, "dopen")) {
                sgp->dopen = True;
                goto eloop;
            }
            if (match(&string, "emit_all")) {
                sdp->emit_all = True;
                goto eloop;
            }
            if (match(&string, "encode")) {
                sdp->encode_flag = True;
                goto eloop;
            }
            if (match(&string, "errors")) {
                sgp->errlog = True;
                goto eloop;
            }
            if (match(&string, "genspt")) {
                sdp->genspt_flag = True;
                goto eloop;
            }
            if (match(&string, "header")) {
                sdp->log_header_flag = True;
                goto eloop;
            }
            if (match(&string, "multi")) {
                PipeModeFlag = False;
		InteractiveFlag = True;
                goto eloop;
            }
            if (match(&string, "pipes")) {
                PipeModeFlag = True;
		InteractiveFlag = False;
		if (!sdp->emit_status) {
		    sdp->emit_status = strdup(pipe_emit);
		}
                goto eloop;
            }
	    if (match(&string, "prewrite")) {
		sdp->prewrite_flag = True;
		goto eloop;
	    }
	    if (match(&string, "recovery")) {
		sgp->recovery_flag = True;
		sdp->recovery_flag = True;
		goto eloop;
	    }
	    if ( match(&string, "raw") || match(&string, "read_after_write") || match(&string, "read_immed") ) {
		sdp->read_after_write = True;
		goto eloop;
	    }
	    if (match(&string, "scsi")) {
		sdp->scsi_info_flag = True;
		goto eloop;
	    }
	    if (match(&string, "sense")) {
		sdp->sense_flag = True;
		goto eloop;
	    }
	    if (match(&string, "unique")) {
		sdp->unique_pattern = True;
		goto eloop;
	    }
	    if (match(&string, "verbose")) {
		sdp->verbose = True;
		goto eloop;
	    }
	    if (match(&string, "warnings")) {
		sdp->warnings_flag = True;
		goto eloop;
	    }
	    if (match(&string, "wait")) {
		sdp->tci.wait_for_status = True;
		goto eloop;
	    }
	    if (match(&string, "zerorod")) {
		sdp->zero_rod_flag = True;
		goto eloop;
	    }
	    Eprintf(sdp, "Invalid enable keyword: %s\n", string);
	    return ( HandleExit(sdp, FATAL_ERROR) );
        }
        if (match (&string, "disable=")) {
dloop:
            if (match(&string, ",")) {
                goto dloop;
	    }
            if (*string == '\0') {
                continue;
	    }
            if (match(&string, "adapter")) {
		sgp->flags = 0;
                goto dloop;
            }
            if (match(&string, "async")) {
                sdp->async = False;
                goto dloop;
            }
	    if (match(&string, "bypass")) {
		sdp->bypass = False;
		goto dloop;
	    }
	    if (match(&string, "compare")) {
		sdp->compare_data = False;
		goto dloop;
	    }
	    if (match(&string, "image")) {
		sdp->image_copy = False;
		goto dloop;
	    }
            if (match(&string, "debug")) {
                sgp->debug = False;
                goto dloop;
            }
            if (match(&string, "Debug")) {
                DebugFlag = sdp->DebugFlag = False;
                goto dloop;
            }
	    if (match(&string, "mdebug")) {
		mDebugFlag = False;
		goto dloop;
	    }
	    if (match(&string, "header")) {
		sdp->logheader_flag = False;
		goto eloop;
	    }
            if (match(&string, "xdebug")) {
                sdp->xDebugFlag = False;
                goto dloop;
            }
	    if (match(&string, "decode")) {
		sdp->decode_flag = False;
		goto dloop;
	    }
            if (match(&string, "dopen")) {
                sgp->dopen = False;
                goto dloop;
            }
            if (match(&string, "emit_all")) {
                sdp->emit_all = False;
                goto dloop;
            }
	    if (match(&string, "encode")) {
		sdp->encode_flag = False;
		goto dloop;
	    }
            if (match(&string, "errors")) {
                sgp->errlog = False;
                goto dloop;
            }
            if (match(&string, "genspt")) {
                sdp->genspt_flag = False;
                goto dloop;
            }
            if (match(&string, "header")) {
                sdp->log_header_flag = False;
                goto dloop;
            }
            if (match(&string, "multi")) {
		InteractiveFlag = False;
                goto dloop;
            }
            if (match(&string, "pipes")) {
                PipeModeFlag = False;
		InteractiveFlag = True;
                goto dloop;
            }
	    if (match(&string, "prewrite")) {
		sdp->prewrite_flag = False;
		goto dloop;
	    }
	    if (match(&string, "recovery")) {
		sgp->recovery_flag = False;
		goto dloop;
	    }
	    if ( match(&string, "raw") || match(&string, "read_after_write") || match(&string, "read_immed") ) {
		sdp->read_after_write = False;
		goto dloop;
	    }
	    if (match(&string, "scsi")) {
		sdp->scsi_info_flag = False;
		goto dloop;
	    }
	    if (match(&string, "sense")) {
		sdp->sense_flag = False;
		goto dloop;
	    }
	    if (match(&string, "unique")) {
		sdp->unique_pattern = False;
		goto dloop;
	    }
	    if (match(&string, "verbose")) {
		sdp->verbose = False;
		goto dloop;
	    }
	    if (match(&string, "warnings")) {
		sdp->warnings_flag = False;
		goto dloop;
	    }
	    if (match(&string, "wait")) {
		sdp->tci.wait_for_status = False;
		goto dloop;
	    }
	    if (match(&string, "zerorod")) {
		sdp->zero_rod_flag = False;
		goto dloop;
	    }
	    Eprintf(sdp, "Invalid disable keyword: %s\n", string);
	    return ( HandleExit(sdp, FATAL_ERROR) );
        }
	/*
	 * Special options to help seed IOT pattern with multiple passes.
	 */
	if (match (&string, "iotpass=")) {
	    int iot_pass = number(sdp, string, ANY_RADIX);
	    sdp->iot_seed *= iot_pass;
	    sdp->iot_pattern = True;
	    continue;
	}
	if (match (&string, "iotseed=")) {
	    sdp->iot_seed = (uint32_t)number(sdp, string, HEX_RADIX);
	    sdp->iot_pattern = True;
	    continue;
	}
	if (match (&string, "boff=")) {
	    if (match(&string, "dec")) {
		sdp->boff_format = DEC_FMT;
	    } else if (match (&string, "hex")) {
		sdp->boff_format = HEX_FMT;
	    } else {
		Eprintf(sdp, "Valid buffer offset formats are: dec or hex\n");
		return ( HandleExit(sdp, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "dfmt=")) {
	    if (match(&string, "byte")) {
		sdp->data_format = BYTE_FMT;
	    } else if (match (&string, "word")) {
		sdp->data_format = WORD_FMT;
	    } else {
		Eprintf(sdp, "Valid data formats are: byte or word\n");
		return ( HandleExit(sdp, FAILURE) );
	    }
	    continue;
	}
	if (match (&string, "keepalive=")) {
	    if (sdp->keepalive) free(sdp->keepalive);
	    sdp->keepalive = strdup(string);
	    if (sdp->keepalive_time == 0) sdp->keepalive_time++;
	    continue;
	}
	if (match (&string, "keepalivet=")) {
	    sdp->keepalive_time = time_value(sdp, string);
	    if (sdp->keepalive_time && (sdp->keepalive == NULL) ) {
		sdp->keepalive = strdup(keepalive);	/* Set the default! */
	    }
	    continue;
	}
	if (match (&string, "onerr=")) {
	    if (match (&string, "continue")) {
		sdp->onerr = ONERR_CONTINUE;
	    } else if (match (&string, "stop")) {
		sdp->onerr = ONERR_STOP;
	    } else {
                Eprintf(sdp, "On error actions are 'continue' or 'stop'.\n");
		return ( HandleExit(sdp, FATAL_ERROR) );
	    }
	    continue;
	}
        if (match (&string, "op=")) {
            if ( match(&string, "ats") || match (&string, "abort_task_set")) {
                sdp->op_type = ABORT_TASK_SET_OP;
            } else if ( match(&string, "br") || match(&string, "bus_reset") ) {
                sdp->op_type = BUS_RESET_OP;
            } else if ( match(&string, "lr") || match(&string, "lun_reset") ) {
                sdp->op_type = LUN_RESET_OP;
            } else if ( match(&string, "bdr") || match(&string, "target_reset") ) {
                sdp->op_type = TARGET_RESET_OP;
            } else if ( match(&string, "scsi_cdb") ) {
                sdp->op_type = SCSI_CDB_OP;
            } else {
                Eprintf(sdp, "Valid operations are: 'abort_task_set'(ats), 'bus_reset'(br), "
			"'lun_reset'(lr), 'target_reset'(bdr) or 'scsi_cdb'.\n");
		return ( HandleExit(sdp, FATAL_ERROR) );
            }
            continue;
        }
        if (match (&string, "path=")) {
            sgp->scsi_addr.scsi_path = (int)number(sdp, string, ANY_RADIX);
            continue;
        }
	if (match (&string, "pattern=")) {
	    size_t size = strlen(string);
	    /* Note: Added this parsing to match dt and keep my sanity! */
	    if ( (size == 3) && (match(&string, "iot") || match(&string, "IOT")) ) {
		sdp->iot_pattern = True;
		sdp->verbose = False;
	    } else {
		sdp->pattern = number(sdp, string, HEX_RADIX);
	    }
	    sdp->user_pattern = True;
	    sdp->compare_data = True;
	    continue;
	}
	if (match (&string, "ptype=")) {
	    int size = strlen(string);
	    if ((size == 3) && match (&string, "iot") || match(&string, "IOT")) {
		sdp->iot_pattern = True;
		sdp->user_pattern = True;
		sdp->compare_data = True;
		sdp->verbose = False;
	    } else {
		Eprintf(sdp, "Pattern types supported include: iot|IOT only!\n");
		return ( HandleExit(sdp, FATAL_ERROR) );
	    }
	    continue;
	}
	if (match (&string, "pin=")) {
	    uint32_t value;
	    char *str, *token;
	    char *pin;
	    sgp->data_dir = scsi_data_read; /* Receiving parameter data from device. */
	    sdp->pin_buffer = malloc_palign(sdp, strlen(string), 0);
	    sdp->pin_length = 0;
	    sdp->pin_data = True;
    	    sdp->compare_data = True;
	    pin = sdp->pin_buffer;
	    str = strdup(string);
	    token = strtok(str, " ");
	    while (token != NULL) {
		value = number(sdp, token, HEX_RADIX);
		if (value > 0xFF) {
		    Eprintf(sdp, "Parameter in byte value %#x is too large!\n", value);
		    Free(sdp, str);
		    return ( HandleExit(sdp, FATAL_ERROR) );
		}
		pin[sdp->pin_length++] = (uint8_t)value;
		token = strtok(NULL, " ");
	    }
	    Free(sdp, str);
	    continue;
	}
        if (match (&string, "pout=")) {
            uint32_t value;
            char *str, *token;
            char *pout;
            sgp->data_dir = scsi_data_write; /* Sending parameter data to device. */
            sgp->data_buffer = malloc_palign(sdp, strlen(string), 0);
            sgp->data_length = 0;
	    sdp->user_data = True;
            pout = sgp->data_buffer;
	    str = strdup(string);
            token = strtok(str, " ");
            while (token != NULL) {
		value = number(sdp, token, HEX_RADIX);
		if (value > 0xFF) {
		    Eprintf(sdp, "Parameter out byte value %#x is too large!\n", value);
		    Free(sdp, str);
		    return ( HandleExit(sdp, FATAL_ERROR) );
		}
                pout[sgp->data_length++] = (uint8_t)value;
                token = strtok(NULL, " ");
            }
	    Free(sdp, str);
            continue;
        }
        if (match (&string, "qtag=")) {
            if (match (&string, "noq")) {
               sgp->qtag_type = SG_NO_Q; 
            } else if (match (&string, "simple")) {
                sgp->qtag_type = SG_SIMPLE_Q; 
	    } else if (match (&string, "headha")) {
		sgp->qtag_type = SG_HEAD_HA_Q;
            } else if (match (&string, "head")) {
                sgp->qtag_type = SG_HEAD_OF_Q; 
            } else if (match (&string, "ordered")) {
                sgp->qtag_type = SG_ORDERED_Q;
            } else {
                Eprintf(sdp, "Valid qtags are: 'noq', 'simple', 'head', 'ordered', or 'headha'.\n");
		return ( HandleExit(sdp, FATAL_ERROR) );
            }
            continue;
        }
	if (match (&string, "iomode=")) {
	    io_params_t *biop = &sdp->io_params[IO_INDEX_DSF];
	    scsi_generic_t *bsgp = &iop->sg;
	    if (match (&string, "copy")) {
		/* Read from the 1st device (our source). */
		if ( (sdp->bypass == False) && (sdp->op_type == UNDEFINED_OP) ) {
		    bsgp->cdb[0] = (uint8_t)sdp->scsi_read_type;
		    bsgp->data_dir = scsi_data_read;
		    sdp->op_type = SCSI_CDB_OP;
		}
		sdp->iomode = IOMODE_COPY;
	    } else if (match (&string, "mirror")) {
		/* Write to 1st device, and read from 2nd! */
		if ( (sdp->bypass == False) && (sdp->op_type == UNDEFINED_OP) ) {
		    bsgp->cdb[0] = (uint8_t)sdp->scsi_write_type;
		    bsgp->data_dir = scsi_data_write;
		    sdp->op_type = SCSI_CDB_OP;
		}
		sdp->iomode = IOMODE_MIRROR;
	    } else if (match (&string, "test")) {
		sdp->iomode = IOMODE_TEST;
	    } else if (match (&string, "verify")) {
		/* Read from the 1st device, and read/compare from 2nd. */
		if ( (sdp->bypass == False) && (sdp->op_type == UNDEFINED_OP) ) {
		    bsgp->cdb[0] = (uint8_t)sdp->scsi_read_type;
		    bsgp->data_dir = scsi_data_read;
		    sdp->op_type = SCSI_CDB_OP;
		}
		sdp->iomode = IOMODE_VERIFY;
	    } else {
		Eprintf(sdp, "The supported I/O modes are: copy, mirror, test, or verify.\n");
		return ( HandleExit(sdp, FATAL_ERROR) );
	    }
	    sdp->encode_flag = True;
	    sdp->verbose = False;
	    continue;
	}
	if (match (&string, "readtype=")) {
	    if (match (&string, "read6")) {
		sdp->scsi_read_type = scsi_read6_cdb;
	    } else if (match (&string, "read10")) {
		sdp->scsi_read_type = scsi_read10_cdb;
	    } else if (match (&string, "read16")) {
		sdp->scsi_read_type = scsi_read16_cdb;
	    } else {
		Eprintf(sdp, "The supported SCSI read types are: read6, read10, or read16.\n");
		return ( HandleExit(sdp, FATAL_ERROR) );
	    }
	    continue;
	}
        if ( match(&string, "readlen=") || match(&string, "readlength=") ) {
            sdp->scsi_read_length = number(sdp, string, ANY_RADIX);
            continue;
	}
	if (match (&string, "writetype=")) {
	    if (match (&string, "write6")) {
		sdp->scsi_write_type = scsi_write6_cdb;
	    } else if (match (&string, "write10")) {
		sdp->scsi_write_type = scsi_read10_cdb;
	    } else if (match (&string, "write16")) {
		sdp->scsi_write_type = scsi_write16_cdb;
	    } else if (match (&string, "writev16")) {
		sdp->scsi_write_type = scsi_writev16_cdb;
	    } else {
		Eprintf(sdp, "The supported SCSI write types are: write6, write10, write16, or writev16.\n");
		return ( HandleExit(sdp, FATAL_ERROR) );
	    }
	    continue;
	}
	if ( match(&string, "writelen=") || match(&string, "writelength=") ) {
	    sdp->scsi_write_length = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if (match (&string, "ranges=")) {
	    sdp->range_count = number(sdp, string, ANY_RADIX);
	    siop->range_count = sdp->range_count; /* per device for token xcopy. */
	    continue;
	}
	if ( match(&string, "repeat=") || match(&string, "passes=") ) {
	    sdp->repeat_count = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if (match (&string, "recovery_delay=")) {
	    sdp->recovery_delay = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if (match (&string, "recovery_retries=")) {
	    sdp->recovery_limit = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if (match (&string, "retry=")) {
	    sdp->retry_limit = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if (match (&string, "runtime=")) {
	    sdp->runtime = time_value(sdp, string);
	    continue;
	}
	if (match (&string, "script=")) {
	    int status;
	    status = OpenScriptFile(sdp, string);
	    if (status == SUCCESS) {
		continue;
	    } else {
		return ( HandleExit(sdp, FATAL_ERROR) );
	    }
	}
	if (match (&string, "segments=")) {
	    int segment_count = number(sdp, string, ANY_RADIX);
	    if (!segment_count && !sdp->bypass) segment_count++;
	    sdp->segment_count = segment_count;
	    sdp->encode_flag = True;
	    continue;
	}
        if ( match(&string, "status=") || match(&string, "scsi_status=") ) {
	    sgp->errlog = False;
	    sdp->tci.check_status = True;
	    if ( isalpha(*string) ) {
		int scsi_status = LookupScsiStatus(string);
		if (scsi_status >= 0) {
		    sdp->tci.exp_scsi_status = (uint8_t)scsi_status;
		} else {
		    Eprintf(sdp, "Invalid status name '%s'!\n", string);
		    return ( HandleExit(sdp, FATAL_ERROR) );
		}
	    } else {
		sdp->tci.exp_scsi_status = number(sdp, string, HEX_RADIX);
	    }
	    continue;
	}
        if ( match(&string, "skey=") || match(&string, "sense_key=") ) {
	    sgp->errlog = False;
	    sdp->tci.check_status = True;
	    if (sdp->tci.exp_scsi_status == SCSI_GOOD) {
		sdp->tci.exp_scsi_status = SCSI_CHECK_CONDITION;
	    }
	    if ( isalpha(*string) ) {
		int sense_key = LookupSenseKey(string);
		if (sense_key >= 0) {
		    sdp->tci.exp_sense_key = (uint8_t)sense_key;
		} else {
		    Eprintf(sdp, "Invalid sense key name '%s'!\n", string);
		    return ( HandleExit(sdp, FATAL_ERROR) );
		}
	    } else {
		sdp->tci.exp_sense_key = number(sdp, string, HEX_RADIX);
	    }
	    continue;
	}
	if (match (&string, "asc=")) {
	    sgp->errlog = False;
	    sdp->tci.check_status = True;
	    if (sdp->tci.exp_scsi_status == SCSI_GOOD) {
		sdp->tci.exp_scsi_status = SCSI_CHECK_CONDITION;
	    }
	    sdp->tci.exp_sense_asc = number(sdp, string, HEX_RADIX);
	    continue;
	}
	if (match (&string, "asq=")) {
	    sgp->errlog = False;
	    sdp->tci.check_status = True;
	    if (sdp->tci.exp_scsi_status == SCSI_GOOD) {
		sdp->tci.exp_scsi_status = SCSI_CHECK_CONDITION;
	    }
	    sdp->tci.exp_sense_asq = number(sdp, string, HEX_RADIX);
	    continue;
	}
	if (match (&string, "resid=")) {
	    sdp->tci.check_resid = True;
	    sdp->tci.exp_residual = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if (match (&string, "transfer=")) {
	    sdp->tci.check_xfer = True;
	    sdp->tci.exp_transfer = number(sdp, string, ANY_RADIX);
	    continue;
	}
        if (match (&string, "sname=")) {
	    if (sdp->scsi_name && sdp->user_sname) free(sdp->scsi_name);
            sgp->cdb_name = sdp->scsi_name = strdup(string);
	    sdp->user_sname = True;
            continue;
        }
	if (match (&string, "sleep=")) {
	    sdp->sleep_value = (uint32_t)time_value(sdp, string);
	    continue;
	}
	if (match (&string, "msleep=")) {
	    sdp->msleep_value = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if (match (&string, "usleep=")) {
	    sdp->usleep_value = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if (match (&string, "slices=")) {
	    sdp->slices = number(sdp, string, ANY_RADIX);
	    sdp->encode_flag = True;
	    continue;
	}
	if (match (&string, "threads=")) {
	    sdp->threads = number(sdp, string, ANY_RADIX);
	    continue;
	}
        if (match (&string, "timeout=")) {
            sgp->timeout = (uint32_t)mstime_value(sdp, string);
            continue;
        }
	/* Options for token based xcopy. */
	if (match (&string, "listid=")) {
	    iop->list_identifier = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if (match (&string, "rod_timeout=")) {
	    sdp->rod_inactivity_timeout = number(sdp, string, ANY_RADIX);
	    continue;
	}
	if ( match (&string, "exit") || match (&string, "quit") ) {
	    ExitFlag = True;
	    continue;
	}
        if (match (&string, "help")) {
            Help(sdp);
	    return ( HandleExit(sdp, SUCCESS) );
        }
	if (match (&string,  "showopcodes")) {
	    ShowScsiOpcodes(sdp);
	    return ( HandleExit(sdp, SUCCESS) );
	}
	/*
	 * Implement a few useful commands Scu supports. 
	 */
	if (match (&string, "eval")) {
	    char *expr = concatenate_args(sdp, argc, argv, ++i);
	    if (expr) {
		uint64_t value;
		value = large_number(sdp, expr, ANY_RADIX);
		show_expression(sdp, value);
		Free(sdp, expr);
	    }
	    return ( HandleExit(sdp, SUCCESS) );
	}
	if ( match(&string, "system") || match(&string, "shell") ) {
	    char *cmd = concatenate_args(sdp, argc, argv, ++i);
	    if (cmd) {
		(void)DoSystemCommand(sdp, cmd);
		Free(sdp, cmd);
	    } else {
		(void)StartupShell(sdp, NULL);
	    }
	    return ( HandleExit(sdp, SUCCESS) );
	}
	if (match (&string, "!")) {
	    char *cmd = concatenate_args(sdp, argc, argv, i);
	    if (cmd) {
		(void)DoSystemCommand(sdp, (cmd + 1));
		Free(sdp, cmd);
	    }
	    return ( HandleExit(sdp, SUCCESS) );
	}
        if (match (&string, "version")) {
            Version(sdp);
	    return ( HandleExit(sdp, SUCCESS) );
        }
	/*
	 * I/O Options:
	 */ 
	if (match (&string, "blocks=")) {
	    siop->cdb_blocks = large_number(sdp, string, ANY_RADIX);
	    sdp->encode_flag = True;
	    sdp->verbose = False;
	    continue;
	}
	if (match (&string, "bs=")) {
	    uint64_t bytes = large_number(sdp, string, ANY_RADIX);
	    if (iop->device_size) {
		siop->cdb_blocks = howmany(bytes, iop->device_size);
	    }
	    sdp->encode_flag = True;
	    sdp->verbose = False;
	    continue;
	}
	if (match (&string, "limit=")) {
	    siop->data_limit = large_number(sdp, string, ANY_RADIX);
	    sdp->encode_flag = True;
	    sdp->verbose = False;
	    continue;
	}
	if (match (&string, "log=")) {
	    if (sdp->log_file) Free(sdp, sdp->log_file);
	    sdp->log_file = strdup(string);
	    sdp->logheader_flag = True;
	    continue;
	}
	if (match (&string, "logprefix=")) {
	    if (sdp->log_prefix) Free(sdp, sdp->log_prefix);
	    sdp->log_prefix = strdup(string);
	    continue;
	}
	if (match (&string, "lba=")) {
	    siop->starting_lba = large_number(sdp, string, ANY_RADIX);
	    siop->ending_lba = (siop->starting_lba + 1);
	    sdp->encode_flag = True;
	    continue;
	}
	if (match (&string, "maxbad=")) {
	    sdp->max_bad_blocks = (uint32_t)number(sdp, string, ANY_RADIX);
            continue;
        }
	if (match (&string, "step=")) {
	    siop->step_value = large_number(sdp, string, ANY_RADIX);
	    sdp->encode_flag = True;
	    sdp->verbose = False;
	    continue;
	}
	if (match (&string, "starting=")) {
	    siop->user_starting_lba = True;
	    siop->starting_lba = large_number(sdp, string, ANY_RADIX);
	    sdp->encode_flag = True;
	    sdp->verbose = False;
	    continue;
	}
	if (match (&string, "ending=")) {
	    siop->ending_lba = large_number(sdp, string, ANY_RADIX);
	    sdp->encode_flag = True;
	    sdp->verbose = False;
	    continue;
	}
	if (match (&string, "rod_token=")) {
	    if (sdp->rod_token_file) free(sdp->rod_token_file);
	    sdp->rod_token_file = strdup(string);
	    /* Used by Populate Token and Receive ROD Token Info. */
	    sdp->decode_flag = True;
	    sdp->encode_flag = True;
	    continue;
	}
	/* A simple way to set some environment variables for scripts! */
	if ( match(&string, "$") && (p = strstr(string, "=")) ) {
	    *p++ = '\0';
	    // int setenv(const char *envname, const char *envval, int overwrite);
	    if (setenv(string, p, TRUE)) {
		Perror(sdp, "setenv() of envname=%s, envvar=%s failed!", string, p);
		return ( HandleExit(sdp, FATAL_ERROR) );
	    }
	    continue;
	}
	if (sdp->script_level) {
	    int level = (sdp->script_level - 1);
	    Eprintf(sdp, "Parsing error in script '%s' at line number %d\n",
		    sdp->script_name[level], sdp->script_lineno[level]);
	}
        Eprintf(sdp, "Invalid option '%s' specified, use 'help' for valid options.\n",
                                                                    string);
	return ( HandleExit(sdp, FATAL_ERROR) );
    }
    /*
     * Option Sanity Checks:
     */
    if (sdp->tci.wait_for_status && !sdp->tci.check_status) {
	Eprintf(sdp, "Please specify the SCSI status to wait for!\n");
	return ( HandleExit(sdp, FATAL_ERROR) );
    }
    return (SUCCESS);
}

/*
 * parse_exp_data() - Parse the expected data keywords.
 *
 * Inputs:
 *	string = The string to parse.
 *	sdp = The SCSI device information.
 *
 * Outputs:
 *	(global) string = Updated input argument pointer.
 *
 * Return Value:
 *	Returns SUCCESS / FAILURE
 */ 
static int
parse_exp_data(char *string, scsi_device_t *sdp)
{
    uint32_t	byte_index;
    exp_data_t	*edp;
    char	*str, *strp, *token;
    int		i;

    if (sdp->exp_data_count == sdp->exp_data_entries) {
	if (expand_exp_data(sdp) == FAILURE) return (FAILURE);
    }
    if (sdp->exp_data == NULL) {
	sdp->exp_data = Malloc(sdp, sdp->exp_data_size);
	if (sdp->exp_data == NULL) return (FAILURE);
    }

    /* Copy string to avoid clobbering dur to strtok() API! */
    str = strp = strdup(string);
    /* Note: Remember match() updates "str"! */
    if ( match(&str, "C:") || match(&str, "CHAR:")) {
	/* Format: C[HAR]:index:string,string */
	token = strtok(str, ":");
	if (token == NULL) goto parse_error;
	byte_index = number(sdp, token, sdp->exp_radix);
	token = strtok(NULL, ",");
	if (token == NULL) goto parse_error;
	while (token != NULL) {
	    while (*token) {
		if (sdp->exp_data_count == sdp->exp_data_entries) {
		    if (expand_exp_data(sdp) == FAILURE) {
			goto error_return;
		    }
		}
		edp = &sdp->exp_data[sdp->exp_data_count];
		edp->exp_byte_index = byte_index++;
		edp->exp_byte_value = *token;
		sdp->exp_data_count++;
		token++;
	    }
	    token = strtok(NULL, ",");
	}
    } else if ( match(&str, "B:") || match(&str, "BYTE:")) {
	uint32_t value;
	/* Format: B[YTE]:index:value,value... */
	token = strtok(str, ":");
	if (token == NULL) goto parse_error;
	byte_index = number(sdp, token, sdp->exp_radix);
	token = strtok(NULL, ",");
	if (token == NULL) goto parse_error;
	while (token != NULL) {
	    if (sdp->exp_data_count == sdp->exp_data_entries) {
		if (expand_exp_data(sdp) == FAILURE) {
		    goto error_return;
		}
	    }
	    value = number(sdp, token, sdp->exp_radix);
	    if (value > 0xFF) {
		Eprintf(sdp, "Byte value %u is too large!\n", (uint32_t)value);
		goto error_return;
	    }
	    edp = &sdp->exp_data[sdp->exp_data_count];
	    edp->exp_byte_index = byte_index++;
	    edp->exp_byte_value = value;
	    sdp->exp_data_count++;
	    token = strtok(NULL, ",");
	}
    } else if ( match(&str, "S:") || match(&str, "SHORT:")) {
	uint32_t value;
	/* Format: S[HORT]:index:value,value... */
	token = strtok(str, ":");
	if (token == NULL) goto parse_error;
	byte_index = number(sdp, token, sdp->exp_radix);
	token = strtok(NULL, ",");
	if (token == NULL) goto parse_error;
	while (token != NULL) {
	    value = number(sdp, token, sdp->exp_radix);
	    if (value > 0xFFFF) {
		Eprintf(sdp, "Short value %u is too large!\n", (uint32_t)value);
		goto error_return;
	    }
	    for (i = sizeof(uint16_t)-1; i >= 0; i--) {
		if (sdp->exp_data_count == sdp->exp_data_entries) {
		    if (expand_exp_data(sdp) == FAILURE) {
			goto error_return;
		    }
		}
		edp = &sdp->exp_data[sdp->exp_data_count];
		edp->exp_byte_index = byte_index++;
		edp->exp_byte_value = LTOB(value,i);
		sdp->exp_data_count++;
	    }
	    token = strtok(NULL, ",");
	}
    } else if ( match(&str, "W:") || match(&str, "WORD:")) {
	uint32_t value;
	/* Format: W[ORD]:index:value,value... */
	token = strtok(str, ":");
	if (token == NULL) goto parse_error;
	byte_index = number(sdp, token, sdp->exp_radix);
	token = strtok(NULL, ",");
	if (token == NULL) goto parse_error;
	while (token != NULL) {
	    value = number(sdp, token, sdp->exp_radix);
	    for (i = sizeof(uint32_t)-1; i >= 0; i--) {
		if (sdp->exp_data_count == sdp->exp_data_entries) {
		    if (expand_exp_data(sdp) == FAILURE) {
			goto error_return;
		    }
		}
		edp = &sdp->exp_data[sdp->exp_data_count];
		edp->exp_byte_index = byte_index++;
		edp->exp_byte_value = LTOB(value,i);
		sdp->exp_data_count++;
	    }
	    token = strtok(NULL, ",");
	}
    } else if ( match(&str, "L:") || match(&str, "LONG:")) {
	uint64_t value;
	/* Format: L[ONG]:index:value,value... */
	token = strtok(str, ":");
	if (token == NULL) goto parse_error;
	byte_index = number(sdp, token, sdp->exp_radix);
	token = strtok(NULL, ",");
	if (token == NULL) goto parse_error;
	while (token != NULL) {
	    value = large_number(sdp, token, sdp->exp_radix);
	    for (i = sizeof(uint64_t)-1; i >= 0; i--) {
		if (sdp->exp_data_count == sdp->exp_data_entries) {
		    if (expand_exp_data(sdp) == FAILURE) {
			goto error_return;
		    }
		}
		edp = &sdp->exp_data[sdp->exp_data_count];
		edp->exp_byte_index = byte_index++;
		edp->exp_byte_value = LTOB(value,i);
		sdp->exp_data_count++;
	    }
	    token = strtok(NULL, ",");
	}
    } else {
	goto parse_error;
    }
    if (strp) Free(sdp, strp);
    return (SUCCESS);

parse_error:
    Fprintf(sdp, "Format is: exp=type:byte_index:[string|value]\n");
    Fprintf(sdp, "Where 'type' is: C[CHAR], B[YTE], S[HORT], W[ORD], or L[ONG]\n");
    Fprintf(sdp, "Note: The max byte index is 32 bits, the max entries is %d.\n",
	    sdp->exp_data_entries);
error_return:
    if (strp) Free(sdp, strp);
    return (FAILURE);
}

/*
 * expand_exp_data() - Expand the expected data bytes buffer.
 * 
 * Description:
 * 	This buffer starts a predefined size (64), but if the user needs
 * more bytes, we'll dynamically expand the space as required.
 * 
 * Inputs:
 * 	sdp = The SCSI device information.
 * 
 * Outputs:
 * 	exp_data, exp_data_count, and exp_data_size adjusted accordingly.
 */ 
static int
expand_exp_data(scsi_device_t *sdp)
{
    exp_data_t	*edp = sdp->exp_data;
    size_t	data_size = sdp->exp_data_size;
    
    sdp->exp_data_entries *= 2;	/* Each call, we'll double our entries. */
    sdp->exp_data_size = (sizeof(exp_data_t) * sdp->exp_data_entries);
    sdp->exp_data = Malloc(sdp, sdp->exp_data_size);
    if (sdp->exp_data == NULL) return (FAILURE);
    /* Copy the previous entries. */
    memcpy(sdp->exp_data, edp, data_size);
    free(edp);
    return (SUCCESS);
}

/*
 * match() - Match a Substring within a String.
 *
 * Inputs:
 *	sptr = Pointer to string pointer.
 *	s = The substring to match.
 *
 * Outputs:
 *	sptr = Points past substring (on match).
 *
 * Return Value:
 *	Returns TRUE/FALSE = Match / Not Matched
 */
static hbool_t
match (char **sptr, char *s)
{
    char *cs;

    cs = *sptr;
    while (*cs++ == *s) {
	if (*s++ == '\0') {
	    goto done;
	}
    }
    if (*s != '\0') {
	return(FALSE);
    }
done:
    cs--;
    *sptr = cs;
    return(TRUE);
}

char *
concatenate_args(scsi_device_t *sdp, int argc, char **argv, int arg_index)
{
    char *buffer, *bp, *string;
    if (arg_index >= argc) return(NULL);
    buffer = bp = Malloc(sdp, KBYTE_SIZE);
    if (buffer == NULL) return(NULL);
    while (arg_index < argc) {
	string = argv[arg_index++];
	bp += sprintf(bp, "%s ", string);
    }
    if ( strlen(buffer) ) bp--;
    if (*bp == ' ') *bp = '\0';
    return(buffer);
}

void
show_expression(scsi_device_t *sdp, uint64_t value)
{
    double blocks, kbytes, mbytes, gbytes, tbytes;
    char blocks_buf[32], kbyte_buf[32], mbyte_buf[32], gbyte_buf[32], tbyte_buf[32];

    blocks = ( (double)value / (double)BLOCK_SIZE);
    kbytes = ( (double)value / (double)KBYTE_SIZE);
    mbytes = ( (double)value / (double)MBYTE_SIZE);
    gbytes = ( (double)value / (double)GBYTE_SIZE);
    tbytes = ( (double)value / (double)TBYTE_SIZE);

    (void)sprintf(blocks_buf, "%f", blocks);
    (void)sprintf(kbyte_buf, "%f", kbytes);
    (void)sprintf(mbyte_buf, "%f", mbytes);
    (void)sprintf(gbyte_buf, "%f", gbytes);
    (void)sprintf(tbyte_buf, "%f", tbytes);

    if (sdp->verbose) {
	Print(sdp, "Expression Values:\n");
	Print(sdp, "            Decimal: " LUF " \n", value);
	Print(sdp, "        Hexadecimal: " LXF " \n", value);
	Print(sdp, "    512 byte Blocks: %s\n", blocks_buf);
	Print(sdp, "          Kilobytes: %s\n", kbyte_buf);
	Print(sdp, "          Megabytes: %s\n", mbyte_buf);
	Print(sdp, "          Gigabytes: %s\n", gbyte_buf);
	Print(sdp, "          Terabytes: %s\n", tbyte_buf);
    } else {
	Print(sdp, "Dec: " LUF " Hex: " LXF " Blks: %s Kb: %s Mb: %s Gb: %s, Tb: %s\n",
	      value, value, blocks_buf, kbyte_buf, mbyte_buf, gbyte_buf, tbyte_buf);
    }
    return;
}

/*
 * number() - Converts ASCII string into numeric value.
 *
 * Inputs:
 *	str = The string to convert.
 *	base = The base for numeric conversions.
 *
 * Return Value:
 *      Returns converted number, exits on invalid numbers.
 */
static uint32_t
number(scsi_device_t *sdp, char *str, int base)
{
    char *eptr;
    uint32_t value;

    value = CvtStrtoValue(str, &eptr, base);

    if (*eptr != '\0') {
        Eprintf(sdp, "Error parsing '%s', invalid character detected in number: '%c'\n",
		str, *eptr);
	return ( HandleExit(sdp, FATAL_ERROR) );
    }
    return(value);
}

static uint64_t
large_number(scsi_device_t *sdp, char *str, int base)
{
    char *eptr;
    uint64_t value;

    value = CvtStrtoLarge(str, &eptr, base);

    if (*eptr != '\0') {
        Eprintf(sdp, "Error parsing '%s', invalid character detected in number: '%c'\n",
		str, *eptr);
	return ( HandleExit(sdp, FATAL_ERROR) );
    }
    return(value);
}

static time_t
mstime_value(scsi_device_t *sdp, char *str)
{
    char *eptr;
    time_t value;

    value = CvtTimetoMsValue(str, &eptr);

    if (*eptr != '\0') {
        Eprintf(sdp, "Error parsing '%s', invalid character detected in number: '%c'\n",
		str, *eptr);
	return ( HandleExit(sdp, FATAL_ERROR) );
    }
    return(value);
}

static time_t
time_value(scsi_device_t *sdp, char *str)
{
    char *eptr;
    time_t value;

    value = CvtTimetoValue(str, &eptr);

    if (*eptr != '\0') {
        Eprintf(sdp, "Error parsing '%s', invalid character detected in number: '%c'\n",
		str, *eptr);
	return ( HandleExit(sdp, FATAL_ERROR) );
    }
    return(value);
}

/*
 * sptGetCommandLine() - Get Command Line to Execute.
 *
 * Description:
 *	This function gets the next command line to execute.  This can
 * come from the user or from a command file.
 *
 * Inputs:
 *	sdp = The SCSI device pointer.
 *
 * Outputs:
 *	Fills in the parser control block with argument count and list.
 *	Returns SUCCESS / FAILURE.
 */
int
sptGetCommandLine(scsi_device_t *sdp)
{
    char *bufptr, *p;
    size_t bufsiz;
    FILE *stream;
    int status;

    /*
     * Done here, since not required for single CLI commands. 
     */
    if (sdp->cmdbufptr == NULL) {
       	sdp->cmdbufsiz = ARGS_BUFFER_SIZE;
	if ( !(sdp->cmdbufptr = Malloc(sdp, sdp->cmdbufsiz)) ) {
	    MyExit(sdp, FATAL_ERROR);
	    
	}
	/*
	 * Allocate an array of pointers for parsing arguments.
	 * Note: This overrides what arrived via the CLI in main()!
	 */ 
	sdp->argv = (char **)Malloc(sdp,  (sizeof(char **) * ARGV_BUFFER_SIZE) );
	if (sdp->argv == NULL) {
	    MyExit(sdp, FATAL_ERROR);
	}
    }
reread:
    bufptr = sdp->cmdbufptr;
    bufsiz = sdp->cmdbufsiz;
    status = SUCCESS;
    if (sdp->script_level) {
	stream = sdp->sfp[sdp->script_level-1];
    } else {
	stream = stdin;
	if (InteractiveFlag) {
	    Print(sdp, "%s> ", OurName); (void)fflush(stdout);
	}
    }
    if (PipeModeFlag) {
	EmitStatus(sdp, sdp->emit_status);
    }
read_more:
    if (fgets (bufptr, bufsiz, stream) == NULL) {
	if (feof (stream) != 0) {
	    status = END_OF_FILE;
	    if (stream != stdin) {
		CloseScriptFile(sdp);
		if (sdp->script_level || InteractiveFlag) {
		    goto reread;
		} else {
		    return (status);
		}
	    }
	} else {
	    status = FAILURE;
	}
	Print(sdp, "\n");
	clearerr (stream);
	return (status);
    }
    if (stream != stdin) {
	sdp->script_lineno[sdp->script_level-1]++;
    }
    /*
     * Handle continuation lines.
     */ 
    if (p = strrchr(bufptr, '\n')) {
	--p;
	if ( (p > bufptr) && (*p == '\\') ) {
	    *p = '\0';
	    bufsiz -= strlen(bufptr);
	    if (bufsiz) {
		bufptr = p;
		if (InteractiveFlag && !sdp->script_level) {
		    Print(sdp, "> "); (void)fflush(stdout);
		}
		goto read_more;
	    }
	}
    }
    status = ExpandEnvironmentVariables(sdp, sdp->cmdbufptr, sdp->cmdbufsiz);
    /*
     * Display the expanded command line. (messy, try to cleanup!)
     */ 
    if ( ((InteractiveFlag || DebugFlag) && sdp->script_level) ||
	 (!StdinIsAtty && PipeModeFlag) ) {
	if (/*DebugFlag &&*/ sdp->script_level) {
	    Print(sdp, "%s> ", OurName); (void)fflush(stdout);
	}
	Print(sdp, "%s", sdp->cmdbufptr);
	(void)fflush(stdout);
    }
    if (status == SUCCESS) {
	sdp->argc = MakeArgList(sdp, sdp->argv, sdp->cmdbufptr);
    }
    return(status);
}

int
ExpandEnvironmentVariables(scsi_device_t *sdp, char *bufptr, size_t bufsiz)
{
    char    *from = bufptr;
    char    *bp, *to, *env, *p;
    size_t length = strlen(from);
    int     status = SUCCESS;

    if ( (p = strstr(from, "${")) == NULL) return(status);
    if ( (to = Malloc(sdp, bufsiz)) == NULL) return(FAILURE);
    bp = to;

    while (length > 0) {
	/*
	 * Parse variables of the form: ${var}
	 */
	if ( (*from == '$') && (*(from+1) == '{') ) {
	    char *var = (from + 2);
	    if ( (p = strchr(var, '}')) ) {
		int var_len = ((p - from) + 1);
		*p = '\0';
		env = getenv(var);
		*p = '}';
		if (env) {
		    size_t env_len;
		    env_len = strlen(env);
		    if ( (size_t)((to + env_len) - bp) < bufsiz) {
			to += Sprintf(to, "%s", env);
			length -= var_len;
			from += var_len;
			continue;
		    }
		} else {
		    Eprintf(sdp, "Failed to expand variable: %.*s\n", var_len, from);
		    return(FAILURE);
		}
	    }
	}
	*to++ = *from++;
	length--;
    }
    (void)strcpy(bufptr, bp);
    free(bp);
    return(status);
}

/*
 * MakeArgList() - Make Argument List from String.
 *
 * Description:
 *	This function creates an argument list from the string passed
 * in.  Arguments are separated by spaces and/or tabs and single and/or
 * double quotes may used to delimit arguments.
 *
 * Inputs:
 * 	sdp = The SCSI device pointer.
 *	argv = Pointer to argument list array.
 *	s = Pointer to string buffer to parse.
 *
 * Outputs:
 *    Initialized the argv array and returns the number of arguments parsed.
 */
int
MakeArgList(scsi_device_t *sdp, char **argv, char *s)
{
    int c, c1;
    char *cp;
    int nargs = 0;
    char *str = s;

    /* Check for a comment. */
    if (*s == '#') {
	return(nargs);
    }
    /*
     * Skip over leading tabs and spaces.
     */
    while ( ((c = *s) == ' ') ||
	    (c == '\t') ) {
	s++;
    }
    if ( (c == '\0') || (c == '\n') ) {
	return(nargs);
    }
    /*
     * Strip trailing tabs and spaces.
     */
    for (cp = s; ( ((c1 = *cp) != '\0') && (c1 != '\n') ); cp++)
	;
    do {
	*cp-- = '\0';
    } while ( ((c1 = *cp) == ' ') || (c1 == '\t') );

    *argv++ = s;
    for (c = *s++; ; c = *s++) {

	switch (c) {
	    
	    case '\t':
	    case ' ':
		*(s-1) = '\0';
		while ( ((c = *s) == ' ') ||
			(c == '\t') ) {
		    s++;
		}
		*argv++ = s;
		nargs++;
		break;

	    case '\0':
	    case '\n':
		nargs++;
		*argv = 0;
		*(s-1) = '\0';
		return(nargs);

	    case '"':
	    case '\'': {
		/* Remove the quoting. */
		char *to = (s-1);
		while ( (c1 = *to++ = *s++) != c) {
		    if ( (c1 == '\0') || (c1 == '\n') ) {
			Printf (sdp, "Missing trailing quote parsing: %s\n", str);
			return(-1);
		    }
		}
		*(to-1) = '\0';
		break;
	    }
	    default:
		break;
	}
    }
}

/*
 * ReadDataFile() - Read Data from a Disk File (or stdin).
 *
 * Inputs:
 * 	sdp    = The SCSI device pointer.
 *	fd     = Pointer to file descriptor. (already open)
 *	bufptr = Pointer to return data buffer allocated.
 *      lenptr = Pointer to the buffer length.
 *
 * Outputs:
 *      Returns 0 / -1 for Success / Failure.
 */
int
ReadDataFile(
    scsi_device_t *sdp,
    HANDLE	  fd,
    void  	  **bufptr,
    size_t	  *lenptr )
{
    hbool_t eof = False;
    size_t  len;
    ssize_t count;
    unsigned char *bp;

    /*
     * If length isn't set, use the file size.
     */
    if (*lenptr == (size_t) 0) {
#if defined(_WIN32)
        if (GetFileType(fd) == FILE_TYPE_DISK) {
            *lenptr = GetFileSize(fd, NULL);
        } else {
            Eprintf(sdp, "Expect regular file for data file!\n");
            return (FAILURE);
        }
#else /* !defined(_WIN32) */
        struct stat sb;
        if (fstat (fd, &sb) < 0) {
            Perror (sdp, "fstat() failed");
            return (FAILURE);
        }
        *lenptr = (size_t) sb.st_size;
#endif /* defined(_WIN32) */
    }
    if (*bufptr) {
        free_palign(sdp, *bufptr); /* Free previous buffer. */
    }
    *bufptr = malloc_palign(sdp, *lenptr, 0);
    bp = *bufptr;
    len = *lenptr;
    /*
     * Loop reading data up to the length or EOF.
     */
    while ( (len > 0) && (eof == False) ) {
#if defined(_WIN32)
      {
        BOOL bResult;
	bResult = ReadFile(fd, bp, len, &count, NULL);
	if (bResult && (count == 0)) {
	    eof = True;
	} else if (!bResult) {
	    count = -1;	/* Mimic Unix error return. */
	}
      }
#else /* !defined(_WIN32) */
	count = read(fd, bp, len);
	if (count == 0) {
	    eof = True;
	}
#endif /* !defined(_WIN32) */
	if (count == FAILURE) {
	    os_perror (sdp, "File read failed while reading %u bytes!", *lenptr);
	    return (FAILURE);
	}
	/* Prepare for next read. */
	bp += count;
	len -= count;
	//Fprintf(sdp, "Debug: read %u bytes, %u remaining...\n", count, len);
    }
    if (len) {
	Wprintf(sdp, "Attempted to read %d bytes, read only %d bytes.\n",
		*lenptr, (*lenptr - len));
	*lenptr -= len; /* Return the length read. */
	return (WARNING);
    }
    return (SUCCESS);
}

/*
 * DoErrorControl() - Do Error Control Processing.
 *
 * Description:
 *      This function is called after each command to process error
 * control attributes.
 *
 * Inputs:
 *      sdp = The SCSI device pointer.
 *	status = The last command status.
 *
 * Return Value:
 *	Returns 1 / -1 = Continue / Stop.
 */
int
DoErrorControl(scsi_device_t *sdp, int status)
{
    if (CmdInterruptedFlag) return(FAILURE);
    if ( (status == SUCCESS) || (status == WARNING) ) {
	return(CONTINUE);
    }
    if (sdp->onerr == ONERR_CONTINUE) {
	return (CONTINUE);
    } else {
	return (FAILURE);
    }
}

int
do_common_thread_startup(scsi_device_t *sdp)
{
    int status = SUCCESS;
 
#if 0
    /* Note: Now done as part of thread startup to avoid races! */
    if (sdp->async_job && (sdp->initial_state == IS_PAUSED) ) {
	sdp->thread_state = TS_PAUSED;
    } else {
	sdp->thread_state = TS_RUNNING;
    }
    ignore_signals(sdp);
    status = acquire_job_lock(sdp, sdp->job);
    if (status == SUCCESS) {
	status = release_job_lock(sdp, sdp->job);
    }
    sdp->program_start = time((time_t) 0);
    /* Prime the keepalive time, if enabled. */
    if (sdp->keepalive_time) {
	sdp->last_keepalive = time((time_t *)0);
    }
#endif /* 0 */
    if (sdp->log_file) {
	if ( (status = create_unique_thread_log(sdp)) == FAILURE) {
	    return(status);
	}
    } else if (sdp->logheader_flag && (sdp->thread_number == 1) ) {
	/* Log the header for the first thread.*/
	log_header(sdp);
    }
    
    /* Display SCSI information, if enabled. */
    /* Logs to all thread logs, otherwise only 1st thread! */
    if ( (sdp->scsi_info_flag == True) &&
	 (sdp->log_file || (sdp->thread_number == 1)) ) {
	report_scsi_information(sdp);
    }
    return(status);
}

/*
 * This function creates a unique log file name.
 * 
 * Inputs:
 * 	sdp = The SCSI device information pointer.
 * 
 * Return Value:
 * 	Returns SUCCESS / FAILURE if log open failed.
 */
int
create_unique_thread_log(scsi_device_t *sdp)
{
    int status = SUCCESS;
    hbool_t make_unique_log_file = True;

    /*
     * For a single thread use the log file name, unless told to be unique.
     */ 
    if ( (sdp->threads <= 1) && (sdp->unique_log == False) ) {
	make_unique_log_file = False;
    }
    if (make_unique_log_file) {
	char logfmt[STRING_BUFFER_SIZE];
	/*
	 * Create a unique log file per thread.
	 */
	strcpy(logfmt, sdp->log_file);
	/* Add default postfix, unless user specified their own via "%". */
	if ( strstr(sdp->log_file, "%") == (char *) 0 ) {
	    strcat(logfmt, sdp->file_sep);
	    strcat(logfmt, sdp->file_postfix);
	}
	sdp->log_file = FmtLogFile(sdp, logfmt, True);
    }
    if (sdp->debug_flag) {
	Printf(NULL, "Job %u, Thread %d, log file is '%s'...\n",
	       sdp->job_id/*sdp->job->ji_job_id*/, sdp->thread_number, sdp->log_file);
    }
    status = do_logfile_open(sdp);
    return(status);
}

int
do_logfile_open(scsi_device_t *sdp)
{
    int status = SUCCESS;
    char *mode = "w";
    FILE *fp;

    if ( strchr(sdp->log_file, '%') ) {
	char *path = FmtLogFile(sdp, sdp->log_file, True);
	if (path) {
	    Free(sdp, sdp->log_file);
	    sdp->log_file = path;
	}
    }
    fp = fopen(sdp->log_file, mode);
    if (fp == NULL) {
	Perror(sdp, "fopen() of %s failed", sdp->log_file);
	status = FAILURE;
    } else {
	sdp->log_opened = True;
	sdp->ofp = sdp->efp = fp;
	if (sdp->logheader_flag) {
	    /* Messy, but don't want this logged to job log too! */
	    sdp->joblog_inhibit = True;
	    log_header(sdp);
	    sdp->joblog_inhibit = False;
	}
    }
    return(status);
}

void
log_header(scsi_device_t *sdp)
{
    /*
     * Write the command line for future reference.
     */
    Printf(sdp, "Command Line:\n");
    Printf(sdp, "\n");
    Printf(sdp, "    %c %s", getuid() ? '%' : '#', sdp->cmd_line);
    Printf(sdp, "\n");
    Printf(sdp, "\t--> %s <--\n", version_str);
    Printf(sdp, "\n");
    return;
}


/*
 * Convert options array into the command string.
 */
char *
make_options_string(scsi_device_t *sdp, int argc, char **argv, hbool_t quoting)
{
    int arg;
    char *bp;
    char *buffer, *options;
    
    bp = buffer = Malloc(sdp, LOG_BUFSIZE);
    if (bp == NULL) return(bp);

    for (arg = 0; arg < argc; arg++) {
	char *opt = argv[arg];
	char *space = strchr(opt, ' ');
	/* Embedded spaces require quoting. */
	//if (quoting && space) {
	if (space) {
	    char *dquote = strchr(opt, '"');
	    char *equals = strchr(opt, '=');
	    char quote = (dquote) ? '\'' : '"';
	    char *p = opt;
	    /* Adding quoting after the option= */
	    if (equals) {
		/* Copy to and including equals sign. */
		do {
		    *bp++ = *p++;
		} while (*(p -1) != '=');
	    }
	    /* TODO: Smarter handling of quotes! */
	    /* Note: Windows removes double quotes. */
	    bp += sprintf(bp, "%c%s%c ", quote, p, quote);
	} else {
	    bp += sprintf(bp, "%s ", opt);
	}
    }
    if (bp > buffer) {
	bp--;
	if (*bp == ' ') *bp = '\0';
    }
    options = Malloc(sdp, strlen(buffer) + 1);
    strcpy(options, buffer);
    Free(sdp, buffer);
    return(options);
}

void
save_cmdline(scsi_device_t *sdp)
{
    char buffer[LOG_BUFSIZE];
    char *options;

    options = make_options_string(sdp, sdp->argc, sdp->argv, True);
    if (options == NULL) return;
    /* Include the spt path with the options. */
    (void)sprintf(buffer, "%s %s\n", sptpath, options);
    if (sdp->cmd_line) Free(sdp, sdp->cmd_line);
    sdp->cmd_line = strdup(buffer);
    Free(sdp, options);
    return;
}

/*
 * init_device_information() - Initialize The Device Information Structure.
 *
 * Return Value:
 *      Returns a pointer to the device information data structure.
 */
static scsi_device_t *
init_device_information(void)
{
    scsi_device_t *sdp;
    
    sdp = (scsi_device_t *)malloc( sizeof(*sdp) );
    if (sdp == NULL) {
	printf("ERROR: We failed to allocate the initial device information of %u bytes!\n",
	       (unsigned)sizeof(*sdp));
	return(NULL);
    }
    memset(sdp, '\0', sizeof(*sdp));
    sdp->efp = efp; // = stderr;
    sdp->ofp = ofp; // = stdout;
    sdp->dir_sep = DIRSEP;
    sdp->file_sep = strdup(DEFAULT_FILE_SEP);
    sdp->file_postfix = strdup(DEFAULT_FILE_POSTFIX);
    //sdp->log_bufsize	= LOG_BUFSIZE;
    //sdp->log_buffer = malloc(sdp->log_bufsize);
    //sdp->log_bufptr = sdp->log_buffer;

    //init_device_defaults(sdp);

    return (sdp);
}
