#ifndef SCSILIB_H
#define SCSILIB_H
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
 * Module:	scsilib.h
 * Author:	Robin T. Miller
 * Date:	March 28th, 2005
 *
 * Description:
 *	This file contains the OS SCSI specific function prototypes.
 *
 * Modification History:
 * 
 * February 12th, 2018 by Robin T. Miller
 *      Add os_find_scsi_devices() and filters to find desired devices.
 */
extern int os_open_device(scsi_generic_t *sgp);
extern int os_close_device(scsi_generic_t *sgp);
extern int os_abort_task_set(scsi_generic_t *sgp);
extern int os_clear_task_set(scsi_generic_t *sgp);
extern int os_cold_target_reset(scsi_generic_t *sgp);
extern int os_warm_target_reset(scsi_generic_t *sgp);
extern int os_reset_bus(scsi_generic_t *sgp);
extern int os_reset_ctlr(scsi_generic_t *sgp);
extern int os_reset_device(scsi_generic_t *sgp);
extern int os_reset_lun(scsi_generic_t *sgp);
extern int os_scan(scsi_generic_t *sgp);
extern int os_resumeio(scsi_generic_t *sgp);
extern int os_suspendio(scsi_generic_t *sgp);
extern int os_get_timeout(scsi_generic_t *sgp, unsigned int *timeout);
extern int os_set_timeout(scsi_generic_t *sgp, unsigned timeout);
extern int os_get_qdepth(scsi_generic_t *sgp, unsigned int *qdepth);
extern int os_set_qdepth(scsi_generic_t *sgp, unsigned int qdepth);
extern int os_spt(scsi_generic_t *sgp);
#if defined(_AIX)
extern int os_spta(scsi_generic_t *sgp);
#endif /* defined(_AIX) */
extern hbool_t os_is_retriable(scsi_generic_t *sgp);
extern char *os_host_status_msg(scsi_generic_t *sgp);
extern char *os_driver_status_msg(scsi_generic_t *sgp);

/* SCSI Filters: */
typedef struct scsi_filters {
    hbool_t	all_device_paths;	/* Include all device paths.	*/
    char	*device_paths;		/* The device paths.		*/
    char	*exclude_paths;		/* The exclude devices.		*/
    uint8_t	*device_types;		/* The device type(s).		*/
    char	*product;		/* The product name.		*/
    char	*vendor;		/* The vendor name.		*/
    char	*revision;		/* The revision level.		*/
    char	*device_id;		/* The LUN device ID.		*/
    char	*serial;		/* The serial number.		*/
    char	*target_port;		/* The target port.		*/
    char	*fw_version;		/* The firmware version.	*/
} scsi_filters_t;

extern hbool_t match_device_paths(char *device_path, char *paths);
extern int os_find_scsi_devices(scsi_generic_t *sgp, scsi_filters_t *sfp, char *paths);

#endif /* SCSILIB_H */
