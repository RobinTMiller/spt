#if !defined(SPT_DEVICES_H)
#define SPT_DEVICES_H 1
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
 * Module:	spt_devices.h
 * Author:	Robin T. Miller
 * Date:	February 8th, 2018
 *
 * Description:
 *	This file contains SCSI device table definitions.
 *
 * Modification History:
 * 
 */ 

/*
 * SCSI Device Names Associated with a SCSI Nexus:
 */
typedef struct scsi_device_name {
    struct scsi_device_name *sdn_flink;
    struct scsi_device_name *sdn_blink;
    char *sdn_device_path;		/* The /dev path name.	    */
    char *sdn_scsi_path;        	/* The SCSI device path.    */
    char *sdn_target_port;          	/* The device target port.  */
    int	sdn_bus;			/* The SCSI bus.	    */
    int sdn_channel;			/* The SCSI channel.	    */
    int sdn_target;			/* The SCSI target ID.	    */
    int sdn_lun;			/* The SCSI LUN.	    */
} scsi_device_name_t;

/*
 * SCSI Device Entry for each SCSI Nexus:
 */
typedef struct scsi_device_entry {
    struct scsi_device_entry *sde_flink;
    struct scsi_device_entry *sde_blink;
    scsi_device_name_t sde_names;	/* List of SCSI names.		*/
    uint8_t	sde_device_type;	/* Inquiry device type.		*/
    char	*sde_product;		/* Inquiry product name.	*/
    char	*sde_vendor;		/* Inquiry vendor name.		*/
    char	*sde_revision;		/* The revision level.		*/
    char	*sde_serial;		/* Device serial number.	*/
    char	*sde_device_id;		/* The LUN device ID (WWID).	*/
    char	*sde_target_port;	/* The target port address.	*/
    char	*sde_fw_version;	/* The firmware version.	*/
} scsi_device_entry_t;

/*
 * Each OS SCSI library will present this table, if supported.
 */
extern scsi_device_entry_t scsiDeviceTable;

typedef struct scsi_dir_path {
    char	*sdp_dir_path;		/* The SCSI directory.		*/
    char	*sdp_dev_name;		/* The SCSI device name.	*/
    char	*sdp_dev_path_type;	/* The device path type.	*/
} scsi_dir_path_t;

extern char *os_get_device_path_type(scsi_device_name_t *sdnp);

#endif /* !defined(SPT_DEVICES_H) */
