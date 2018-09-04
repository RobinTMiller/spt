#if !defined(SPT_SES_H)
#define SPT_SES_H 1
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
 * File:	spt_ses.h
 * Author:	Robin T. Miller
 * Date:	December 3rd, 2016
 *
 * Descriptions:
 *	SCSI Enclosure Services (SES) declarations.
 *
 * Modification History:
 * 
 */
#include "scsi_cdbs.h"

/*
 * External Declarations: 
 */
extern int setup_receive_diagnostic(scsi_device_t *sdp, scsi_generic_t *sgp, size_t data_length, uint8_t page);
extern int setup_send_diagnostic(scsi_device_t *sdp, scsi_generic_t *sgp, uint8_t page);

extern int receive_diagnostic_page(scsi_device_t *sdp, io_params_t *iop,
				   scsi_generic_t *sgp, void **data, uint8_t page);
extern int send_diagnostic_page(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
				void *data_buffer, size_t data_length, uint8_t page);

extern int receive_diagnostic_encode(void *arg);
extern int receive_diagnostic_decode(void *arg);

extern int send_diagnostic_encode(void *arg);
extern int send_diagnostic_decode(void *arg);

extern uint8_t find_diagnostic_page_code(scsi_device_t *sdp, char *page_name, int *status);

extern element_type_t find_element_type(scsi_device_t *sdp, char *element_type, int *status);
extern element_status_t find_element_status(scsi_device_t *sdp, char *element_status, int *status);

#endif /* !defined(SPT_SES_H) */
