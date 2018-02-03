/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2017			    *
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
 * Module:	spt_scsi.c
 * Author:	Robin T. Miller
 * Date:	October 31th, 2013
 *
 * Description:
 *	This module provides functions to query SCSI information.
 * 
 * Modification History:
 * 
 * October 3rd, 2014 by Robin T. Miller
 * 	Modified SCSI generic opaque pointer to tool_specific information,
 * so we can pass more tool specific information (e.g. dt versus spt).
 * 
 * October 31th, 2013 by Robin T. Miller
 * 	Creating this file and import functions from dt tool.
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

#if defined(_WIN32)
#  include "spt_win.h"
#else /* !defined(_WIN32) */
#  include "spt_unix.h"
#endif /* defined(_WIN32) */

/*
 * Forward Reference:
 */ 
void strip_trailing_spaces(char *bp);
scsi_generic_t *init_sg_info(scsi_device_t *sdp, tool_specific_t *tsp);
void SetupIdentification(scsi_device_t *sdp, io_params_t *iop);

void
strip_trailing_spaces(char *bp)
{
    size_t i = strlen(bp);

    while (i) {
	if (bp[--i] == ' ') {
	    bp[i] = '\0';
	    continue;
	}
	break;
    }
    return;
}

void
clone_scsi_information(io_params_t *iop, io_params_t *ciop)
{
    scsi_information_t *sip = iop->sip;
    scsi_information_t *csip;
    
    if (sip == NULL) return;
    ciop->sip = csip = Malloc(NULL, sizeof(*sip) );
    if (csip == NULL) return;
    *csip = *sip;

    if (sip->si_inquiry) {
	csip->si_inquiry = NULL;
    }
    if (sip->si_vendor_id) {
	csip->si_vendor_id = strdup(sip->si_vendor_id);
    }
    if (sip->si_product_id) {
	csip->si_product_id = strdup(sip->si_product_id);
    }
    if (sip->si_revision_level) {
	csip->si_revision_level = strdup(sip->si_revision_level);
    }
    if (sip->si_device_id) {
	csip->si_device_id = strdup(sip->si_device_id);
    }
    if (sip->si_serial_number) {
	csip->si_serial_number = strdup(sip->si_serial_number);
    }
    return;
}

void
free_scsi_information(io_params_t *iop)
{
    scsi_information_t *sip = iop->sip;

    if (sip == NULL) return;
#if 0
    if (sip->si_sgp) {
	scsi_generic_t *sgp = sip->si_sgp;
	if (sgp->fd != INVALID_HANDLE_VALUE) {
	    (void)os_close_device(sgp);
	}
	if (sgp->sense_data) {
	    free_palign(NULL, sgp->sense_data);
	    sgp->sense_data = NULL;
	}
	free(sip->si_sgp);
	sip->si_sgp = NULL;
    }
#endif /* 0 */
    if (sip->si_inquiry) {
	free(sip->si_inquiry);
	sip->si_inquiry = NULL;
    }
    if (sip->si_vendor_id) {
	free(sip->si_vendor_id);
	sip->si_vendor_id = NULL;
    }
    if (sip->si_product_id) {
	free(sip->si_product_id);
	sip->si_product_id = NULL;
    }
    if (sip->si_revision_level) {
	free(sip->si_revision_level);
	sip->si_revision_level = NULL;
    }
    if (sip->si_device_id) {
	free(sip->si_device_id);
	sip->si_device_id = NULL;
    }
    if (sip->si_serial_number) {
	free(sip->si_serial_number);
	sip->si_serial_number = NULL;
    }
    free(sip);
    iop->sip = NULL;
    return;
}

scsi_generic_t *
init_sg_info(scsi_device_t *sdp, tool_specific_t *tsp)
{
    scsi_generic_t *sgp;

    sgp = init_scsi_generic(tsp);
    if (sgp == NULL) return(sgp);

    //sgp->opaque = sdp;
    /*
     * Log errors as warnings (for now).
     */
    sgp->warn_on_error = True;
    return(sgp);
}

int
get_scsi_information(scsi_device_t *sdp, io_params_t *iop)
{
    scsi_generic_t	*sgp, *dsgp;
    scsi_information_t	*sip;
    hbool_t		opened_device;
    int			status = SUCCESS;
    
    sgp = init_sg_info(sdp, &iop->tool_specific);
    if (sgp == NULL) return(FAILURE);
    free_palign(sdp, sgp->sense_data);

    dsgp = &iop->sg;	/* Don't overwrite current sg info! */
    *sgp = *dsgp;

    sgp->warn_on_error = True;
    opened_device = False;
    if (sgp->fd == INVALID_HANDLE_VALUE) {
	status = os_open_device(sgp);
	if (status) return(status);
	opened_device = True;
    }

    iop->sip = sip = Malloc(sdp, sizeof(*sip) );
    if (sip == NULL) return(FAILURE);

    status = get_standard_scsi_information(sdp, iop, sgp);

    if (opened_device == True) {
	(void)os_close_device(sgp);
    }
    free(sgp);
    return(status);
}

int
get_standard_scsi_information(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp)
{
    scsi_information_t	*sip = iop->sip;
    inquiry_t		*inquiry;
    int			status = SUCCESS;


    status = get_inquiry_information(sdp, iop, sgp);
    if (status == FAILURE) {
	return(status);
    }
#if 0
    if (sip == NULL) {
	iop->sip = sip = Malloc(sdp, sizeof(*sip) );
	if (sip == NULL) return(FAILURE);
    }
    if ( (inquiry = sip->si_inquiry) == NULL) {
	sip->si_inquiry = inquiry = Malloc(sdp, sizeof(*inquiry) );
	if (sip->si_inquiry == NULL) return(FAILURE);
    }

    status = Inquiry(sgp->fd, sgp->dsf, sgp->debug, sgp->errlog,
		     NULL, &sgp, inquiry, sizeof(*inquiry), 0, 0, sgp->timeout, sgp->tsp);
    if (status) return(status);

    sip->si_vendor_id = Malloc(sdp, INQ_VID_LEN + 1 );
    sip->si_product_id = Malloc(sdp, INQ_PID_LEN + 1 );
    sip->si_revision_level = Malloc(sdp, INQ_REV_LEN + 1 );
    (void)strncpy(sip->si_vendor_id, (char *)inquiry->inq_vid, INQ_VID_LEN);
    (void)strncpy(sip->si_product_id, (char *)inquiry->inq_pid, INQ_PID_LEN);
    (void)strncpy(sip->si_revision_level, (char *)inquiry->inq_revlevel, INQ_REV_LEN);
    /* Remove trailing spaces. */
    strip_trailing_spaces(sip->si_vendor_id);
    strip_trailing_spaces(sip->si_product_id);
    strip_trailing_spaces(sip->si_revision_level);
#endif /* 0 */

    inquiry = sip->si_inquiry;
    if ( (inquiry->inq_dtype == DTYPE_DIRECT) || (inquiry->inq_dtype == DTYPE_RAID) ||
	 (inquiry->inq_dtype == DTYPE_MULTIMEDIA) || (inquiry->inq_dtype == DTYPE_OPTICAL) ||
	 (inquiry->inq_dtype == DTYPE_WORM) ) {
	status = GetCapacity(sdp, iop);
    }

    sip->si_idt = IDT_BOTHIDS;

    if (sip->si_idt == IDT_DEVICEID) {
	sip->si_device_id  = GetDeviceIdentifier(sgp->fd, sgp->dsf, sgp->debug, sgp->errlog,
						 NULL, &sgp, inquiry, sgp->timeout, sgp->tsp);
    } else if (sip->si_idt == IDT_SERIALID) {
	sip->si_serial_number = GetSerialNumber(sgp->fd, sgp->dsf, sgp->debug, sgp->errlog,
						NULL, &sgp, inquiry, sgp->timeout, sgp->tsp);
    } else if (sip->si_idt == IDT_BOTHIDS) {
	sip->si_device_id  = GetDeviceIdentifier(sgp->fd, sgp->dsf, sgp->debug, sgp->errlog,
						 NULL, &sgp, inquiry, sgp->timeout, sgp->tsp);
	sip->si_serial_number = GetSerialNumber(sgp->fd, sgp->dsf, sgp->debug, sgp->errlog,
						NULL, &sgp, inquiry, sgp->timeout, sgp->tsp);
    }
    return(status);
}

int
get_inquiry_information(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *csgp)
{
    scsi_generic_t	*sgp = NULL;
    scsi_information_t	*sip = iop->sip;
    inquiry_t		*inquiry = NULL;
    int			status = SUCCESS;

    sgp = Malloc(sdp, sizeof(*sgp));
    if (sgp == NULL) return(FAILURE);
    *sgp = *csgp;
    if (sip == NULL) {
	iop->sip = sip = Malloc(sdp, sizeof(*sip) );
	if (sip == NULL) {
	    free(sgp);
	    return(FAILURE);
	}
    }
    if ( (inquiry = sip->si_inquiry) == NULL) {
	sip->si_inquiry = inquiry = Malloc(sdp, sizeof(*inquiry) );
	if (sip->si_inquiry == NULL) return(FAILURE);
    }

    status = Inquiry(sgp->fd, sgp->dsf, sgp->debug, sgp->errlog,
		     NULL, &sgp, inquiry, sizeof(*inquiry), 0, 0, sgp->timeout, sgp->tsp);
    if (status) {
	Free(sdp, sgp);
	return(status);
    }

    sip->si_vendor_id = Malloc(sdp, INQ_VID_LEN + 1 );
    sip->si_product_id = Malloc(sdp, INQ_PID_LEN + 1 );
    sip->si_revision_level = Malloc(sdp, INQ_REV_LEN + 1 );
    (void)strncpy(sip->si_vendor_id, (char *)inquiry->inq_vid, INQ_VID_LEN);
    (void)strncpy(sip->si_product_id, (char *)inquiry->inq_pid, INQ_PID_LEN);
    (void)strncpy(sip->si_revision_level, (char *)inquiry->inq_revlevel, INQ_REV_LEN);
    /* Remove trailing spaces. */
    strip_trailing_spaces(sip->si_vendor_id);
    strip_trailing_spaces(sip->si_product_id);
    strip_trailing_spaces(sip->si_revision_level);

    SetupIdentification(sdp, iop);

    Free(sdp, sgp);
    return (status);
}

/*
 * Inquiry Helper Function.
 */
void
SetupIdentification(scsi_device_t *sdp, io_params_t *iop)
{
    scsi_information_t *sip = iop->sip;
    vendor_id_t vendor_id = VID_UNKNOWN;
    product_id_t product_id = PID_UNKNOWN;
    uint8_t device_type = iop->sip->si_inquiry->inq_dtype;

    /*
     * Setup Vendor Identifier from Vendor ID and Product ID strings.
     */
    if ( EqualStringLength(sip->si_vendor_id, "CELESTIC", 8) ) {
	vendor_id = VID_CELESTICA; /* Ouray */
	if ( EqualStringLength(sip->si_product_id, "X2012-MT", 8) ||
	     EqualStringLength(sip->si_product_id, "X2024-MT", 8) ||
	     EqualStringLength(sip->si_product_id, "2U24_STOR_ENCL", 14) ) {
	    product_id = PID_OURAY;
	}
    } else if ( EqualStringLength(sip->si_vendor_id, "HGST", 4) &&
	        EqualStringLength(sip->si_vendor_id, "4U60_STOR_ENCL", 14) ) {
	vendor_id = VID_CELESTICA;
	if ( EqualStringLength(sip->si_product_id, "4U60_STOR_ENCL", 14) ) {
	    product_id = PID_KEPLER;
	}
    } else if ( EqualStringLength(sip->si_vendor_id, "HGST", 4) ) {
	/*
	 * Since we have enclosures identified as both HGST and WDC, we will 
	 * make all enclosures look like vendor WDC (for now). We cannot do 
	 * this with disks since each vendor has its' own unique extensions. 
	 */
	if (device_type == DTYPE_ENCLOSURE) {
	    vendor_id = VID_WDC;
	} else {
	    vendor_id = VID_HGST;
	}
	if ( EqualStringLength(sip->si_product_id, "STOR ENCL JBOD", 14) ) {
	    product_id = PID_PIKESPEAK;
	}
    } else if ( EqualStringLength(sip->si_vendor_id, "WDC", 3) ) {
	vendor_id = VID_WDC;
	if ( EqualStringLength(sip->si_product_id, "4U60G2_STOR_ENCL", 16) ) {
	    product_id = PID_CASTLEPEAK;
	} else if ( EqualStringLength(sip->si_product_id, "InfiniFlash A", 13) ||
		    EqualStringLength(sip->si_product_id, "InfiniFlash P200", 16) ) {
	    product_id = PID_MISSIONPEAK;
	} else if ( EqualStringLength(sip->si_product_id, "Mt Madonna 4U102", 15) ) {
	    product_id = PID_MT_MADONNA;
	}
    } else {
	vendor_id = VID_UNKNOWN;
    }
    iop->vendor_id = vendor_id;
    iop->product_id = product_id;
    return;
}

void
report_scsi_information(scsi_device_t *sdp)
{
    scsi_information_t	*sip;
    io_params_t		*iop;
    int			device_index;

    for (device_index = 0; device_index < sdp->io_devices; device_index++) {
	iop = &sdp->io_params[device_index];
	if ( (sip = iop->sip) == NULL) continue;
	report_standard_scsi_information(sdp, iop);
    }
    return;
}

void
report_standard_scsi_information(scsi_device_t *sdp, io_params_t *iop)
{
    scsi_information_t	*sip = iop->sip;
    scsi_generic_t	*sgp = &iop->sg;
    inquiry_t		*inquiry;

    if (sip == NULL) return;

    Printf(sdp, "\n");
    Printf(sdp, "SCSI Information:\n");
    Printf(sdp, "%30.30s%s\n", "SCSI Device: ", sgp->dsf);
    if (sip->si_vendor_id && sip->si_product_id && sip->si_revision_level) {
	Printf(sdp, "%30.30s", "Inquiry information: ");
	Print(sdp, "Vid=%s, Pid=%s, Rev=%s\n",
	       sip->si_vendor_id, sip->si_product_id, sip->si_revision_level);
    }
    if (inquiry = sip->si_inquiry) {
	char *alua_str;
	switch (inquiry->inq_tpgs) {
	    case 0:
		alua_str = "ALUA not supported";
		break;
	    case 1:
		alua_str = "implicit ALUA";
		break;
	    case 2:
		alua_str = "explicit ALUA";
		break;
	    case 3:
		alua_str = "explicit & implicit ALUA";
		break;
	}
	Printf(sdp, "%30.30s%u (%s)\n",
	       "Target Port Group Support: ",
	       inquiry->inq_tpgs, alua_str);
    }
    Printf(sdp, "%30.30s%u\n", "Block Length: ", iop->device_size);
    Printf(sdp, "%30.30s" LUF " (%.3f Mbytes)\n",
	   "Maximum Capacity: ", iop->device_capacity,
	   (double)(iop->device_capacity * iop->device_size) / (double)MBYTE_SIZE );

    if (iop->lbpmgmt_valid == True) {
	Printf(sdp, "%30.30s%s Provisioned\n",
	       "Provisioning Management: ", (iop->lbpme_flag) ? "Thin" : "Full");
    }
    if (sip->si_device_id) {
	Printf(sdp, "%30.30s%s\n",
	       "Device Identifier: ", sip->si_device_id);
    }
    if (sip->si_serial_number) {
	Printf(sdp, "%30.30s%s\n",
	       "Device Serial Number: ", sip->si_serial_number);
    }
    if (sip->si_mgmt_address) {
	Printf(sdp, "%30.30s%s\n",
	       "Management Network Address: ", sip->si_mgmt_address);
    }
    Printf(sdp, "\n");
    return;
}
