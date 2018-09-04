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
 * Module:	libscsi.c
 * Author:	Robin T. Miller
 * Date:	March 24th, 2005
 *
 * Description:
 *  This module contains common SCSI functions, which are meant to be
 * a front-end to calling the underlying OS dependent SCSI functions,
 * when appropriate, to send a SCSI CDB (Command Descriptor Block).
 * 
 * Modification History:
 * 
 * July 3rd, 2018 by Robin T. Miller
 *      In isSenseRetryable(), be more selective on "Not Ready" errors.
 *      This is required to avoid excessive retries during Format Unit,
 * Sanitize, or short/extended self-tests, which are long operations.
 *      Previously, with the default 60 retries and two second delay,
 * we'd retry for two minutes *per* command, making us appear hung!
 * 
 * June 7th, 2018 by Robin T. Miller
 *      Add support for ATA pass-through and Identify command.
 * 
 * October 13th, 2017 by Robin T. Miller
 *      Handle sense data in both fixed and descriptor formats.
 * 
 * October 3rd, 2014 by Robin T. Miller
 * 	Modified SCSI generic opaque pointer to tool_specific information,
 * so we can pass more tool specific information (e.g. dt versus spt).
 * 
 * January 23rd, 2013 by Robin T. Miller
 * 	Update libExecuteCdb() to allow a user defined execute CDB function.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#if !defined(_WIN32)
#  include <unistd.h>
#endif /* !defined(_WIN32) */

#include "spt.h"
//#include "include.h"
//#include "libscsi.h"
#include "scsi_cdbs.h"
#include "scsi_opcodes.h"

/* Note: Added for os_sleep()! */
#if defined(_WIN32)
#  include "spt_win.h"
#else /* !defined(_WIN32) */
#  include "spt_unix.h"
#endif /* defined(_WIN32) */

/*
 * Local Declarations:
 */

/*
 * Common Text Strings:
 */
static char *bad_conversion_str = "Warning: unexpected conversion size of %d bytes.\n";

/*
 * Forward Declarations:
 */
scsi_generic_t *init_scsi_generic(tool_specific_t *tsp);
hbool_t	isSenseRetryable(scsi_generic_t *sgp, int scsi_status, scsi_sense_t *ssp);

int verify_inquiry_header(inquiry_t *inquiry, inquiry_header_t *inqh, unsigned char page);
int verify_vdisk_headers(scsi_generic_t *sgp, void *data, unsigned char page);

/* ======================================================================== */

scsi_generic_t *
init_scsi_generic(tool_specific_t *tsp)
{
    scsi_generic_t *sgp;
    sgp = Malloc(NULL, sizeof(*sgp));
    if (sgp == NULL) return (NULL);
    init_scsi_defaults(sgp, tsp);
    return (sgp);
}

void
init_scsi_defaults(scsi_generic_t *sgp, tool_specific_t *tsp)
{
    scsi_addr_t *sap;
    /*
     * Initial Defaults: 
     */
    sgp->fd             = INVALID_HANDLE_VALUE;
    sgp->tsp		= tsp;
    sgp->sense_length   = RequestSenseDataLength;
    sgp->sense_data     = malloc_palign(NULL, sgp->sense_length, 0);

    sgp->debug          = ScsiDebugFlagDefault;
    sgp->errlog         = ScsiErrorFlagDefault;
    sgp->timeout        = ScsiDefaultTimeout;

    sgp->qtag_type      = SG_SIMPLE_Q;

    /*
     * Recovery Parameters:
     */ 
    sgp->recovery_flag  = ScsiRecoveryFlagDefault;
    sgp->restart_flag	= ScsiRestartFlagDefault;
    sgp->recovery_delay = ScsiRecoveryDelayDefault;
    sgp->recovery_limit = ScsiRecoveryRetriesDefault;

    sap = &sgp->scsi_addr;
    /* Note: Only AIX uses this, but must be -1 for any path! */
    sap->scsi_path      = -1;	/* Indicates no path specified. */

    return;
}

hbool_t
isSenseRetryable(scsi_generic_t *sgp, int scsi_status, scsi_sense_t *ssp)
{
    hbool_t retriable = False;
    unsigned char sense_key, asc, asq;

    GetSenseErrors(ssp, &sense_key, &asc, &asq);

    if (sgp->debug == True) {
	print_scsi_status(sgp, scsi_status, sense_key, asc, asq);
    }
    if ( (scsi_status == SCSI_BUSY) ||
	 (scsi_status == SCSI_QUEUE_FULL) ) {
	retriable = True;
    } else if (scsi_status == SCSI_CHECK_CONDITION) {
	if (sense_key == SKV_UNIT_ATTENTION) {
	    if (asc != ASC_RECOVERED_DATA) {
		retriable = True;
	    }
	} else if ( (sense_key == SKV_NOT_READY) &&
		    (asc == ASC_NOT_READY) ) {
	    /* Be selective on "Not Ready" conditions, to avoid excessive retries. */
	    /* Note: We don't take any actions, such as Start Unit to spin disk up. */
	    switch (asq) {
		case 0x00:		/* Logical unit not ready, cause not reportable. */
		case 0x01:		/* Logical unit is in process of becoming ready. */
		case 0x05:		/* Logical unit not ready, rebuild in progress. */
		case 0x06:		/* Logical unit not ready, recalculation in progress. */
		case 0x07:		/* Logical unit not ready, operation in progress. */
		case 0x08:		/* Logical unit not ready, long write in progress. */
		case 0x0A:		/* Logical unit not accessible, asymmetric access state transition. */
		case 0x14:		/* Logical unit not ready, space allocation in progress. */
		    retriable = True;
		    break;
		default:
		    break;
	    }
	}
    }
    return(retriable);
}

/*
 * Command Specific XCOPY Byte Definitions:
 */ 
#define CMD_SRC_DEVICE		0
#define CMD_DST_DEVICE		1

hbool_t
libIsRetriable(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    hbool_t retriable = False;

    if (sgp->recovery_retries++ < sgp->recovery_limit) {
	/*
	 * Try OS specific first, then check for common retriables.
	 */ 
	retriable = os_is_retriable(sgp);
	if (retriable == False) {
	    scsi_sense_t *ssp = sgp->sense_data;
	    retriable = isSenseRetryable(sgp, sgp->scsi_status, ssp);
	    if (retriable == False) {
		/*
		 * Note: For XCOPY, there may be additional SCSI sense data. 
		 * Also Note: This only works for fixed sesne data! 
		 */
		if ( (sgp->cdb[0] == SOPC_EXTENDED_COPY) &&
		     (ssp->sense_key == SKV_COPY_ABORTED) ) {
		    scsi_sense_t *xssp = NULL;
		    uint8_t scsi_status = 0;
		    char *device = NULL;
		    if (ssp->cmd_spec_info[CMD_SRC_DEVICE]) {
			uint8_t *bp = (unsigned char *)ssp + ssp->cmd_spec_info[CMD_SRC_DEVICE];
			scsi_status = bp[0];
			xssp = (scsi_sense_t *)&bp[1];
			device = "source";
		    }
		    if (ssp->cmd_spec_info[CMD_DST_DEVICE]) {
			uint8_t *bp = (unsigned char *)ssp + ssp->cmd_spec_info[CMD_DST_DEVICE];
			scsi_status = bp[0];
			xssp = (scsi_sense_t *)&bp[1];
			device = "destination";
		    }
		    if (xssp) {
			/* Now see if the extended sense data is retriable. */
			retriable = isSenseRetryable(sgp, scsi_status, xssp);
			if (sgp->errlog == True) {
			    if (retriable == True) {
				Fprintf(opaque, "Retriable %s device error...\n", device);
			    }
			    libReportScsiSense(sgp, scsi_status, xssp);
			}
		    }
		}
	    }
	}
    }
    return (retriable);
}

/* ======================================================================== */

/*
 * libExecuteCdb() = Execute a SCSI Command Descriptor Block (CDB).
 *
 * Inputs:
 *  sgp = Pointer to SCSI generic pointer.
 *
 * Return Value:
 *      Returns 0/-1/253 for Success/Failure/Restart.
 */
int
libExecuteCdb(scsi_generic_t *sgp)
{
    tool_specific_t *tsp = sgp->tsp;
    void *opaque = (tsp) ? tsp->opaque : NULL;
    hbool_t retriable;
    int error;

    /*
     * Allow user to define their own execute CDB function.
     */
    if (tsp && tsp->execute_cdb && tsp->opaque) {
	error = (*tsp->execute_cdb)(tsp->opaque, sgp);
	return (error);
    }

    if (/*sgp->debug &&*/ tsp == NULL) {
	Printf(opaque, "No SCSI device for cdb=%02x\n", sgp->cdb[0]);
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
	if ( ((error == FAILURE) || (sgp->error == True)) && sgp->recovery_flag) {
	    if (sgp->recovery_retries == sgp->recovery_limit) {
		Fprintf(opaque, "Exceeded retry limit (%u) for this request!\n", sgp->recovery_limit);
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
			    Wprintf(opaque, "Restarting %s after detecting retriable error...\n", sgp->cdb_name);
			    return(RESTART);
			}
			Wprintf(opaque, "Retrying %s after %u second delay, retry #%u...\n",
			       sgp->cdb_name, sgp->recovery_delay, sgp->recovery_retries);
		    }
		}
	    }
	}
    } while (retriable == True);

    if (error == FAILURE) {		/* The system call failed! */
	if (sgp->errlog == True) {
	    libReportIoctlError(sgp, sgp->warn_on_error);
	}
    } else if ( (sgp->error == True) && (sgp->errlog == True) ) { /* SCSI error. */
	libReportScsiError(sgp, sgp->warn_on_error);
    }

    if (sgp->error == True) {
        error = FAILURE;      /* Tell caller we've had an error! */
    }
    return (error);
}

void
libReportIoctlError(scsi_generic_t *sgp, hbool_t warn_on_error)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;

    if (sgp->errlog == True) {
	time_t error_time = time((time_t *) 0);
	Fprintf(opaque, "%s: Error occurred on %s",
		(warn_on_error == True) ? "Warning" : "ERROR",
		ctime(&error_time));
	Fprintf(opaque, "%s failed on device %s\n", sgp->cdb_name, sgp->dsf);
    }
    return;
}

void
libReportScsiError(scsi_generic_t *sgp, hbool_t warn_on_error)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    time_t error_time = time((time_t *) 0);
    char *host_msg = os_host_status_msg(sgp);
    char *driver_msg = os_driver_status_msg(sgp);
    scsi_sense_t *ssp = sgp->sense_data;
    unsigned char sense_key, asc, asq;
    char *ascq_msg;

    /* Get sense key and sense code/qualifiers. */
    GetSenseErrors(ssp, &sense_key, &asc, &asq);
    ascq_msg = ScsiAscqMsg(asc, asq);

    Fprintf(opaque, "%s: Error occurred on %s",
	    (warn_on_error == True) ? "Warning" : "ERROR",
	    ctime(&error_time));
    Fprintf(opaque, "%s failed on device %s\n", sgp->cdb_name, sgp->dsf);
    //ReportCdbScsiInformation(opaque, sgp);	/* Too chatty? */
    Fprintf(opaque, "SCSI Status = %#x (%s)\n", sgp->scsi_status, ScsiStatus(sgp->scsi_status));
    if (host_msg && driver_msg) {
	Fprintf(opaque, "Host Status = %#x (%s), Driver Status = %#x (%s)\n",
	       sgp->host_status, host_msg, sgp->driver_status, driver_msg);
    } else if (host_msg || driver_msg) {
	if (host_msg) {
	    Fprintf(opaque, "Host Status = %#x (%s)\n", sgp->host_status, host_msg);
	}
	if (driver_msg) {
	    Fprintf(opaque, "Driver Status = %#x (%s)\n", sgp->driver_status, driver_msg);
	}
    } else if (sgp->host_status || sgp->driver_status) {
	Fprintf(opaque, "Host Status = %#x, Driver Status = %#x\n",
		sgp->host_status, sgp->driver_status);
    }
    Fprintf(opaque, "Sense Key = %d = %s, Sense Code/Qualifier = (%#x, %#x)",
	    sense_key, SenseKeyMsg(sense_key), asc, asq);
    if (ascq_msg) {
      Fprint(opaque, " - %s", ascq_msg);
    }
    Fprintnl(opaque);
    if ( ssp->error_code && (sgp->debug || sgp->sense_flag) ) {
	DumpSenseData(sgp, False, ssp);
    }
    return;
}

void
libReportScsiSense(scsi_generic_t *sgp, int scsi_status, scsi_sense_t *ssp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    unsigned char sense_key, asc, asq;
    char *ascq_msg;

    GetSenseErrors(ssp, &sense_key, &asc, &asq);
    ascq_msg = ScsiAscqMsg(asc, asq);
    Fprintf(opaque, "SCSI Status = %#x (%s)\n", scsi_status, ScsiStatus(scsi_status));
    Fprintf(opaque, "Sense Key = %d = %s, Sense Code/Qualifier = (%#x, %#x)",
	    sense_key, SenseKeyMsg(sense_key), asc, asq);
    if (ascq_msg) {
      Fprint(opaque, " - %s", ascq_msg);
    }
    Fprintnl(opaque);
    return;
}

/* ======================================================================== */

#include "inquiry.h"

#if 0
/*
 * Declarations/Definitions for Inquiry Command:
 */
#define InquiryName           "Inquiry"
#define InquiryOpcode         0x12
#define InquiryCdbSize        6
#define InquiryTimeout        ScsiDefaultTimeout

/*
 * Inquiry Command Descriptor Block:
 */
struct Inquiry_CDB {
	u_char	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	u_char	evpd	: 1,		/* Enable Vital Product Data.	[1] */
			: 4,		/* Reserved.			    */
		lun	: 3;		/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	u_char	lun	: 3,		/* Logical Unit Number.		    */
			: 4,		/* Reserved.			    */
		evpd	: 1;		/* Enable Vital Product Data.	[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	u_char	pgcode	: 8;		/* EVPD Page Code.		[2] */
	u_char		: 8;		/* Reserved.			[3] */
	u_char	alclen;			/* Allocation Length.		[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	u_char	link	: 1,		/* Link.			[5] */
		flag	: 1,		/* Flag.			    */
			: 4,		/* Reserved.			    */
		vendor	: 2;		/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	u_char	vendor	: 2,		/* Vendor Unique.		    */
			: 4,		/* Reserved.			    */
		flag	: 1,		/* Flag.			    */
		link	: 1;		/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};
#endif /* 0 */

/*
 * Inquiry() - Send a SCSI Inquiry Command.
 *
 * Inputs:
 *  fd     = The file descriptor to issue Inquiry to.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debugging.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to scsi generic pointer (optional).
 *  data   = Buffer for received Inquiry data.
 *  len    = The length of the data buffer.
 *  page   = The Inquiry VPD page (if any).
 *  sflags = OS specific SCSI flags (if any).
 *  timeout = The timeout value (in ms).
 *  tsp = The tool specific information.
 *
 * Return Value:
 *  Returns the status from the IOCTL request which is:
 *    0 = Success, -1 = Failure
 *
 * Note:  If the caller supplies a SCSI generic pointer, then
 * it's the callers responsibility to free this structure, along
 * with the data buffer and sense buffer.  This capability is
 * provided so the caller can examine SCSI status, sense data,
 * and data transfers, to make (more) intelligent decisions.
 */
int
Inquiry(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
	scsi_addr_t *sap, scsi_generic_t **sgpp,
	void *data, unsigned int len, unsigned char page,
	unsigned int sflags, unsigned int timeout, tool_specific_t *tsp)
{
    scsi_generic_t scsi_generic;
    scsi_generic_t *sgp = &scsi_generic;
    void *opaque = (tsp) ? tsp->opaque : NULL;
    struct Inquiry_CDB *cdb; 
    int error;

    /*
     * Setup and/or allocate a SCSI generic data structure.
     */
    if (sgpp && *sgpp) {
	sgp = *sgpp;
    } else {
	sgp = init_scsi_generic(tsp);
    }
    sgp->fd         = fd;
    sgp->dsf        = dsf;
    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    if (data && len) memset(data, 0, len);
    cdb             = (struct Inquiry_CDB *)sgp->cdb;
    cdb->opcode     = InquiryOpcode;
    if (page) {
	cdb->page_code = page;
	cdb->evpd   = True;
    }
    cdb->allocation_length = len;
    sgp->cdb_size   = InquiryCdbSize;
    sgp->cdb_name   = InquiryName;
    sgp->data_dir   = scsi_data_read;
    sgp->data_buffer= data;
    sgp->data_length= len;
    sgp->debug      = debug;
    sgp->errlog     = errlog;
    sgp->sflags     = sflags;
    sgp->timeout    = (timeout) ? timeout : InquiryTimeout;

    /*
     * If a SCSI address was specified, do a structure copy.
     */
    if (sap) {
	sgp->scsi_addr = *sap;	/* Copy the SCSI address info. */
    }

    error = libExecuteCdb(sgp);

    /*
     * If the user supplied a pointer, send the SCSI generic data
     * back to them for further analysis.
     */
    if (sgpp) {
	if (*sgpp == NULL) {
	    *sgpp = sgp;        /* Return the generic data pointer. */
	}
    } else {
	free_palign(opaque, sgp->sense_data);
	free(sgp);
    }
    return(error);
}

int
verify_inquiry_header(inquiry_t *inquiry, inquiry_header_t *inqh, unsigned char page)
{
    if ( (inqh->inq_page_length == 0) ||
	 (inqh->inq_page_code != page) ||
	 (inqh->inq_dtype != inquiry->inq_dtype) ) {
	return(FAILURE);
    }
    return(SUCCESS);
}

/* ======================================================================== */

/*
 * GetDeviceIdentifier() - Gets Inquiry Device ID Page.
 * 
 * Description:
 *      This function decodes each of the device ID descriptors and applies
 * and precedence algorthm to find the *best* device identifier (see table).
 * 
 * Note: This API is a wrapper around Inquiry, originally designed to simply
 * return an identifier string. Therefore, a buffer and length are omitted.
 *
 * Inputs:
 *  fd     = The file descriptor.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to SCSI generic pointer (optional).
 *  inqp   = Pointer to device Inquiry data.
 *  timeout = The timeout value (in ms).
 *  tsp = The tool specific information.
 *
 * Return Value:
 *    Returns NULL of no device ID page or it's invalid.
 *    Otherwise, returns a pointer to a malloc'd buffer w/ID.
 */
char *
GetDeviceIdentifier(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
		    scsi_addr_t *sap, scsi_generic_t **sgpp,
		    void *inqp, unsigned int timeout, tool_specific_t *tsp)
{
    void *opaque = (tsp) ? tsp->opaque : NULL;
    inquiry_t *inquiry = inqp;
    inquiry_page_t inquiry_data;  
    inquiry_page_t *inquiry_page = &inquiry_data;
    inquiry_header_t *inqh = &inquiry_page->inquiry_hdr;
    unsigned char page = INQ_DEVICE_PAGE;
    int status;

    status = Inquiry(fd, dsf, debug, errlog, NULL, NULL, inquiry_page,
		     sizeof(*inquiry_page), page, 0, timeout, tsp);

    if (status != SUCCESS) return(NULL);

    if (verify_inquiry_header(inquiry, inqh, page) == FAILURE) return(NULL);

    return( DecodeDeviceIdentifier(opaque, inquiry, inquiry_page, True) );
}

/*
 * DecodeDeviceIdentifier() - Decode the Inquiry Device Identifier. 
 *  
 * Note: We allow separate decode, since we may be extracting different parts 
 * of the device ID page, such as LUN device ID and target port address, and we 
 * wish to avoid multiple Inquiry page requests (we may have lots of devices). 
 *  
 * Return Value: 
 *      The LUN device identifier string or NULL if none found.
 *      The buffer is dynamically allocated, so caller must free it.
 */
char *
DecodeDeviceIdentifier(void *opaque, inquiry_t *inquiry,
		       inquiry_page_t *inquiry_page, hbool_t hyphens)
{
    inquiry_ident_descriptor_t *iid;
    size_t page_length;
    char *bp = NULL;
    /* Identifiers in order of precedence: (the "Smart" way :-) */
    /* REMEMBER: The lower values have *higher* precedence!!! */
    enum pidt {
	REGEXT, REG, EXT_V, EXT_0, EUI64, TY1_VID, BINARY, ASCII, NONE
    };
    enum pidt pid_type = NONE;	/* Precedence ID type. */

    page_length = (size_t) inquiry_page->inquiry_hdr.inq_page_length;
    iid = (inquiry_ident_descriptor_t *)inquiry_page->inquiry_page_data;

    /*
     * Notes:
     *	- We loop through ALL descriptors, enforcing the precedence
     *	  order defined above (see enum pidt). This is because some
     *	  devices return more than one identifier.
     */
    while ( (ssize_t)page_length > 0 ) {
	unsigned char *fptr = (unsigned char *)iid + sizeof(*iid);

	switch (iid->iid_code_set) {
	    
	    case IID_CODE_SET_ASCII: {
		/* Only accept Vendor ID's of Type 1. */
		if ( (pid_type > TY1_VID) &&
		     (iid->iid_ident_type == IID_ID_TYPE_T10_VID) ) {
		    int id_len = iid->iid_ident_length + sizeof(inquiry->inq_pid);
		    if (bp) {
			free(bp) ; bp = NULL;
		    };
		    bp = (char *)malloc(id_len + 1);
		    if (bp == NULL) return(NULL);
		    pid_type = TY1_VID;
		    memcpy(bp, inquiry->inq_pid, sizeof(inquiry->inq_pid));
		    memcpy((bp + sizeof(inquiry->inq_pid)), fptr, iid->iid_ident_length);
		}
		/* Continue looping looking for IEEE identifier. */
		break;
	    } /* end case IID_CODE_SET_ASCII */

	    case IID_CODE_SET_BINARY: {
		/*
		 * This is the preferred (unique) identifier.
		 */
		switch (iid->iid_ident_type) {
		    
		    case IID_ID_TYPE_NAA: {
			enum pidt npid_type;
			int i = 0;

			/*
			 * NAA is the high order 4 bits of the 1st byte.
			 */
			switch ( (*fptr >> 4) & 0xF) {

			    case NAA_IEEE_REG_EXTENDED:
				npid_type = REGEXT;
				break;
			    case NAA_IEEE_REGISTERED:
				npid_type = REG;
				break;
			    case NAA_IEEE_EXTENDED:
				npid_type = EXT_V;
				break;
			    case 0x1:	  /* ???? */
				npid_type = EXT_0;
				break;
			    default:
				/* unrecognized */
				npid_type = BINARY;
				break;
			}
			/*
			 * If the previous precedence ID is of lower priority,
			 * that is a higher value, then make this pid the new.
			 */
			if ( (pid_type > npid_type) ) {
			    int blen = (iid->iid_ident_length * 3);
			    char *bptr;
			    pid_type = npid_type; /* Set the new precedence type */
			    if (bp) {
				free(bp) ; bp = NULL;
			    };
			    bptr = bp = Malloc(opaque, blen);
			    if (bp == NULL) return(NULL);
			    if (hyphens == False) {
				bptr += sprintf(bptr, "0x");
			    }

			    /* Format as: xxxx-xxxx... */
			    while (i < (int)iid->iid_ident_length) {
				bptr += sprintf(bptr, "%02x", fptr[i++]);
				if (hyphens == True) {
				    if (( (i % 2) == 0) &&
					(i < (int)iid->iid_ident_length) ) {
					bptr += sprintf(bptr, "-");
				    }
				}
			    }
			}
			break;
		    }
		    case IID_ID_TYPE_EUI64: {
			int blen, i = 0;
			char *bptr;

			if ( (pid_type <= EUI64) ) {
			    break;
			}
			pid_type = EUI64;
			blen = (iid->iid_ident_length * 3);
			if (bp) {
			    free(bp) ; bp = NULL;
			};
			bptr = bp = (char *)malloc(blen);
			if (bp == NULL)	return(NULL);
			if (hyphens == False) {
			    bptr += sprintf(bptr, "0x");
			}

			/* Format as: xxxx-xxxx... */
			while (i < (int)iid->iid_ident_length) {
			    bptr += sprintf(bptr, "%02x", fptr[i++]);
			    if (hyphens == True) {
				if (( (i % 2) == 0) &&
				    (i < (int)iid->iid_ident_length) ) {
				    bptr += sprintf(bptr, "-");
				}
			    }
			}
			break;
		    }
		    case IID_ID_TYPE_VS:
		    case IID_ID_TYPE_T10_VID:
		    case IID_ID_TYPE_RELTGTPORT:
		    case IID_ID_TYPE_TGTPORTGRP:
		    case IID_ID_TYPE_LOGUNITGRP:
		    case IID_ID_TYPE_MD5LOGUNIT:
		    case IID_ID_TYPE_SCSI_NAME:
		    case IID_ID_TYPE_PROTOPORT:
			break;

		    default: {
			/* Note: We need updated with new descriptors added! */
			//Fprintf(opaque, "Unknown identifier type %#x\n", iid->iid_ident_type);
			break;
		    }
		} /* switch (iid->iid_ident_type) */
		break;
	    } /* end case IID_CODE_SET_BINARY */

	    case IID_CODE_SET_ISO_IEC:
		break;

	    default: {
		//Fprintf(opaque, "Unknown identifier code set %#x\n", iid->iid_code_set);
		break;
	    } /* end case of code set. */
	} /* switch (iid->iid_code_set) */

	page_length -= iid->iid_ident_length + sizeof(*iid);
	iid = (inquiry_ident_descriptor_t *)((ptr_t) iid + iid->iid_ident_length + sizeof(*iid));

    } /* while ( (ssize_t)page_length > 0 ) */

    /* NOTE: Caller MUST free allocated buffer! */
    return(bp);
}

/*
 * GetNAAIdentifier() - Gets Inquiry Device ID NAA Identifier.
 *
 * Inputs:
 *  fd     = The file descriptor.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sgpp   = Pointer to SCSI generic pointer (optional).
 *  naa_id = Pointer to return NAA identifier buffer.
 *  naa_len = Pointer to return NAA identifier length;
 *  tsp = The tool specific information.
 *
 * Return Value:
 *    Returns Success / Failure.
 */
int
GetNAAIdentifier(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
		 scsi_generic_t **sgpp, uint8_t **naa_id, int *naa_len, tool_specific_t *tsp)
{
    inquiry_page_t inquiry_data;  
    inquiry_page_t *inquiry_page = &inquiry_data;
    inquiry_ident_descriptor_t *iid;
    unsigned char page = INQ_DEVICE_PAGE;
    size_t page_length;
    int status;

    status = Inquiry(fd, dsf, debug, errlog, NULL, NULL,
		     inquiry_page, sizeof(*inquiry_page), page, 0, 0, tsp);

    if (status != SUCCESS) return(status);

    page_length = (size_t) inquiry_page->inquiry_hdr.inq_page_length;
    iid = (inquiry_ident_descriptor_t *)inquiry_page->inquiry_page_data;

    /*
     * Loop through all identifiers until we find NAA ID.* 
     */
    while ( (ssize_t)page_length > 0 ) {

	/*
	 * Yes this could be simplied with a series of "if" statements, but
	 * I'm leaving "as is", since this should be a general lookup function.
	 */ 
	switch (iid->iid_code_set) {
	
	    case IID_CODE_SET_BINARY: {

		switch (iid->iid_association) {
		    
		    case IID_ASSOC_LOGICAL_UNIT: {

			switch (iid->iid_ident_type) {
			
			    case IID_ID_TYPE_NAA: {
				unsigned char *fptr = (unsigned char *)iid + sizeof(*iid);
				size_t id_size = (size_t)iid->iid_ident_length;
	
				*naa_id = Malloc(NULL, id_size);
				*naa_len = (int)id_size;
				memcpy(*naa_id, fptr, id_size);
				return (SUCCESS);
			    }
			    default:
				break;
			} /* switch (iid->iid_ident_type) */
		    }
		    default:
			break;
		} /* switch (iid->iid_association) */
	    } /* case IID_CODE_SET_BINARY */

	    default:
		break;
	} /* switch (iid->iid_code_set) */

	page_length -= iid->iid_ident_length + sizeof(*iid);
	iid = (inquiry_ident_descriptor_t *)((ptr_t) iid + iid->iid_ident_length + sizeof(*iid));

    } /* while ( (ssize_t)page_length > 0 ) */

    return (FAILURE);
}

/*
 * GetTargetPortIdentifier() - Gets Inquiry Device ID Target Port Identifier.
 *
 * Inputs:
 *  fd     = The file descriptor.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sgpp   = Pointer to SCSI generic pointer (optional).
 *  timeout = The timeout value (in ms).
 *  tsp = The tool specific information.
 *
 * Return Value:
 *    Returns dynamically allocated buffer w/Target Port or NULL if not found.
 */
char *
GetTargetPortIdentifier(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
			scsi_generic_t **sgpp, void *inqp,
			unsigned int timeout, tool_specific_t *tsp)
{
    void *opaque = (tsp) ? tsp->opaque : NULL;
    inquiry_t *inquiry = inqp;
    inquiry_page_t inquiry_data;  
    inquiry_page_t *inquiry_page = &inquiry_data;
    inquiry_header_t *inqh = &inquiry_page->inquiry_hdr;
    unsigned char page = INQ_DEVICE_PAGE;
    int status;

    status = Inquiry(fd, dsf, debug, errlog, NULL, NULL,
		     inquiry_page, sizeof(*inquiry_page), page, 0, 0, tsp);

    if (status != SUCCESS) return(NULL);

    if (verify_inquiry_header(inquiry, inqh, page) == FAILURE) return(NULL);
    
    return( DecodeTargetPortIdentifier(opaque, inquiry, inquiry_page) );
}

/*
 * DecodeTargetPortIdentifier() - Decode the Target Port Identifier.
 *  
 * Note: We allow separate decode, since we may be extracting different parts 
 * of the device ID page, such as LUN device ID and target port address, and we 
 * wish to avoid multiple Inquiry page requests (we may have lots of devices). 
 *  
 * Return Value: 
 *      The target port identifier string or NULL if none found.
 *      The buffer is dynamically allocated, so caller must free it.
 */
char *
DecodeTargetPortIdentifier(void *opaque, inquiry_t *inquiry, inquiry_page_t *inquiry_page)
{
    inquiry_ident_descriptor_t *iid;
    uint8_t *ucp = NULL;
    size_t page_length;

    page_length = (size_t)inquiry_page->inquiry_hdr.inq_page_length;
    iid = (inquiry_ident_descriptor_t *)inquiry_page->inquiry_page_data;

    /*
     * Loop through all identifiers until we find NAA ID. 
     */
    while ( (ssize_t)page_length > 0 ) {
	/*
	 * Yes this could be simplied with a series of "if" statements, but
	 * I'm leaving "as is", since this should be a general lookup function.
	 */ 
	switch (iid->iid_code_set) {
	
	    case IID_CODE_SET_BINARY: {

		switch (iid->iid_association) {
		    
		    case IID_ASSOC_TARGET_PORT: {

			switch (iid->iid_ident_type) {
			
			    case IID_ID_TYPE_NAA: {
				unsigned char *fptr = (unsigned char *)iid + sizeof(*iid);
				size_t id_size = (size_t)iid->iid_ident_length;
				char *bp;
				int i = 0;

				bp = ucp = Malloc(opaque, id_size + 3);
				if (ucp == NULL) return(ucp);
				ucp += sprintf(ucp, "0x");
				while (i < (int)iid->iid_ident_length) {
				    ucp += sprintf(ucp, "%02x", fptr[i++]);
				}
				return(bp);
			    }
			    default:
				break;
			} /* switch (iid->iid_ident_type) */
		    }
		    default:
			break;
		} /* switch (iid->iid_association) */
	    } /* case IID_CODE_SET_BINARY */

	    default:
		break;
	} /* switch (iid->iid_code_set) */

	page_length -= iid->iid_ident_length + sizeof(*iid);
	iid = (inquiry_ident_descriptor_t *)((ptr_t) iid + iid->iid_ident_length + sizeof(*iid));

    } /* while ( (ssize_t)page_length > 0 ) */

    return(NULL);
}

/*
 * GetSerialNumber() - Gets Inquiry Serial Number Page.
 *
 * Inputs:
 *  fd     = The file descriptor.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to SCSI generic pointer (optional).
 *  inqp   = Pointer to device Inquiry data.
 *  timeout = The timeout value (in ms).
 *  tsp = The tool specific information.
 *
 * Return Value:
 *    Returns NULL if no serial number page or it's invalid.
 *    Otherwise, returns a pointer to a malloc'd buffer w/serial #.
 */
char *
GetSerialNumber(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
		scsi_addr_t *sap, scsi_generic_t **sgpp,
		void *inqp, unsigned int timeout, tool_specific_t *tsp)
{
    inquiry_t *inquiry = inqp;
    inquiry_page_t inquiry_data;  
    inquiry_page_t *inquiry_page = &inquiry_data;
    inquiry_header_t *inqh = &inquiry_page->inquiry_hdr;
    unsigned char page = INQ_SERIAL_PAGE;
    size_t page_length;
    char *bp;
    int status;

    status = Inquiry(fd, dsf, debug, errlog, NULL, NULL,
		     inquiry_page, sizeof(*inquiry_page), page, 0,
		     timeout, tsp);

    if (status != SUCCESS) return(NULL);

    if (verify_inquiry_header(inquiry, inqh, page) == FAILURE) return(NULL);

    page_length = (size_t)inquiry_page->inquiry_hdr.inq_page_length;
    bp = (char *)malloc(page_length + 1);
    if (bp == NULL) return(NULL);
    strncpy (bp, (char *)inquiry_page->inquiry_page_data, page_length);
    bp[page_length] = '\0';
    /* NOTE: Caller MUST free allocated buffer! */
    return(bp);
}

/*
 * GetMgmtNetworkAddress() - Gets Inquiry Managment Network Address.
 *
 * Inputs:
 *  fd     = The file descriptor.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to SCSI generic pointer (optional).
 *  inqp   = Pointer to device Inquiry data.
 *  timeout = The timeout value (in ms).
 *  tsp = The tool specific information.
 *
 * Return Value:
 *    Returns NULL if no serial number page or it's invalid.
 *    Otherwise, returns a pointer to a malloc'd buffer w/mgmt address.
 */
char *
GetMgmtNetworkAddress(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
	       scsi_addr_t *sap, scsi_generic_t **sgpp,
	       void *inqp, unsigned int timeout, tool_specific_t *tsp)
{
    inquiry_t *inquiry = inqp;
    inquiry_page_t inquiry_data;  
    inquiry_page_t *inquiry_page = &inquiry_data;
    inquiry_header_t *inqh = &inquiry_page->inquiry_hdr;
    inquiry_network_service_page_t *inap;
    unsigned char page = INQ_MGMT_NET_ADDR_PAGE;
    size_t address_length;
    char *bp;
    int status;

    status = Inquiry(fd, dsf, debug, errlog, NULL, sgpp,
		     inquiry_page, sizeof(*inquiry_page), page, 0, timeout, tsp);

    if (status != SUCCESS) return(NULL);

    if (verify_inquiry_header(inquiry, inqh, page) == FAILURE) return(NULL);

    inap = (inquiry_network_service_page_t *)&inquiry_page->inquiry_page_data;
    address_length = (size_t)StoH(inap->address_length);
    if (address_length == 0) return(NULL);
    bp = (char *)Malloc(NULL, (address_length + 1) );
    if (bp == NULL) return(NULL);
    strncpy(bp, (char *)inap->address, address_length);
    bp[address_length] = '\0';
    /* NOTE: Caller MUST free allocated buffer! */
    return(bp);
}

/*
 * GetUniqueID - Get The Devices' Unique ID.
 *
 * Description:
 *  This function returns a unique identifer for this device.
 * We attemp to obtain the Inquiry Device ID page first, then
 * if that fails we attempt to obtain the serial num,ber page.
 *
 * Inputs:
 *  fd     = The file descriptor.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  identifier = Pointer to buffer to return identifier.
 *  idt    = The ID type(s) to attempt (IDT_BOTHIDS, IDT_DEVICEID, IDT_SERIALID)
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  timeout = The timeout value (in ms).
 *  tsp = The tool specific information.
 *
 * Returns:
 *  Returns IDT_NONE and set identifier ptr to NULL if an error occurs.
 *  Returns IDT_DEVICEID if identifier points to an actual device ID.
 *  Returns IDT_SERIALID if identifier points to a manufactured identifier,
 *    using Inquiry vendor/product info and serial number page.
 *
 * Note: The identifier is dynamically allocated, so the caller is
 * responsible to free that memory, when it's no longer desired.
 */
idt_t
GetUniqueID(HANDLE fd, char *dsf,
	    char **identifier, idt_t idt,
	    hbool_t debug, hbool_t errlog,
	    unsigned int timeout, tool_specific_t *tsp)
{
    inquiry_t inquiry_data;
    inquiry_t *inquiry = &inquiry_data;
    char *serial_number;
    int status;

    *identifier = NULL;
    /*
     * Start off by requesting the standard Inquiry data.
     */
    status = Inquiry(fd, dsf, debug, errlog, NULL, NULL,
		     inquiry, sizeof(*inquiry), 0, 0, timeout, tsp);
    if (status != SUCCESS) {
	return(IDT_NONE);
    }

    if ( (idt == IDT_BOTHIDS) || (idt == IDT_DEVICEID) ) {
	/*
	 * The preferred ID, is from Inquiry Page 0x83 (Device ID).
	 */
	if (*identifier = GetDeviceIdentifier(fd, dsf, debug, errlog,
					      NULL, NULL, inquiry, timeout, tsp)) {
	    return(IDT_DEVICEID);
	}
    }

    if ( (idt == IDT_BOTHIDS) || (idt == IDT_SERIALID) ) {
	/*
	 * The less preferred WWID, is the serial number prepended with
	 * the vendor and product names to attempt uniqueness.
	 */
	if (serial_number = GetSerialNumber(fd, dsf, debug, errlog,
					    NULL, NULL, inquiry, timeout, tsp)) {
	    *identifier = malloc(MAX_INQ_LEN + INQ_VID_LEN + INQ_PID_LEN);
	    *identifier[0] = '\0';
	    (void)strncpy(*identifier, (char *)inquiry->inq_vid, INQ_VID_LEN);
	    (void)strncat(*identifier, (char *)inquiry->inq_pid, INQ_PID_LEN);
	    (void)strcat(*identifier, serial_number);
	    (void)free(serial_number);
	    return(IDT_SERIALID);
	}
    }
    return(IDT_NONE);
}

/* ======================================================================== */

/*
 * AtaGetDriveFwVersion() - Get the ATA Drive FW Version.
 *
 * Inputs:
 *  fd     = The file descriptor.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to SCSI generic pointer (optional).
 *  inqp   = Pointer to device Inquiry data.
 *  timeout = The timeout value (in ms).
 *  tsp = The tool specific information.
 *
 * Return Value:
 *    Returns NULL if no FW version or CDB is invalid (failure).
 *    Otherwise, returns a pointer to a malloc'd buffer w/FW version.
 */
char *
AtaGetDriveFwVersion(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
		     scsi_addr_t *sap, scsi_generic_t **sgpp,
		     void *inqp, unsigned int timeout, tool_specific_t *tsp)
{
    void *opaque = (tsp) ? tsp->opaque : NULL;
    uint8_t identify_data[IDENTIFY_DATA_LENGTH];
    unsigned int identify_length = IDENTIFY_DATA_LENGTH;
    char *bp, *fwver;
    int i, status;

    status = AtaIdentify(fd, dsf, debug, errlog, NULL, NULL,
			 identify_data, identify_length, 0, 0, tsp);

    if (status != SUCCESS) return(NULL);

    fwver = Malloc(opaque, IDENTIFY_FW_LENGTH + 1);
    if (fwver == NULL) return(fwver);

    /*
     * Due to the strangeness of ATA, we MUST byte swap ASCII strings! 
     * Note: ATA data is packed in 16-bit words, with 2nd character in 
     * low byte and 1st character in high byte.
     */
    for (i = 0, bp = fwver; i < IDENTIFY_FW_LENGTH; i += 2) {
	*bp++ = identify_data[IDENTIFY_FW_OFFSET + i + 1];
	*bp++ = identify_data[IDENTIFY_FW_OFFSET + i];
    }
    fwver[IDENTIFY_FW_LENGTH] = '\0';

    return(fwver);
}

/*
 * AtaIdentify() - Send an ATA Identify Command.
 *
 * Inputs:
 *  fd     = The file descriptor to issue Inquiry to.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debugging.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to scsi generic pointer (optional).
 *  data   = Buffer for received Identify data.
 *  len    = The length of the data buffer.
 *  sflags = OS specific SCSI flags (if any).
 *  timeout = The timeout value (in ms).
 *  tsp = The tool specific information.
 *
 * Return Value:
 *  Returns the status from the IOCTL request which is:
 *    0 = Success, -1 = Failure
 *
 * Note:  If the caller supplies a SCSI generic pointer, then
 * it's the callers responsibility to free this structure, along
 * with the data buffer and sense buffer.  This capability is
 * provided so the caller can examine SCSI status, sense data,
 * and data transfers, to make (more) intelligent decisions.
 */
int
AtaIdentify(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
	    scsi_addr_t *sap, scsi_generic_t **sgpp,
	    void *data, unsigned int len,
	    unsigned int sflags, unsigned int timeout, tool_specific_t *tsp)
{
    scsi_generic_t scsi_generic;
    scsi_generic_t *sgp = &scsi_generic;
    void *opaque = (tsp) ? tsp->opaque : NULL;
    AtaPassThrough16_CDB_t *cdb; 
    int error;

    /*
     * Setup and/or allocate a SCSI generic data structure.
     */
    if (sgpp && *sgpp) {
	sgp = *sgpp;
    } else {
	sgp = init_scsi_generic(tsp);
    }
    sgp->fd         = fd;
    sgp->dsf        = dsf;
    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    if (data && len) memset(data, 0, len);

    cdb             = (AtaPassThrough16_CDB_t *)sgp->cdb;
    cdb->opcode     = ATA_PASSTHROUGH_OPCODE;
    cdb->command    = ATA_IDENTIFY_COMMAND;
    cdb->protocol   = PROTOCOL_PIO_DATA_IN;
    cdb->t_length   = T_LENGTH_SECTOR_COUNT;
    cdb->byt_blok   = BYT_BLOK_TRANSFER_BLOCKS;
    cdb->t_dir      = T_DIR_FROM_ATA_DEVICE;
    cdb->sector_count_low = IDENTIFY_SECTOR_COUNT;

    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "ATA Identify";
    sgp->data_dir   = scsi_data_read;
    sgp->data_buffer= data;
    sgp->data_length= len;
    sgp->debug      = debug;
    sgp->errlog     = errlog;
    sgp->sflags     = sflags;
    sgp->timeout    = (timeout) ? timeout : InquiryTimeout;

    /*
     * If a SCSI address was specified, do a structure copy.
     */
    if (sap) {
	sgp->scsi_addr = *sap;	/* Copy the SCSI address info. */
    }

    error = libExecuteCdb(sgp);

    /*
     * If the user supplied a pointer, send the SCSI generic data
     * back to them for further analysis.
     */
    if (sgpp) {
	if (*sgpp == NULL) {
	    *sgpp = sgp;        /* Return the generic data pointer. */
	}
    } else {
	free_palign(opaque, sgp->sense_data);
	free(sgp);
    }
    return(error);
}

/* ======================================================================== */

/*
 * Declarations/Definitions for Read Capacity(10) Command:
 */
#define ReadCapacity10Name	"Read Capacity(10)"
#define ReadCapacity10Opcode	0x25
#define ReadCapacity10CdbSize	10 
#define ReadCapacity10Timeout	ScsiDefaultTimeout

/*
 * ReadCapacity10() - Issue Read Capacity (10) Command.
 *
 * Inputs:
 *  fd     = The file descriptor to issue bus reset to.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to SCSI generic pointer (optional).
 *  data   = Buffer for received capacity data.
 *  len    = The length of the data buffer.
 *  sflags = OS specific SCSI flags (if any).
 *  timeout = The timeout value (in ms).
 *  tsp = The tool specific information.
 *
 * Return Value:
 *  Returns the status from the IOCTL request which is:
 *    0 = Success, -1 = Failure
 *
 * Note:  If the caller supplies a SCSI generic pointer, then
 * it's the callers responsibility to free this structure, along
 * with the data buffer and sense buffer.  This capability is
 * provided so the caller can examine SCSI status, sense data,
 * and data transfers, to make (more) intelligent decisions.
 */
int
ReadCapacity10(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
	       scsi_addr_t *sap, scsi_generic_t **sgpp,
	       void *data, unsigned int len,
	       unsigned int sflags, unsigned int timeout, tool_specific_t *tsp)
{
    void *opaque = (tsp) ? tsp->opaque : NULL;
    scsi_generic_t scsi_generic;
    scsi_generic_t *sgp = &scsi_generic;
    int error;

    /*
     * Setup and/or allocate a SCSI generic data structure.
     */
    if (sgpp && *sgpp) {
	sgp = *sgpp;
    } else {
	sgp = init_scsi_generic(tsp);
    }
    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    if (data && len) memset(data, 0, len);
    sgp->fd         = fd;
    sgp->dsf        = dsf;
    sgp->cdb[0]     = ReadCapacity10Opcode;
    sgp->cdb_size   = ReadCapacity10CdbSize;
    sgp->cdb_name   = ReadCapacity10Name;
    sgp->data_dir   = scsi_data_read;
    sgp->data_buffer= data;
    sgp->data_length= len;
    sgp->debug	    = debug;
    sgp->errlog     = errlog;
    sgp->timeout    = (timeout) ? timeout : ReadCapacity10Timeout;
    /*
     * If a SCSI address was specified, do a structure copy.
     */
    if (sap) {
	sgp->scsi_addr = *sap;	/* Copy the SCSI address info. */
    }

    error = libExecuteCdb(sgp);

    /*
     * If the user supplied a pointer, send the SCSI generic data
     * back to them for further analysis.
     */
    if (sgpp) {
	if (*sgpp == NULL) {
	    *sgpp = sgp;        /* Return the generic data pointer. */
	}
    } else {
	free_palign(opaque, sgp->sense_data);
	free(sgp);
    }
    return(error);
}
/* ======================================================================== */

/*
 * Declarations/Definitions for Read Capacity(16) Command:
 */
#define ReadCapacity16Name	"Read Capacity(16)"
#define ReadCapacity16Opcode	0x9e
#define ReadCapacity16Subcode	0x10
#define ReadCapacity16CdbSize	16 
#define ReadCapacity16Timeout	ScsiDefaultTimeout

/*
 * ReadCapacity16() - Issue Read Capacity (16) Command.
 *
 * Inputs:
 *  fd     = The file descriptor to issue bus reset to.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to SCSI generic pointer (optional).
 *  data   = Buffer for received capacity data.
 *  len    = The length of the data buffer.
 *  sflags = OS specific SCSI flags (if any).
 *  timeout = The timeout value (in ms).
 *  tsp = The tool specific information.
 *
 * Return Value:
 *  Returns the status from the IOCTL request which is:
 *    0 = Success, -1 = Failure
 *
 * Note:  If the caller supplies a SCSI generic pointer, then
 * it's the callers responsibility to free this structure, along
 * with the data buffer and sense buffer.  This capability is
 * provided so the caller can examine SCSI status, sense data,
 * and data transfers, to make (more) intelligent decisions.
 */
int
ReadCapacity16(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
	       scsi_addr_t *sap, scsi_generic_t **sgpp,
	       void *data, unsigned int len,
	       unsigned int sflags, unsigned int timeout, tool_specific_t *tsp)
{
    void *opaque = (tsp) ? tsp->opaque : NULL;
    scsi_generic_t scsi_generic;
    scsi_generic_t *sgp = &scsi_generic;
    ReadCapacity16_CDB_t *cdb;
    int error;

    /*
     * Setup and/or allocate a SCSI generic data structure.
     */
    if (sgpp && *sgpp) {
	sgp = *sgpp;
    } else {
	sgp = init_scsi_generic(tsp);
    }
    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    if (data && len) memset(data, 0, len);
    sgp->fd         = fd;
    sgp->dsf        = dsf;
    cdb = (ReadCapacity16_CDB_t *)sgp->cdb;
    cdb->opcode     = ReadCapacity16Opcode;
    cdb->service_action = ReadCapacity16Subcode;
    HtoS(cdb->allocation_length,  len);
    sgp->cdb_size   = ReadCapacity16CdbSize;
    sgp->cdb_name   = ReadCapacity16Name;
    sgp->data_dir   = scsi_data_read;
    sgp->data_buffer= data;
    sgp->data_length= len;
    sgp->debug      = debug;
    sgp->errlog     = errlog;
    sgp->timeout    = (timeout) ? timeout : ReadCapacity16Timeout;
    /*
     * If a SCSI address was specified, do a structure copy.
     */
    if (sap) {
	sgp->scsi_addr = *sap;	/* Copy the SCSI address info. */
    }

    error = libExecuteCdb(sgp);

    /*
     * If the user supplied a pointer, send the SCSI generic data
     * back to them for further analysis.
     */
    if (sgpp) {
	if (*sgpp == NULL) {
	    *sgpp = sgp;        /* Return the generic data pointer. */
	}
    } else {
	free_palign(opaque, sgp->sense_data);
	free(sgp);
    }
    return(error);
}

/* ======================================================================== */

#define ReadTimeout        ScsiDefaultTimeout

int
ReadData(scsi_io_type_t read_type, scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int status;

    switch (read_type) {
	case scsi_read6_cdb:
	    status = Read6(sgp, (uint32_t)lba, (uint8_t)blocks, bytes);
	    break;

	case scsi_read10_cdb:
	    status = Read10(sgp, (uint32_t)lba, (uint16_t)blocks, bytes);
	    break;

	case scsi_read16_cdb:
	    status = Read16(sgp, (uint64_t)lba, (uint32_t)blocks, bytes);
	    break;

	default:
	    Fprintf(opaque, "Invalid read I/O type detected, type = %d\n", read_type);
	    status = FAILURE;
	    break;
    }
    return (status);
}

/*
 * Read6() - Send a Read(6) CDB.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	lba = The starting logical block address.
 * 	blocks = The number of blocks to transfer.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
Read6(scsi_generic_t *sgp, uint32_t lba, uint8_t blocks, uint32_t bytes)
{
    DirectRW6_CDB_t *cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (DirectRW6_CDB_t *)sgp->cdb;
    cdb->opcode     = SOPC_READ_6;
    HtoS(cdb->lba, lba);
    cdb->length	    = blocks;
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "Read(6)";
    sgp->data_dir   = scsi_data_read;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = ReadTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

/*
 * Read10() - Send a Read(10) CDB.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	lba = The starting logical block address.
 * 	blocks = The number of blocks to transfer.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
Read10(scsi_generic_t *sgp, uint32_t lba, uint16_t blocks, uint32_t bytes)
{
    DirectRW10_CDB_t *cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (DirectRW10_CDB_t *)sgp->cdb;
    cdb->opcode     = SOPC_READ_10;
    HtoS(cdb->lba, lba);
    HtoS(cdb->length, blocks);
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "Read(10)";
    sgp->data_dir   = scsi_data_read;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = ReadTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

/*
 * Read16() - Send a Read(16) CDB.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	lba = The starting logical block address.
 * 	blocks = The number of blocks to transfer.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
Read16(scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes)
{
    DirectRW16_CDB_t *cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (DirectRW16_CDB_t *)sgp->cdb;
    cdb->opcode     = SOPC_READ_16;
    HtoS(cdb->lba, lba);
    HtoS(cdb->length, blocks);
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "Read(16)";
    sgp->data_dir   = scsi_data_read;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = ReadTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

/* ======================================================================== */

#define WriteTimeout        ScsiDefaultTimeout

int
WriteData(scsi_io_type_t write_type, scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int status;

    switch (write_type) {
	case scsi_write6_cdb:
	    status = Write6(sgp, (uint32_t)lba, (uint8_t)blocks, bytes);
	    break;

	case scsi_write10_cdb:
	    status = Write10(sgp, (uint32_t)lba, (uint16_t)blocks, bytes);
	    break;

	case scsi_write16_cdb:
	case scsi_writev16_cdb:
	    status = Write16(sgp, (uint64_t)lba, (uint32_t)blocks, bytes);
	    break;

	default:
	    Fprintf(opaque, "Invalid write I/O type detected, type = %d\n", write_type);
	    status = FAILURE;
	    break;
    }
    return (status);
}

/*
 * Write6() - Send a Write(6) CDB.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	lba = The starting logical block address.
 * 	blocks = The number of blocks to transfer.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
Write6(scsi_generic_t *sgp, uint32_t lba, uint8_t blocks, uint32_t bytes)
{
    DirectRW6_CDB_t *cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (DirectRW6_CDB_t *)sgp->cdb;
    cdb->opcode     = SOPC_WRITE_6;
    HtoS(cdb->lba, lba);
    cdb->length	    = blocks;
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "Write(6)";
    sgp->data_dir   = scsi_data_write;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = WriteTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

/*
 * Write10() - Send a Write(10) CDB.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	lba = The starting logical block address.
 * 	blocks = The number of blocks to transfer.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
Write10(scsi_generic_t *sgp, uint32_t lba, uint16_t blocks, uint32_t bytes)
{
    DirectRW10_CDB_t *cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (DirectRW10_CDB_t *)sgp->cdb;
    cdb->opcode     = SOPC_WRITE_10;
    HtoS(cdb->lba, lba);
    HtoS(cdb->length, blocks);
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "Write(10)";
    sgp->data_dir   = scsi_data_write;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = WriteTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

/*
 * Write16() - Send a Write(16) CDB.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	lba = The starting logical block address.
 * 	blocks = The number of blocks to transfer.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
Write16(scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes)
{
    DirectRW16_CDB_t *cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (DirectRW16_CDB_t *)sgp->cdb;
    cdb->opcode     = SOPC_WRITE_16;
    HtoS(cdb->lba, lba);
    HtoS(cdb->length, blocks);
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "Write(16)";
    sgp->data_dir   = scsi_data_write;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = ReadTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

#if 0

/* ======================================================================== */

/*
 * PopulateToken() - Send XCOPY Populate Token CDB.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	listid = The list identifier.
 * 	buffer = The data buffer address.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
PopulateToken(scsi_generic_t *sgp, unsigned int listid, void *data, unsigned int bytes)
{
    scsi_populate_token_cdb *cdb = (scsi_populate_token_cdb *)sgp->cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (scsi_populate_token_cdb *)sgp->cdb;
    cdb->opcode     = SOPC_EXTENDED_COPY;
    cdb->service_action = SCSI_XCOPY_POPULATE_TOKEN;
    HtoS(cdb->list_id, listid);
    HtoS(cdb->parameter_list_length, bytes);
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "XCOPY - Populate Token";
    sgp->data_dir   = scsi_data_write;
    sgp->data_buffer = data;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = WriteTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

/*
 * ReceiveRodTokenInfo() - Receieve ROD Token Information.
 *
 * Inputs:
 * 	sgp = The SCSI generic data.
 * 	listid = The list identifier.
 * 	buffer = The data buffer address.
 * 	bytes = The number of data bytes to transfer.
 *
 * Return Value:
 *	Returns the status from the IOCTL request which is:
 *	    0 = Success, -1 = Failure
 */
int
ReceiveRodTokenInfo(scsi_generic_t *sgp, unsigned int listid, void *data, unsigned int bytes)
{
    scsi_receive_copy_results *cdb;
    int error;

    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    cdb             = (scsi_receive_copy_results *)sgp->cdb;
    cdb->opcode     = SOPC_RECEIVE_ROD_TOKEN_INFO;
    cdb->service_action = SCSI_TGT_RECEIVE_ROD_TOKEN_INFORMATION;
    HtoS(cdb->list_id, listid);
    HtoS(cdb->allocation_length, bytes);
    sgp->cdb_size   = sizeof(*cdb);
    sgp->cdb_name   = "Receive ROD Token Information";
    sgp->data_dir   = scsi_data_read;
    sgp->data_buffer = data;
    sgp->data_length = bytes;
    if (!sgp->timeout) {
	sgp->timeout = ReadTimeout;
    }
    
    error = libExecuteCdb(sgp);

    return(error);
}

#endif /* 0 */

/* ======================================================================== */

/*
 * Declarations/Definitions for Test Unit Ready Command:
 */
#define TestUnitReadyName     "Test Unit Ready"
#define TestUnitReadyOpcode   0
#define TestUnitReadyCdbSize  6
#define TestUnitReadyTimeout  ScsiDefaultTimeout

/*
 * TestUnitReady() - Send a SCSI Test Unit Teady (tur).
 *
 * Inputs:
 *  fd     = The file descriptor to issue bus reset to.
 *  dsf    = The device special file (raw or "sg" for Linux).
 *  debug  = Flag to control debug output.
 *  errlog = Flag to control error logging. (True logs error)
 *                                          (False suppesses)
 *  sap    = Pointer to SCSI address (optional).
 *  sgpp   = Pointer to scsi generic pointer (optional).
 *  timeout = The timeout value (in ms).
 *  tsp = The tool specific information.
 *
 * Return Value:
 *  Returns the status from the IOCTL request which is:
 *    0 = Success, -1 = Failure
 *
 * Note:  If the caller supplies a SCSI generic pointer, then
 * it's the callers responsibility to free this structure, along
 * with the data buffer and sense buffer.  This capability is
 * provided so the caller can examine SCSI status, sense data,
 * and data transfers, to make (more) intelligent decisions.
 */
int
TestUnitReady(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
	      scsi_addr_t *sap, scsi_generic_t **sgpp,
	      unsigned int timeout, tool_specific_t *tsp)
{
    scsi_generic_t scsi_generic;
    scsi_generic_t *sgp = &scsi_generic;
    void *opaque = (tsp) ? tsp->opaque : NULL;
    int error;

    /*
     * Setup and/or allocate a SCSI generic data structure.
     */
    if (sgpp && *sgpp) {
	sgp = *sgpp;
    } else {
	sgp = init_scsi_generic(tsp);
    }
    memset(sgp->cdb, 0, sizeof(sgp->cdb));
    sgp->fd         = fd;
    sgp->dsf        = dsf;
    sgp->cdb[0]     = TestUnitReadyOpcode;
    sgp->cdb_size   = TestUnitReadyCdbSize;
    sgp->cdb_name   = TestUnitReadyName;
    sgp->data_dir   = scsi_data_none;
    sgp->debug      = debug;
    sgp->errlog     = errlog;
    sgp->timeout    = (timeout) ? timeout : TestUnitReadyTimeout;

    /*
     * If a SCSI address was specified, do a structure copy.
     */
    if (sap) {
	sgp->scsi_addr = *sap;	/* Copy the SCSI address info. */
    }

    error = libExecuteCdb(sgp);

    /*
     * If the user supplied a pointer, send the SCSI generic data
     * back to them for further analysis.
     */
    if (sgpp) {
	if (*sgpp == NULL) {
	    *sgpp = sgp;        /* Return the generic data pointer. */
	}
    } else {
	free_palign(opaque, sgp->sense_data);
	free(sgp);
    }
    return(error);
}

/************************************************************************
 *									*
 * stoh() - Convert SCSI bytes to Host short/int/long format.		*
 *									*
 * Description:								*
 *	This function converts SCSI (big-endian) byte ordering to the	*
 * format necessary by this host.					*
 *									*
 * Inputs:	bp = Pointer to SCSI data bytes.			*
 *		size = The conversion size in bytes.			*
 *									*
 * Return Value:							*
 *		Returns a long value with the proper byte ordering.	*
 *									*
 ************************************************************************/
uint64_t
stoh(unsigned char *bp, size_t size)
{
    uint64_t value = 0L;

    switch (size) {
	
	case sizeof(unsigned char):
	    value = (uint64_t) bp[0];
	    break;

	case sizeof(unsigned short):
	    value = ( ((uint64_t)bp[0] << 8) | (uint64_t)bp[1] );
	    break;

	case 0x03:
	    value = ( ((uint32_t)bp[0] << 16) | ((uint32_t)bp[1] << 8) |
		      (uint32_t) bp[2] );
	    break;

	case sizeof(unsigned int):
	    value = ( ((uint32_t)bp[0] << 24) | ((uint32_t)bp[1] << 16) |
		      ((uint32_t)bp[2] << 8)  | (uint32_t)bp[3]);
	    break;

	case 0x05:
	    value = ( ((uint64_t)bp[0] << 32L) | ((uint64_t)bp[1] << 24) |
		      ((uint64_t)bp[2] << 16)  | ((uint64_t)bp[3] << 8) |
		      (uint64_t)bp[4] );       
	    break;

	case 0x06:
	    value = ( ((uint64_t)bp[0] << 40L) | ((uint64_t)bp[1] << 32L) |
		      ((uint64_t)bp[2] << 24)  | ((uint64_t)bp[3] << 16) |
		      ((uint64_t)bp[4] << 8)   | (uint64_t)bp[5] );
	    break;

	case 0x07:
	    value = ( ((uint64_t)bp[0] << 48L) | ((uint64_t)bp[1] << 40L) |
		      ((uint64_t)bp[2] << 32L) | ((uint64_t)bp[3] << 24) |
		      ((uint64_t)bp[4] << 16)  | ((uint64_t)bp[5] << 8) |
		      (uint64_t)bp[6] );       
	    break;

	case sizeof(uint64_t):
	    /*
	     * NOTE: Compiler dependency? If I don't cast each byte
	     * below to uint64_t, the code generated simply ignored the
	     * bytes [0-3] and sign extended bytes [4-7].  Strange.
	     */
	    value = ( ((uint64_t)bp[0] << 56L) | ((uint64_t)bp[1] << 48L) |
		      ((uint64_t)bp[2] << 40L) | ((uint64_t)bp[3] << 32L) |
		      ((uint64_t)bp[4] << 24)  | ((uint64_t)bp[5] << 16) |
		      ((uint64_t)bp[6] << 8)   | (uint64_t)bp[7] );
	    break;

	default:
	    Fprintf(NULL, bad_conversion_str, size);
	    break;

    }
    return(value);
}

/************************************************************************
 *									*
 * htos() - Convert Host short/int/long format to SCSI bytes.		*
 *									*
 * Description:								*
 *	This function converts host values to SCSI (big-endian) byte	*
 * ordering.								*
 *									*
 * Inputs:	bp = Pointer to SCSI data bytes.			*
 *		value = The numeric value to convert.			*
 *		size = The conversion size in bytes.			*
 *									*
 * Return Value:							*
 *		Void.							*
 *									*
 ************************************************************************/
void
htos (unsigned char *bp, uint64_t value, size_t size)
{
    switch (size) {
	
	case sizeof(unsigned char):
	    bp[0] = (unsigned char) value;
	    break;

	case sizeof(unsigned short):
	    bp[0] = (unsigned char) (value >> 8);
	    bp[1] = (unsigned char) value;
	    break;

	case 0x03:
	    bp[0] = (unsigned char) (value >> 16);
	    bp[1] = (unsigned char) (value >> 8);
	    bp[2] = (unsigned char) value;
	    break;

	case sizeof(uint32_t):
	    bp[0] = (unsigned char) (value >> 24);
	    bp[1] = (unsigned char) (value >> 16);
	    bp[2] = (unsigned char) (value >> 8);
	    bp[3] = (unsigned char) value;
	    break;

	case 0x05:
	    bp[0] = (unsigned char) (value >> 32L);
	    bp[1] = (unsigned char) (value >> 24);
	    bp[2] = (unsigned char) (value >> 16);
	    bp[3] = (unsigned char) (value >> 8);
	    bp[4] = (unsigned char) value;
	    break;

	case 0x06:
	    bp[0] = (unsigned char) (value >> 40L);
	    bp[1] = (unsigned char) (value >> 32L);
	    bp[2] = (unsigned char) (value >> 24);
	    bp[3] = (unsigned char) (value >> 16);
	    bp[4] = (unsigned char) (value >> 8);
	    bp[5] = (unsigned char) value;
	    break;

	case 0x07:
	    bp[0] = (unsigned char) (value >> 48L);
	    bp[1] = (unsigned char) (value >> 40L);
	    bp[2] = (unsigned char) (value >> 32L);
	    bp[3] = (unsigned char) (value >> 24);
	    bp[4] = (unsigned char) (value >> 16);
	    bp[5] = (unsigned char) (value >> 8);
	    bp[6] = (unsigned char) value;
	    break;

	case sizeof(uint64_t):
	    bp[0] = (unsigned char) (value >> 56L);
	    bp[1] = (unsigned char) (value >> 48L);
	    bp[2] = (unsigned char) (value >> 40L);
	    bp[3] = (unsigned char) (value >> 32L);
	    bp[4] = (unsigned char) (value >> 24);
	    bp[5] = (unsigned char) (value >> 16);
	    bp[6] = (unsigned char) (value >> 8);
	    bp[7] = (unsigned char) value;
	    break;

	default:
	    Fprintf(NULL, bad_conversion_str, size);
	    break;

    }
}

/*
 * scmn_cdb_length() - Calculate the Command Descriptor Block length.
 *
 * Description:
 *	This function is used to determine the SCSI CDB length.  This
 * is done by checking the command group code.  The command specified
 * is expected to be the actual SCSI command byte, not a psuedo command
 * byte.  There should be tables for vendor specific commands, since
 * there is no way of determining the length of these commands.
 *
 * Inputs:
 *	opcode = The SCSI operation code.
 *
 * Return Value:
 *	Returns the CDB length.
 */
int
GetCdbLength(unsigned char opcode)
{
    int cdb_length = 0;

    /*
     * Calculate the size of the SCSI command.
     */
    switch (opcode & SCSI_GROUP_MASK) {
	
	case SCSI_GROUP_0:
	    cdb_length = 6;	    /* 6 byte CDB. */
	    break;

	case SCSI_GROUP_1:
	case SCSI_GROUP_2:
	    cdb_length = 10;	    /* 10 byte CDB. */
	    break;

	case SCSI_GROUP_5:
	    cdb_length = 12;	    /* 12 byte CDB. */
	    break;

	case SCSI_GROUP_3:
	    cdb_length = 0;	    /* Reserved group. */
	    break;

	case SCSI_GROUP_4:
	    cdb_length = 16;	    /* 16 byte CDB. */
	    break;

	case SCSI_GROUP_6:
	case SCSI_GROUP_7:
	    cdb_length = 10;	/* Vendor unique (guess). */
	    break;
    }
    return(cdb_length);
}
