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
 * Module:	scsilib-linux.c
 * Author:	Robin T. Miller
 * Date:	March 28th, 2005
 *
 * Description:
 *  This module contains the OS SCSI specific functions for Linux.
 *
 * Modification History:
 * 
 * July 8th, 2019 by Robin T. Miller
 *      Update find_scsi_devices() to close file descriptors in error paths.
 * 
 * April 26th, 2019 by Robin T. Miller
 *      Switch from strtok() to reentrant version strtok_r() to avoid issues
 * when multiple functions invoke this API and to be thread safe (libspt.so).
 * 
 * May 17th, 2018 by Robin T. Miller
 *      Update SCSI generic device lookup processing, to map "sg" to "sd"
 * devices, so these can be reported together to using this information.
 * When adding multiple device names, sort the names so "sdb" is prior to
 * say "sdbq", rather than always adding devices to the device name tail.
 * Also, acquire and save the SAS target port per device path.
 * 
 * May 1st, 2018 by Robin T. Miller
 *      Update acquring of SCSI device nexus information, to use the /sys
 * file system, since the older IOCTL method truncates values to 8 bits.
 * 
 * November 26th, 2018 by Robin T. Miller.
 *      Update find_device_entry() to allow lookup by SCSI nexus only.
 *      Add the ability to exclude devices and associated device paths.
 *      Thus, /dev/sdN and its' /dev/sgN and /dev/mapper are excluded.
 * 
 * November 23rd, 2018 by Robin T. Miller
 *      Try to acquire vendor specific Inquiry serial number, to avoid
 * requesting the Inquiry serial number page.
 * 
 * February 22nd, 2018 by Robin T. Miller
 *      Filter out more /dev/mapper names, such as "*.{home,root,swap)".
 *      When sorting, ensure "sda,sdb,..." come before "sdaa,sdab,etc".
 * 
 * February 21st, 2018 by Robin T. Miller
 *      If sg device does *not* have a device ID (WWN) or serial number,
 * then try to match the bus/channel/target/lun to a prior "sd" device.
 * This more closely mimic's what 'lsscsi' does for sd/sg devices.
 * 
 * February 12th, 2018 by Robin T. Miller
 *      Add os_find_scsi_devices() and filters to find desired devices.
 * 
 * November 2nd, 2017 by Robin T. Miller
 *      Added support for mapping from device name to SCSI device name.
 * 
 * August 14th, 2012 by Robin T. Miller
 *	Added new host status codes introduced for multipathing.
 *
 * February 6th, 2012 by Robin T. Miller
 * 	Properly display the sense lengths when debug is enabled.
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
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>	/* for dirname() and basename() */
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <scsi/sg.h>
#include <scsi/scsi.h>

#include "spt.h"
#include "spt_devices.h"

/*
 * Local Definitions:
 */
#define DEV_PATH	"/dev"
#define SD_PATH_PREFIX	"/dev/sd"
#define SD_PATH_SIZE	7
#define SG_PATH_PREFIX	"/dev/sg"
#define SG_PATH_SIZE	7
#define DM_PATH_PREFIX	"/dev/dm"
#define DM_PATH_SIZE	7
#define DMMP_PATH_PREFIX "/dev/mapper"
#define DMMP_PATH_SIZE	11

/*
 * Forward Declarations:
 */
static char *find_scsi_device(scsi_generic_t *sgp);
static char *find_sg_device(scsi_generic_t *sgp, int bus, int channel, int target, int lun);
static char *linux_HostStatus(unsigned short host_status);
static char *linux_DriverStatus(unsigned short driver_status);
static char *linux_SuggestStatus(unsigned short suggest_status);
static void DumpScsiCmd(scsi_generic_t *sgp, sg_io_hdr_t *siop);
static int force_path_failover(scsi_generic_t *sgp);

static int get_device_nexus(scsi_generic_t *sgp, char *devname, int fd, int *bus, int *channel, int *target, int *lun);
static int find_scsi_devices(scsi_generic_t *sgp, char *devpath, char *scsi_name, scsi_filters_t *sfp);
static scsi_device_name_t *find_device_by_nexus(scsi_generic_t *sgp, scsi_device_entry_t *sdep,
						char *path, int bus, int channel, int target, int lun);
static hbool_t omit_device_name(scsi_generic_t *sgp, char *name);

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
    char *dsf = NULL;

    /* Find assocoated /dev/sgN device, unless disabled or specified. */
    if ( sgp->mapscsi && (sgp->adsf == NULL) &&
	 (strncmp(sgp->dsf, SG_PATH_PREFIX, SG_PATH_SIZE) != 0) ) {
	sgp->adsf = find_scsi_device(sgp);
    }
    dsf = (sgp->adsf) ? sgp->adsf : sgp->dsf;
    /* 
     * /dev/sg devices do NOT like the direct I/O flag!
     * Results in: errno = 22 - Invalid argument
     */
    if (strncmp(dsf, SG_PATH_PREFIX, SG_PATH_SIZE) != 0) {
	oflags |= O_DIRECT;
    }
    if (sgp->debug == True) {
	Printf(opaque, "Opening device %s, open flags = %#o (%#x)...\n",
	       dsf, oflags, oflags);
    }
    if ( (sgp->fd = open(dsf, oflags)) < 0) {
	if (errno == EROFS) {
	    int oflags = (O_RDONLY|O_NONBLOCK);
	    if (strncmp(dsf, SG_PATH_PREFIX, SG_PATH_SIZE) != 0) {
		oflags |= O_DIRECT;
	    }
	    if (sgp->debug == True) {
		Printf(opaque, "Opening device %s read-only, open flags = %#o (%#x)...\n",
		       dsf, oflags, oflags);
	    }
	    sgp->fd = open(dsf, oflags);
	}
	if (sgp->fd == INVALID_HANDLE_VALUE) {
	    if (sgp->errlog == True) {
		os_perror(opaque, "open() of %s failed!", dsf);
	    }
	    status = FAILURE;
	}
    }
    if ( (sgp->debug == True) && (status == SUCCESS) ) {
	Printf(opaque, "Device %s successfully opened, fd = %d\n", dsf, sgp->fd);
    }
    return(status);
}

static char *
find_scsi_device(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int bus, target, lun, channel, ids[2];
    char *scsi_device = NULL;
    int oflags = (O_RDONLY|O_NONBLOCK);
    int fd, rc;

    fd = open(sgp->dsf, oflags);
    if (fd == INVALID_HANDLE_VALUE) {
	return(scsi_device);
    }
    rc = get_device_nexus(sgp, sgp->dsf, fd, &bus, &channel, &target, &lun);
    if (rc == SUCCESS) {
	scsi_device = find_sg_device(sgp, bus, channel, target, lun);
	if (scsi_device && sgp->debug == True) {
	    Printf(opaque, "Device '%s', Bus %u, Channel %u, Target %u, LUN %u, SCSI Device: %s\n",
		   sgp->dsf, bus, channel, target, lun, scsi_device);
	}
    }
    (void)close(fd);
    return(scsi_device);
}

static char *
find_sg_device(scsi_generic_t *sgp, int bus, int channel, int target, int lun)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    struct sg_scsi_id scsi_id, *sid = &scsi_id;
    DIR *dir;
    struct dirent *dirent;
    char path[PATH_BUFFER_SIZE];
    char *sg_device = NULL;
    int fd = INVALID_HANDLE_VALUE;
    int oflags = (O_RDONLY|O_NONBLOCK);

    dir = opendir(DEV_PATH);
    if (dir) {
	while ((dirent = readdir(dir)) != NULL) {
	    if ((dirent->d_type != DT_CHR) &&
		(dirent->d_type != DT_LNK)) {
		continue;
	    }
	    if (strncmp(dirent->d_name, "sg", 2)) {
		continue;
	    }
	    snprintf(path, sizeof(path), "%s/%s", DEV_PATH, dirent->d_name);
	    fd = open(path, oflags);
	    if (fd == INVALID_HANDLE_VALUE) {
		continue;
	    }
	    memset(sid, '\0', sizeof(*sid));
	    /* This IOCTL only works on "sg" devices (of course). */
	    if (ioctl(fd, SG_GET_SCSI_ID, sid) == SUCCESS) {
		if ( (sid->host_no == bus) && (sid->channel == channel) &&
		     (sid->scsi_id == target) && (sid->lun == lun) ) {
#if 0
		    if (sgp->debug == True) {
			Printf(opaque, "Host: %u\n", sid->host_no);     /* Bus */
			Printf(opaque, "Channel: %u\n", sid->channel);	/* Channel */
			Printf(opaque, "SCSI ID: %u\n", sid->scsi_id);	/* Target */
			Printf(opaque, "LUN: %u\n", sid->lun);		/* LUN */
			Printf(opaque, "SCSI Type: %u\n", sid->scsi_type);
			Printf(opaque, "Cmds per LUN: %u\n", sid->h_cmd_per_lun);
			Printf(opaque, "Queue Depth: %u\n", sid->d_queue_depth);
		    }
#endif /* 0 */
		    sg_device = strdup(path);
		}
	    }
	    (void)close(fd);
	    if (sg_device) {
		break;
	    }
	}
	closedir(dir);
    }
    return(sg_device);
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
    char *dsf = (sgp->adsf) ? sgp->adsf : sgp->dsf;
    int error;

    if (sgp->debug == True) {
	Printf(opaque, "Closing device %s, fd %d...\n", dsf, sgp->fd);
    }
    if ( (error = close(sgp->fd)) < 0) {
	os_perror(opaque, "close() of %s failed", dsf);
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
    char *dsf = (sgp->adsf) ? sgp->adsf : sgp->dsf;
    int arg = SG_SCSI_RESET_BUS;
    int error;

    if ( (error = ioctl(sgp->fd, SG_SCSI_RESET, &arg)) < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    os_perror(opaque, "SCSI reset bus (SG_SCSI_RESET_BUS) failed on %s!", dsf);
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
    char *dsf = (sgp->adsf) ? sgp->adsf : sgp->dsf;
    int arg = SG_SCSI_RESET_HOST;
    int error;

    if ( (error = ioctl(sgp->fd, SG_SCSI_RESET, &arg)) < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    os_perror(opaque, "SCSI reset controller (SG_SCSI_RESET_HOST) failed on %s!", dsf);
	}
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
/* Added in linux kernel 2.6.27 */
#ifndef SG_SCSI_RESET_TARGET
#  define SG_SCSI_RESET_TARGET 4
#endif

int
os_reset_device(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    char *dsf = (sgp->adsf) ? sgp->adsf : sgp->dsf;
    int arg = SG_SCSI_RESET_TARGET;
    int error;

    if ( (error = ioctl(sgp->fd, SG_SCSI_RESET_TARGET, &arg)) < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    os_perror(opaque, "SCSI reset device (SG_SCSI_RESET_TARGET) failed on %s!", dsf);
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
    char *dsf = (sgp->adsf) ? sgp->adsf : sgp->dsf;
    int arg = SG_SCSI_RESET_DEVICE;
    int error;

    if ( (error = ioctl(sgp->fd, SG_SCSI_RESET, &arg)) < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    os_perror(opaque, "SCSI reset device (SG_SCSI_RESET_DEVICE) failed on %s!", dsf);
	}
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
 * Note of interest at this link: 
 *   https://www.spinics.net/lists/linux-scsi/msg68822.html
 *  
 * ...by default both the bsg and sg drivers "queue at head"
 * when they inject SCSI commands into the block layer.
 * 
 * The bsg driver has a BSG_FLAG_Q_AT_TAIL flag to change
 * that queueing to what may be preferable for your purposes.
 * The sg driver could, but does not, support that flag. 
 *  
 * TODO: Switch to bsg interface, make queue at tail the default, 
 * and allow the user to control head vs. tail via command flag.
 */
 
/*
 * os_spt() - OS Specific SCSI Pass-Through (spt).
 *
 * Description:
 *  This function takes a high level SCSI command, converts it
 * into the format necessary for this OS, then executes it and
 * returns an OS independent format to the caller.
 *
 * Inputs:
 *      sgp = Pointer to the SCSI generic data structure.
 *
 * Return Value:
 *      Returns the status from the SCSI request which is:
 *        0 = Success, -1 = Failure
 */
int
os_spt(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    char *dsf = (sgp->adsf) ? sgp->adsf : sgp->dsf;
    sg_io_hdr_t sgio;
    sg_io_hdr_t *siop = &sgio;
    int error;

    memset(siop, 0, sizeof(*siop));

    siop->interface_id = 'S';
    siop->cmdp         = sgp->cdb;
    siop->cmd_len      = sgp->cdb_size;
    siop->dxferp       = sgp->data_buffer;
    siop->dxfer_len    = sgp->data_length;
    /*
     * Setup the data direction:
     */
    if (sgp->data_dir == scsi_data_none) {
	siop->dxfer_direction = SG_DXFER_NONE;	   /* No data to be transferred.  */
    } else if (sgp->data_dir == scsi_data_read) {
	siop->dxfer_direction = SG_DXFER_FROM_DEV; /* Reading data from device.   */
    } else { /* (sgp->data_dir == scsi_data_write) */
	siop->dxfer_direction = SG_DXFER_TO_DEV;   /* Writing data to the device. */
    }
    siop->sbp       = sgp->sense_data;
    /*
     * Setup (optional) transfer modes, etc.
     */
    if ( (sgp->flags & SG_DIRECTIO) ) {
	siop->flags |= SG_FLAG_DIRECT_IO;     /* Direct I/O vs. indirect I/O. */
    }
    siop->mx_sb_len = sgp->sense_length;
    siop->timeout   = sgp->timeout;    /* Timeout in milliseconds. */

    /*
     * Finally, execute the SCSI command:
     */
    error = ioctl(sgp->fd, SG_IO, siop);

    /*
     * Handle errors, and send pertinent data back to the caller.
     */
    if (error < 0) {
	sgp->os_error = errno;
	if (sgp->errlog == True) {
	    os_perror(opaque, "SCSI request (SG_IO) failed on %s!", dsf);
	}
	sgp->error = True;
	goto error;
    }
    if (siop->status == SCSI_GOOD) {
	sgp->error = False; /* Show SCSI command was successful. */
    } else {
	sgp->error = True;  /* Tell caller we've had some sort of error. */
	if ( (sgp->errlog == True) && (siop->status != SCSI_CHECK_CONDITION) ) {
	    Fprintf(opaque, "%s failed, SCSI status = %d (%s)\n", sgp->cdb_name,
		    siop->status, ScsiStatus(siop->status));
	}
    }
    if ( (siop->status == SCSI_CHECK_CONDITION) && siop->sb_len_wr) {
	sgp->sense_valid = True;
	sgp->sense_resid = (sgp->sense_length - siop->sb_len_wr);
    } else if (siop->host_status || siop->driver_status) {
	/* some error occured */
	sgp->error = True;  /* Tell caller we've had some sort of error. */
    }
    sgp->data_resid    = siop->resid;
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
    sgp->scsi_status   = siop->status;
    sgp->duration      = siop->duration;
    sgp->host_status   = siop->host_status;
    sgp->driver_status = siop->driver_status;
error:
    if (sgp->debug == True) {
	DumpScsiCmd(sgp, siop);
    }
    return(error);
}

/*
 * Defines snarf'ed from src/linux/drivers/scsi/scsi.h since
 * they are not (currently) exported to user space :-)
 * [ this is NOT good coding practice, so this may go! ]
 */
#define DID_OK          0x00 /* NO error                                */
#define DID_NO_CONNECT  0x01 /* Couldn't connect before timeout period  */
#define DID_BUS_BUSY    0x02 /* BUS stayed busy through time out period */
#define DID_TIME_OUT    0x03 /* TIMED OUT for other reason              */
#define DID_BAD_TARGET  0x04 /* BAD target.                             */
#define DID_ABORT       0x05 /* Told to abort for some other reason     */
#define DID_PARITY      0x06 /* Parity error                            */
#define DID_ERROR       0x07 /* Internal error                          */
#define DID_RESET       0x08 /* Reset by somebody.                      */
#define DID_BAD_INTR    0x09 /* Got an interrupt we weren't expecting.  */ 
#define DID_PASSTHROUGH 0x0a /* Force command past mid-layer            */
#define DID_SOFT_ERROR  0x0b /* The low level driver just wish a retry  */
#define DID_IMM_RETRY   0x0c /* Retry without decrementing retry count  */
#define DID_REQUEUE     0x0d /* Requeue command (no immediate retry) also */
			     /*   without decrementing the retry count.	*/
/*
 * multipath-tools add these status codes.
 * URL: http://christophe.varoqui.free.fr/
 */
#define DID_TRANSPORT_DISRUPTED 0x0e /* Transport error disrupted execution
				      * and the driver blocked the port to
				      * recover the link. Transport class will
				      * retry or fail IO */
#define DID_TRANSPORT_FAILFAST	0x0f /* Transport class fastfailed the io */

/* 
 * More status codes:
 * URL: http://lxr.free-electrons.com/source/include/scsi/scsi.h#L450
 */ 
#define DID_TARGET_FAILURE 0x10 /* Permanent target failure, do not retry on
				 * other paths */
#define DID_NEXUS_FAILURE 0x11 /* Permanent nexus failure, retry on other
				* paths might yield different results */
#define DID_ALLOC_FAILURE 0x12 /* Space allocation on the device failed */
#define DID_MEDIUM_ERROR  0x13 /* Medium error */

/*
 * Host (DID) Status Code Table.
 */
struct HostStatusTable {
    unsigned short host_status;
    char           *host_msg;
} host_StatusTable[] = {
    {   DID_OK,                 "DID_OK"
	/* 0x00 = "NO error" */					},
    {   DID_NO_CONNECT,         "DID_NO_CONNECT"
	/* 0x01 = "Couldn't connect before timeout period" */	},
    {   DID_BUS_BUSY,           "DID_BUS_BUSY"
	/* 0x02 = "BUS stayed busy through time out period" */	},
    {   DID_TIME_OUT,           "DID_TIME_OUT"
	/* 0x03 = "TIMED OUT for other reason" */		},
    {   DID_BAD_TARGET,         "DID_BAD_TARGET"
	/* 0x04 = "BAD target" */				},
    {   DID_ABORT,              "DID_ABORT"
	/* 0x05 = "Told to abort for some other reason" */	},
    {   DID_PARITY,             "DID_PARITY"
	/* 0x06 = "Parity error" */				},
    {   DID_ERROR,              "DID_ERROR"
	/* 0x07 = "Internal error" */				},
    {   DID_RESET,              "DID_RESET"
	/* 0x08  = "Reset by somebody" */			},
    {   DID_BAD_INTR,           "DID_BAD_INTR"
	/* 0x09 = "Got an interrupt we weren't expecting" */	},
    {   DID_PASSTHROUGH,        "DID_PASSTHROUGH"
	/* 0x0a = "Force command past mid-layer" */		},
    {   DID_SOFT_ERROR,         "DID_SOFT_ERROR"
	/* 0x0b = "Low level driver just wishs a retry" */	},
    {   DID_IMM_RETRY,		"DID_IMM_RETRY"
	/* 0x0c = Retry without decrementing retry count */	},
    {   DID_REQUEUE,		"DID_REQUEUE"
        /* 0x0d = Requeue command (no immediate retry) also
                  without decrementing the retry count */	},
    {	DID_TRANSPORT_DISRUPTED,"DID_TRANSPORT_DISRUPTED"
        /* 0x0e - Transport error disrupted execution
	 * and the driver blocked the port to
	 * recover the link. Transport class will
	 * retry or fail IO */					},
    {	DID_TRANSPORT_FAILFAST,	"DID_TRANSPORT_FAILFAST"
	/* Transport class fastfailed the io */			},
    {	DID_TARGET_FAILURE, "DID_TARGET_FAILURE",
	/* Permanent target failure, do not retry on other paths */ },
    {	DID_NEXUS_FAILURE, "DID_NEXUS_FAILURE"
	/* Permanent nexus failure, retry on other
	 * paths might yield different results */		},
    {	DID_ALLOC_FAILURE, "DID_ALLOC_FAILURE"
	/* Space allocation on the device failed */		},
    {	DID_MEDIUM_ERROR, "DID_MEDIUM_ERROR"
	/* Medium error */					}
};
static int host_StatusEntrys =
    sizeof(host_StatusTable) / sizeof(host_StatusTable[0]);

static char *
linux_HostStatus(unsigned short host_status)
{
    struct HostStatusTable *hst = host_StatusTable;
    int entrys;

    for (entrys = 0; entrys < host_StatusEntrys; hst++, entrys++) {
	if (hst->host_status == host_status) {
	    return(hst->host_msg);
	}
    }
    return("???");
}

#define DRIVER_OK	    0x00 
#define DRIVER_BUSY         0x01
#define DRIVER_SOFT         0x02
#define DRIVER_MEDIA        0x03
#define DRIVER_ERROR        0x04    

#define DRIVER_INVALID      0x05
#define DRIVER_TIMEOUT      0x06
#define DRIVER_HARD         0x07
#define DRIVER_SENSE        0x08

#define DRIVER_MASK         0x0f
#define SUGGEST_MASK        0xf0

/*
 * Driver Status Code Table.
 */
struct DriverStatusTable {
    unsigned short  driver_status;
    char            *driver_msg;
} driver_StatusTable[] = {
    {   DRIVER_OK,      "DRIVER_OK"	}, 
    {   DRIVER_BUSY,    "DRIVER_BUSY"	},
    {   DRIVER_SOFT,    "DRIVER_SOFT"	},
    {   DRIVER_MEDIA,   "DRIVER_MEDIA"	},
    {   DRIVER_ERROR,   "DRIVER_ERROR"	},
    {   DRIVER_INVALID, "DRIVER_INVALID"},
    {   DRIVER_TIMEOUT, "DRIVER_TIMEOUT"},
    {   DRIVER_HARD,    "DRIVER_HARD"	},
    {   DRIVER_SENSE,   "DRIVER_SENSE"	}
};
static int driver_StatusEntrys =
    sizeof(driver_StatusTable) / sizeof(driver_StatusTable[0]);

static char *
linux_DriverStatus (unsigned short driver_status)
{
    struct DriverStatusTable *dst = driver_StatusTable;
    int entrys;

    for (entrys = 0; entrys < driver_StatusEntrys; dst++, entrys++) {
	if (dst->driver_status == driver_status) {
	    return(dst->driver_msg);
	}
    }
    return("???");
}

#define SUGGEST_RETRY       0x10
#define SUGGEST_ABORT       0x20 
#define SUGGEST_REMAP       0x30
#define SUGGEST_DIE         0x40
#define SUGGEST_SENSE       0x80
#define SUGGEST_IS_OK       0xff

/*
 * Suggest Status Code Table.
 */
struct SuggestStatusTable {
    uint8_t suggest_status;
    char *suggest_msg;
} suggest_StatusTable[] = {
    {   SUGGEST_RETRY,  "SUGGEST_RETRY"	},
    {   SUGGEST_ABORT,  "SUGGEST_ABORT"	},
    {   SUGGEST_REMAP,  "SUGGEST_REMAP"	},
    {   SUGGEST_DIE,    "SUGGEST_DIE"	},
    {   SUGGEST_SENSE,  "SUGGEST_SENSE"	}
};
static int suggest_StatusEntrys =
    sizeof(suggest_StatusTable) / sizeof(suggest_StatusTable[0]);

static char *
linux_SuggestStatus(unsigned short suggest_status)
{
    struct SuggestStatusTable *sst = suggest_StatusTable;
    int entrys;

    for (entrys = 0; entrys < suggest_StatusEntrys; sst++, entrys++) {
	if (sst->suggest_status == suggest_status) {
	    return(sst->suggest_msg);
	}
    }
    return("???");
}

static void
DumpScsiCmd(scsi_generic_t *sgp, sg_io_hdr_t *siop)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int i;
    char buf[128];
    char *bp = buf;
    char *msgp = NULL;
    char *dsf = (sgp->adsf) ? sgp->adsf : sgp->dsf;

    Printf(opaque, "SCSI I/O Structure:\n");

    Printf(opaque, "    Device Special File .............................: %s\n", dsf);
    Printf(opaque, "    File Descriptor .............................. fd: %d\n", sgp->fd);
    switch (siop->dxfer_direction) {
	case SG_DXFER_NONE:
	    msgp = "SG_DXFER_NONE";
	    break;
	case SG_DXFER_TO_DEV:
	    msgp = "SG_DXFER_TO_DEV";
	    break;
	case SG_DXFER_FROM_DEV:
	    msgp = "SG_DXFER_FROM_DEV";
	    break;
    }
    Printf(opaque, "    Data Direction .................. dxfer_direction: %d (%s)\n",
	   siop->dxfer_direction, msgp);
    Printf(opaque, "    Control Flags ............................. flags: %#x\n", siop->flags);
    Printf(opaque, "    SCSI CDB Status .......................... status: %#x (%s)\n",
	   siop->status, ScsiStatus(siop->status));
    Printf(opaque, "    SCSI Masked Status ................ masked_status: %#x\n", siop->masked_status);
    Printf(opaque, "    Command Timeout ......................... timeout: %u ms (%u seconds)\n",
	   siop->timeout, (siop->timeout / MSECS));
    if (!siop->cmd_len) bp = '\0';
    for (i = 0; (i < siop->cmd_len); i++) {
	bp += sprintf(bp, "%x ", siop->cmdp[i]);
    }
    Printf(opaque, "    Command Descriptor Block .................... cdb: %s(%s)\n", buf, sgp->cdb_name);
    Printf(opaque, "    CDB Length .............................. cmd_len: %u\n", siop->cmd_len);
    Printf(opaque, "    I/O Buffer Address ....................... dxferp: %#lx\n", siop->dxferp);
    Printf(opaque, "    I/O Buffer Length ..................... dxfer_len: %d (%#x)\n",
	   siop->dxfer_len, siop->dxfer_len);
    Printf(opaque, "    I/O Data Residual ......................... resid: %d (%#x)\n",
	   siop->resid, siop->resid);
    Printf(opaque, "    Request Sense Buffer ........................ sbp: %#lx\n", siop->sbp);
    Printf(opaque, "    Request Sense Length .................. mx_sb_len: %u (%#x)\n",
	   siop->mx_sb_len, siop->mx_sb_len);
    Printf(opaque, "    Request Sense Returned ................ sb_len_wr: %u (%#x)\n",
	   siop->sb_len_wr, siop->sb_len_wr);
    Printf(opaque, "    Host Status ......................... host_status: %#x (%s)\n",
	   siop->host_status, linux_HostStatus(siop->host_status));
#if 1
    Printf(opaque, "    Driver Status ..................... driver_status: %#x (%s)\n",
	   siop->driver_status,
	   linux_DriverStatus(siop->driver_status & DRIVER_MASK) );
#else /* Obsolete */
    Printf(opaque, "    Driver Status ..................... driver_status: %#x (%s, %s)\n",
	   siop->driver_status,
	   linux_SuggestStatus(siop->driver_status & SUGGEST_MASK),
	   linux_DriverStatus(siop->driver_status & DRIVER_MASK) );
#endif
    Printf(opaque, "    Messaging Level Data (optional) ...... msg_status: %u\n", siop->msg_status);
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
    return( linux_HostStatus((unsigned short)sgp->host_status) );
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
    return( linux_DriverStatus((unsigned short)sgp->driver_status) );
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
    
    /*
     * The MPIO driver is returning EAGAIN when the current path has
     * disappeared. Furthermore, due to a driver bug, once must do I/O
     * to force a path change, thus the read below.
     */ 
    if ( (sgp->os_error == EAGAIN)		      ||
	 (sgp->host_status == DID_TRANSPORT_FAILFAST) ||
	 (sgp->host_status == DID_TRANSPORT_DISRUPTED) ) {
	if (sgp->debug == True) {
	    if (sgp->os_error == EAGAIN) {
		Printf(opaque, "DEBUG: EAGAIN detected on %s...\n", sgp->cdb_name);
	    } else if (sgp->host_status == DID_TRANSPORT_FAILFAST) {
		Printf(opaque, "DEBUG: DID_TRANSPORT_FAILFAST detected on %s...\n", sgp->cdb_name);
	    } else if (sgp->host_status == DID_TRANSPORT_DISRUPTED) {
		Printf(opaque, "DEBUG: DID_TRANSPORT_DISRUPTED detected on %s...\n", sgp->cdb_name);
	    }
	}
	(void)force_path_failover(sgp);
	is_retriable = True;
    }
    return (is_retriable);
}

/*
 * force_path_failover() - Force DMMP (MPIO) Path Failover. 
 *  
 * Description: 
 *      Since the Linux I/O stack does *not* do failovers with SCSI pass-through API,
 * we must do a normal read through the disk driver to force path failovers. 
 *  
 */
static int
force_path_failover(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    unsigned char *buffer;
    size_t bytes = 4096; /* Handles 512 or 4k disk sector sizes. */
    ssize_t bytes_read = 0;
    HANDLE fd = sgp->fd;
    hbool_t close_device = False;

    if (sgp->adsf && (strncmp(sgp->dsf, SG_PATH_PREFIX, SG_PATH_SIZE) != 0) ) {
	int oflags = (O_RDONLY|O_NONBLOCK|O_DIRECT);
	fd = open(sgp->dsf, oflags);
	if (fd == INVALID_HANDLE_VALUE) {
	    if (sgp->errlog == True) {
		os_perror(opaque, "force_path_failover(), open() of %s failed!", sgp->dsf);
	    }
	    return(FAILURE);
	}
	close_device = True;
    }
    buffer = malloc_palign(opaque, bytes, 0);
    if (buffer == NULL) return (FAILURE);
    if (sgp->debug) {
	Printf(opaque, "DEBUG: Reading %u bytes at lba 0 to force path failover...\n", bytes);
    }
    bytes_read = read(fd, buffer, bytes);
    /* Note: Multiple read()'s may be necessary to overcome EAGAIN! */
    if ( (bytes_read == FAILURE) && sgp->debug) {
	os_perror(opaque, "force_path_failover(), read() failed");
    }
    free_palign(opaque, buffer);
    if (close_device) {
	(void)close(fd);
    }
    return(SUCCESS);
}

/* ============================================================================================= */
/*
 * Functions for Managing the SCSI Device Table:
 *  
 * Note: This is unique to Linux, since most OS's have one device path. 
 * IOW, this list of directories are *not* required for say Windows. 
 */
static scsi_dir_path_t scsi_dir_paths[] = {
    {	DEV_PATH,		"sd",	"Linux Device",	True	},
    {	DEV_PATH,		"sg",	"SCSI Device",	True	},
    {	DEV_PATH,		"dm",	"DMMP Device",	False	},
    {	DMMP_PATH_PREFIX,	NULL,	"DMMP Device",	True	},
    {	NULL,		NULL		}
};

char *
os_get_device_path_type(scsi_device_name_t *sdnp)
{
    char path[PATH_BUFFER_SIZE];
    scsi_dir_path_t *sdp;

    for (sdp = scsi_dir_paths; sdp->sdp_dir_path; sdp++) {
	if (sdp->sdp_dev_name) {
	    snprintf(path, sizeof(path), "%s/%s", sdp->sdp_dir_path, sdp->sdp_dev_name);
	} else {
	    snprintf(path, sizeof(path), "%s", sdp->sdp_dir_path);
	}
	if (strncmp(path, sdnp->sdn_device_path, strlen(path)) == 0) {
	    return( sdp->sdp_dev_path_type );
	}
    }
    return(NULL);
}

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
get_device_nexus(scsi_generic_t *sgp, char *devpath, int fd, int *bus, int *channel, int *target, int *lun)
{
    void *opaque = sgp->tsp->opaque;
    char syspath[LARGE_BUFFER_SIZE];
    char symlink[LARGE_BUFFER_SIZE];
    ssize_t linksize = 0;
    char *devname = NULL;
    int status = SUCCESS;

    /* Note: /dev/mapper names are symlinks to ../dmNN device names. */
    /* But that said, DM-MP entries do NOT have a "device" synlink! */
    devname = strrchr(devpath, '/');
    if (devname) {
	devname++;
    } else {
	devname = devpath;
    }
    (void)sprintf(syspath, "/sys/block/%s/device", devname);
    linksize = readlink(syspath, symlink, sizeof(symlink));
    if (linksize) {
	char *nexus = NULL;
	symlink[linksize] = '\0';
	nexus = strrchr(symlink, '/');
	if (nexus) {
	    int cvt_count;
	    nexus++;
	    //Printf(opaque, "%s Symlink: %s\n", devname, p);
	    /* Expected Format: 0:0:11:0 <- H:C:T:L */
	    cvt_count = sscanf(nexus, "%d:%d:%d:%d", bus, channel, target, lun);
	    if (cvt_count) {
		if (sgp->debug) {
		    Printf(opaque, "%s nexus: [%d:%d:%d:%d]\n", syspath, *bus, *channel, *target, *lun);
		}
		return(status);
	    } else if (sgp->debug) {
		Printf(opaque, "%s failed converting %s, count = %d\n", devname, nexus, cvt_count);
	    }
	    /* Fall through to try older IOCTL method. */
	}
    }
    /* Note: This works for all device names, including DM-MP. */
    /*       Why a new IOCTL was *not* introduced, is a mystery! */
    if (ioctl(fd, SCSI_IOCTL_GET_BUS_NUMBER, bus) == SUCCESS) {
	int ids[2];
	/* Beware: This older method truncates large values! */
	if (ioctl(fd, SCSI_IOCTL_GET_IDLUN, &ids) == SUCCESS) {
	    *target = (ids[0] & 0xff);
	    *lun = ((ids[0] >> 8) & 0xff);
	    *channel = ((ids[0] >> 16) & 0xff);
	} else {
	    Perror(opaque, "SCSI_IOCTL_GET_BUS_NUMBER failed on device %s", devpath);
	    status = FAILURE;
	}
    } else {
	if (sgp->debug) {
	    Perror(opaque, "SCSI_IOCTL_GET_BUS_NUMBER failed on device %s", devpath);
	}
	status = FAILURE;
    }
    return(status);
}

static int
find_scsi_devices(scsi_generic_t *sgp, char *devpath, char *scsi_name, scsi_filters_t *sfp)
{
    void *opaque = sgp->tsp->opaque;
    int bus, target, lun, channel, ids[2];
    char *scsi_device = NULL;
    char *serial = NULL, *device_id = NULL, *target_port = NULL;
    char *fw_version = NULL;
    inquiry_t *inquiry = NULL;
    inquiry_page_t inquiry_data;  
    inquiry_page_t *inquiry_page = &inquiry_data;
    scsi_device_entry_t *sdep = NULL;
    struct sg_scsi_id scsi_id, *sid = &scsi_id;
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
	    if ( (dirent->d_type != DT_CHR) &&
		 (dirent->d_type != DT_LNK) &&
		 (dirent->d_type != DT_BLK) ) {
		continue;
	    }
	    if (scsi_name) {
		if (strncmp(dirent->d_name, scsi_name, strlen(scsi_name))) {
		    continue;
		}
	    }
	    /* Omit device names with partition postfix. */
	    if (omit_device_name(sgp, dirent->d_name)) {
		if (sgp->debug == True) {
		    Printf(opaque, "Omitting device %s...\n", dirent->d_name);
		}
		continue;
	    }
	    snprintf(path, sizeof(path), "%s/%s", devpath, dirent->d_name);
#if 0
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
#endif /* 0 */
	    fd = open(path, oflags);
	    if (fd == INVALID_HANDLE_VALUE) {
		if ( (errno == ENODEV) || (errno == ENXIO) || (errno == ENOENT) ) {
		    ;
		} else { /* Control w/debug? */
		    Perror(opaque, "Failed to open device %s", path);
		}
		goto close_and_continue;
	    }
	    if (strncmp(dirent->d_name, "sg", 2) == 0) {
		if (ioctl(fd, SG_GET_SCSI_ID, sid) == SUCCESS) {
		    bus = sid->host_no;
		    channel = sid->channel;
		    target = sid->scsi_id;
		    lun = sid->lun;
		} else {
		    Perror(opaque, "SG_GET_SCSI_ID failed on device %s", path);
		    goto close_and_continue;
		}
	    } else {
		int rc = get_device_nexus(sgp, path, fd, &bus, &channel, &target, &lun);
		if (rc == FAILURE) {
		    goto close_and_continue;
		}
	    }
	    if (sgp->debug == True) {
		Printf(opaque, "Device %s, Bus %u, Channel %u, Target %u, LUN %u\n",
		       path, bus, channel, target, lun);
	    }
#define SinglePathDevices 1
#if SinglePathDevices
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
#endif /* SinglePathDevices */
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
	/* Note: Directory, such as /dev/mapper, may *not* exist! */
	//status = FAILURE;
    }
    if (inquiry) Free(opaque, inquiry);
    if (serial) Free(opaque, serial);
    if (device_id) Free(opaque, device_id);
    if (target_port) Free(opaque, target_port);
    if (fw_version) Free(opaque, fw_version);
    return(status);
}

static hbool_t
omit_device_name(scsi_generic_t *sgp, char *name)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;

    /* Omit device names with partition postfix. (ugly) */
    /* Multipath devices: (don't you love it? NOT!)
     *  /dev/mapper/mpatha1 with "user_friendly_names yes"
     *  or /dev/mapper/35000cca251029300-part2
     *  or /dev/mapper/35000cca251029300p1 without user friendly.
     *  or /dev/mapper/control for the MPIO control device.
     *  or /dev/mapper/*-{home,root,swap} (maybe more?)
     */ 
    if (strncmp(name, "control", 7) == 0) {
	return(True);
    }
    if ( strstr(name, "-home") || strstr(name, "-root") || strstr(name, "-swap") ) {
	return(True);
    }
    if ( isxdigit(name[0]) ) {
	if ( strstr(name, "-part") ||
	     (name[strlen(name) - 2] == 'p') ) {
	    return(True);
	}
    }
    if ( (strncmp(name, "sd", 2) == 0) ||
	 (strncmp(name, "mpath", 5) == 0) ) {
	if (!isalpha(name[strlen(name) - 1])) {
	    return(True);
	}
    }
    return(False);
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
	scsi_device_name_t *sdnp;
	sdnp = update_device_entry(sgp, sdep, path, inquiry, serial, device_id,
				   target_port, bus, channel, target, lun);
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

/* 
 * Find device name (sd) by SCSI nexus (bus/channel/target/lun).
 */
static scsi_device_name_t *
find_device_by_nexus(scsi_generic_t *sgp, scsi_device_entry_t *sdep,
		     char *path, int bus, int channel, int target, int lun)
{
    scsi_device_name_t *sdnh = &sdep->sde_names;
    scsi_device_name_t *sdnp;

    for (sdnp = sdnh->sdn_flink; (sdnp != sdnh); sdnp = sdnp->sdn_flink) {
	/* Note: We do not wish to match SCSI generic (sg) devices. */
	if (strncmp(sdnp->sdn_device_path, SG_PATH_PREFIX, SG_PATH_SIZE) == 0) {
	    continue;
	}
	if ( (sdnp->sdn_bus == bus) && (sdnp->sdn_channel == channel) &&
	     (sdnp->sdn_target == target) && (sdnp->sdn_lun == lun) ) {
	    return( sdnp );
	}
    }
    return(NULL);
}

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
