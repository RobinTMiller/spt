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
 * Module:	scsilib-aix.c
 * Author:	Robin T. Miller
 * Date:	March 29th, 2005
 *
 * Description:
 *  This module contains the OS SCSI specific functions for AIX.
 *
 * Modification History:
 * 
 * January 5th, 2021 by Robin T. Miller
 *      Remove extra DecodeTargetPortIdentifier() in find_scsi_devices(),
 * left over by accident, after refactoring code.
 * 
 * September 8th, 2020 by Robin T. Miller
 *      Add support for showing devices.
 * 
 * May 22nd, 2020 by Robin T. Miller
 *      Updates to resolve 64-bit compilation warnings.
 * 
 * August 26th, 2010 by Robin T. Miller
 * 	When opening device, on EROFS errno, try opening read-only.
 * 
 * August 6th, 2007 by Robin T. Miller.
 *	Added OS open and close functions.
 *
 * February 10th, 2007 by Robin T. Miller.
 *      Added support for queue tag message types, and abort task set.
 *      Don't stop adapter, unless we started it! (forced errors)
 *
 * February 6th, 2007 by Robin T. Miller
 *      For AIX, added support for issuing commands to iSCSI adapter.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>	/* for dirname() and basename() */
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/scsi.h>
#include <sys/scdisk.h>
#include <sys/scsi_buf.h>

#include <odmi.h>
#include <sys/cfgodm.h>

/* For inet_addr() API */
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "spt.h"
#include "spt_devices.h"

/*
 * Local Definitions:
 */
#define DEV_PATH		"/dev"	/* Device directory prefix.	*/
#define AIX_MAX_TIMEOUT		60	/* Real max timeout?		*/
                                        /* Avoids SC_PASSTHRU_INV_TO!	*/

/*
 * Define Adapter Types:
 */
typedef enum adapter_type {
     ATYPE_FSCSI, ATYPE_ISCSI, ATYPE_PSCSI, ATYPE_VSCSI, ATYPE_UNKNOWN
} adapter_type_t;

/*
 * LUN Information from ODM required by controller start operations:
 */
typedef struct lun_info {
    adapter_type_t      adapter_type;   /* The adapter type.            */
    unsigned long long  scsi_id;        /* The SCSI target ID.          */
    unsigned long long  lun_id;         /* The Logical Unit Number ID.  */
    char               *parent;         /* The parent == adapter name.  */
    char               *target_name;    /* The iSCSI target name.       */
    char               *host_addr;      /* The iSCSI host address.      */
    iscsi_ip_addr       iscsi_ip;       /* The binary iSCSI address.    */
    uint64_t            port_num;       /* The iSCSI port number.       */
} lun_info_t;

/*
 * Forward Declarations:
 */
static void DumpScsiCmd(scsi_generic_t *sgp, struct sc_passthru *spt);
static int GetLunInfo(scsi_generic_t *sgp, lun_info_t *lunip);
static int GetFscsiInfo(scsi_generic_t *sgp, lun_info_t *lunip, char *hdisk);
static int GetIscsiInfo(scsi_generic_t *sgp, lun_info_t *lunip, char *hdisk);
static int StartAdapter(scsi_generic_t *sgp, lun_info_t *lunip, struct scsi_sciolst *sciop);
static int StopAdapter(scsi_generic_t *sgp, lun_info_t *lunip, struct scsi_sciolst *sciop);
static void DumpSciolst(scsi_generic_t *sgp, lun_info_t *lunip, struct scsi_sciolst *sciop, char *operation);

static int find_scsi_devices(scsi_generic_t *sgp, char *devpath, char *scsi_name, scsi_filters_t *sfp);

static scsi_device_entry_t *add_device_entry(scsi_generic_t *sgp, char *path, inquiry_t *inquiry,
					     char *serial, char *device_id, char *target_port,
					     int bus, int channel, int target, int lun);
static scsi_device_entry_t *create_device_entry(scsi_generic_t *sgp, char *path, inquiry_t *inquiry,
						char *serial, char *device_id, char *target_port,
						int bus, int channel, int target, int lun);
static scsi_device_entry_t *find_device_entry(scsi_generic_t *sgp, char *path, char *serial,
					      char *device_id, int bus, int channel, int target, int lun);
static scsi_device_name_t *update_device_entry(scsi_generic_t *sgp, scsi_device_entry_t *sdep,
					       char *path, inquiry_t *inquiry,
					       char *serial, char *device_id, char *target_port,
					       int bus, int channel, int target, int lun);

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
    struct scsi_sciolst Startsciolst;
    struct scsi_sciolst *startsciop = &Startsciolst;
    struct scsi_sciolst Abortsciolst;
    struct scsi_sciolst *abortsciop = &Abortsciolst;
    lun_info_t          lun_info;
    lun_info_t          *lunip = &lun_info;
    int                 error;

    if (error = StartAdapter(sgp, lunip, startsciop)) {
	return(error);
    }

    /* Abort Task Set */
    if (sgp->debug == True) {
	Printf(opaque, "Issuing abort task set to %s, adapter %s...\n", sgp->dsf, lunip->parent);
    }
    *abortsciop = *startsciop;
    abortsciop->flags = 0; /* Clear LOGIN, etc. */
    error = ioctl(sgp->afd, SCIOLHALT, abortsciop);
    if (error && sgp->errlog) {
	sgp->os_error = errno;
        if (sgp->errlog) {
            os_perror(opaque, "SCIOLRESET failed on %s, adapter %s!", sgp->dsf, lunip->parent);
        }
    }
    if (sgp->debug == True) {
	DumpSciolst(sgp, lunip, abortsciop, "SCIOLHALT");
    }
    (void)StopAdapter(sgp, lunip, startsciop);
    if (lunip->parent) {
	(void)free(lunip->parent);
    }
    if (lunip->target_name) {
	(void)free(lunip->target_name);
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
 *        0 = Success, 1 = Warning (not supported), -1 = Failure
 */
int
os_cold_target_reset(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

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
 *        0 = Success, 1 = Warning (not supported), -1 = Failure
 */
int
os_reset_bus(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(opaque, "SCSI reset bus is not implemented!\n");
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
 *        0 = Success, 1 = Warning (not supported), -1 = Failure
 */
int
os_reset_ctlr(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(opaque, "SCSI reset controller is not supported!\n");
    }
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
 *        0 = Success, 1 = Warning (not supported), -1 = Failure
 */
int
os_reset_device(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    struct scsi_sciolst Startsciolst;
    struct scsi_sciolst *startsciop = &Startsciolst;
    struct scsi_sciolst Resetsciolst;
    struct scsi_sciolst *resetsciop = &Resetsciolst;
    lun_info_t          lun_info;
    lun_info_t          *lunip = &lun_info;
    int                 error;

    if (error = StartAdapter(sgp, lunip, startsciop)) {
        return (error);
    }

    /* Target Reset */
    if (sgp->debug == True) {
        Printf(opaque, "Issuing target reset to %s, adapter %s...\n", sgp->dsf, lunip->parent);
    }
    *resetsciop = *startsciop;
    resetsciop->flags = 0; /* Clear LOGIN, etc. */
    error = ioctl(sgp->afd, SCIOLRESET, resetsciop);
    if (error && sgp->errlog) {
	sgp->os_error = errno;
        if (sgp->errlog) {
            os_perror(opaque, "SCIOLRESET failed on %s, adapter %s!", sgp->dsf, lunip->parent);
        }
    }
    if (sgp->debug == True) {
	DumpSciolst(sgp, lunip, resetsciop, "SCIOLRESET");
    }
    (void)StopAdapter(sgp, lunip, startsciop);
    if (lunip->parent) {
        (void)free(lunip->parent);
    }
    if (lunip->target_name) {
        (void)free(lunip->target_name);
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
    struct scsi_sciolst Startsciolst;
    struct scsi_sciolst *startsciop = &Startsciolst;
    struct scsi_sciolst Resetsciolst;
    struct scsi_sciolst *resetsciop = &Resetsciolst;
    lun_info_t          lun_info;
    lun_info_t          *lunip = &lun_info;
    int                 error;

    if (error = StartAdapter(sgp, lunip, startsciop)) {
        return (error);
    }

    /* LUN Reset */
    if (sgp->debug == True) {
        Printf(opaque, "Issuing LUN reset to %s, adapter %s...\n", sgp->dsf, lunip->parent);
    }
    *resetsciop = *startsciop;
    resetsciop->flags = SCIOLRESET_LUN_RESET;
    error = ioctl(sgp->afd, SCIOLRESET, resetsciop);
    if (error && sgp->errlog) {
	sgp->os_error = errno;
        if (sgp->errlog) {
            os_perror(opaque, "SCIOLRESET failed on %s, adapter %s!", sgp->dsf, lunip->parent);
        }
    }
    if (sgp->debug == True) {
	DumpSciolst(sgp, lunip, resetsciop, "SCIOLRESET");
    }
    (void)StopAdapter(sgp, lunip, startsciop);
    if (lunip->parent) {
        (void)free(lunip->parent);
    }
    if (lunip->target_name) {
        (void)free(lunip->target_name);
    }
    return(error);
}

/*
 * os_scan() - Do an I/O Scan.
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
 */
int
os_get_qdepth(scsi_generic_t *sgp, unsigned int *qdepth)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(opaque, "Get queue depth is not implemented!\n");
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
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(opaque, "Set queue depth is not implemented!\n");
    }
    return(error);
}

/*
 * os_spt() - OS Specific SCSI Pass-Through (spt).
 *
 * Description:
 *      This function takes a high level SCSI command, converts it
 * into the format necessary for this OS, then executes it and
 * returns an OS independent format to the caller.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the SCSI request which is:
 *          0 = Success, 1 = Warning, -1 = Failure
 *      sgp->error set True/False if SCSI command failed!
 */
int
os_spt(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    struct sc_passthru	passthru;
    struct sc_passthru	*spt = &passthru;
    unsigned int	timeout;
    int			error;

    if (sgp->flags & SG_ADAPTER) {
        return ( os_spta(sgp) );
    }

    memset(spt, 0, sizeof(*spt));

    /*
     * Sanity check the CDB size (just in case).
     */
    if (sgp->cdb_size > sizeof(spt->scsi_cdb)) {
        Fprintf(opaque, "CDB size of %d is too large for max OS CDB of %d!\n",
                sgp->cdb_size, sizeof(spt->scsi_cdb));
        return(-1);
    }
    (void)memcpy(spt->scsi_cdb, sgp->cdb, (size_t)sgp->cdb_size);

    spt->version		= SCSI_VERSION_2; 
    spt->command_length		= sgp->cdb_size;
    spt->flags			= sgp->sflags;
    spt->q_tag_msg              = sgp->qtag_type;
    /*
     * Setup the data direction:
     */
    if (sgp->data_dir == scsi_data_none) {
        spt->flags |= B_READ;         /* No data to be transferred.    */
    } else if (sgp->data_dir == scsi_data_read) {
        spt->flags |= B_READ;         /* Reading data from the device.  */
    } else { /* (sgp->data_dir == scsi_data_write) */
        spt->flags |= B_WRITE;        /* Writing data to the device.    */
    }
    timeout = (sgp->timeout / MSECS); /* Timeout in secs.  */
    if (timeout == 0) { timeout++; }
    spt->buffer			= sgp->data_buffer;
    spt->data_length		= (long long)sgp->data_length;
    spt->autosense_buffer_ptr	= sgp->sense_data;
    spt->autosense_length	= (ushort)sgp->sense_length;
    /*
     * The maximum timeout with SC_MIXED_IO is 60 seconds.
     * Therefore, for allow longer timeouts, set SC_QUIESCE_IO.
     *
     * Important AIX Notes from URL:
     * http://publib.boulder.ibm.com/infocenter/pseries/v5r3/index.jsp?
     *      topic=/com.ibm.aix.kernelext/doc/kernextc/fcp_drivers_ioctl.htm
     *
     * DK_PASSTHRU operations are further subdivided into requests which
     * quiesce other I/O prior to issuing the request and requests that
     * do not quiesce I/O. These subdivisions are based on the devflags
     * field of the sc_passthru structure. When the devflags field of the
     * sc_passthru structure has a value of SC_MIX_IO, the DK_PASSTHRU
     * operation will be mixed with other I/O requests. SC_MIX_IO requests
     * that write data to devices are prohibited and will fail. When this
     * happens -1 is returned, and the errno global variable is set to EINVAL.
     * When the devflags field of the sc_passthru structure has a value of
     * SC_QUIESCE_IO, all other I/O requests will be quiesced before the
     * DK_PASSTHRU request is issued to the device. If an SC_QUIESCE_IO
     * request has its timeout_value field set to 0, the DK_PASSTHRU
     * request fails with a return code of -1, the errno global variable
     * is set to EINVAL, and the einval_arg field is set to a value of
     * SC_PASSTHRU_INV_TO (defined in the /usr/include/sys/scsi.h file).
     * If an SC_QUIESCE_IO request has a nonzero timeout value that is
     * too large for the device, the DK_PASSTHRU request fails with a
     * return code of -1, the errno global variable is set to EINVAL, the
     * einval_arg field is set to a value of SC_PASSTHRU_INV_TO (defined
     * in the /usr/include/sys/scsi.h file), and the timeout_value is set
     * to the largest allowed value.
     *
     * NOTE: Haven't found a way to alter the default max timeout of 60!
     */
    if ( (timeout > AIX_MAX_TIMEOUT) || (sgp->data_dir == scsi_data_write) ) {
        spt->devflags		= SC_QUIESCE_IO;
        spt->timeout_value	= timeout;
    } else {
        spt->devflags		= SC_MIX_IO;
        spt->timeout_value	= min(AIX_MAX_TIMEOUT, timeout);
    }

    if (sgp->flags & SG_INIT_ASYNC) {
        spt->flags |= SC_ASYNC;       /* Enable async mode.             */
    } else if (sgp->flags & SG_NO_DISC) {
        spt->flags |= SC_NODISC;      /* Disable disconnects.           */
    }

    /*
     * Issue the SCSI pass-thru.
     */
    if (sgp->scsi_addr.scsi_path < 0) {
	error = ioctl(sgp->fd, DK_PASSTHRU, spt);
    } else {
	struct scdisk_pathiocmd pcmd;
	/*
	 * Setup the MPIO path command.
	 */
	pcmd.path_id        = sgp->scsi_addr.scsi_path;
	pcmd.size           = sizeof(*spt);
	pcmd.path_iocmd     = spt;
	error = ioctl(sgp->fd, DKPATHPASSTHRU, &pcmd);
    }

    /*
     * Handle errors, and send pertinent data back to the caller.
     */
    if (error < 0) {
        sgp->os_error = errno;
	/*
	 * This OS returns failure on the IOCTL, even though the SPT data was
	 * valid, and the actual error is from the adapter or SCSI CDB, so we
	 * handle that difference here.  Basically, we don't wish to log an
	 * IOCTL error, when it might be just a SCSI Check Condition.
	 */
	if (spt->status_validity) {
	    error = 0;		   /* Examine SCSI or Adapter status for errors. */
	} else {
	    if (sgp->errlog == True) {
		os_perror(opaque, "SCSI request (DK_PASSTHRU) failed on %s!", sgp->dsf);
	    }
	    sgp->error = True;
	    goto error;
	}
    }
    if ( (spt->status_validity == 0) ||	/* No error status means success! */
	 ((spt->scsi_bus_status == SC_GOOD_STATUS) && (spt->adapter_status == 0)) ) {
	sgp->error = False;	/* Show SCSI command was successful. */
    } else {
	sgp->error = True;	/* Tell caller we've had some sort of error. */
	if (sgp->errlog == True) {
	    /*
	     * Note: The caller is expected to handle/report check conditions.
	     */
	    if ( (spt->status_validity & SC_SCSI_ERROR) &&
		 (spt->scsi_bus_status != SC_CHECK_CONDITION) ) {
		Fprintf(opaque, "%s failed, SCSI status = %#x (%s)\n", sgp->cdb_name,
			spt->scsi_bus_status, ScsiStatus(spt->scsi_bus_status));
	    } else if (spt->status_validity & SC_ADAPTER_ERROR) {
		Fprintf(opaque, "%s failed, Adapter status = %#x\n", sgp->cdb_name,
			spt->adapter_status);
	    } else if ( (spt->adapter_status != 0) ||
			(spt->scsi_bus_status != SC_CHECK_CONDITION) ) {
		/* Just in case bad status isn't marked as valid error status! */
		Fprintf(opaque, "%s failed, Adapter status = %#x, SCSI status = %#x\n",
			sgp->cdb_name, spt->adapter_status, spt->scsi_bus_status);
	    }
	}
    }

    sgp->data_resid    = (unsigned int)spt->residual;
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
    sgp->host_status   = spt->adapter_status;
    sgp->scsi_status   = spt->scsi_bus_status;
    sgp->driver_status = spt->add_device_status;
error:
    if (sgp->debug == True) {
	DumpScsiCmd (sgp, spt);
    }
    return (error);
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

/*
 * ReportOdmError() - Report an ODM Error Message.
 *
 * Inputs:
 *    criteria = Message indicating ODM string that failed.
 *
 * Return Value:
 *    We don't return, but rather exit (at this time).
 */
int
ReportOdmError(char *criteria)
{
    int status;
    char *odmmsg;

    status = odm_err_msg(odmerrno, &odmmsg);
    if (status < 0) {
        Fprintf(NULL, "odm_err_msg() failed, odmerrno = %d\n", odmerrno);
    } else {
        Fprintf(NULL, "Failure on '%s' - %s\n", criteria, odmmsg);
    }
    return(FAILURE);
}

/*
 * GetLunInfo() - Get LUN Information.
 *
 * Description:
 *      This function looks up the necessary information to issue a
 * START operation to the adapter for issuing commands.  At the present
 * time, the parent, SCSI ID, and LUN ID are obtained from the ODM.
 *
 * Inputs:
 *      sgp = The SCSI Generic Pointer.
 *      lunip = The LUN Information Pointer.
 *
 * Return Value:
 *      Returns 0 / -1 = Success / Failure.
 */
static int
GetLunInfo(scsi_generic_t *sgp, lun_info_t *lunip)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    struct CuDv     cudv;
    struct CuDv     *cudvp;
    struct CuPath   cupath;
    struct CuPath   *cupathp;
    char            criteria[256];
    char            *hdisk;
    int             status;

    odm_initialize();

    memset(lunip, 0, sizeof(*lunip));
    /*
     * Skip over the /dev/r portion to isolate hdisk name.
     */
    if (hdisk = strrchr(sgp->dsf, '/')) {
        hdisk++;        /* Point past the '/' */
        if (*hdisk == 'r') hdisk++;
    } else {
        hdisk = sgp->dsf;    /* Assume no /dev specified. */
    }

    /*
     * Request the device attributes to get the parent name.
     */
    sprintf(criteria, "name='%s'", hdisk);
    /*
     * If a path was specified, get the parent for that path.
     */
    if (sgp->scsi_addr.scsi_path >= 0) {
        sprintf(criteria, "name='%s' AND path_id=%u",
                hdisk, sgp->scsi_addr.scsi_path);
        cupathp = odm_get_obj(CuPath_CLASS, criteria, &cupath, ODM_FIRST);
        if (cupathp == (struct CuPath *) -1) {
            return ( ReportOdmError(criteria) );
        } else if (cupathp == NULL) {
            Fprintf(opaque, "Didn't find path_id attribute for '%s'!\n", hdisk);
            return (FAILURE);
        }
        lunip->parent = strdup(cupathp->parent);  /* Caller must free! */
    } else {
        cudvp = odm_get_obj(CuDv_CLASS, criteria, &cudv, ODM_FIRST);
        if (cudvp == (struct CuDv *) -1) {
            return ( ReportOdmError(criteria) );
        } else if (cudvp == NULL) {
            Fprintf(opaque, "Didn't find criteria for '%s'!\n", hdisk);
            return (FAILURE);
        }
        /*
         * Parent = "scsiN|fscsiN|iscsiN|vscsiN" for pSCSI/FCP/iSCSI/vSCSI
         *
         * See thie link for info required for each interface:
         *
         * http://publib.boulder.ibm.com/infocenter/pseries/v5r3/index.jsp?
         *   topic=/com.ibm.aix.kernelext/doc/kernextc/fcp_drivers_ioctl.htm
         */
        lunip->parent = strdup(cudvp->parent);  /* Caller must free! */
    }

    /*
     * Set the adapter type to key off later.
     */
    if (strncmp(lunip->parent, "scsi", 4) == 0) {
        lunip->adapter_type = ATYPE_PSCSI;
    } else if (strncmp(lunip->parent, "fscsi", 5) == 0) {
        lunip->adapter_type = ATYPE_FSCSI;
        status = GetFscsiInfo(sgp, lunip, hdisk);
    } else if (strncmp(lunip->parent, "iscsi", 5) == 0) {
        lunip->adapter_type = ATYPE_ISCSI;
        status = GetIscsiInfo(sgp, lunip, hdisk);
    } else if (strncmp(lunip->parent, "vscsi", 5) == 0) {
        lunip->adapter_type = ATYPE_VSCSI;
    } else {
        lunip->adapter_type = ATYPE_UNKNOWN;
    }

    (void)odm_terminate();
    return (SUCCESS);
}

/*
 * GetFscsiInfo() - Get Fscsi LUN Information.
 *
 * Description:
 *      This function looks up the necessary information to issue a
 * START operation to the adapter for issuing commands.  For fscsi,
 * the SCSI (target) ID, and LUN ID are obtained from the ODM.
 *
 * Inputs:
 *      sgp = The SCSI Generic Pointer.
 *      lunip = The LUN Information Pointer.
 *      hdisk = Pointer to the hdisk name.
 *
 * Return Value:
 *      Returns 0 / -1 = Success / Failure.
 */
static int
GetFscsiInfo(scsi_generic_t *sgp, lun_info_t *lunip, char *hdisk)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    struct CuAt     cuat;
    struct CuAt     *cuatp;
    char            criteria[256];

    /*
     * If a path was specified, get the SCSI ID for that path.
     */
    if (sgp->scsi_addr.scsi_path < 0) {
        sprintf(criteria, "name='%s' AND attribute='scsi_id'", hdisk);
        cuatp = odm_get_obj(CuAt_CLASS, criteria, &cuat, ODM_FIRST);
        if (cuatp == (struct CuAt *) -1) {
            return ( ReportOdmError(criteria) );
        } else if (cuatp == NULL) {
            Fprintf(opaque, "Didn't find scsi_id attribute for '%s'!\n", hdisk);
            return (FAILURE);
        }
        lunip->scsi_id = strtoull(cuatp->value, (char **)NULL, 16);
    } else {
        struct CuPathAt cupathat;
        struct CuPathAt *cupathatp;
        sprintf(criteria, "name='%s' AND path_id=%u AND attribute='scsi_id'",
                hdisk, sgp->scsi_addr.scsi_path);
        cupathatp = odm_get_obj(CuPathAt_CLASS, criteria, &cupathat, ODM_FIRST);
        if (cupathatp == (struct CuPathAt *) -1) {
            return ( ReportOdmError(criteria) );
        } else if (cupathatp == NULL) {
            Fprintf(opaque, "Didn't find scsi_id or path_id attribute for '%s'!\n", hdisk);
            return (FAILURE);
        }
        lunip->scsi_id = strtoull(cupathatp->value, (char **)NULL, 16);
    }

    sprintf(criteria, "name='%s' AND attribute='lun_id'", hdisk);
    cuatp = odm_get_obj(CuAt_CLASS, criteria, &cuat, ODM_FIRST);
    if (cuatp == (struct CuAt *) -1) {
        return ( ReportOdmError(criteria) );
    } else if (cuatp == NULL) {
        Fprintf(opaque, "Didn't find lun_id attribute for '%s'!\n", hdisk);
        return (FAILURE);
    }
    lunip->lun_id = strtoull(cuatp->value, (char **)NULL, 16);

    return (SUCCESS);
}

/*
 * GetIscsiInfo() - Get Iscsi LUN Information.
 *
 * Description:
 *      This function looks up the necessary information to issue a
 * START operation to the adapter for issuing commands.  For iscsi,
 * the target name, host address, port number, and lun ID are obtained
 * from the ODM.
 *
 * Inputs:
 *      sgp = The SCSI Generic Pointer.
 *      lunip = The LUN Information Pointer.
 *      hdisk = Pointer to the hdisk name.
 *
 * Return Value:
 *      Returns 0 / -1 = Success / Failure.
 */
static int
GetIscsiInfo(scsi_generic_t *sgp, lun_info_t *lunip, char *hdisk)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    struct CuAt     cuat;
    struct CuAt     *cuatp;
    char            criteria[256];

    sprintf(criteria, "name='%s' AND attribute='target_name'", hdisk);
    cuatp = odm_get_obj(CuAt_CLASS, criteria, &cuat, ODM_FIRST);
    if (cuatp == (struct CuAt *) -1) {
        return ( ReportOdmError(criteria) );
    } else if (cuatp == NULL) {
        Fprintf(opaque, "Didn't find target_name attribute for '%s'!\n", hdisk);
        return (FAILURE);
    }
    lunip->target_name = strdup(cuatp->value);

    sprintf(criteria, "name='%s' AND attribute='host_addr'", hdisk);
    cuatp = odm_get_obj(CuAt_CLASS, criteria, &cuat, ODM_FIRST);
    if (cuatp == (struct CuAt *) -1) {
        return ( ReportOdmError(criteria) );
    } else if (cuatp == NULL) {
        Fprintf(opaque, "Didn't find host_addr attribute for '%s'!\n", hdisk);
        return (FAILURE);
    }
    lunip->host_addr = strdup(cuatp->value);
    lunip->iscsi_ip.addr_type = ISCSI_IPV4_ADDR;
    lunip->iscsi_ip.addr[0] = (uint64_t)inet_addr(cuatp->value);

    sprintf(criteria, "name='%s' AND attribute='port_num'", hdisk);
    cuatp = odm_get_obj(CuAt_CLASS, criteria, &cuat, ODM_FIRST);
    if (cuatp == (struct CuAt *) -1) {
        return ( ReportOdmError(criteria) );
    } else if (cuatp == NULL) {
        Fprintf(opaque, "Didn't find port_num attribute for '%s'!\n", hdisk);
        return (FAILURE);
    }
    lunip->port_num = strtoull(cuatp->value, (char **)NULL, 16);

    sprintf(criteria, "name='%s' AND attribute='lun_id'", hdisk);
    cuatp = odm_get_obj(CuAt_CLASS, criteria, &cuat, ODM_FIRST);
    if (cuatp == (struct CuAt *) -1) {
        return ( ReportOdmError(criteria) );
    } else if (cuatp == NULL) {
        Fprintf(opaque, "Didn't find lun_id attribute for '%s'!\n", hdisk);
        return (FAILURE);
    }
    lunip->lun_id = strtoull(cuatp->value, (char **)NULL, 16);

    return (SUCCESS);
}

static int
StartAdapter(scsi_generic_t *sgp, lun_info_t *lunip, struct scsi_sciolst *sciop)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    char    parent[32];
    int	    error = SUCCESS;
    int     fd;

    if (error = GetLunInfo(sgp, lunip)) {
        return (error);
    }

    (void)sprintf(parent, "/dev/%s", lunip->parent);
    fd = open(parent, O_RDWR);
    if (fd < 0) {
        os_perror(opaque, "open() of %s failed!", parent);
        return (FAILURE);
    }
    sgp->afd = fd;

    /*
     * Setup the protocol specific addressing information.
     */
    memset(sciop, 0, sizeof(*sciop));

    sciop->version	= SCSI_VERSION_1;
    sciop->flags	= ISSUE_LOGIN;
    //sciop->flags	= (ISSUE_LOGIN | FORCED);
    if (lunip->adapter_type == ATYPE_FSCSI) {
        sciop->scsi_id	= lunip->scsi_id;
        sciop->lun_id	= lunip->lun_id;
    } else if (lunip->adapter_type == ATYPE_ISCSI) {
        sciop->lun_id	= lunip->lun_id;
        sciop->parms.iscsi.flags = SCIOL_ISCSI_LOCATE_IPADDR;
        sciop->parms.iscsi.loc_type = SCIOL_ISCSI_LOC_IPV_ADDR;
        (void)strcpy(sciop->parms.iscsi.name, lunip->target_name);
        sciop->parms.iscsi.port_num = lunip->port_num;
        sciop->parms.iscsi.location.addr = lunip->iscsi_ip;
    }

    if (sgp->debug == True) {
        Printf(opaque, "Starting adapter %s...\n", lunip->parent);
    }
    /*
     * SCIOLSTART fails if the device is open by another process,
     * or if the LOGIN flag is omitted.
     * NOTE: A process login to a target flushes all commands to
     * all luns on the target! (say AIX docs)
     */
    /* Gotta use SCIOSTART for pSCSI! */
    //error = ioctl(fd, SCIOSTART, IDLUN(scsi_id,lun_id));
    error = ioctl(fd, SCIOLSTART, sciop);
    if (error < 0) {
        sgp->os_error = errno;
        /*
         * With iSCSI on AIX 5.3, SCSI_DEV_STARTED is not set!
         * But, errno says success, and issuing the CDB works!
         * SCSI_DEV_STARTED == This device is already started.                                                  !
         */
        if ( (sciop->adap_set_flags & SCSI_DEV_STARTED) || (errno == 0) ) {
            error = SUCCESS;
        } else if (sgp->errlog == True) {
            os_perror(opaque, "SCIOLSTART failed on %s!", parent);
        }
    }
    if (sgp->debug == True) {
        DumpSciolst(sgp, lunip, sciop, "SCIOLSTART");
    }
    return (error);
}

static int
StopAdapter(scsi_generic_t *sgp, lun_info_t *lunip, struct scsi_sciolst *sciop)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = SUCCESS;

    /*
     * Only stop the adapter, if *we* started it!
     */
    if ( (sciop->adap_set_flags & SCSI_DEV_STARTED) == 0 ) {
        if (sgp->debug == True) {
            Printf(opaque, "Stopping adapter %s...\n", lunip->parent);
        }
        error = ioctl(sgp->afd, SCIOLSTOP, sciop);
        if (error) {
            sgp->os_error = errno;
            if (errno) {
                os_perror(opaque, "SCIOLSTOP failed for %s, adapter %s!", sgp->dsf, lunip->parent);
            }
            if (sgp->debug == True) {
                DumpSciolst(sgp, lunip, sciop, "SCIOLSTOP");
            }
        }
    }
    (void)close(sgp->afd);
    sgp->afd = INVALID_HANDLE_VALUE;
    return (error);
}

/*
 * os_spta() - OS Adapter SCSI Pass-Through API.
 *
 * Description:
 *      This is mostly a duplicate of the os_spt() function, but
 * needs re-written to setup and use a different data structure
 * for SCIOLCMD (as mentioned below).
 *
 * Note: We should be using SCIOLCMD and struct scsi_iocmd as
 * its' third paramter, but this older SCIOCMD seems to work!
 */
int
os_spta(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    struct sc_passthru	passthru;
    struct sc_passthru	*spt = &passthru;
    unsigned int	timeout;
    int			error;
    struct scsi_sciolst sciolst;
    struct scsi_sciolst *sciop = &sciolst;
    lun_info_t          lun_info;
    lun_info_t          *lunip = &lun_info;

    memset(spt, 0, sizeof(*spt));

    if (error = StartAdapter(sgp, lunip, sciop)) {
        return (error);
    }

    /*
     * Sanity check the CDB size (just in case).
     */
    if (sgp->cdb_size > sizeof(spt->scsi_cdb)) {
        Fprintf(opaque, "CDB size of %d is too large for max OS CDB of %d!\n",
                sgp->cdb_size, sizeof(spt->scsi_cdb));
        return(FAILURE);
    }
    (void)memcpy(spt->scsi_cdb, sgp->cdb, (size_t)sgp->cdb_size);

    /*
     * SCSI_VERSION_1 *must* be used when dynamic tracking is enabled.
     */
    if (sciop->adap_set_flags & SCIOL_DYNTRK_ENABLED) {
        spt->version		= SCSI_VERSION_1;
    } else {
        /*
         * When the version field is set to SCSI_VERSION_2, the residual
         * field indicates left over data, so set when we can! Variable
         * length CDB's also require version 2 (go figure).
         */
        spt->version		= SCSI_VERSION_2;
    }
    spt->command_length		= sgp->cdb_size;
    spt->flags			= sgp->sflags;
    /*
     * Setup the data direction:
     */
    if (sgp->data_dir == scsi_data_none) {
        spt->flags |= B_READ;         /* No data to be transferred.    */
    } else if (sgp->data_dir == scsi_data_read) {
        spt->flags |= B_READ;         /* Reading data from the device.  */
    } else { /* (sgp->data_dir == scsi_data_write) */
        spt->flags |= B_WRITE;        /* Writing data to the device.    */
    }
    timeout = (sgp->timeout / MSECS); /* Timeout in secs.  */
    if (timeout == 0) { timeout++; }
    spt->buffer			= sgp->data_buffer;
    spt->data_length		= (long long)sgp->data_length;
    spt->autosense_buffer_ptr	= sgp->sense_data;
    spt->autosense_length	= (ushort)sgp->sense_length;
    /*
     * There are no restrictions spt'ing to adapter.
     */
    spt->devflags	= SC_MIX_IO;
    spt->timeout_value  = timeout;

    if (sgp->flags & SG_INIT_ASYNC) {
        spt->flags |= SC_ASYNC;       /* Enable async mode.             */
    } else if (sgp->flags & SG_NO_DISC) {
        spt->flags |= SC_NODISC;      /* Disable disconnects.           */
    }

    spt->q_tag_msg = sgp->qtag_type;

    spt->scsi_id = lunip->scsi_id;
    spt->lun_id  = lunip->lun_id;

    /*
     * Dynamic tracking requires the WWN and node name!
     */
    if (sciop->adap_set_flags & SCIOL_DYNTRK_ENABLED) {
        spt->node_name       = sciop->node_name;
        spt->world_wide_name = sciop->world_wide_name;
    }

    /*
     * Issue the SCSI pass-thru.
     *
     * Note: We should be using SCIOLCMD and struct scsi_iocmd as
     * its' third paramter, but this older SCIOCMD seems to work!
     */
    error = ioctl(sgp->afd, SCIOCMD, spt);

    /*
     * Handle errors, and send pertinent data back to the caller.
     */
    if (error < 0) {
	sgp->os_error = errno;
      /*
       * This OS returns failure on the IOCTL, even though the SPT data was
       * valid, and the actual error is from the adapter or SCSI CDB, so we
       * handle that difference here.  Basically, we don't wish to log an
       * IOCTL error, when it might be just a SCSI Check Condition.
       */
      if (spt->status_validity) {
          error = 0;             /* Examine SCSI or Adapter status for errors. */
      } else {
          if (sgp->errlog == True) {
              os_perror(opaque, "SCSI request (SCIOCMD) failed on %s!", sgp->dsf);
          }
          sgp->error = True;
          goto error;
      }
    }
    if ( (spt->status_validity == 0) || /* No error status means success! */
         ((spt->scsi_bus_status == SC_GOOD_STATUS) && (spt->adapter_status == 0)) ) {
        sgp->error = False;     /* Show SCSI command was successful. */
    } else {
        sgp->error = True;      /* Tell caller we've had some sort of error. */
	if (sgp->errlog == True) {
	    /*
	     * Note: The caller is expected to handle/report check conditions.
	     */
	    if ( (spt->status_validity & SC_SCSI_ERROR) &&
		 (spt->scsi_bus_status != SC_CHECK_CONDITION) ) {
		Fprintf(opaque, "%s failed, SCSI status = %#x (%s)\n", sgp->cdb_name,
			spt->scsi_bus_status, ScsiStatus(spt->scsi_bus_status));
	    } else if (spt->status_validity & SC_ADAPTER_ERROR) {
		Fprintf(opaque, "%s failed, Adapter status = %#x\n", sgp->cdb_name,
			spt->adapter_status);
	    } else if ( (spt->adapter_status != 0) ||
			(spt->scsi_bus_status != SC_CHECK_CONDITION) ) {
		/* Just in case bad status isn't marked as valid error status! */
		Fprintf(opaque, "%s failed, Adapter status = %#x, SCSI status = %#x\n",
			sgp->cdb_name, spt->adapter_status, spt->scsi_bus_status);
	    }
	}
    }

    sgp->data_resid    = (unsigned int)spt->residual;
    sgp->host_status   = spt->adapter_status;
    sgp->scsi_status   = spt->scsi_bus_status;
    sgp->driver_status = spt->add_device_status;
error:
    if (sgp->debug == True) {
	DumpScsiCmd (sgp, spt);
    }
    (void)StopAdapter(sgp, lunip, sciop);
    if (lunip->parent) {
        (void)free(lunip->parent);
    }
    if (lunip->target_name) {
        (void)free(lunip->target_name);
    }
    return (error);
}

/*
 * AIX EINVAL Reason Translation Table.
 */
static struct einval_ReasonTable {
	unsigned einval_reason;
	char	*einval_msg_brief;
	char	*einval_msg_full;
} einval_ReasonTable[] = {
    {	SC_PASSTHRU_INV_VERS,		"SC_PASSTHRU_INV_VERS",
	/* 1 */ "Version field is invalid"			},
    {	SC_PASSTHRU_INV_Q_TAG_MSG,	"SC_PASSTHRU_INV_Q_TAG_MSG"
        /* 9 */ "q_tag_msg field is invalid"			},
    {	SC_PASSTHRU_INV_FLAGS,		"SC_PASSTHRU_INV_FLAGS",
        /* 10 */ "flags field is invalid"			},
    {	SC_PASSTHRU_INV_DEVFLAGS,	"SC_PASSTHRU_INV_DEVFLAGS",
	/* 11 */ "devflags field is invalid"			},
    {	SC_PASSTHRU_INV_Q_FLAGS,	"SC_PASSTHRU_INV_Q_FLAGS",
	/* 12 */ "q_flags field is invalid"			},
    {	SC_PASSTHRU_INV_CDB_LEN,	"SC_PASSTHRU_INV_CDB_LEN",
	/* 13 */ "command_length field is invalid"		},
    {	SC_PASSTHRU_INV_AS_LEN,		"SC_PASSTHRU_INV_AS_LEN",
	/* 15 */ "autosense_length field is invalid"		},
    {	SC_PASSTHRU_INV_CDB,		"SC_PASSTHRU_INV_CDB",
	/* 16 */ "scsi_cdb field is invalid"			},
    {	SC_PASSTHRU_INV_TO,		"SC_PASSTHRU_INV_TO",
	/* 17 */ "timeout_value field is invalid"		},
    {	SC_PASSTHRU_INV_D_LEN,		"SC_PASSTHRU_INV_D_LEN",
	/* 18 */ "data_length field is invalid"			},
    {	SC_PASSTHRU_INV_SID,		"SC_PASSTHRU_INV_SID",
	/* 19 */ "scsi_id field is invalid"			},
    {	SC_PASSTHRU_INV_LUN,		"SC_PASSTHRU_INV_LUN",
	/* 20 */ "lun_id field is invalid"			},
    {	SC_PASSTHRU_INV_BUFF,		"SC_PASSTHRU_INV_BUFF",
	/* 21 */ "buffer field is invalid"			},
    {	SC_PASSTHRU_INV_AS_BUFF,	"SC_PASSTHRU_INV_BUFF",
	/* 22 */ "autosense_buffer_ptr is invalid"		},
    {	SC_PASSTHRU_INV_VAR_CDB_LEN,	"SC_PASSTHRU_INV_VAR_CDB_LEN",
	/* 23 */ "variable_cdb_length field is invalid"		},
    {	SC_PASSTHRU_INV_VAR_CDB,	"SC_PASSTHRU_INV_VAR_CDB",
	/* 24 */ "variable_cdb_ptr field is invalid"		}
};
static int einval_ReasonEntrys =
		sizeof(einval_ReasonTable) / sizeof(einval_ReasonTable[0]);

static char *
aix_EinvalReason(unsigned einval_reason, hbool_t report_brief)
{
    struct einval_ReasonTable *aer = einval_ReasonTable; 
    int entrys;

    for (entrys = 0; entrys < einval_ReasonEntrys; aer++, entrys++) {
        if (aer->einval_reason == einval_reason) {
            if (report_brief == True) {
                return(aer->einval_msg_brief);
            } else {
                return(aer->einval_msg_full);
            }
        }
    }
    return((report_brief == False) ? "Unknown EINVAL Reason" : "Unknown");
}

/*
 * Adapter Status Translation Table (see sys/scsi_buf.h for details).
 */
static struct adapter_sam_statusTable {
	unsigned adapter_status;
	char	*adapter_status_msg;
} adapter_sam_statusTable[] = {
    {	SCSI_HOST_IO_BUS_ERR,		"SCSI_HOST_IO_BUS_ERR"	},
    {	SCSI_TRANSPORT_FAULT,		"SCSI_TRANSPORT_FAULT"	},
    {	SCSI_CMD_TIMEOUT,		"SCSI_CMD_TIMEOUT"	},
    {	SCSI_NO_DEVICE_RESPONSE,	"SCSI_NO_DEVICE_RESPONSE" },
    {	SCSI_ADAPTER_HDW_FAILURE,	"SCSI_ADAPTER_HDW_FAILURE" },
    {	SCSI_ADAPTER_SFW_FAILURE,	"SCSI_ADAPTER_SFW_FAILURE" },
    {	SCSI_WW_NAME_CHANGE,		"SCSI_WW_NAME_CHANGE"	},
    {	SCSI_FUSE_OR_TERMINAL_PWR,	"SCSI_FUSE_OR_TERMINAL_PWR" },
    {	SCSI_TRANSPORT_RESET,		"SCSI_TRANSPORT_RESET"	},
    {	SCSI_TRANSPORT_BUSY,		"SCSI_TRANSPORT_BUSY"	},
    {	SCSI_TRANSPORT_DEAD,		"SCSI_TRANSPORT_DEAD"	},
    {	SCSI_VERIFY_DEVICE,		"SCSI_VERIFY_DEVICE"	}
#if defined(SCSI_ERROR_NO_RETRY)
                                                                 ,
    {	SCSI_ERROR_NO_RETRY,		"SCSI_ERROR_NO_RETRY"	}
#endif
#if defined(SCSI_ERROR_DELAY_LOG)
                                                                 ,
    {	SCSI_ERROR_DELAY_LOG,		"SCSI_ERROR_DELAY_LOG"	}
#endif
};
static int adapter_sam_statusEntrys =
		sizeof(adapter_sam_statusTable) / sizeof(adapter_sam_statusTable[0]);

static char *
AdapterSamStatus(unsigned adapter_status)
{
    struct adapter_sam_statusTable *ast = adapter_sam_statusTable; 
    int entrys;

    for (entrys = 0; entrys < adapter_sam_statusEntrys; ast++, entrys++) {
        if (ast->adapter_status == adapter_status) {
            return(ast->adapter_status_msg);
        }
    }
    return("Unknown Adapter Status");
}

static void
DumpScsiCmd(scsi_generic_t *sgp, struct sc_passthru *spt)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int i;
    char buf[128];
    char *bp = buf;
    char *msgp = NULL;

    Printf(opaque, "SCSI I/O Structure:\n");

    Printf(opaque, "    Device Name ............................ sgp->dsf: %s\n", sgp->dsf);
    /* Adapter or Device? */
    if (sgp->afd != INVALID_HANDLE_VALUE) {
	Printf(opaque, "    File Descriptor ........................ sgp->afd: %d\n", sgp->afd);
    } else {
	Printf(opaque, "    File Descriptor ......................... sgp->fd: %d\n", sgp->fd);
    }
    Printf(opaque, "    Version ................................. version: %u\n", spt->version);
    if (sgp->scsi_addr.scsi_path >= 0) {
	Printf(opaque, "    Path ID .................................. pathid: %u\n",
	       sgp->scsi_addr.scsi_path);
    }
    if (spt->status_validity == SC_SCSI_ERROR) {
	msgp = " (SC_SCSI_ERROR - SCSI Status Relects Error)";
    } else if (spt->status_validity == SC_ADAPTER_ERROR) {
	msgp = " (SC_ADAPTER_ERROR - Adapter Status Reflects Error)";
    } else {
	msgp = "";
    }
    Printf(opaque, "    Status Validity ................. status_validity: %#x%s\n",
	   spt->status_validity, msgp);
    if ( (spt->status_validity == SC_SCSI_ERROR)      ||
	 /* Display good SCSI status if no other error! */
	 ( (spt->status_validity != SC_ADAPTER_ERROR) &&
	   (spt->einval_arg == 0)                     &&
	   (spt->scsi_bus_status == SCSI_GOOD) ) ) {
	msgp = buf;
	(void)sprintf(buf, " (%s)", ScsiStatus(spt->scsi_bus_status));
    } else {
	msgp = "";
    }
    Printf(opaque, "    SCSI Bus Status ................. scsi_bus_status: %#x%s\n",
	    spt->scsi_bus_status, msgp);
    if (spt->adap_status_type == SC_ADAP_SC_ERR) {
	msgp = " (Parallel SCSI adapter status)";
    } else if (spt->adap_status_type == SC_ADAP_SAM_ERR) {
	msgp = " (SAM compliant adapter status)";
    } else {
	msgp = "";
    }
    Printf(opaque, "    Adapter Status Type ............ adap_status_type: %#x%s\n",
	   spt->adap_status_type, msgp);
    if ( (spt->status_validity == SC_ADAPTER_ERROR) && spt->adapter_status) {
	msgp = buf;
	(void)sprintf(buf, " (%s)\n", AdapterSamStatus(spt->adapter_status));
    } else {
	msgp = "";
    }
    Printf(opaque, "    Adapter Status ................... adapter_status: %#x%s\n",
	   spt->adapter_status, msgp);
    if (spt->adap_set_flags & SC_AUTOSENSE_DATA_VALID) {
	msgp = " (SC_AUTOSENSE_DATA_VALID - Autosense data valid)";
    } else if (spt->adap_set_flags & SC_RET_ID) {
	msgp = " (SC_RET_ID - SCSI ID different from callers)";
    } else {
	msgp = "";
    }
    Printf(opaque, "    Adapter Set Flags ................ adap_set_flags: %#x%s\n",
	   spt->adap_set_flags, msgp);
    Printf(opaque, "    Adapter Queue Status .............. adap_q_status: %#x\n",
	   spt->adap_q_status);
    Printf(opaque, "    Additional Device Status ...... add_device_status: %#x\n",
	   spt->add_device_status);
    if (spt->q_tag_msg == SC_SIMPLE_Q) {
	msgp = " (SC_SIMPLE_Q)";
    } else if (spt->q_tag_msg == SC_HEAD_OF_Q) {
	msgp = " (SC_HEAD_OF_Q)";
    } else if (spt->q_tag_msg == SC_ORDERED_Q) {
	msgp = " (SC_ORDERED_Q)";
    } else if (spt->q_tag_msg == SC_ACA_Q) {
	msgp = " (SC_ACA_Q)";
    } else {
	msgp = " (SC_NO_Q)";
    }
    Printf(opaque, "    Queue Tag Message ..................... q_tag_msg: %#x%s\n",
	   spt->q_tag_msg, msgp);
    if (spt->flags & B_READ) {
	msgp = " (B_READ)";
    } else {
	msgp = " (B_WRITE)";
    }
    Printf(opaque, "    Control Flags ............................. flags: %#x%s\n",
	   spt->flags, msgp);
    if (spt->devflags & SC_MIX_IO) {
	msgp = " (SC_MIX_IO)";
    } else {
	msgp = " (SC_QUIESCE_IO)";
    }
    Printf(opaque, "    Device Flags ........................... devflags: %#x%s\n",
	   spt->devflags, msgp);
    Printf(opaque, "    Queue Flags ............................. q_flags: %#x\n",
	   spt->q_flags);
    if (spt->q_flags & SC_RESUME) {
	Printf(opaque, "                                                       %#x = SC_RESUME\n",
	       SC_RESUME);
    }
    if (spt->q_flags & SC_DELAY_CMD) {
	Printf(opaque, "                                                       %#x = SC_DELAY_CMD\n",
	       SC_DELAY_CMD);
    }
    if (spt->q_flags & SC_Q_CLR) {
	Printf(opaque, "                                                       %#x = SC_Q_CLR\n",
	       SC_Q_CLR);
    }
    if (spt->q_flags & SC_Q_RESUME) {
	Printf(opaque, "                                                       %#x = SC_Q_RESUME\n",
	       SC_Q_RESUME);
    }
    if (spt->q_flags & SC_CLEAR_ACA) {
	Printf(opaque, "                                                       %#x = SC_CLEAR_ACA\n",
	       SC_CLEAR_ACA);
    }
    if (spt->q_flags & SC_TARGET_RESET) {
	Printf(opaque, "                                                       %#x = SC_TARGET_RESET\n",
	       SC_TARGET_RESET);
    }
    if (spt->q_flags & SC_DEV_RESTART) {
	Printf(opaque, "                                                       %#x = SC_DEV_RESTART\n",
	       SC_DEV_RESTART);
    }
    if (spt->q_flags & SC_LUN_RESET) {
	Printf(opaque, "                                                       %#x = SC_LUN_RESET\n",
	       SC_LUN_RESET);
    }
    if (spt->einval_arg) {
	msgp = buf;
	(void)sprintf(buf, " (%s - %s)\n",
	       aix_EinvalReason(spt->einval_arg, True),
	       aix_EinvalReason(spt->einval_arg, False));
    } else {
	msgp = "";
    }
    Printf(opaque, "    EINVAL argument ...................... einval_arg: %u%s\n",
	   spt->einval_arg, msgp);
    Printf(opaque, "    Command Timeout ................... timeout_value: %u seconds\n",
	   spt->timeout_value);
    for (i = 0; (i < spt->command_length); i++) {
	bp += sprintf(bp, "%x ", spt->scsi_cdb[i]);
    }
    Printf(opaque, "    Command Descriptor Block ............... scsi_cdb: %s(%s)\n", buf, sgp->cdb_name);
    Printf(opaque, "    CDB Length ........................... cdb_length: %d\n", spt->command_length);
    Printf(opaque, "    SCSI ID ................................. scsi_id: " LXF "\n", spt->scsi_id);
    Printf(opaque, "    LUN ID ................................... lun_id: " LXF "\n", spt->lun_id);
    Printf(opaque, "    I/O Buffer Address ....................... buffer: 0x%p\n", spt->buffer);
    Printf(opaque, "    I/O Buffer Length ................... data_length: " LUF " (" LXF ")\n",
	   spt->data_length, spt->data_length);
    Printf(opaque, "    Request Sense Buffer ....... autosense_buffer_ptr: 0x%p\n", spt->autosense_buffer_ptr);
    Printf(opaque, "    Request Sense Length ........... autosense_length: %d (%#x)\n",
	   spt->autosense_length, spt->autosense_length);
    Printf(opaque, "    Target's WWN .................... world_wide_name: " LXF "\n", spt->world_wide_name);
    Printf(opaque, "    Target's Node Name .................... node_name: " LXF "\n", spt->node_name);
    Printf(opaque, "    Variable CDB Length ......... variable_cdb_length: %d\n", spt->variable_cdb_length);
    Printf(opaque, "    Variable CDB Pointer ........... variable_cdb_ptr: 0x%p\n", spt->variable_cdb_ptr);
    Printf(opaque, "    Residual (bytes not transferred) ....... residual: " LUF " (" LXF ")\n",
	   spt->residual, spt->residual);
    DumpCdbData(sgp);
    Printf(opaque, "\n");
}

static void
DumpSciolst(scsi_generic_t *sgp, lun_info_t *lunip, struct scsi_sciolst *sciop, char *operation)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    Printf(opaque, "\nDumping SCSI Adapter Structure: %#lx\n\n", sciop);

    Printf(opaque, "    Operation .......................................: %s\n", operation);
    Printf(opaque, "    Adapter Name ...................... lunip->parent: %s\n", lunip->parent);
    Printf(opaque, "    File Descriptor ........................ sgp->afd: %d\n", sgp->afd);
    Printf(opaque, "    Version ................................. version: %u\n", sciop->version);
    Printf(opaque, "    Flags ..................................... flags: %#x\n", sciop->flags);
    if (sciop->flags & ISSUE_LOGIN) {
        Printf(opaque, "                                                       %#x = ISSUE_LOGIN\n", ISSUE_LOGIN);
    }
    if (sciop->flags & FORCED) {
        Printf(opaque, "                                                       %#x = FORCED\n", FORCED);
    }
    if (sciop->flags & SCIOLRESET_LUN_RESET) {
        Printf(opaque, "                                                       %#x = SCIOLRESET_LUN_RESET\n",
                                                                               SCIOLRESET_LUN_RESET);
    }
    Printf(opaque, "    Adapter Flags .................... adap_set_flags: %#x\n", sciop->adap_set_flags);
    if (sciop->adap_set_flags & WWN_VALID) {
        Printf(opaque, "                                                       %#x = WWN_VALID\n", WWN_VALID);
    }
    if (sciop->adap_set_flags & DEVFLG_VALID) {
        Printf(opaque, "                                                       %#x = DEVFLG_VALID\n", DEVFLG_VALID);
    }
    if (sciop->adap_set_flags & SCSI_MSK_VALID) {
        Printf(opaque, "                                                       %#x = SCSI_MSK_VALID\n", SCSI_MSK_VALID);
    }
    if (sciop->adap_set_flags & SCSI_DFLT_VALID) {
        Printf(opaque, "                                                       %#x = SCSI_DFLT_VALID\n", SCSI_DFLT_VALID);
    }
    if (sciop->adap_set_flags & SCSI_DEV_STARTED) {
        Printf(opaque, "                                                       %#x = SCSI_DEV_STARTED\n", SCSI_DEV_STARTED);
    }
    if (sciop->adap_set_flags & SCIOL_RET_ID_ALIAS) {
        Printf(opaque, "                                                       %#x = SCIOL_RET_ID_ALIAS\n", SCIOL_RET_ID_ALIAS);
    }
    if (sciop->adap_set_flags & SCIOL_RET_HANDLE) {
        Printf(opaque, "                                                       %#x = SCIOL_RET_HANDLE\n", SCIOL_RET_HANDLE);
    }
    if (sciop->adap_set_flags & SCIOL_DYNTRK_ENABLED) {
        Printf(opaque, "                                                       %#x = SCIOL_DYNTRK_ENABLED\n", SCIOL_DYNTRK_ENABLED);
    }
    Printf(opaque, "    Additional Device Flags ........... add_dev_flags: %#x\n", sciop->add_dev_flags);
    Printf(opaque, "    Device Flags ....................... device_flags: %#x\n", sciop->device_flags);
    Printf(opaque, "    Default Setting .................... dflt_setting: %#x\n", sciop->dflt_setting);
    Printf(opaque, "    Setting Mask ....................... setting_mask: %#x\n", sciop->setting_mask);
    Printf(opaque, "    Target's WWN .................... world_wide_name: " LXF "\n", sciop->world_wide_name);
    Printf(opaque, "    Target's Node Name .................... node_name: " LXF "\n", sciop->node_name);
    Printf(opaque, "    Password ............................... password: %#x\n", sciop->password);
    Printf(opaque, "    SCSI ID ................................. scsi_id: " LXF "\n", sciop->scsi_id);
    Printf(opaque, "    LUN ID ................................... lun_id: " LXF " (real %u)\n",
            sciop->lun_id, (uint16_t)(sciop->lun_id >> 48L)); /* Real LUN # is in upper 16 bits! */
    if (lunip->adapter_type == ATYPE_ISCSI) {
        Printf(opaque, "    Flags for union ............... parms.iscsi.flags: %#x\n", sciop->parms.iscsi.flags);
        Printf(opaque, "    Login Status Class ..... parms.iscsi.status_class: %#x\n", sciop->parms.iscsi.status_class);
        Printf(opaque, "    Login Status Detail ... parms.iscsi.status_detail: %#x\n", sciop->parms.iscsi.status_detail);
        Printf(opaque, "    Location Type .............. parms.iscsi.loc_type: %#x\n", sciop->parms.iscsi.loc_type);
      if (sciop->parms.iscsi.loc_type & SCIOL_ISCSI_LOC_HOSTNAME) {
        Printf(opaque, "    Location Hostname . parms.iscsi.location.hostname: %#x\n", sciop->parms.iscsi.location.hostname);
      } else if (sciop->parms.iscsi.loc_type & SCIOL_ISCSI_LOC_IPV_ADDR) {
        Printf(opaque, "    iSCSI Host Address ............. lunip->host_addr: %s\n", lunip->host_addr);
        Printf(opaque, "    Location Address ...... parms.iscsi.location.addr: (type=%#x, addr=" LXF "," LXF ")\n",
                sciop->parms.iscsi.location.addr.addr_type,
                sciop->parms.iscsi.location.addr.addr[0], sciop->parms.iscsi.location.addr.addr[1]);
      }
        Printf(opaque, "    iSCSI Target Name ......... parms.iscsi.name[256]: %s\n", sciop->parms.iscsi.name);
        Printf(opaque, "    iSCSI TCP Port Number ...... parms.iscsi.port_num: " LXF " (" LUF ")\n",
                sciop->parms.iscsi.port_num, sciop->parms.iscsi.port_num);
    }
    if (sciop->adap_set_flags & SCIOL_RET_HANDLE) {
        Printf(opaque, "    Kernel Extension Handle ... handle.kernext_handle: %#p\n", sciop->handle.kernext_handle);
    } else {
        Printf(opaque, "    Application Handle ............ handle.app_handle: " LXF "\n", sciop->handle.app_handle);
    }
    DumpCdbData(sgp);
    Printf(opaque, "\n");
    return;
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
    /* Note: We store spt->adapter_status here! */
    if (sgp->host_status) {
	return ( AdapterSamStatus(sgp->host_status) );
    } else {
	return (NULL);
    }
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
    return (NULL);
}

/* ============================================================================================= */
/*
 * Functions for Managing the SCSI Device Table:
 */

static scsi_dir_path_t scsi_dir_paths[] = {
    {	DEV_PATH,	"rhdisk",   "Device Path",	True	},
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
	    if (scsi_name) {
		if (strncmp(dirent->d_name, scsi_name, strlen(scsi_name))) {
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
	//scsi_device_name_t *sdnp;
	//sdnp = update_device_entry(sgp, sdep, path, inquiry, serial, device_id,
				   //target_port, bus, channel, target, lun);
	Eprintf(opaque, "Found unexpected duplicate device %s with %s %s, ignoring...\n",
	        path, (serial) ? "serial number" : "device ID", (serial) ? serial : device_id);
	Fprintf(opaque, "Previous device is %s, which is NOT expected with proper multi-pathing!\n",
		sdep->sde_names.sdn_flink->sdn_device_path);
	//abort();	/* Do NOT abort, this is misleading even to the author! */
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
