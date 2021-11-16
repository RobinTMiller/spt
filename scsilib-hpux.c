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
 * Module:	scsilib-ux.c
 * Author:	Robin T. Miller
 * Date:	March 24th, 2005
 *
 * Description:
 *  This module contains the OS SCSI specific functions for HP-UX.
 *
 * Modification History:
 *
 * April 5th, 2021 by Robin T. Miller
 *      Add support for showing devices.
 * 
 * August 26th, 2010 by Robin T. Miller
 * 	When opening device, on EROFS errno, try opening read-only.
 * 
 * August 6th, 2007 by Robin T. Miller.
 *	Added OS open and close functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>	/* for dirname() and basename() */
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/scsi.h>

#include "spt.h"
#include "spt_devices.h"

/*
 * Local Definitions:
 */
#define DEV_PATH	"/dev/rdisk"	/* Device directory prefix. */

/*
 * Forward Declarations:
 */
static void DumpScsiCmd(scsi_generic_t *sgp, struct sctl_io *siop);
static char *hpux_ScsiStatus(unsigned scsi_status);

static int find_scsi_devices(scsi_generic_t *sgp, char *devpath, char *scsi_name, scsi_filters_t *sfp);

static scsi_device_entry_t *add_device_entry(scsi_generic_t *sgp, char *path, inquiry_t *inquiry,
					     char *serial, char *device_id, char *target_port,
					     int bus, int channel, int target, int lun);
static scsi_device_entry_t *create_device_entry(scsi_generic_t *sgp, char *path, inquiry_t *inquiry,
						char *serial, char *device_id, char *target_port,
						int bus, int channel, int target, int lun);
static scsi_device_entry_t *find_device_entry(scsi_generic_t *sgp, char *path, char *serial,
					      char *device_id, int bus, int channel, int target, int lun);

static scsi_device_name_t *create_exclude_entry(scsi_generic_t *sgp, char *path, 
						int bus, int channel, int target, int lun);
static scsi_device_name_t *find_exclude_entry(scsi_generic_t *sgp, char *path, int bus, int channel, int target, int lun);
static void FreeScsiExcludeTable(scsi_generic_t *sgp);

/*
 * os_open_device() = Open Device.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the open() which is:
 *        0 = Success or -1 = Failure
 */
int
os_open_device(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int status = SUCCESS;
    int oflags = (O_RDWR|O_NONBLOCK);

    if (sgp->debug == True) {
	Printf(opaque, "Opening device %s, open flags = %#o (%#x)...\n",
	       sgp->dsf, oflags, oflags);
    }
    if ( (sgp->fd = open(sgp->dsf, oflags)) < 0) {
	if (errno == EROFS) {
	    int oflags = (O_RDONLY|O_NONBLOCK);
	    if (sgp->debug == True) {
		Printf(opaque, "Opening device %s read-only, open flags = %#o (%#x)...\n",
		       sgp->dsf, oflags, oflags);
	    }
	    sgp->fd = open(sgp->dsf, oflags);
	}
	if (sgp->fd == INVALID_HANDLE_VALUE) {
	    if (sgp->errlog == True) {
		os_perror(opaque, "open() of %s failed!", sgp->dsf);
	    }
	    status = FAILURE;
	}
    }
    if ( (sgp->debug == True) && (sgp->fd != INVALID_HANDLE_VALUE) ) {
	Printf(opaque, "Device %s successfully opened, fd = %d\n", sgp->dsf, sgp->fd);
    }
    return(status);
}

/*
 * os_close_device() = Close Device.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the close() which is:
 *        0 = Success or -1 = Failure
 */
int
os_close_device(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error;

    if (sgp->debug == True) {
	Printf(opaque, "Closing device %s, fd %d...\n", sgp->dsf, sgp->fd);
    }
    if ( (error = close(sgp->fd)) < 0) {
	os_perror(opaque, "close() of %s failed", sgp->dsf);
    }
    sgp->fd = INVALID_HANDLE_VALUE;
    return(error);
}

/*
 * os_abort_task_set() - Send Abort Task Set.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_abort_task_set(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

    /* TODO:  Revisit this for 11.31! Implement Abort Task Set. */
    if (sgp->errlog == True) {
	Printf(opaque, "Abort Task Set is not supported!\n");
    }
    return(error);
}

/*
 * os_clear_task_set() - Send Clear Task Set.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_clear_task_set(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

    /* TODO:  Revisit this for 11.31! Implement Clear Task Set. */
    if (sgp->errlog == True) {
	Printf(opaque, "Clear Task Set is not supported!\n");
    }
    return(error);
}

/*
 * os_cold_target_reset() - Send a Cold Target Reset.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_cold_target_reset(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

    /* TODO:  Revisit this for 11.31! */
    if (sgp->errlog == True) {
	Printf(opaque, "Cold Target Reset is not implemented!\n");
    }
    return(error);
}

/*
 * os_warm_target_reset() - Send a Warm Target Reset.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_warm_target_reset(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

    /* TODO:  Revisit this for 11.31! */
    if (sgp->errlog == True) {
	Printf(opaque, "Warm Target Reset is not implemented!\n");
    }
    return(error);
}

/*
 * os_reset_bus() - Reset the SCSI Bus (All targets and LUNs).
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_reset_bus(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error;

    if ( (error = ioctl(sgp->fd, SIOC_RESET_BUS, 0)) < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    os_perror(opaque, "SCSI reset bus (SIOC_RESET_BUS) failed on %s!", sgp->dsf);
	}
    }
    return(error);
}

/*
 * os_reset_ctlr() - Reset the Controller.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_reset_ctlr(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
#if 1
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(opaque, "SCSI reset controller is not supported!\n");
    }
#else
    int one = 1;
    int error;

    if ( (error = ioctl(sgp->fd, DIOC_RSTCLR, &one)) < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    Fprintf(opaque, "SCSI reset controller (DIOC_RSTCLR) failed on %s!\n", sgp->dsf);
	}
    }
#endif
    return(error);
}

/*
 * os_reset_device() - Reset the SCSI Device (including all LUNs).
 *
 * Note:  A device reset is also known as a Bus Device Reset (BDR).
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_reset_device(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error;

    if ( (error = ioctl(sgp->fd, SIOC_RESET_DEV, 0)) < 0) {
	if (sgp->errlog == True) {
	    os_perror(opaque, "SCSI reset device (SIOC_RESET_DEV) failed on %s!", sgp->dsf);
	}
    }
    return(error);
}

/*
 * os_reset_lun() - Reset the SCSI LUN (Logical Unit only).
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_reset_lun(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

    /* TODO: Update for 11.31 with task management lun reset. */
    if (sgp->errlog == True) {
	Printf(opaque, "SCSI reset lun is not supported!\n");
    }
    return(error);
}

/*
 * os_scan() - Scan For Devices.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_scan(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(opaque, "Scan for devices is not implemented!\n");
    }
    return(error);
}

/*
 * os_resumeio() - Resume I/O to a Suspended Disk.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_resumeio(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(opaque, "Resume I/O is not implemented!\n");
    }
    return(error);
}

/*
 * os_suspendio() - Suspend I/O to This Disk.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_suspendio(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(opaque, "Suspend I/O is not implemented!\n");
    }
    return(error);
}

/*
 * os_get_timeout() - Get Device Timeout.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *      timeout= Pointer to store the timeout value.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 *    0 = Success, 1 = Warning, -1 = Failure
 */
int
os_get_timeout(scsi_generic_t *sgp, unsigned int *timeout)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(opaque, "Get timeout is not implemented!\n");
    }
    return(error);
}

/*
 * os_set_timeout() - Set the Device Timeout.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *      timeout= The timeout value to set.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 *    0 = Success, 1 = Warning, -1 = Failure
 */
int
os_set_timeout(scsi_generic_t *sgp, unsigned timeout)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(opaque, "Set timeout is not implemented!\n");
    }
    return(error);
}

/*
 * os_get_qdepth() - Get the Device Queue Depth.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *      qdepth = Pointer to store the queue depth value.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 *    0 = Success, 1 = Warning, -1 = Failure
 */
int
os_get_qdepth(scsi_generic_t *sgp, unsigned int *qdepth)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    struct sioc_lun_limits lun_limits;
    int error;

    memset(&lun_limits, '\0', sizeof(lun_limits));
    if ( (error = ioctl(sgp->fd, SIOC_GET_LUN_LIMITS, &lun_limits)) < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    Fprintf(opaque, "SIOC_GET_LUN_LIMITS on %s failed!\n", sgp->dsf);
	}
    } else {
	*qdepth = lun_limits.max_q_depth;
    }
    return(error);
}

/*
 * os_set_qdepth() - Set the Device Queue Depth.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *      qdepth = The queue depth value to set.
 *
 * Return Value:
 *      Returns the status from the IOCTL request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_set_qdepth(scsi_generic_t *sgp, unsigned int qdepth)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    struct sioc_lun_limits lun_limits;
    int error;

    memset(&lun_limits, '\0', sizeof(lun_limits));
    lun_limits.max_q_depth = qdepth;
    /*
     * For performance testing, allow disabling tags.
     */
    if (qdepth == 0) {
#if defined(SCTL_DISABLE_TAGS)
	lun_limits.flags = SCTL_DISABLE_TAGS;
#else /* !defined(SCTL_DISABLE_TAGS) */
	lun_limits.flags = 0;
#endif /* defined(SCTL_DISABLE_TAGS) */
    } else {
	lun_limits.flags = SCTL_ENABLE_TAGS;
    }
    if ( (error = ioctl(sgp->fd, SIOC_SET_LUN_LIMITS, &lun_limits)) < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    Fprintf(opaque, "SIOC_SET_LUN_LIMITS failed on %s!\n", sgp->dsf);
	}
    }
    return(error);
}

/*
 * os_spt() - OS Specific SCSI Pass-Through (spt).
 *
 * Description:
 *  This function takes a high level SCSI command, converts it
 * into the format necessary for this OS, then executes it and
 * returns an OS independent format to the caller.
 *
 * Inputs:
 *  sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *  Returns the status from the SCSI request which is:
 *    0 = Success, -1 = Failure
 */
int
os_spt(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    struct sctl_io sctl_io;
    struct sctl_io *siop = &sctl_io;
    int error;

    memset(siop, 0, sizeof(*siop));
    /*
     * Sanity check the CDB size (just in case).
     */
    if (sgp->cdb_size > sizeof(siop->cdb)) {
	Fprintf(opaque, "CDB size of %d is too large for max OS CDB of %d!\n",
		sgp->cdb_size, sizeof(siop->cdb));
	return(-1);
    }
    (void)memcpy(siop->cdb, sgp->cdb, (size_t)sgp->cdb_size);

    siop->flags       = sgp->sflags;
    siop->cdb_length  = sgp->cdb_size;
    siop->data        = sgp->data_buffer;
    siop->data_length = sgp->data_length;
    /*
     * Setup the data direction:
     */
    if (sgp->data_dir == scsi_data_none) {
	;				/* No data to be transferred.    */
    } else if (sgp->data_dir == scsi_data_read) {
	siop->flags |= SCTL_READ;	/* Reading data from the device. */
    } else { /* (sgp->data_dir == scsi_data_write) */
	;			       /* Writing data to the device.    */
    }
    siop->max_msecs = sgp->timeout;  /* Timeout in milliseconds.       */

    if (sgp->flags & SG_INIT_SYNC) {
	siop->flags |= SCTL_INIT_SDTR; /* Negotiate sync data transfers. */
    }
    if (sgp->flags & SG_INIT_WIDE) {
	siop->flags |= SCTL_INIT_WDTR; /* Negotiate wide data transfers. */
    }

    /*
     * Finally, execute the SCSI command:
     */
    error = ioctl(sgp->fd, SIOC_IO, siop);

    /*
     * Handle errors, and send pertinent data back to the caller.
     */
    if (error < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    os_perror(opaque, "SCSI request (SIOC_IO) failed on %s!", sgp->dsf);
	}
	sgp->error = True;
	goto error;
    }
    if (siop->cdb_status == S_GOOD) {
	sgp->error = False;	/* Show SCSI command was successful. */
    } else {
	sgp->error = True;	/* Tell caller we've had some sort of error. */
	if ( (sgp->errlog == True) && (siop->cdb_status != S_CHECK_CONDITION) ) {
	    Fprintf(opaque, "%s failed, SCSI status = %d (%s)\n", sgp->cdb_name,
		    siop->cdb_status, hpux_ScsiStatus(siop->cdb_status));
	}
    }
    if ( (siop->cdb_status == S_CHECK_CONDITION) &&
	 (siop->sense_status == S_GOOD) ) {
	int sense_length = min(sgp->sense_length, sizeof(siop->sense));
	sgp->sense_valid = True;
	sgp->sense_resid = (sgp->sense_length - siop->sense_xfer);
	(void)memcpy(sgp->sense_data, &siop->sense, sense_length);
    }
    sgp->data_resid   = (sgp->data_length - siop->data_xfer);

    /*
     * Interesting, our resid can be greater than our data length if the CDB
     * length is larger than the specified data length (at least on Linux).
     * Note: This length mismatch caused an ABORT, but data is transferred!
     */
    if (sgp->data_resid > sgp->data_length) {
	sgp->data_transferred = sgp->data_length;
    } else {
	sgp->data_transferred = (sgp->data_length - sgp->data_resid);
    }
    sgp->scsi_status  = siop->cdb_status;
    sgp->sense_status = siop->sense_status;

    /*
     * Please Beware: The siop->flags may get altered by the IOCTL!
     *                For example, SCTL_INIT_SDTR and SCTL_INIT_WDTR 
     */
error:
    if (sgp->debug == True) {
	DumpScsiCmd(sgp, siop);
    }
    return(error);
}

/*
 * os_is_retriable() - OS Specific Checks for Retriable Errors.
 *
 * Description:
 *  This OS specific function determines if the last SCSI request is a
 * retriable error. Note: The checks performed here are those that are
 * OS specific, such as looking a host, driver, or syscall errors that can
 * be retried automatically, and/or to perform any OS specific recovery.
 * 
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the SCSI request which is:
 *        0 = False, 1 = True
 */
hbool_t
os_is_retriable(scsi_generic_t *sgp)
{
    hbool_t is_retriable = False;
    
    return (is_retriable);
}

static void
DumpScsiCmd(scsi_generic_t *sgp, struct sctl_io *siop)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int i;
    char buf[128];
    char *bp = buf;

    Printf(opaque, "SCSI I/O Structure:\n");

    Printf(opaque, "    Device Special File .............................: %s\n", sgp->dsf);
    Printf(opaque, "    File Descriptor .............................. fd: %d\n", sgp->fd);
    /*
     * Decode the SCSI flags.
     */
    if (siop->flags & SCTL_READ) {
	bp += sprintf(bp, "SCTL_READ(%x)", SCTL_READ);
    } else {
	bp += sprintf(bp, "SCTL_WRITE(0)");
    }
    if (siop->flags & SCTL_INIT_WDTR) {
	bp += sprintf(bp, "|SCTL_INIT_WDTR(%x)", SCTL_INIT_WDTR);
    }
    if (siop->flags & SCTL_INIT_SDTR) {
	bp += sprintf(bp, "|SCTL_INIT_SDTR(%x)", SCTL_INIT_SDTR);
    }
    if (siop->flags & SCTL_NO_DISC) {
	bp += sprintf(bp, "|SCTL_NO_DISC(%x)", SCTL_NO_DISC);
    }
    Printf(opaque, "    Control Flags ............................. flags: %#x = %s)\n",
	   siop->flags, buf);
    Printf(opaque, "    SCSI CDB Status ...................... cdb_status: %#x (%s)\n",
	   siop->cdb_status, hpux_ScsiStatus(siop->cdb_status));
    Printf(opaque, "    Command Timeout ....................... max_msecs: %u ms (%u seconds)\n",
	   siop->max_msecs, (siop->max_msecs / MSECS));
    for (bp = buf, i = 0; (i < siop->cdb_length); i++) {
	bp += sprintf(bp, "%x ", siop->cdb[i]);
    }
    Printf(opaque, "    Command Descriptor Block .................... cdb: %s(%s)\n", buf, sgp->cdb_name);
    Printf(opaque, "    CDB Length ........................... cdb_length: %d\n", siop->cdb_length);
    Printf(opaque, "    I/O Buffer Address ......................... data: %#lx\n", siop->data);
    Printf(opaque, "    I/O Buffer Length ................... data_length: %d (%#x)\n",
	   siop->data_length, siop->data_length);
    Printf(opaque, "    I/O Data Transferred .................. data_xfer: %d (%#x)\n",
	   siop->data_xfer, siop->data_xfer);
    Printf(opaque, "    Request Sense Buffer ...................... sense: %#lx\n", &siop->sense);
    Printf(opaque, "    Request Sense Length .............. sizeof(sense): %d (%#x)\n",
	   sizeof(siop->sense), sizeof(siop->sense));
    Printf(opaque, "    Request Sense Status ............... sense_status: %#x (%s)\n",
	   siop->sense_status, hpux_ScsiStatus(siop->sense_status));
    DumpCdbData(sgp);
    Printf(opaque, "\n");
}

/*
 * HP-UX SCSI Status Code Table.
 */
static struct hpuxSCSI_StatusTable {
    unsigned  scsi_status;  /* The SCSI status code. */
    char      *status_msg;  /* The SCSI text message. */
} hpuxscsi_StatusTable[] = {
    /* See /usr/include/sys/scsi.h for descriptions. */
    { S_GOOD,               "S_GOOD",		    /* 0x00 */},
    { S_CHECK_CONDITION,    "S_CHECK_CONDITION",    /* 0x02 */},
    { S_CONDITION_MET,      "S_CONDITION_MET",	    /* 0x04 */},
    { S_BUSY,               "S_BUSY",		    /* 0x08 */},
    { S_INTERMEDIATE,       "S_INTERMEDIATE",	    /* 0x10 */},
    { S_I_CONDITION_MET,    "S_I_CONDITION_MET",    /* 0x14 */},
    { S_RESV_CONFLICT,      "S_RESV_CONFLICT",	    /* 0x18 */},
    { S_COMMAND_TERMINATED, "S_COMMAND_TERMINATED", /* 0x22 */},
    { S_QUEUE_FULL,         "S_QUEUE_FULL",	    /* 0x28 */},
    /*
     * Additional SCSI status returned by HP-UX drivers.
     */
    { SCTL_INVALID_REQUEST, "SCTL_INVALID_REQUEST", /* 0x100 */},
    { SCTL_SELECT_TIMEOUT,  "SCTL_SELECT_TIMEOUT",  /* 0x200 */},
    { SCTL_INCOMPLETE,      "SCTL_INCOMPLETE",	    /* 0x400 */},
    { SCTL_POWERFAIL,       "SCTL_POWERFAIL",	    /* 0x800 */}
#if defined(UX1131)
    /* New for 11.31 */
    ,
    { SCTL_NO_RESOURCE,     "SCTL_NO_RESOURCE"	    /* 0x1000 */},
    { SCTL_TP_OFFLINE,      "SCTL_TP_OFFLINE"	    /* 0x2000 */},
    { SCTL_IO_TIMEOUT,      "SCTL_IO_TIMEOUT"	    /* 0x3000 */},
    { SCTL_IO_ABORTED,      "SCTL_IO_ABORTED"	    /* 0x4000 */},
    { SCTL_RESET_OCCURRED,  "SCTL_RESET_OCCURRED"   /* 0x5000 */}
#endif /* defined(UX1131) */
};
static int hpuxscsi_StatusEntrys =
sizeof(hpuxscsi_StatusTable) / sizeof(hpuxscsi_StatusTable[0]);

static char *
hpux_ScsiStatus(unsigned scsi_status)
{
    struct hpuxSCSI_StatusTable *cst = hpuxscsi_StatusTable;
    int entrys;

    for (entrys = 0; entrys < hpuxscsi_StatusEntrys; cst++, entrys++) {
	if (cst->scsi_status == scsi_status) {
	    return(cst->status_msg);
	}
    }
    return("???");
}

/*
 * os_host_status_msg() - Get the Host Status Message.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns host status message or NULL if not found or unsupported.
 */
char *
os_host_status_msg(scsi_generic_t *sgp)
{
    return(NULL);
}

/*
 * os_driver_status_msg() - Get the Driver Status Message.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns driver status message or NULL if not found or unsupported.
 */
char *
os_driver_status_msg(scsi_generic_t *sgp)
{
    return(NULL);
}

/* ============================================================================================= */
/*
 * Functions for Managing the SCSI Device Table:
 */
/* Note: Path Format: /dev/rdsk/c0tFEFED90D6FDB3D736C9CE9005010B919d0s0 */

static scsi_dir_path_t scsi_dir_paths[] = {
    {	DEV_PATH,	"disk",	   "Device Path",	True	},
    {	NULL,		NULL		                        }
};

int
os_find_scsi_devices(scsi_generic_t *sgp, scsi_filters_t *sfp, char *paths)
{
    void *opaque = sgp->tsp->opaque;
    scsi_dir_path_t *sdp;
    int status;

    /* Allow user to overide our default directory/device name list. */
    if (paths) {
	char *path, *sep, *str;
	char *saveptr;
	/* Allow comma separated list of device paths. */
	str = strdup(paths);
	sep = ",";
	path = strtok_r(str, sep, &saveptr);
	while (path) {
	    /* Manual page states these API's modify path, so we must duplicate! */
	    char *dirp = strdup(path);
	    char *basep = strdup(path);
	    char *dir_path = dirname(dirp);
	    char *dev_name = basename(basep);
	    if (strcmp(dev_name, "*") == 0) {
		dev_name = NULL;	/* Wildcard '*' says use all device names! */
	    }
	    if (dir_path) {
		status = find_scsi_devices(sgp, dir_path, dev_name, sfp);
	    }
	    Free(opaque, dirp);
	    Free(opaque, basep);
	    path = strtok_r(NULL, sep, &saveptr);
	}
	Free(opaque, str);
    } else {
	for (sdp = scsi_dir_paths; sdp->sdp_dir_path; sdp++) {
	    if ( (sfp->all_device_paths == False) && (sdp->default_scan == False) ) {
		continue;
	    }
	    status = find_scsi_devices(sgp, sdp->sdp_dir_path, sdp->sdp_dev_name, sfp);
	}
    }
    if (sfp && sfp->exclude_paths) {
	FreeScsiExcludeTable(sgp);
    }
    return(status);
}

static int
find_scsi_devices(scsi_generic_t *sgp, char *devpath, char *scsi_name, scsi_filters_t *sfp)
{
    void *opaque = sgp->tsp->opaque;
    int bus = -1, target = -1, lun = -1, channel = -1;
    char *scsi_device = NULL;
    char *serial = NULL, *device_id = NULL, *target_port = NULL;
    char *fw_version = NULL;
    inquiry_t *inquiry = NULL;
    inquiry_page_t inquiry_data;  
    inquiry_page_t *inquiry_page = &inquiry_data;
    scsi_device_entry_t *sdep = NULL;
    DIR *dir;
    struct dirent *dirent;
    char path[PATH_BUFFER_SIZE];
    char *dsf = sgp->dsf;
    int fd = INVALID_HANDLE_VALUE;
    int oflags = (O_RDONLY|O_NONBLOCK);
    int status = SUCCESS;

    if (sgp->debug == True) {
	Printf(opaque, "Open'ing device path %s...\n", devpath);
    }
    dir = opendir(devpath);
    if (dir) {
	while ((dirent = readdir(dir)) != NULL) {
	    if (sgp->debug) {
		Printf(opaque, "Processing %s...\n", dirent->d_name);
	    }
	    if (scsi_name) {
		if (strncmp(dirent->d_name, scsi_name, strlen(scsi_name))) {
		    continue;
		}
		/* For HP-UX, we don't want disk partitions! */
		/* Format: /dev/rdisk/disk2_p1 */
		int soff = (int)strlen(dirent->d_name) - 3;
		if (soff <= 0) continue; /* Should not happen! */
		if (strncmp(dirent->d_name + soff, "_p", 2) == 0) {
		    //if (sgp->debug) {
			//Printf(opaque, "Skipping device %s...\n", dirent->d_name);
		    //}
		    continue;
		}
	    }
	    snprintf(path, sizeof(path), "%s/%s", devpath, dirent->d_name);
	    /* Filtering here is safe for non-Linux systems. */
	    /* Filter on the device or exclude paths, if specified. */
	    /* Note: Filtering here means we won't find all device paths! */
	    if (sfp && sfp->device_paths) {
		if ( match_device_paths(path, sfp->device_paths) == False ) {
		    continue; /* No match, skip this device. */
		}
	    }
	    if (sfp && sfp->exclude_paths) {
		if ( match_device_paths(path, sfp->exclude_paths) == True ) {
		    continue; /* Match, so exclude this device. */
		}
	    }
	    fd = open(path, oflags);
	    if (fd == INVALID_HANDLE_VALUE) {
		if ( (errno == ENODEV) || (errno == ENXIO) || (errno == ENOENT) ) {
		    ;
		} else { /* Control w/debug? */
		    Perror(opaque, "Failed to open device %s", path);
		}
		goto close_and_continue;
	    }
            /* 
             * Filter on the device name path(s), if specified.
             * Note: Filtering here means we won't find all device paths!
             * Our goal here is to limit device access to those in the list. 
             * FYI: This logic works fine for single path devices. (YMMV) 
             * If this is undesirable, remove, filtering is done during show. 
             */ 
	    if (sfp && sfp->device_paths) {
		/* 
		 * Since we have multiple device paths, do lookup by SCSI nexus.
		 * This allows us to find additional devices such as "sg" or DM-MP. 
		 * But, this won't find multiple DM-MP device paths, nexus differ! 
		 * FYI: DM-MP /dev/mapper returns nexus for the active (sd) path. 
		 */ 
		sdep = find_device_entry(sgp, path, NULL, NULL, bus, channel, target, lun);
		if (sdep == NULL) {
		    if (match_device_paths(path, sfp->device_paths) == False) {
			if (sgp->debug == True) {
			    Printf(opaque, "Skipping device %s...\n", path);
			}
			goto close_and_continue; /* No match, skip this device. */
		    }
		}
		/* Note: We don't update the entry found, since we may filter below. */
	    }
	    /*
	     * We exclude devices at this point to avoid issuing SCSI commands. 
	     * The idea here is that the device may be broken, so avoid touching! 
	     * Note: Excluding /dev/mapper will NOT exclude /dev/{sd|sg) devices, 
	     * due to the order paths are searched. (see above) 
	     */
	    if (sfp && sfp->exclude_paths) {
		scsi_device_name_t *sdnp;
		sdnp = find_exclude_entry(sgp, path, bus, channel, target, lun);
		if (sdnp == NULL) {
		    if (match_device_paths(path, sfp->exclude_paths) == True) {
			sdnp = create_exclude_entry(sgp, path, bus, channel, target, lun);
		    }
		}
		if (sdnp) {
		    if (sgp->debug == True) {
			Printf(opaque, "Excluding device %s...\n", path);
		    }
		    goto close_and_continue; /* Match, so skip this device. */
		}
	    }
	    if (inquiry == NULL) {
		inquiry = Malloc(opaque, sizeof(*inquiry));
		if (inquiry == NULL) return(FAILURE);
	    }
	    /*
	     * int
	     * Inquiry(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
	     *         scsi_addr_t *sap, scsi_generic_t **sgpp,
	     *         void *data, unsigned int len, unsigned char page,
	     *         unsigned int sflags, unsigned int timeout, tool_specific_t *tsp)
	     *  
	     * Note: We are *not* using our own SCSI generic (sdp) structure, but rather 
	     * leave this empty so one is dynamically allocated and initialized. This is 
	     * done since we are querying multiple devices and avoids sgp cleanup.
	     */
	    status = Inquiry(fd, path, sgp->debug, False, NULL, NULL,
			     inquiry, sizeof(*inquiry), 0, 0, sgp->timeout, sgp->tsp);
	    if (status) {
		goto close_and_continue;
	    }
	    /* SCSI Filters */
	    if ( sfp ) {
		int length = 0;
		/* List of device types. */
		if (sfp->device_types) {
		    int dindex;
		    hbool_t dtype_found = False;
		    /* List is terminated with DTYPE_UNKNOWN. */
		    for (dindex = 0; (sfp->device_types[dindex] != DTYPE_UNKNOWN); dindex++) {
			if (inquiry->inq_dtype == sfp->device_types[dindex]) {
			    dtype_found = True;
			    break;
			}
		    }
		    if (dtype_found == False) {
			goto close_and_continue;
		    }
		}
		if (sfp->product) {
		    char pid[sizeof(inquiry->inq_pid)+1];
		    strncpy(pid, (char *)inquiry->inq_pid, sizeof(inquiry->inq_pid));
		    pid[sizeof(inquiry->inq_pid)] = '\0';
		    /* Allow substring match to find things like "SDLF". */
		    if (strstr(pid, sfp->product) == NULL) {
			goto close_and_continue;
		    }
		}
		if (sfp->vendor) {
		    length = strlen(sfp->vendor);
		    if (strncmp(sfp->vendor, (char *)inquiry->inq_vid, length)) {
			goto close_and_continue;
		    }
		}
		if (sfp->revision) {
		    length = strlen(sfp->revision);
		    if (strncmp(sfp->revision, (char *)inquiry->inq_revlevel, length)) {
			goto close_and_continue;
		    }
		}
	    }
	    if (serial == NULL) {
		/*
		 * Get the Inquiry Serial Number page (0x80).
		 */
		serial = GetSerialNumber(fd, path, sgp->debug, False,
					 NULL, NULL, inquiry, sgp->timeout, sgp->tsp);
	    }
	    /* 
	     * We delay filtering until showing device to acquire all paths.
	     */
	    if ( serial && sfp && sfp->serial) {
		/* Use substring search due to leading spaces in serial number! */
		if (strstr(serial, sfp->serial) == NULL) {
		    goto close_and_continue;
		}
	    } else if (sfp && sfp->serial) { /* Skip devices without a serial number. */
		goto close_and_continue;
	    }
	    /*
	     * Get Inquiry Device Identification page (0x83).
	     */
	    status = Inquiry(fd, path, sgp->debug, False, NULL, NULL,
			     inquiry_page, sizeof(*inquiry_page), INQ_DEVICE_PAGE,
			     0, sgp->timeout, sgp->tsp);
	    if (status == SUCCESS) {
		/*
		 * Get the LUN device identifier (aka WWID).
		 */
		device_id = DecodeDeviceIdentifier(opaque, inquiry, inquiry_page, False);
		if ( device_id && sfp && sfp->device_id) {
		    if (strcmp(sfp->device_id, device_id) != 0) {
			goto close_and_continue;
		    }
		} else if (sfp && sfp->device_id) { /* Skip devices without a device ID. */
		    goto close_and_continue;
		}
		/*
		 * For SAS protocol, the target port is the drive SAS address.
		 */
		target_port = DecodeTargetPortIdentifier(opaque, inquiry, inquiry_page);
		if ( target_port && sfp && sfp->target_port) {
		    if (strcmp(sfp->target_port, target_port) != 0) {
			goto close_and_continue;
		    }
		} else if (sfp && sfp->target_port) { /* Skip devices without a target port. */
		    goto close_and_continue;
		}
	    } else {
		status = SUCCESS; /* Device may not support the device ID page, but continue... */
	    }
 
	    /*
	     * Get the full firmware version string. 
	     * Note: This provides 8 character string versus truncated Inquiry revision! 
	     */
	    if ( (inquiry->inq_dtype == DTYPE_DIRECT) &&
		 (strncmp((char *)inquiry->inq_vid, "ATA", 3) == 0) ) {
		fw_version = AtaGetDriveFwVersion(fd, path, sgp->debug, False,
						  NULL, NULL, inquiry, sgp->timeout, sgp->tsp);
	    }

	    /*
	     * Filter on user specified FW version (if any).
	     */
	    if ( fw_version && sfp && sfp->fw_version) {
		if (strcmp(sfp->fw_version, fw_version) != 0) {
		    goto close_and_continue;
		}
	    } else if (sfp && sfp->fw_version) { /* Skip devices without a FW version. */
		goto close_and_continue;
	    }

	    /* 
	     * Now, create the SCSI device table entries.
	     */
	    sdep = add_device_entry(sgp, path, inquiry, serial, device_id,
				    target_port, bus, channel, target, lun);

	    if (fw_version && sdep->sde_fw_version == NULL) {
		sdep->sde_fw_version = strdup(fw_version);
	    }
#if defined(Nimble)
	    if ( (inquiry->inq_dtype == DTYPE_DIRECT) &&
		 (strncmp((char *)inquiry->inq_vid, "Nimble", 6) == 0) ) {
		nimble_vu_disk_inquiry_t *nimble_inq = (nimble_vu_disk_inquiry_t *)&inquiry->inq_vendor_unique;
		char text[SMALL_BUFFER_SIZE];
        	char *target_type = NULL;
		sdep->sde_nimble_device = True;
		(void)memcpy(text, nimble_inq->array_sw_version, sizeof(nimble_inq->array_sw_version));
		text[sizeof(nimble_inq->array_sw_version)] = '\0';
        	sdep->sde_sw_version = strdup(text);
		target_type = (nimble_inq->target_type == NIMBLE_VOLUME_SCOPED_TARGET)
          					? "Volume Scoped" : "Group Scoped";
        	sdep->sde_target_type = strdup(target_type);
        	sdep->sde_sync_replication = (nimble_inq->sync_replication == True);
	    } else {
		sdep->sde_nimble_device = False;
	    }
#endif /* defined(Nimble) */

close_and_continue:
	    (void)close(fd);
	    fd = INVALID_HANDLE_VALUE;
	    if (serial) {
		Free(opaque, serial);
		serial = NULL;
	    }
	    if (device_id) {
		Free(opaque, device_id);
		device_id = NULL;
	    }
	    if (target_port) {
		Free(opaque, target_port);
		target_port = NULL;
	    }
	    if (fw_version) {
		Free(opaque, fw_version);
		fw_version = NULL;
	    }
	}
	closedir(dir);
    } else {
	if (sgp->debug) {
	    Perror(opaque, "Failed to open directory %s", devpath);
	}
	status = FAILURE;
    }
    if (inquiry) Free(opaque, inquiry);
    if (serial) Free(opaque, serial);
    if (device_id) Free(opaque, device_id);
    if (target_port) Free(opaque, target_port);
    if (fw_version) Free(opaque, fw_version);
    return(status);
}

static scsi_device_entry_t *
add_device_entry(scsi_generic_t *sgp, char *path, inquiry_t *inquiry,
		 char *serial, char *device_id, char *target_port,
		 int bus, int channel, int target, int lun)
{
    void *opaque = sgp->tsp->opaque;
    scsi_device_entry_t *sdep = NULL;

    /* Find device via device ID (WWN), serial number, or SCSI nexus. */
    sdep = find_device_entry(sgp, path, serial, device_id, bus, channel, target, lun);
    if (sdep == NULL) {
	sdep = create_device_entry(sgp, path, inquiry, serial, device_id,
				   target_port, bus, channel, target, lun);
    } else { /* Update existing device entry. */
	/* Note: We only expect multiple paths with Linux, no other OS! */
	Eprintf(opaque, "Found unexpected duplicate device %s with %s %s, ignoring...\n",
	        path, (serial) ? "serial number" : "device ID", (serial) ? serial : device_id);
	Fprintf(opaque, "Previous device is %s, which is NOT expected with proper multi-pathing!\n",
		sdep->sde_names.sdn_flink->sdn_device_path);
    }
    return( sdep );
}

static scsi_device_entry_t *
create_device_entry(scsi_generic_t *sgp, char *path, inquiry_t *inquiry,
		    char *serial, char *device_id, char *target_port,
		    int bus, int channel, int target, int lun)
{
    void *opaque = sgp->tsp->opaque;
    scsi_device_entry_t *sdeh = &scsiDeviceTable;
    struct scsi_device_entry *sptr = NULL;
    scsi_device_entry_t *sdep;
    scsi_device_name_t *sdnp;
    struct scsi_device_name *sdnh;

    sdep = Malloc(opaque, sizeof(*sdep));
    if (sdep == NULL) return(NULL);
    /* Initialize the device name header. */
    sdnh = &sdep->sde_names;
    sdnh->sdn_flink = sdnh->sdn_blink = sdnh;
    /* Setup a new device name entry. */
    sdnp = Malloc(opaque, sizeof(*sdnp));
    if (sdnp == NULL) return(NULL);
    /* Insert at the tail. */
    sdnh->sdn_flink = sdnh->sdn_blink = sdnp;
    sdnp->sdn_flink = sdnp->sdn_blink = sdnh;
    sdnp->sdn_flink = sdnh;
    sdnh->sdn_blink = sdnp;
    /* Setup the device information. */
    sdnp->sdn_device_path = strdup(path);
    sdnp->sdn_bus = bus;
    sdnp->sdn_channel = channel;
    sdnp->sdn_target = target;
    sdnp->sdn_lun = lun;
    /* Some devices do *not* support device ID or serial numbers. (CD/DVD, KVM, SES, etc) */
    if (device_id) {
	sdep->sde_device_id = strdup(device_id);
    }
    if (serial) {
	sdep->sde_serial = strdup(serial);
    }
    if (target_port) {
	sdnp->sdn_target_port = strdup(target_port);
	/* Historic, save the target port (for now), even though this is per path! */
	sdep->sde_target_port = strdup(target_port);
    }
    sdep->sde_device_type = inquiry->inq_dtype;
    sdep->sde_vendor = Malloc(opaque, INQ_VID_LEN + 1);
    sdep->sde_product = Malloc(opaque, INQ_PID_LEN + 1);
    sdep->sde_revision = Malloc(opaque, INQ_REV_LEN + 1);
    (void)strncpy(sdep->sde_vendor, (char *)inquiry->inq_vid, INQ_VID_LEN);
    (void)strncpy(sdep->sde_product, (char *)inquiry->inq_pid, INQ_PID_LEN);
    (void)strncpy(sdep->sde_revision, (char *)inquiry->inq_revlevel, INQ_REV_LEN);
    /* Sort by the first device name. */
    for (sptr = sdeh->sde_flink; (sptr != sdeh) ; sptr = sptr->sde_flink) {
	if (sptr->sde_names.sdn_flink) {
	    /* Sort by name, ensuring "sda,sdb,..." comes before "sdaa", etc. */
	    if ( ( strlen(path) < strlen(sptr->sde_names.sdn_flink->sdn_device_path) ) ||
		 ((strlen(path) == strlen(sptr->sde_names.sdn_flink->sdn_device_path) &&
		   strcmp(path, sptr->sde_names.sdn_flink->sdn_device_path) < 0)) ) {
		sdep->sde_flink = sptr;
		sdep->sde_blink = sptr->sde_blink;
		sptr->sde_blink->sde_flink = sdep;
		sptr->sde_blink = sdep;
		return( sdep );
	    }
	}
    }
    /* Insert at the tail. */
    sptr = sdeh->sde_blink;
    sptr->sde_flink = sdep;
    sdep->sde_blink = sptr;
    sdep->sde_flink = sdeh;
    sdeh->sde_blink = sdep;
    return( sdep );
}

static scsi_device_entry_t *
find_device_entry(scsi_generic_t *sgp, char *path, char *serial,
		  char *device_id, int bus, int channel, int target, int lun)
{
    void *opaque = sgp->tsp->opaque;
    scsi_device_entry_t *sdeh = &scsiDeviceTable, *sdep;

    /* We do lookup by WWN, serial number, or SCSI nexus. */
    for (sdep = sdeh->sde_flink; (sdep != sdeh) ; sdep = sdep->sde_flink) {
	/* Lookup device by device ID or serial number. */
	if (device_id && sdep->sde_device_id) {
	    if (strcmp(sdep->sde_device_id, device_id) == 0) {
		return( sdep );
	    }
	}
	/* Do not do lookup by serial number, if we have a device ID. */
	/* Note: Some storage, like 3PAR, use same serial number across LUNs! */
	if ( (device_id == NULL) && serial && sdep->sde_serial) {
	    if (strcmp(sdep->sde_serial, serial) == 0) {
		return( sdep );
	    }
	}
#if 0
	/* Find device by SCSI nexus (bus/channel/target/lun). */
	if ( ( (device_id == NULL) && (serial == NULL) ) ||
	     ( (sdep->sde_device_id == NULL) && (sdep->sde_serial == NULL) ) ) {
	    scsi_device_name_t *sdnh = &sdep->sde_names;
	    scsi_device_name_t *sdnp;
	    for (sdnp = sdnh->sdn_flink; (sdnp != sdnh); sdnp = sdnp->sdn_flink) {
		if ( (sdnp->sdn_bus == bus) && (sdnp->sdn_channel == channel) &&
		     (sdnp->sdn_target == target) && (sdnp->sdn_lun == lun) ) {
		    return( sdep );
		}
	    }
	}
#endif /* 0 */
    }
    return(NULL);
}

/* ========================================================================================================= */
/*
 * Definitions and functions for managing the exclude device table.
 */
scsi_device_name_t scsiExcludeTable = { .sdn_flink = &scsiExcludeTable, .sdn_blink = &scsiExcludeTable };

static scsi_device_name_t *
create_exclude_entry(scsi_generic_t *sgp, char *path, int bus, int channel, int target, int lun)
{
    void *opaque = sgp->tsp->opaque;
    struct scsi_device_name *sdnh = &scsiExcludeTable;
    scsi_device_name_t *sdnp;
    scsi_device_name_t *sptr;

    /* We just need to add a new device name. */
    sdnp = Malloc(opaque, sizeof(*sdnp));
    if (sdnp == NULL) return(NULL);
    sdnp->sdn_device_path = strdup(path);
    sdnp->sdn_bus = bus;
    sdnp->sdn_channel = channel;
    sdnp->sdn_target = target;
    sdnp->sdn_lun = lun;
    /* Insert at the tail. */
    sptr = sdnh->sdn_blink;
    sptr->sdn_flink = sdnp;
    sdnp->sdn_blink = sptr;
    sdnp->sdn_flink = sdnh;
    sdnh->sdn_blink = sdnp;
    return( sdnp );
}

/* Find device by SCSI nexus (bus/channel/target/lun). */
static scsi_device_name_t *
find_exclude_entry(scsi_generic_t *sgp, char *path, int bus, int channel, int target, int lun)
{
    void *opaque = sgp->tsp->opaque;
    scsi_device_name_t *sdnh = &scsiExcludeTable;
    scsi_device_name_t *sdnp;

    for (sdnp = sdnh->sdn_flink; (sdnp != sdnh); sdnp = sdnp->sdn_flink) {
	if ( (sdnp->sdn_bus == bus) && (sdnp->sdn_channel == channel) &&
	     (sdnp->sdn_target == target) && (sdnp->sdn_lun == lun) ) {
	    return( sdnp );
	}
    }
    return(NULL);
}

static void
FreeScsiExcludeTable(scsi_generic_t *sgp)
{
    void *opaque = sgp->tsp->opaque;
    scsi_device_name_t *sdnh = &scsiExcludeTable;
    scsi_device_name_t *sdnp;

    while ( (sdnp = sdnh->sdn_flink) != sdnh) {
	sdnp->sdn_blink->sdn_flink = sdnp->sdn_flink;
	sdnp->sdn_flink->sdn_blink = sdnp->sdn_blink;
	Free(opaque, sdnp->sdn_device_path);
	Free(opaque, sdnp);
    }
    return;
}
