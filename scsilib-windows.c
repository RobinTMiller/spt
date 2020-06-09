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
 * Module:	scsilib-windows.c
 * Author:	Robin T. Miller
 * Date:	March 29th, 2005
 *
 * Description:
 *  This module contains the OS SCSI specific functions for Windows.
 *
 * Modification History:
 *
 * May 28th, 2020 by Robin T. Miller
 *      Add "The requested resource is in use." (ERROR_BUSY) as retriable
 * error, since this is being returned during path failovers due to faults.
 * 
 * June 19th, 2019 by Robin T. Miller
 *      Add os_find_scsi_devices() to lookup SCSI devices.
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
#include <stddef.h>
#include <fcntl.h>
#include <sys/types.h>

#include "spt.h"
#include "spt_devices.h"

/*
 * Local Definitions:
 */

/*
 * Forward Declarations:
 */
static int process_device(scsi_generic_t *sgp, char *devpath, scsi_filters_t *sfp);
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


#if !defined(WIN_SCSILIB_H)
#define WIN_SCSILIB_H

/*
 * Note: The best I can recall, this stuff is inline to avoid requiring the DDK include files!
 * 	 These definitions come from: NTDDK/inc/basetsd.h (only place I find them!)
 */
#if defined(_WIN64)
    typedef __int64 INT_PTR, *PINT_PTR;
    typedef unsigned __int64 UINT_PTR, *PUINT_PTR;

    typedef __int64 LONG_PTR, *PLONG_PTR;
    typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;

    #define __int3264   __int64

#else /* !defined(_WIN64) */
    typedef _W64 int INT_PTR, *PINT_PTR;
    typedef _W64 unsigned int UINT_PTR, *PUINT_PTR;

    typedef _W64 long LONG_PTR, *PLONG_PTR;
    typedef _W64 unsigned long ULONG_PTR, *PULONG_PTR;

    #define __int3264   __int32

#endif /* defined(_WIN64) */

#include <winioctl.h>
#include <ntddscsi.h>

typedef struct _SPTDWB {
    SCSI_PASS_THROUGH_DIRECT spt;
    ULONG filler;
    UCHAR senseBuf[256]; 
} SPTWB, *PSPTWB;  

#endif /* !defined(WIN_SCSILIB_H) */

/*
 * Forward Declarations:
 */

static void DumpScsiCmd(scsi_generic_t *sgp, SPTWB sptwb);

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
    char *wdsf = sgp->dsf;
    hbool_t wdsf_allocated = False;

    /*
     * Automatically add the hidden device directory (for ease of use).
     */ 
    if (strncmp(sgp->dsf, DEV_DIR_PREFIX, DEV_DIR_LEN) != 0) {
	wdsf = Malloc(opaque, DEV_DEVICE_LEN);
	if (wdsf) {
	    wdsf_allocated = True;
	    (void)sprintf(wdsf, "%s%s", DEV_DIR_PREFIX, sgp->dsf);
	} else {
	    wdsf = sgp->dsf;
	}
    }
    if (sgp->debug == True) {
	Printf(opaque, "Opening device %s...\n", wdsf);
    }
    sgp->fd = CreateFile(wdsf, (GENERIC_READ | GENERIC_WRITE),
			 (FILE_SHARE_READ | FILE_SHARE_WRITE), NULL,
			 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    /*
     * If write protected, try open'ing read-only. 
     */
    if ( (sgp->fd == INVALID_HANDLE_VALUE) &&
	 (GetLastError() == ERROR_WRITE_PROTECT) ) {
	if (sgp->debug == True) {
	    Printf(opaque, "Opening device %s read-only...\n", wdsf);
	}
	sgp->fd = CreateFile(sgp->dsf, GENERIC_READ, FILE_SHARE_READ, NULL,
			     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (sgp->fd == INVALID_HANDLE_VALUE) {
	if (sgp->errlog == True) {
	    os_perror(opaque, "CreateFile() failed on %s", wdsf);
	}
	status = FAILURE;
    }
    if ( (sgp->debug == True) && (sgp->fd != INVALID_HANDLE_VALUE) ) {
	Printf(opaque, "Device %s successfully opened, handle = %d\n", wdsf, sgp->fd);
    }
    if (wdsf_allocated) free(wdsf);
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
	Printf(opaque, "Closing device %s, handle = %d...\n", sgp->dsf, sgp->fd);
    }
    if ( (error = CloseHandle(sgp->fd)) == 0) {
	os_perror(opaque, "CloseHandle() failed on %s", sgp->dsf);
	error = -1;
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
    int error = SUCCESS;
    STORAGE_BUS_RESET_REQUEST sbr;
    DWORD bc; 

    sbr.PathId = sgp->scsi_addr.scsi_bus;
    if (DeviceIoControl(sgp->fd,
			IOCTL_STORAGE_RESET_BUS,
			&sbr,
			sizeof(sbr),
			NULL,
			0,
			&bc,
			NULL) == 0) {
	error = FAILURE; 
	sgp->os_error = GetLastError();
	if (sgp->errlog == True) {
	    os_perror(opaque, "SCSI reset bus (IOCTL_STORAGE_RESET_BUS) failed on %s", sgp->dsf);
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
    int error = WARNING;

    if (sgp->errlog == True) {
	Printf(opaque, "SCSI reset device is not implemented!\n");
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
 *    This function takes a high level SCSI command, converts it
 * into the format necessary for this OS, then executes it and
 * returns an OS independent format to the caller.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the SCSI request which is:
 *        0 = Success, 1 = Warning, -1 = Failure
 */
int
os_spt(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int error = SUCCESS;
    SPTWB sptwb;
    PSCSI_PASS_THROUGH_DIRECT pspt = &sptwb.spt;
    DWORD bc;
    int i;

    memset(&sptwb, 0, sizeof(sptwb));
    pspt->Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    pspt->PathId = sgp->scsi_addr.scsi_bus;
    pspt->TargetId = sgp->scsi_addr.scsi_target;
    pspt->Lun = sgp->scsi_addr.scsi_lun;

    /*
     * Timeout passed by the calling function is always in milliseconds.
     */
    pspt->TimeOutValue = (sgp->timeout / MSECS);

    /*
     * Set up the direction:
     */
    if (sgp->data_dir == scsi_data_none)
	pspt->DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;  /* Direction of data transfer unspecified */
    else if (sgp->data_dir == scsi_data_read)
	pspt->DataIn = SCSI_IOCTL_DATA_IN;	     /* Read data from the device */
    else if (sgp->data_dir == scsi_data_write)
	pspt->DataIn = SCSI_IOCTL_DATA_OUT;	     /* Write data to the device */

    pspt->DataTransferLength = sgp->data_length;
    pspt->DataBuffer = sgp->data_buffer;
    pspt->SenseInfoLength = sgp->sense_length;
    pspt->SenseInfoOffset = offsetof(SPTWB, senseBuf);

    pspt->CdbLength = sgp->cdb_size;
    for (i=0; i<sgp->cdb_size; i++) {
	pspt->Cdb[i] = sgp->cdb[i];
    }

    /*
     * Finally execute the SCSI command:
     */
    if (DeviceIoControl(sgp->fd,
			IOCTL_SCSI_PASS_THROUGH_DIRECT,
			&sptwb,
			sizeof(SPTWB),
			&sptwb,
			sizeof(SPTWB),
			&bc,
			NULL) == 0) {
	error = FAILURE;
    }

    /*
     * Handle errors, and send pertinent data back to caller.
     */
    if (error < 0) {
	sgp->os_error = GetLastError();
	if (sgp->errlog == True) {
	    os_perror(opaque, "Scsi Request IOCTL_SCSI_PASS_THROUGH_DIRECT failed on %s", sgp->dsf);
	}
	sgp->error = True;
	goto error;
    }

    if (pspt->ScsiStatus == SCSI_GOOD) {
	sgp->error = False;	  /* Show SCSI command was successful. */
    } else {
	sgp->error = True;	  /* Tell caller we've had some sort of error */ 
	if ( (sgp->errlog == True) && (pspt->ScsiStatus != SCSI_CHECK_CONDITION)) {
	    Fprintf(opaque, "%s failed, SCSI Status = %d (%s)\n",sgp->cdb_name, 
		    pspt->ScsiStatus, ScsiStatus(pspt->ScsiStatus));
	}
    }

    if ( pspt->ScsiStatus == SCSI_CHECK_CONDITION ) {
	sgp->sense_valid = True;
	(void)memcpy(sgp->sense_data, &sptwb.senseBuf, pspt->SenseInfoLength);
    }

    sgp->scsi_status = pspt->ScsiStatus;
    sgp->data_resid = (sgp->data_length - pspt->DataTransferLength);     
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
error:
    if (sgp->debug == True) {
	DumpScsiCmd(sgp, sptwb);
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
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    hbool_t is_retriable = False;
    
    if (sgp->os_error == ERROR_DEV_NOT_EXIST) {
	//
	// MessageId: ERROR_DEV_NOT_EXIST
	//
	// MessageText:
	//
	//  The specified network resource or device is no longer available.
	//
	// #define ERROR_DEV_NOT_EXIST              55L    // dderror
	//
	if (sgp->debug == True) {
	    Printf(opaque, "DEBUG: ERROR_DEV_NOT_EXIST detected on %s...\n", sgp->cdb_name);
	}
	is_retriable = True;
    } else if (sgp->os_error == ERROR_BUSY) {
	//
	// MessageId: ERROR_BUSY
	//
	// MessageText:
	//
	// The requested resource is in use.
	//
	// #define ERROR_BUSY                       170L    // dderror
        //
	if (sgp->debug == True) {
	    Printf(opaque, "DEBUG: ERROR_BUSY detected on %s...\n", sgp->cdb_name);
	}
	is_retriable = True;
    } else if (sgp->os_error == ERROR_IO_DEVICE) {
	//
	// MessageId: ERROR_IO_DEVICE
	//
	// MessageText:
	//
	//  The request could not be performed because of an I/O device error.
	//
	// #define ERROR_IO_DEVICE                  1117L
	//
	if (sgp->debug == True) {
	    Printf(opaque, "DEBUG: ERROR_IO_DEVICE detected on %s...\n", sgp->cdb_name);
	}
	is_retriable = True;
    } else if (sgp->os_error == ERROR_DEVICE_NOT_CONNECTED) {
	//
	// MessageId: ERROR_DEVICE_NOT_CONNECTED
	//
	// MessageText:
	//
	//  The device is not connected.
	//
	// #define ERROR_DEVICE_NOT_CONNECTED       1167L
	//
	if (sgp->debug == True) {
	    Printf(opaque, "DEBUG: ERROR_DEVICE_NOT_CONNECTED detected on %s...\n", sgp->cdb_name);
	}
	is_retriable = True;
	
    } else if (sgp->os_error == ERROR_NO_SYSTEM_RESOURCES) {
	//
	// This error is occurring intermittently, so we will retry since we
	// believe this is a transient error. (resources should become available)
	// 
	// MessageId: ERROR_NO_SYSTEM_RESOURCES
	//
	// MessageText:
	//
	//  Insufficient system resources exist to complete the requested service.
	//
	// #define ERROR_NO_SYSTEM_RESOURCES        1450L
	//
	if (sgp->debug == True) {
	    Printf(opaque, "DEBUG: ERROR_NO_SYSTEM_RESOURCES detected on %s...\n", sgp->cdb_name);
	}
	is_retriable = True;
    }
    return (is_retriable);
}

static void
DumpScsiCmd(scsi_generic_t *sgp, SPTWB sptwb)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int i;
    char buf[128];
    char *bp = buf;
    char *msgp = NULL;
    PSCSI_PASS_THROUGH_DIRECT pspt = &sptwb.spt; 

    Printf(opaque, "SCSI I/O Structure\n");

    Printf(opaque, "    Device Special File .............................: %s\n", sgp->dsf);
    Printf(opaque, "    File Descriptor .............................. fd: %d\n", sgp->fd);

    if (pspt->DataIn == SCSI_IOCTL_DATA_IN)
	msgp = "SCSI_IOCTL_DATA_IN";
    else if (pspt->DataIn == SCSI_IOCTL_DATA_OUT)
	msgp = "SCSI_IOCTL_DATA_OUT";
    else if (pspt->DataIn == SCSI_IOCTL_DATA_UNSPECIFIED)
	msgp  = "SCSI_IOCTL_DATA_UNSPECIFIED";

    Printf(opaque, "    Data Direction ........................... DataIn: %#x (%s)\n", pspt->DataIn,
	   msgp);
    Printf(opaque, "    SCSI CDB Status ...................... ScsiStatus: %#x (%s)\n", pspt->ScsiStatus,
	   ScsiStatus(pspt->ScsiStatus));  
    Printf(opaque, "    Command Timeout .................... TimeOutValue: %lu\n", pspt->TimeOutValue);

    for (i = 0; (i < pspt->CdbLength); i++) {
	bp += sprintf(bp, "%x ", pspt->Cdb[i]);
    }
    Printf(opaque, "    Command Descriptor Block .................... Cdb: %s (%s)\n",buf, sgp->cdb_name);
    Printf(opaque, "    I/O Buffer .............................. dataBuf: 0x%p\n", sgp->data_buffer);
    Printf(opaque, "    I/O Buffer Length ................... data_length: %u\n", pspt->DataTransferLength);     
    Printf(opaque, "    Request Sense Buffer ................... senseBuf: 0x%p\n", sptwb.senseBuf);
    Printf(opaque, "    Request Sense Length ............... sense_length: %u\n", pspt->SenseInfoLength);
    /* Note: Windows SPT alters DataTransferLength to be the bytes actually transferred. */
    Printf(opaque, "    Requested Data Length .......... sgp->data_length: %u\n", sgp->data_length);
    Printf(opaque, "    Residual Data Length ............ sgp->data_resid: %u\n", sgp->data_resid);
    Printf(opaque, "    Data Bytes Transferred .... sgp->data_transferred: %u\n", sgp->data_transferred);
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

int
os_find_scsi_devices(scsi_generic_t *sgp, scsi_filters_t *sfp, char *paths)
{
    void *opaque = sgp->tsp->opaque;
    FILE *pipef;
    char cmd_output[STRING_BUFFER_SIZE];
    char *cmd = "wmic diskdrive get DeviceID";
    int lines = 0;
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
	    status = process_device(sgp, path, sfp);
	    //status = find_scsi_devices(sgp, dir_path, dev_name, sfp);
	    path = strtok_r(NULL, sep, &saveptr);
	}
	Free(opaque, str);
    } else {
	if (sgp->debug) {
	    Printf(opaque, "Executing: %s\n", cmd);
	}
	pipef = popen(cmd, "r");
	if (pipef == NULL) {
	    //os_perror(opaque, "popen() failed");
	    return(FAILURE);
	}
	memset(cmd_output, '\0', sizeof(cmd_output));
	/*
	 * Read and log output from the command.
	 */
	while (fgets(cmd_output, sizeof(cmd_output), pipef) == cmd_output) {
	    char *p;
	    if (lines++ == 0) {
		/* First line is heading 'DeviceID' so skip this. */
		continue;
	    }
	    p = strchr(cmd_output, ' ');
	    if (p) { *p = '\0'; }
	    p = strchr(cmd_output, '\r');
	    if (p) { *p = '\0'; }
	    if (strlen(cmd_output)) {
		status = process_device(sgp, cmd_output, sfp);
	    }
	}
	status = pclose(pipef);
	status = WEXITSTATUS(status);
    }
    return(status);
}

static int
process_device(scsi_generic_t *sgp, char *devpath, scsi_filters_t *sfp)
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
    //struct sg_scsi_id scsi_id, *sid = &scsi_id;
    char *path = devpath;
    char *dsf = sgp->dsf;
    HANDLE fd = INVALID_HANDLE_VALUE;
    int status = SUCCESS;

    if (sgp->debug == True) {
	Printf(opaque, "Processing device %s...\n", devpath);
    }

    /* Filtering here is safe for non-Linux systems. */
    /* Filter on the device or exclude paths, if specified. */
    /* Note: Filtering here means we won't find all device paths! */
    if (sfp && sfp->device_paths) {
	if ( match_device_paths(path, sfp->device_paths) == False ) {
	    return(status); /* No match, skip this device. */
	}
    }
    if (sfp && sfp->exclude_paths) {
	if ( match_device_paths(path, sfp->exclude_paths) == True ) {
	    return(status); /* Match, so exclude this device. */
	}
    }

    fd = CreateFile(path, (GENERIC_READ | GENERIC_WRITE),
		    (FILE_SHARE_READ | FILE_SHARE_WRITE), NULL,
		    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fd == INVALID_HANDLE_VALUE) {
	os_perror(opaque, "Failed to open device %s", path);
	return(FAILURE);
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
	    length = (int)strlen(sfp->vendor);
	    if (strncmp(sfp->vendor, (char *)inquiry->inq_vid, length)) {
		goto close_and_continue;
	    }
	}
	if (sfp->revision) {
	    length = (int)strlen(sfp->revision);
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
     * For SAS protocol, the target port is the drive SAS address.
     */
    target_port = DecodeTargetPortIdentifier(opaque, inquiry, inquiry_page);

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

close_and_continue:
    (void)CloseHandle(fd);
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
	//scsi_device_name_t *sdnp;
	//sdnp = update_device_entry(sgp, sdep, path, inquiry, serial, device_id,
				   //target_port, bus, channel, target, lun);
	abort();
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
	if (serial && sdep->sde_serial) {
	    if (strcmp(sdep->sde_serial, serial) == 0) {
		return( sdep );
	    }
	}
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
    }
    return(NULL);
}

#if 0
static scsi_device_name_t *
update_device_entry(scsi_generic_t *sgp, scsi_device_entry_t *sdep,
		    char *path, inquiry_t *inquiry,
		    char *serial, char *device_id, char *target_port,
		    int bus, int channel, int target, int lun)
{
    void *opaque = sgp->tsp->opaque;
    struct scsi_device_name *sdnh = &sdep->sde_names;
    scsi_device_name_t *sdnp;
    scsi_device_name_t *sptr;

    /* For SCSI generic "sg" devices, try to find an "sd" device path. */
    if (strncmp(path, SG_PATH_PREFIX, SG_PATH_SIZE) == 0) {
	sdnp = find_device_by_nexus(sgp, sdep, path, bus, channel, target, lun);
	if (sdnp) {
	    sdnp->sdn_scsi_path = strdup(path);
	    return( sdnp );
	}
    }
    /* We just need to add a new device name. */
    sdnp = Malloc(opaque, sizeof(*sdnp));
    if (sdnp == NULL) return(NULL);
    sdnp->sdn_device_path = strdup(path);
    if (target_port) {
	sdnp->sdn_target_port = strdup(target_port);
    }
    sdnp->sdn_bus = bus;
    sdnp->sdn_channel = channel;
    sdnp->sdn_target = target;
    sdnp->sdn_lun = lun;
    /* Sort by device name. */
    for (sptr = sdnh->sdn_flink; (sptr != sdnh) ; sptr = sptr->sdn_flink) {
	if (sptr->sdn_flink) {
	    /* Sort by name, ensuring "sda,sdb,..." comes before "sdaa", etc. */
	    if ( ( strlen(path) < strlen(sptr->sdn_device_path) ) ||
		 ((strlen(path) == strlen(sptr->sdn_device_path) &&
		   strcmp(path, sptr->sdn_device_path) < 0)) ) {
		sdnp->sdn_flink = sptr;
		sdnp->sdn_blink = sptr->sdn_blink;
		sptr->sdn_blink->sdn_flink = sdnp;
		sptr->sdn_blink = sdnp;
		return( sdnp );
	    }
	}
    }
    /* Insert at the tail. */
    sptr = sdnh->sdn_blink;
    sptr->sdn_flink = sdnp;
    sdnp->sdn_blink = sptr;
    sdnp->sdn_flink = sdnh;
    sdnh->sdn_blink = sdnp;
    return( sdnp );
}
#endif /* 0 */

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
