/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2021			    *
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
 * File:	spt_usage.c
 * Author:	Robin T. Miller
 * Date:	September 27th, 2006
 *
 * Description:
 *	This utility permits sending any SCSI command desired.
 *
 * Modification History:
 * 
 * May 9th, 2010 by Robin T. Miller
 * 	Initial creation, moving usage here from spt.c
 */
#include <stdio.h>
#include <sys/types.h>

#include "spt.h"
#include "spt_version.h"

/*
 * Version, usage, and help text.
 */
#define P       Print
#define D       Fprint

void
Usage(scsi_device_t *sdp)
{
    D (sdp, "Usage: %s options...\n", OurName);
    D (sdp, " Type '%s help' for a list of valid options.\n", OurName);
    return;
}

void
Version(scsi_device_t *sdp)
{
    if (sdp->output_format == JSON_FMT) {
	char text[STRING_BUFFER_SIZE];
	char *tp = text;
	tp += sprintf(tp, "{ \"Author\": \"%s\", ", ToolAuthor);
	tp += sprintf(tp, "\"Date\": \"%s\", ", ToolDate);
	tp += sprintf(tp, "\"Version\": \"%s\" }", ToolRevision);
	P(sdp, "%s\n", text);
    } else {
	P (sdp, "    --> " ToolVersion " <--\n");
    }
    return;
}

void Help(scsi_device_t *sdp)
{
    static char *enabled_str = "enabled";
    static char *disabled_str = "disabled";
    io_params_t	*iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;

    P (sdp, "Usage: %s options...\n", OurName);
    P (sdp, "\n    Where options are:\n");
    P (sdp, "\tdsf=device            The device special file.\n");
    P (sdp, "\tdsf1=device           The 2nd device special file.\n");
    P (sdp, "\tdin=filename          Data (in) file for reading.\n");
    P (sdp, "\tdout=filename         Data (out) file for writing.\n");
    
    P (sdp, "\tkeepalive=string      The keepalive message string.\n");
    P (sdp, "\tkeepalivet=time       The keepalive message frequency.\n");
    
    P (sdp, "\tlog=filename          The log file name to write.\n");
    P (sdp, "\tlogprefix=string      The per line logging prefix.\n");
    P (sdp, "\toutput-format=string  The output format to: ascii or json. (Default: %s)\n",
       (sdp->output_format == ASCII_FMT) ? "ascii" : "json");
    P (sdp, "\treport-format=string  The report format to: brief or full (Default: %s).\n",
       (sdp->report_format == REPORT_BRIEF) ? "brief": "full");
    
    P (sdp, "\taborts=value          Set the abort frequency.  (Default: 0)\n");
    P (sdp, "\tabort_timeout=value   Set the abort timeout.    (Default: %ums)\n",
       sdp->abort_timeout);
    P (sdp, "\tboff=string           Set the buffer offsets to: dec or hex (Default: %s)\n",
       (sdp->boff_format == DEC_FMT) ? "dec" : "hex");
    P (sdp, "\tdfmt=string           Set the data format to: byte or word (Default: %s)\n",
       (sdp->data_format == BYTE_FMT) ? "byte" : "word");
    P (sdp, "\tdlimit=value          Set the dump data buffer limit. (Default: %u)\n",
       sdp->dump_limit);
    P (sdp, "\temit=string OR        Emit status format control string.\n");
    P (sdp, "\temit={default|multi}  Default or multi devices emit strings.\n");
    P (sdp, "\texit or quit          Exit when running in pipe mode.\n");

    P (sdp, "\tcdb='hh hh ...'       The SCSI CDB to execute.\n");
    P (sdp, "\tcdbsize=value         The CDB size (overrides auto set).\n");
    P (sdp, "\tcapacity=value        Set the device capacity in bytes.\n");
    P (sdp, "\tcapacityp=value       Set capacity by percentage (range: 0-100).\n");
    P (sdp, "\tdir=direction         Data direction {none|read|write}.\n");
    P (sdp, "\tiomode=mode           Set I/O mode to: {copy, mirror, test, or verify}.\n");
    P (sdp, "\tlength=value          The data length to read or write.\n");
    P (sdp, "\top=string             The operation type (see below).\n");
    P (sdp, "\tmaxbad=value          Set maximum bad blocks to display. (Default: %d)\n",
       sdp->max_bad_blocks);
    P (sdp, "\tonerr=action          The error action: {continue or stop}.\n");
    P (sdp, "\tpage={value|string}   The page code (command specific).\n");
    P (sdp, "\tpath=value            The (MPIO) path to issue command.\n");
    P (sdp, "\tpattern=value         The 32 bit hex data pattern to use.\n");
    P (sdp, "\tpin='hh hh ...'       The parameter in data to compare.\n");
    P (sdp, "\tpout='hh hh ...'      The parameter data to send device.\n");
    P (sdp, "\tqtag=string           The queue tag message type (see below).\n");
    P (sdp, "\tranges=value          The number of range descriptors.\n");
    P (sdp, "\trepeat=value          The number of times to repeat a cmd.\n");
    P (sdp, "\tretry=value           The number of times to retry a cmd.\n");
    P (sdp, "\truntime=time          The number of seconds to execute.\n");
    P (sdp, "\tscript=filename       The script file name to execute.\n");
    P (sdp, "\treport-format=string  The report format: brief or full. (or rfmt=)\n");
    P (sdp, "\tshow devices [filters] Show SCSI devices (see filters below).\n");
    P (sdp, "\tshow scsi [filters]   Show SCSI sense errors (see filters below).\n");
    P (sdp, "\tsname=string          The SCSI opcode name (for errors).\n");
    P (sdp, "\tsleep=time            The sleep time (in seconds).\n");
    P (sdp, "\tmsleep=value          The msleep time (in milliseconds).\n");
    P (sdp, "\tusleep=value          The usleep time (in microseconds).\n");
    P (sdp, "\ttest option           Perform a diagnostic test (see below).\n");
    P (sdp, "\tthreads=value         The number of threads to execute.\n");
    P (sdp, "\ttimeout=value         The timeout value (in milliseconds).\n");
    P (sdp, "\tenable=flag,flag      Enable one or more flags (see below).\n");
    P (sdp, "\tdisable=flag          Disable one or more flags (see below).\n");
    P (sdp, "\tiotpass=value         Set the IOT pattern for specified pass.\n");
    P (sdp, "\tiotseed=value         Set the IOT pattern block seed value.\n");
    P (sdp, "\thelp                  Display this help text.\n");
    P (sdp, "\teval EXPR             Evaluate expression, show values.\n");
    P (sdp, "\tsystem CMD            Execute a system command.\n");
    P (sdp, "\t!CMD                  Same as above, short hand.\n");
    P (sdp, "\tshell                 Startup a system shell.\n");
    P (sdp, "\tversion               Display the version information.\n");
    P (sdp, "\tshowopcodes           Display the SCSI operation codes.\n");
    P (sdp, "\n    Note: din/dout file can be '-' for stdin/stdout.\n");

    P (sdp, "\n    Job Start Options:\n");
    P (sdp, "\ttag=string            Specify job tag when starting tests.\n");
    P (sdp, "\n    Job Control Options: (partial, compared to dt)\n");
    P (sdp, "\tjobs[:full][={jid|tag}] | [job=value] | [tag=string]\n");
    P (sdp, "\t                      Show all jobs or specified job.\n");
    P (sdp, "\twait[={jid|tag}] | [job=value] | [tag=string]\n");
    P (sdp, "\t                      Wait for all jobs or specified job.\n");

    P (sdp, "\n    Shorthand Commands:\n");
    P (sdp, "\tcopyparams            Show copy operating parameters.\n");
    P (sdp, "\tgetlbastatus          Show mapped/deallocated blocks.\n");
    P (sdp, "\tinquiry {page=value}  Show Inquiry or specific page.\n");
    P (sdp, "\tlogsense {page=value} Show Log pages supported or page.\n");
    P (sdp, "\tzerolog {page=value}  Zero all Log pages or specific page.\n");
    P (sdp, "\treadcapacity10        Show disk capacity (10 byte CDB).\n");
    P (sdp, "\treadcapacity16        Show disk capacity (16 byte CDB).\n");
    P (sdp, "\trequestsense          Show request sense information.\n");
    P (sdp, "\trtpg                  Report target port groups.\n");
    P (sdp, "\tread10                Read media (10 byte CDB).\n");
    P (sdp, "\tread16                Read media (16 byte CDB).\n");
    P (sdp, "\twrite10               Write media (10 byte CDB).\n");
    P (sdp, "\twrite16               Write media (16 byte CDB).\n");
    P (sdp, "\tverify10              Verify media (10 byte CDB).\n");
    P (sdp, "\tverify16              Verify media (16 byte CDB).\n");
    P (sdp, "\twritesame10           Write same (10 byte CDB).\n");
    P (sdp, "\twritesame16           Write same (16 byte CDB).\n");
    P (sdp, "\tunmap                 Unmap blocks.\n");
    P (sdp, "\txcopy                 Extended copy (VMware XCOPY).\n");
    P (sdp, "\twut or odx            Block ROD token (Windows ODX).\n");
    P (sdp, "\tzerorod               Zero ROD token (unmaps blocks).\n");
    P (sdp, "\n    Examples:\n");
    P (sdp, "\t# spt inquiry page=block_limits\n");
    P (sdp, "\t# spt logsense page=protocol\n");
    P (sdp, "\t# spt src=${SRC} dst=${DST} xcopy emit=multi limit=1g\n");
    P (sdp, "\t# spt src=${SRC} dst=${DST} wut ofmt=json emit=multi limit=1g\n");
    P (sdp, "\n");
    P (sdp, "    Note: Only a few Inquiry/Log pages are decoded today!\n");

    P (sdp, "\n    Storage Enclosure Services (SES) Specific Options:\n");
    P (sdp, "\telement_index=value   The element index.       (or element=)\n");
    P (sdp, "\telement_tcode=value   The element type code.   (or etcode=)\n");
    P (sdp, "\telement_scode=value   The element status code. (or escode=)\n");
    P (sdp, "\telement_type=string   The element type.        (or etype=)\n");
    P (sdp, "\telement_status=string The element status.      (or estatus=)\n");
    P (sdp, "\trcvdiag               Issue a receive diagnostic command.\n");
    P (sdp, "\tsenddiag              Issue a send diagnostic command.\n");
    P (sdp, "\tshowhelp              Show enclosure help text diagnostic page.\n");
    P (sdp, "\tses {clear|set}={devoff|fail/fault|ident/locate|unlock}\n");
    P (sdp, "\t                      Modify SES control elements.\n");
    P (sdp, "\n    Examples:\n");
    P (sdp, "\t# spt rcvdiag page=3\n");
    P (sdp, "\t# spt senddiag page=4 pout=\"02 00\"\n");
    P (sdp, "\t# spt ses set=ident etype=array element=1\n");

    P (sdp, "\n    Expect Data Options:\n");
    P (sdp, "\texp_radix={any,dec,hex} The default is any radix.\n\n");
    P (sdp, "\texp[ect]=type:byte_index:{string|value},...\n\n");
    P (sdp, "\tWhere type is:\n");
    P (sdp, "\t    C[HAR]            Character strings to expect.\n");
    P (sdp, "\t    B[YTE]            Byte (8 bit) values to expect.\n");
    P (sdp, "\t    S[HORT]           Short (16 bit) values to expect.\n");
    P (sdp, "\t    W[ORD]            Word (32 bit) values to expect.\n");
    P (sdp, "\t    L[ONG]            Long (64 bit) values to expect.\n");
    P (sdp, "\n\tNote: Byte index and values are taken as decimal (by default).\n");
    P (sdp, "\n    Inquiry Verify Example: (Nimble Storage)\n");
    P (sdp, "\t# spt dsf=/dev/sg3                                          \\\n");
    P (sdp, "\t      cdb='12 00 00 00 ff 00' dir=read length=255           \\\n");
    P (sdp, "\t      expect=BYTE:0:0x00,0x00,0x05,0x32,0x3f,0x18,0x10,0x02 \\\n");
    P (sdp, "\t      expect=C:8:'Nimble  ','Server          '              \\\n");
    P (sdp, "\t      expect=CHAR:32:'1.0 ' disable=verbose\n");
    P (sdp, "\n    Please see Test Check Options below for more test controls.\n");

    P (sdp, "\n    Unpack Data Options:\n");
    P (sdp, "\tunpack=string         The unpack format string.\n");
    P (sdp, "\tunpack_fmt={dec,hex}  The unpack data format. (Default: dec)\n\n");
    P (sdp, "\tWhere unpack format string is:\n");
    P (sdp, "\t    C[HAR]:[index]:length       Character of length.\n");
    P (sdp, "\t    F[IELD]:[index]:[start]:length Extract bit field.\n");
    P (sdp, "\t    O[FFSET]:index              Set the buffer offset.\n");
    P (sdp, "\t    B[YTE][:index]              Decode byte (8 bit) value.\n");
    P (sdp, "\t    S[HORT][:index]             Decode short (16 bit) value.\n");
    P (sdp, "\t    W[ORD][:index]              Decode word (32 bit) values.\n");
    P (sdp, "\t    L[ONG][:index]              Decode long (64 bit) values.\n");
    P (sdp, "\n    Inquiry Unpack Examples:\n");
    P (sdp, "\t# spt dsf=/dev/sdb inquiry disable=decode \\\n");
    P (sdp, "\t      unpack='Device Type: %BYTE, Vendor: %%CHAR:8:8, Product: %%C::16, Revision: %%C::4\\n'\n");
    P (sdp, "    OR Create your own JSON: (multiple unpack's permitted)\n");
    P (sdp, "\t# spt dsf=/dev/sdb inquiry disable=decode \\\n");
    P (sdp, "\t      unpack='{ \"Device Type\": %%BYTE, \"Vendor\": \"%%C:8:8\",' \\\n");
    P (sdp, "\t      unpack=' \"Product\": \"%%C::16\", \"Revision\": \"%%C::4\" }\\n'\n");

    P (sdp, "\n    I/O Options:\n");
    P (sdp, "\tlba=value             The logical block address.\n");
    P (sdp, "\tbs=value              The number of bytes per request.\n");
    P (sdp, "\tblocks=value          The number of blocks per request.\n");
    P (sdp, "\tlimit=value           The data limit to transfer (bytes).\n");
    P (sdp, "\tptype=string          The pattern type (only 'iot' now).\n");
    P (sdp, "\tending=value          The ending logical block address.\n");
    P (sdp, "\tstarting=value        The starting logical block address.\n");
    P (sdp, "\tslice=value           The specific slice to operate upon.\n");
    P (sdp, "\tslices=value          The slices to divide capacity between.\n");
    P (sdp, "\tstep=value            The bytes to step after each request.\n");

    P (sdp, "\n    I/O Range Options:\n");
    P (sdp, "\tmin=value             Set the minumum size to transfer.\n");
    P (sdp, "\tmax=value             Set the maximum size to transfer.\n");
    P (sdp, "\tincr=value            Set the increment size.\n");
    P (sdp, "    or\tincr=var[iable]       Enables variable increments.\n");
    P (sdp, "\n");
    P (sdp, "    Note: These options are only supported for Unmap (at present).\n");
    P (sdp, "          For Unmap, the values specified are range block sizes.\n");

    P (sdp, "\n    Error Recovery Options:\n");
    P (sdp, "\trecovery_delay=value   The amount of time to delay before retrying. (Default: %u secs)\n",
       RecoveryDelayDefault);
    P (sdp, "\trecovery_retries=value The number of times to retry a SCSI request. (Default: %u)\n",
       RecoveryRetriesDefault);
    P (sdp, "\n");
    P (sdp, "    Errors retried are OS specific, plus SCSI Busy and Unit Attention\n");
    P (sdp, "    Note: Errors are NOT automatically retried, use enable=recovery required.\n");

    P (sdp, "\n    Extended Copy Options:\n");
    P (sdp, "\tsrc=device            The source special file.\n");
    P (sdp, "\tdst=device            The destination special file.\n");
    P (sdp, "\treadlength=value      The SCSI read length (in bytes).\n");
    P (sdp, "\treadtype=string       The SCSI read type (read8, read10, read16).\n");
    P (sdp, "\twritetype=string      The SCSI write type (write8, write10, write16, writev16).\n");
    P (sdp, "\tlistid=value          The destination list identifier.\n");
    P (sdp, "\tslistid=value         The source list identifier.\n");
    P (sdp, "\tranges=value          The block device range descriptors.\n");
    P (sdp, "\trod_timeout=value     The ROD inactivity timeout (in secs).\n");
    //P (sdp, "\trod_token=file        The extended copy ROD token file.\n");
    P (sdp, "\tsegments=value        The number of extended copy segments.\n");
    P (sdp, "\n");
    P (sdp, "    These can be used in conjunction with the I/O options.\n");
    P (sdp, "    Note: The read options are only used with data compares.\n");
    //P (sdp, "    Also Note: Token based xcopy is *not* fully supported.\n");

    P (sdp, "\n    Show Devices Filters:\n");
    P (sdp, "\tdevice(s)=string      The device path(s).\n");
    P (sdp, "\tdevice_type(s)={value|string},...\n");
    P (sdp, "\t                      The device type. (or dtype=)\n");
    P (sdp, "\texclude=string        The exclude path(s).\n");
    P (sdp, "\tproduct=string        The product name. (or pid=)\n");
    P (sdp, "\tvendor=string         The vendor name. (or vid=)\n");
    P (sdp, "\trevision=string       The revision level. (or rev=)\n");
    P (sdp, "\tfw_version=string     The firmware version. (or fwver=)\n");
    P (sdp, "\tserial=string         The serial number.\n");
    P (sdp, "\tshow-fields=string    Show devices brief fields. (or sflds=).\n");
    P (sdp, "\tshow-format=string    Show devices format control. (or sfmt=).\n");
    P (sdp, "\tshow-path=string,...  Show devices using path. (or spath=).\n");

    P (sdp, "\n    Examples:\n");
    P (sdp, "\tshow devices dtypes=direct,enclosure vid=HGST\n");
    P (sdp, "\tshow edt devices=/dev/sdl,/dev/sdm\n");
    P (sdp, "\tshow edt exclude=/dev/sdl,/dev/sdm\n");

    P (sdp, "\n    Show Devices Format Control Strings:\n");
    P (sdp, "\t	           %%paths = The device paths.\n");
    P (sdp, "\t	     %%device_type = The device type. (or %%dtype)\n");
    P (sdp, "\t	         %%product = The product identification. (or %%pid)\n");
    P (sdp, "\t	          %%vendor = The vendor identification. (or %%vid)\n");
    P (sdp, "\t	        %%revision = The firmware revision level. (or %%rev)\n");
    P (sdp, "\t	      %%fw_version = The full firmware version. (or %%fwver)\n");
    P (sdp, "\t	          %%serial = The device serial number.\n");
    P (sdp, "\t	       %%device_id = The device identification. (or %%wwn)\n");
    P (sdp, "\t	     %%target_port = The device target port. (or %%tport)\n");
    P (sdp, "\n    Example:\n");
    P (sdp, "\tshow devices sfmt='Device Type: %%dtype, Paths: %%path'\n");

    P (sdp, "\n    Show Devices Brief Field: (strings same as above w/o %%)\n");
    P (sdp, "\t    Default: dtype,pid,rev,serial,tport,paths\n");
    P (sdp, "\n    Example:\n");
    P (sdp, "\tshow devices show-fields=vid,pid,wwn,paths\n");

    P (sdp, "\n    Show SCSI Filters:\n");
    P (sdp, "\tascq=value           The additional sense message.\n");
    P (sdp, "\tkey=value            The SCSI sense key message.\n");
    P (sdp, "\tstatus=value         The SCSI status message.\n");

    P (sdp, "\n    Examples:\n");
    P (sdp, "\tshow scsi ascq=0x0404\n");
    P (sdp, "\tshow scsi key=0x2\n");
    P (sdp, "\tshow scsi status=28\n");

    P (sdp, "\n    Test Options:\n");
    P (sdp, "\tabort                 Abort a background test.\n");
    P (sdp, "\tselftest              Standard self test.\n");
    P (sdp, "\tbextended             Background extended self test.\n");
    P (sdp, "\tbshort                Background short self test.\n");
    P (sdp, "\textended              Foreground extended self test.\n");
    P (sdp, "\tshort                 Foreground short self test.\n");

    P (sdp, "\n    Test Check Options:\n");
    P (sdp, "\tresid=value           The expected residual count.\n");
    P (sdp, "\ttransfer=value        The expected transfer count.\n");
    P (sdp, "\tstatus=value          The expected SCSI status.\n");
    P (sdp, "\tskey=value            The expected SCSI sense key.\n");
    P (sdp, "\tasc=value             The expected SCSI sense code.\n");
    P (sdp, "\tasq=value             The expected SCSI sense qualifier.\n");
    P (sdp, "\n    Example:\n");
    P (sdp, "\tcdb='1c 01 01 ff fc 00' dir=read length=65532 \\\n");
    P (sdp, "\ttransfer=240 disable=verbose exp_radix=hex expect=BYTE:0:01:...\n");
    P (sdp, "\n    Note: The enable=wait option can be used to wait for status.\n");

    P (sdp, "\n");
    P (sdp, "\tSCSI Status         Keyword      Value\n");
    P (sdp, "\t-----------         -------      -----\n");
    P (sdp, "\tGOOD                 good        0x00 \n");
    P (sdp, "\tCHECK_CONDITION      cc          0x02 \n");
    P (sdp, "\tCONDITION_MET        cmet        0x04 \n");
    P (sdp, "\tBUSY                 busy        0x08 \n");
    P (sdp, "\tINTERMEDIATE         inter       0x10 \n");
    P (sdp, "\tINTER_COND_MET       icmet       0x14 \n");
    P (sdp, "\tRESERVATION_CONFLICT rescon      0x18 \n");
    P (sdp, "\tCOMMAND_TERMINATED   term        0x22 \n");
    P (sdp, "\tQUEUE_FULL           qfull       0x28 \n");
    P (sdp, "\tACA_ACTIVE           aca_active  0x30 \n");
    P (sdp, "\tTASK_ABORTED         aborted     0x40 \n");
    P (sdp, "\n    Example:\n");
    P (sdp, "\t# spt cdb=0 status=good retry=100 msleep=100 enable=wait\n");
 
    P (sdp, "\n    Flags to enable/disable:\n");
    P (sdp, "\tadapter          SPT via HBA driver.        (Default: %s)\n", disabled_str);
    P (sdp, "\tasync            Execute asynchronously.    (Default: %s)\n", disabled_str);
    P (sdp, "\tbypass           Bypass sanity checks.      (Default: %s)\n",
                                (sdp->bypass) ? enabled_str : disabled_str);
    P (sdp, "\tcompare          Data comparison.           (Default: %s)\n",
                                (sdp->compare_data) ? enabled_str : disabled_str);
    P (sdp, "\tdebug            The SCSI debug flag.       (Default: %s)\n",
                                (sgp->debug) ? enabled_str : disabled_str);
    P (sdp, "\tDebug            The program debug flag.    (Default: %s)\n",
                                (DebugFlag) ? enabled_str : disabled_str);
    P (sdp, "\tjdebug           Job control debug.         (Default: %s)\n",
                                (sdp->jDebugFlag) ? enabled_str : disabled_str);
    P (sdp, "\tmdebug           Memory related debug.      (Default: %s)\n",
                                (mDebugFlag) ? enabled_str : disabled_str);
    P (sdp, "\txdebug           The extended debug flag.   (Default: %s)\n", disabled_str);
    P (sdp, "\tdecode           Decode control flag.       (Default: %s)\n", disabled_str);
    P (sdp, "\temit_all         Emit status all cmds.      (Default: %s)\n", disabled_str);
    P (sdp, "\tencode           Encode control flag.       (Default: %s)\n", disabled_str);
    P (sdp, "\terrors           Report errors flag.        (Default: %s)\n",
                                (sgp->errlog) ? enabled_str : disabled_str);
    P (sdp, "\tgenspt           Generate spt command.      (Default: %s)\n",
                                (sdp->genspt_flag) ? enabled_str : disabled_str);
    P (sdp, "\theader           Log header control flag.   (Default: %s)\n",
                                (sdp->log_header_flag) ? enabled_str : disabled_str);
    P (sdp, "\timage            Image mode copy.           (Default: %s)\n",
                                (sdp->image_copy) ? enabled_str : disabled_str);
    P (sdp, "\tjson_pretty      JSON pretty control.       (Default: %s)\n",
                                (sdp->json_pretty) ? enabled_str : disabled_str);
    P (sdp, "\tmapscsi          Map device to SCSI device. (Default: %s)\n",
			 	(sgp->mapscsi) ? enabled_str : disabled_str);
    P (sdp, "\tmulti            Multiple commands.         (Default: %s)\n",
			 	(InteractiveFlag) ? enabled_str : disabled_str);
    P (sdp, "\tpipes            Pipe mode flag.            (Default: %s)\n",
			 	(PipeModeFlag) ? enabled_str : disabled_str);
    P (sdp, "\tprewrite         Prewrite data blocks.      (Default: %s)\n",
			 	(sdp->prewrite_flag) ? enabled_str : disabled_str);
    P (sdp, "\trecovery         Automatic error recovery.  (Default: %s)\n",
			 	(sgp->recovery_flag) ? enabled_str : disabled_str);
    P (sdp, "\tread_after_write Read after write (or raw). (Default: %s)\n",
			 	(sdp->read_after_write) ? enabled_str : disabled_str);
    P (sdp, "\tsata             SATA device handling.      (Default: %s)\n",
                                (sdp->sata_device_flag) ? enabled_str : disabled_str);
    P (sdp, "\tscsi             Report SCSI information.   (Default: %s)\n",
                                (sdp->scsi_info_flag) ? enabled_str : disabled_str);
    P (sdp, "\tsense            Display sense data flag.   (Default: %s)\n",
			 	(sdp->sense_flag) ? enabled_str : disabled_str);
    P (sdp, "\tshow_caching     Show device caching flag.  (Default: %s)\n",
                                (sdp->show_caching_flag) ? enabled_str : disabled_str);
    P (sdp, "\tshow_header      Show devices header flag.  (Default: %s)\n",
			 	(sdp->show_header_flag) ? enabled_str : disabled_str);
    P (sdp, "\tunique           Unique pattern flag.       (Default: %s)\n",
			 	(sdp->unique_pattern) ? enabled_str : disabled_str);
    P (sdp, "\tverbose          Verbose output flag.       (Default: %s)\n",
			 	(sdp->verbose) ? enabled_str : disabled_str);
    P (sdp, "\tverify           Verify data flag.          (Default: %s)\n",
			 	(sdp->verify_data) ? enabled_str : disabled_str);
    P (sdp, "\twarnings         Warnings control flag.     (Default: %s)\n",
			 	(sdp->warnings_flag) ? enabled_str : disabled_str);
    P (sdp, "\twait             Wait for SCSI status.      (Default: %s)\n", disabled_str);
    P (sdp, "\trrti_wut         RRTI after WUT flag.       (Default: %s)\n",
				(sdp->rrti_wut_flag) ? enabled_str : disabled_str);
    P (sdp, "\tzerorod          Zero ROD token flag.       (Default: %s)\n",
				(sdp->zero_rod_flag) ? enabled_str : disabled_str);

    P (sdp, "\n    Operation Types:\n");
    P (sdp, "\tabort_task_set   Abort task set (ats).\n");
    P (sdp, "\tbus_reset        Bus reset (br).\n");
    P (sdp, "\tlun_reset        LUN reset (lr).\n");
    P (sdp, "\ttarget_reset     Target reset (bdr).\n");
    P (sdp, "\tscsi_cdb         SCSI CDB (default).\n");
    P (sdp, "\n    Shorthands: ats, br, lr, or bdr permitted.\n");
    P (sdp, "\n    Example: op=lun_reset\n");

    P (sdp, "\n    Queue Tag Message Types:\n");
    P (sdp, "\thead             Head of queue.\n");
    P (sdp, "\tordered          Ordered queuing.\n");
    P (sdp, "\tsimple           Simple queueing (default).\n");
    P (sdp, "\tnoq              Disable tagged queuing.\n");
    P (sdp, "\theadhs           Head of HA queue (Solaris).\n");
    P (sdp, "\n    Example: qtag=simple\n");

    P (sdp, "\n    Numeric Input:\n");
    P (sdp, "\tFor options accepting numeric input, the string may contain any\n");
    P (sdp, "\tcombination of the following characters:\n");
    P (sdp, "\n\tSpecial Characters:\n");
    P (sdp, "\t    w = words (%d bytes)", (int)sizeof(int));
    P (sdp, "            q = quadwords (%d bytes)\n", (int)sizeof(uint64_t));
    P (sdp, "\t    b = blocks (512 bytes)         k = kilobytes (1024 bytes)\n");
    P (sdp, "\t    m = megabytes (1048576 bytes)  \n");
    P (sdp, "\t    g = gigabytes (%ld bytes)\n", GBYTE_SIZE);
    P (sdp, "\t    t = terabytes (" LDF " bytes)\n", TBYTE_SIZE);
    P (sdp, "\t    inf or INF = infinity (" LUF " bytes)\n", MY_INFINITY);
    P (sdp, "\n\tArithmetic Characters:\n");
    P (sdp, "\t    + = addition                   - = subtraction\n");
    P (sdp, "\t    * or x = multiplication        / = division\n");
    P (sdp, "\t    %% = remainder\n");
    P (sdp, "\n\tBitwise Characters:\n");
    P (sdp, "\t    ~ = complement of value       >> = shift bits right\n");
    P (sdp, "\t   << = shift bits left            & = bitwise 'and' operation\n");
    P (sdp, "\t    | = bitwise 'or' operation     ^ = bitwise exclusive 'or'\n\n");
    P (sdp, "\tThe default base for numeric input is decimal, but you can override\n");
    P (sdp, "\tthis default by specifying 0x or 0X for hexadecimal conversions, or\n");
    P (sdp, "\ta leading zero '0' for octal conversions.  NOTE: Evaluation is from\n");
    P (sdp, "\tright to left without precedence, and parenthesis are not permitted.\n");

    P (sdp, "\n    Emit Status Format Control:\n");
    P (sdp, "\t         %%progname = Our program name (%s).\n", OurName);
    P (sdp, "\t           %%thread = The thread number.\n");
    P (sdp, "\t              %%cdb = The SCSI CDB bytes.\n");
    P (sdp, "\t              %%dir = The data direction.\n");
    P (sdp, "\t           %%length = The data length.\n");
    P (sdp, "\t           %%device = The base device name.\n");
#if defined(__linux__)
    P (sdp, "\t             %%adsf = The alternate special file.\n");
#endif
    P (sdp, "\t              %%dsf = The device special file.\n");
    P (sdp, "\t             %%dsf1 = The 2nd device special file.\n");
    P (sdp, "\t              %%dst = The destination device name.\n");
    P (sdp, "\t              %%src = The source device name.\n");
    P (sdp, "\t         %%src[1-2] = The other source devices.\n");
    P (sdp, "\t             %%srcs = All the source devices.\n");
    P (sdp, "\t           %%status = The command (IOCTL) status.\n");
    P (sdp, "\t       %%status_msg = The IOCTL status message.\n");
    P (sdp, "\t        %%scsi_name = The SCSI opcode name.\n");
    P (sdp, "\t      %%scsi_status = The SCSI status.\n");
    P (sdp, "\t         %%scsi_msg = The SCSI message.\n");
    P (sdp, "\t      %%host_status = The host status.\n");
    P (sdp, "\t         %%host_msg = The host status message.\n");
    P (sdp, "\t    %%driver_status = The driver status.\n");
    P (sdp, "\t       %%driver_msg = The driver status message.\n");
    P (sdp, "\t       %%sense_code = The sense error code.\n");
    P (sdp, "\t        %%sense_msg = The sense code message.\n");
    P (sdp, "\t       %%info_valid = The information valid bit.\n");
    P (sdp, "\t        %%info_data = The information field data.\n");
    P (sdp, "\t       %%cspec_data = The cmd spec information data.\n");
    P (sdp, "\t            %%resid = The residual bytes.\n");
    P (sdp, "\t           %%blocks = The blocks transferred.\n");
    P (sdp, "\t         %%capacity = The device capacity (in blocks).\n");
    P (sdp, "\t      %%device_size = The device block size.\n");
    P (sdp, "\t       %%iterations = The iterations executed.\n");
    P (sdp, "\t       %%operations = The operations executed.\n");
    P (sdp, "\t         %%starting = The starting logical block.\n");
    P (sdp, "\t           %%ending = The ending logical block.\n");
    P (sdp, "\t     %%total_blocks = The total blocks transferred.\n");
    P (sdp, "\t %%total_operations = The total operations executed.\n");
    P (sdp, "\t             %%xfer = The bytes transferred. (or %bytes)\n");
    P (sdp, "\t       %%total_xfer = The total bytes transferred.\n");
    P (sdp, "\t        %%sense_key = The sense key.\n");
    P (sdp, "\t         %%skey_msg = The sense key message.\n");
    P (sdp, "\t              %%ili = Illegal length indicator.\n");
    P (sdp, "\t              %%eom = End of medium.\n");
    P (sdp, "\t               %%fm = Tape file mark.\n");
    P (sdp, "\t             %%ascq = The asc/ascq pair.\n");
    P (sdp, "\t         %%ascq_msg = The asc/ascq message.\n");
    P (sdp, "\t              %%asc = The additional sense code.\n");
    P (sdp, "\t              %%asq = The additional sense qualifier.\n");
    P (sdp, "\t              %%fru = The field replaceable unit code.\n");
    P (sdp, "\t       %%sense_data = All the sense data.\n");
    P (sdp, "\t          %%timeout = The command timeout (in ms).\n");
    P (sdp, "\t      %%deallocated = The deallocated blocks.\n");
    P (sdp, "\t           %%mapped = The mapped blocks.\n");
    
    P (sdp, "\n    Time Keywords:\n");
    P (sdp, "\t    %%date         = The current date/time.\n");
    P (sdp, "\t    %%seconds      = The time in seconds.\n");
    P (sdp, "\t    %%start_time   = The test start time.\n");
    P (sdp, "\t    %%end_time     = The test end time.\n");
    P (sdp, "\t    %%elapsed_time = The elapsed time.\n");

    P (sdp, "\n    Performance Keywords:\n");
    P (sdp, "\t    %%bps  = The bytes per second.     %%lbps = Logical blocks per second.\n");
    P (sdp, "\t    %%kbps = Kilobytes per second.     %%mbps = The megabytes per second.\n");
    P (sdp, "\t    %%iops = The I/O's per second.     %%spio = The seconds per I/O.\n");

    P (sdp, "\n    Log File/Prefix Format Keywords:\n");
    P (sdp, "\t    %%dsf    = The device name.        %%dsf1   = The second device.\n");
    P (sdp, "\t    %%src    = The source device.      %%src1   = The source 1 device.\n");
    P (sdp, "\t    %%src2   = The source 2 device.    %%srcs   = All source devices.\n");
    P (sdp, "\t    %%host   = The host name.          %%user   = The user name.\n");
    P (sdp, "\t    %%job    = The job ID.             %%tag    = The job tag.\n");
    P (sdp, "\t    %%tid    = The thread ID.          %%thread = The thread number.\n");
    P (sdp, "\t    %%pid    = The process ID.         %%prog   = The program name.\n");
    P (sdp, "\t    %%ymd    = The year,month,day.     %%hms    = The hour,day,seconds.\n");
    P (sdp, "\t    %%dfs    = The directory separator ('%c')\n", sdp->dir_sep);
    P (sdp, "\t    %%date   = The date string.        %%et     = The elapsed time.\n");
    P (sdp, "\t    %%tod    = The time of day.        %%etod   = Elapsed time of day.\n");
    P (sdp, "\t    %%secs   = Seconds since start.    %%seq    = The sequence number.\n");
    P (sdp, "\t    %%month  = The month of the year.  %%day    = The day of the month.\n");
    P (sdp, "\t    %%year   = The four digit year.    %%hour   = The hour of the day.\n");
    P (sdp, "\t    %%minutes= The minutes of hour.    %%seconds= The seconds of minute.\n");
    P (sdp, "\t    %%tmpdir = The temporary directory.\n");
    P (sdp, "\n");
    P (sdp, "\t    String 'gtod' = \"%%tod (%%etod) %%prog (j:%%job t:%%thread): \"\n");
    P (sdp, "\n");
    P (sdp, "      Examples: log=spt_%%host-j%%jobt%%thread.log\n");
    P (sdp, "                logprefix=\"%%seq %%ymd %%hms %%et %%prog (j:%%job t:%%thread): \"\n");

    P (sdp, "\n    Time Input:\n");
    P (sdp, "\t    d = days (%d seconds),      h = hours (%d seconds)\n",
						SECS_PER_DAY, SECS_PER_HOUR);
    P (sdp, "\t    m = minutes (%d seconds),      s = seconds (the default)\n\n",
								SECS_PER_MIN);
    P (sdp, "\tArithmetic characters are permitted, and implicit addition is\n");
    P (sdp, "\tperformed on strings of the form '1d5h10m30s'.\n");

    P (sdp, "\n    Timeout Value:\n");
    P (sdp, "\t    d = days (%d ms),        h = hours (%d ms)\n",
                                         MSECS_PER_DAY, MSECS_PER_HOUR);
    P (sdp, "\t    m = minutes (%d ms),        s = seconds (%d ms)\n",
                                         MSECS_PER_MIN, MSECS_PER_SEC);
    P (sdp, "\n    The default SCSI timeout is %u milliseconds (%u seconds).\n",
					 sgp->timeout, TIMEOUT_SECONDS);
    P (sdp, "\n    Examples:\n\n");
    P (sdp, "    Define Device: (or use dsf= option)\n");
    P (sdp, "\t# export SPT_DEVICE=/dev/sdi\n");
    P (sdp, "    Inquiry:\n");
    P (sdp, "\t# spt cdb='12 00 00 00 ff 00' dir=read length=255\n");
    P (sdp, "    Inquiry Serial Number:\n");
    P (sdp, "\t# spt cdb='12 01 80 00 ff 00' dir=read length=255\n");
    P (sdp, "    Inquiry Device Identification:\n");
    P (sdp, "\t# spt cdb='12 01 83 00 ff 00' dir=read length=255\n");
    P (sdp, "    Inquiry Management Network Addresses:\n");
    P (sdp, "\t# spt cdb='12 01 85 00 ff 00' dir=read length=255\n");
    P (sdp, "    Format Unit:\n");
    P (sdp, "\t# spt cdb='04 10 00 00 00 00'\n");
    P (sdp, "    Mode Sense(6) (request all pages):\n");
    P (sdp, "\t# spt cdb='1a 00 3f 00 ff 00' dir=read length=255\n");
    P (sdp, "    Mode Sense(10) (request all pages):\n");
    P (sdp, "\t# spt cdb='5a 00 3f 00 00 00 00 00 ff 00' dir=read length=255\n");
    P (sdp, "    Mode Sense(6) (error control page):\n");
    P (sdp, "\t# spt cdb='1a 00 01 00 18 00' dir=read length=24\n");
    P (sdp, "    Mode Select(6) (error recovery page): (Seagate ST336607LC)\n");
    P (sdp, "\t# spt cdb='15 11 00 00 18 00' \\\n"
	    "\t      pout='00 00 00 08 04 3d 67 1f 00 00 02 00 01 0a c0 0b ff 00 00 00 05 00 ff ff'\n");
    P (sdp, "    Mode Select(10) (error recovery page):\n");
    P (sdp, "\t# spt cdb='55 11 00 00 00 00 00 00 1c 00' \\\n"
	    "\t      pout='00 00 00 00 00 00 00 08 04 3d 67 1f 00 00 02 00 81 0a c0 0b ff 00 00 00 05 00 ff ff'\n");
    P (sdp, "    Persistent Reserve In (Read Keys):\n");
    P (sdp, "\t# spt cdb='5e 00 00 00 00 00 00 10 08 00' dir=read length=4104\n");
    P (sdp, "    Persistent Reserve In (Read Reservations):\n");
    P (sdp, "\t# spt cdb='5e 01 00 00 00 00 00 20 08 00' dir=read length=8200\n");
    P (sdp, "    Persistent Reserve Out (Clear):\n");
    P (sdp, "\t# spt cdb='5f 03 00 00 00 00 00 00 18 00' length=24 \\\n"
	    "\t      pout='11 22 33 44 55 66 77 88 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00'\n"); 
    P (sdp, "    Persistent Reserve Out (Register):\n");
    P (sdp, "\t# spt cdb='5f 00 00 00 00 00 00 00 18 00' length=24 \\\n"
	    "\t      pout='00 00 00 00 00 00 00 00 11 22 33 44 55 66 77 88 00 00 00 00 01 00 00 00'\n"); 
    
    P (sdp, "    Extended Copy: (source lun to destination lun)\n");
    P (sdp, "\t# spt cdb=0x83 src=/dev/sdj dst=/dev/sdd enable=sense'\n");
    P (sdp, "    Extended Copy (Populate Token): (List ID 0A, two block range descriptors)\n");
    P (sdp, "\t# spt cdb='83 10 00 00 00 00 00 00 00 0A 00 00 00 30 00 00' length=48 \\\n"
	    "\t      pout='0 2e 0 0  0 0 0 0  0 0 0 0 0 0  0 20  0 0 0 0 0 0 0 20  0 0 0 10  0 0 0 0  0 0 0 0 0 0 0 40  0 0 0 10  0 0 0 0'\n"); 
    P (sdp, "    Receive ROD Token Information (RRTI): (write ROD token to file 'token.dat')\n");
    P (sdp, "\t# spt cdb='84 07 00 00 00 0A 00 00 00 00 00 00 02 26' dir=read length=550 rod_token=token.dat\n");
    P (sdp, "    Write Using Token (WUT): (List ID 0E, one descriptor, read ROD token from file 'token.dat')\n");
    P (sdp, "\t# spt cdb='83 11 00 00 00 00 00 00 00 0E 00 00 02 28 00 00' length=552 rod_token=token.dat \\\n"
	    "\t      pout='0 2e 0 0  0 0 0 0  0 0 0 0 0 0  0 20  0 0 0 0 0 0 0 20  0 0 0 10  0 0 0 0'\n"); 

    P (sdp, "    Compare and Write(16): (with read-after-write)\n");
    P (sdp, "\t# spt cdb=89 starting=0 limit=25m ptype=iot enable=raw\n");
    P (sdp, "    Read(6) 1 block: (lba 2097151)\n");
    P (sdp, "\t# spt cdb='08 1f ff ff 01 00' dir=read length=512\n");         
    P (sdp, "    Read(10) 1 block: (lba 134217727)\n");
    P (sdp, "\t# spt cdb='28 00 ff ff ff ff 00 00 01 00' dir=read length=512\n");
    P (sdp, "    Read(16) 1 block: (lba 34359738367)\n");
    P (sdp, "\t# spt cdb='88 00 00 00 0f ff ff ff ff ff 00 00 00 01 00 00' dir=read length=512\n");
    P (sdp, "    Read Capacity(10):\n");
    P (sdp, "\t# spt cdb='25 00 00 00 00 00 00 00 00 00' dir=read length=8\n");
    P (sdp, "    Read Capacity(16):\n");
    P (sdp, "\t# spt cdb='9e 10 00 00 00 00 00 00 00 00 00 00 00 20 00 00' dir=read length=32\n");
    P (sdp, "    Report LUNs:\n");
    P (sdp, "\t# spt cdb='a0 00 00 00 00 00 00 00 08 08 00 00' dir=read length=2056\n");
    P (sdp, "    Report Target Group Support:\n");
    P (sdp, "\t# spt cdb='a3 0a 00 00 00 00 00 00 04 84 00 00' dir=read length=1156\n");
    P (sdp, "    Reserve Unit(6):\n");
    P (sdp, "\t# spt cdb='16 00 00 00 00 00'\n");
    P (sdp, "    Reserve Unit(10):\n");
    P (sdp, "\t# spt cdb='56 00 00 00 00 00 00 00 00 00'\n");
    P (sdp, "    Release Unit(6):\n");
    P (sdp, "\t# spt cdb='17 00 00 00 00 00'\n");
    P (sdp, "    Release Unit(10):\n");
    P (sdp, "\t# spt cdb='57 00 00 00 00 00 00 00 00 00'\n");
    P (sdp, "    Request Sense:\n");
    P (sdp, "\t# spt cdb='03 00 00 00 ff 00' dir=read length=255\n");
    P (sdp, "    Seek (lba 99999):\n");
    P (sdp, "\t# spt cdb='2b 00 00 01 86 9f 00 00 00 00'\n");
    P (sdp, "    Send Diagnostic (execute self-test):\n");
    P (sdp, "\t# spt cdb='1d 04 00 00 00 00'\n");
    P (sdp, "    Stop Unit:\n");
    P (sdp, "\t# spt cdb='1b 00 00 00 00 00'\n");
    P (sdp, "\t# spt cdb='1b 01 00 00 00 00' (stop immediate)\n");
    P (sdp, "    Start Unit:\n");
    P (sdp, "\t# spt cdb='1b 00 00 00 01 00'\n");
    P (sdp, "\t# spt cdb='1b 01 00 00 01 00' (start immediate)\n");
    P (sdp, "    Synchronize Cache(10):\n");
    P (sdp, "\t# spt cdb='35 00 00 00 00 00 00 00 00 00'\n");
    P (sdp, "    Synchronize Cache(16):\n");
    P (sdp, "\t# spt cdb='91 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00'\n");
    P (sdp, "    Test Unit Ready:\n");
    P (sdp, "\t# spt cdb='00 00 00 00 00 00'\n"); 
    P (sdp, "    Unmap: [ all blocks ]\n");
    P (sdp, "\t# spt cdb=42 starting=0 enable=sense,recovery\n");
    P (sdp, "    Verify(10): [ lba 65535 for 64K blocks ]\n");
    P (sdp, "\t# spt cdb='2f 00 00 00 ff ff 00 ff ff 00'\n");
    P (sdp, "    Verify(16): [ lba 65535 for 64K blocks ]\n");
    P (sdp, "\t# spt cdb='8f 00 00 00 00 00 00 00 ff ff 00 00 ff ff 00 00'\n");
    P (sdp, "    Write(6) 1 block: (lba 2097151)\n");
    P (sdp, "\t# spt cdb='0a 1f ff ff 01 00' dir=write din=data length=512\n");
    P (sdp, "    Write(10) 1 block: (lba 134217727)\n");
    P (sdp, "\t# spt cdb='2a 00 ff ff ff ff 00 00 01 00' dir=write din=data length=512\n");
    P (sdp, "    Write(16) 1 block: (lba 34359738367)\n");
    P (sdp, "\t# spt cdb='8a 00 00 00 0f ff ff ff ff ff 00 00 00 01 00 00' dir=write din=data length=512\n");
    P (sdp, "    Write Same(10) all blocks:\n");
    P (sdp, "\t# spt cdb='41 00 00 00 00 00 00 00 00 00' dir=write length=512 timeout=5m\n");
    P (sdp, "    Write Same(16) 499712 blocks: (unmap)\n");
    P (sdp, "\t# spt cdb='93 08 00 00 00 00 00 00 00 00 07 a0 00 00 00 00' dir=write length=512\n");
    P (sdp, "    Write and Verify(10) 8 blocks: (lba 2097151)\n");
    P (sdp, "\t# spt cdb='2e 00 00 1f ff ff 00 00 08 00' dir=write din=data length=4096\n");
    P (sdp, "    Write and Verify(16) 8 blocks: (lba 2097151)\n");
    P (sdp, "\t# spt cdb='8e 00 00 00 00 00 00 1f ff ff 00 00 00 08 00 00' dir=write din=data length=4096\n");
    P (sdp, "    Abort Task Set:\n");
    P (sdp, "\t# spt op=abort_task_set\n");
    P (sdp, "    LUN Reset:\n");
    P (sdp, "\t# spt op=lun_reset path=3\n");
    P (sdp, "    Target Reset:\n");
    P (sdp, "\t# spt op=target_reset enable=debug\n");

    P (sdp, "\n    Builtin Support Examples:\n\n");
    P (sdp, "    Inquiry Information: (human readable)\n");
    P (sdp, "\t# spt inquiry logprefix=\n");
    P (sdp, "    Read Capacity(10): (for older SCSI devices or USB)\n");
    P (sdp, "\t# spt readcapacity10\n");
    P (sdp, "    Read Capacity(16): (shows thin provisioning)\n");
    P (sdp, "\t# spt readcapacity16 ofmt=json\n");
    P (sdp, "    Report LUNs:\n");
    P (sdp, "\t# spt cdb=a0 enable=encode,decode disable=verbose\n");
    P (sdp, "    Report Target Group Support:\n");
    P (sdp, "\t# spt cdb='a3 0a' enable=encode,decode disable=verbose\n");
    P (sdp, "    Write and Read/Compare IOT Pattern: (32k, all blocks)\n");
    P (sdp, "\t# spt cdb=8a dir=write length=32k enable=compare,recovery,sense starting=0 ptype=iot\n");
    P (sdp, "    Read and Compare IOT Pattern: (32k, all blocks)\n");
    P (sdp, "\t# spt cdb=88 dir=read length=32k enable=compare,recovery,sense starting=0 ptype=iot\n");
    P (sdp, "    Write and Read/Compare IOT Pattern w/immediate Read-After-Write: (64k, 1g data)\n");
    P (sdp, "\t# spt cdb=8a starting=0 bs=64k limit=1g ptype=iot enable=raw emit=default\n");
    P (sdp, "    Write Same: (all blocks)\n");
    P (sdp, "\t# spt cdb='93' starting=0 dir=write length=4k blocks=4m/b\n");
    P (sdp, "    Write Same w/Unmap: (all blocks)\n");
    P (sdp, "\t# spt cdb='93 08' starting=0 dir=write length=512 blocks=4m/b\n");
    P (sdp, "    Unmap All Blocks: (incrementing blocks per range)\n");
    P (sdp, "\t# spt cdb=42 starting=0 ranges=64 min=8 max=128 incr=8\n");
    P (sdp, "    Get LBA Status: (reports mapped/deallocated blocks)\n");
    P (sdp, "\t# spt cdb='9e 12' starting=0\n");
    P (sdp, "    Extended Copy Operation: (non-token LID1 xcopy, used by VMware)\n");
    P (sdp, "\t# spt cdb=83 src=${SRC} starting=0 dst=${DST} starting=0 enable=compare,recovery,sense\n");
    P (sdp, "    Extended Copy Operation: (ROD token xcopy, used by Microsoft, aka ODX)\n");
    P (sdp, "\t# spt cdb='83 11' src=${SRC} starting=0 dst=${DST} starting=0 enable=compare,recovery,sense\n");
    P (sdp, "    Extended Copy Operation: (ROD Token, same disk)\n");
    P (sdp, "\t# spt cdb='83 11' dsf=${DST} starting=0 enable=Debug,recovery,sense emit=default\n");
    P (sdp, "    Zero ROD Token: (10 slices, all blocks, space allocation needs enabled)\n");
    P (sdp, "\t# spt cdb='83 11' dsf=${DST} starting=0 enable=zerorod slices=10 enable=recovery,sense\n");
    P (sdp, "    Copy/Verify Source to Destination Device: (uses read/write operations)\n");
    P (sdp, "\t# spt iomode=copy length=32k dsf=${SRC} starting=0 dsf1=${DST} starting=0 enable=compare,recovery,sense\n");
    P (sdp, "    Write Source and Verify with Mirror Device: (10 threads for higher performance)\n");
    P (sdp, "\t# spt iomode=mirror length=32k dsf=${SRC} starting=0 dsf1=${DST} starting=0 enable=compare slices=10\n");

    P (sdp, "\n    Environment Variables:\n");
    P (sdp, "\t# export SPT_DEVICE='/dev/sdi'\n");
    P (sdp, "\t# export SPT_SHOW_FIELDS='dtype,vid,pid,did,tport,paths'\n");
    P (sdp, "\t# export SPT_EMIT_STATUS='Status: %%status, SCSI Status: %%scsi_status, Sense Code: %%sense_code, "
       "Sense Key: %%sense_key, Ascq: %%ascq, Resid: %%resid'\n");

    P (sdp, "\n    --> " ToolVersion " <--\n");

    return;
}
