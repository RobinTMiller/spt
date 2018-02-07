#if !defined(SCSI_DIAG_H)
#define SCSI_DIAG_H 1
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
/************************************************************************
 *									*
 * File:	scsi_diag.h						*
 * Date:	December 5th, 2016					*
 * Author:	Robin T. Miller						*
 *									*
 * Description:								*
 *	Definitions for SCSI Diagnostic Pages.				*
 *									*
 * Modification History:						*
 *									*
 ************************************************************************/

/*
 * The maximum diagnostic buffer size:
 */
#define DIAGNOSTIC_BUFFER_SIZE	65535
#define DIAGNOSTIC_PAGE_UNKNOWN	-1

/*
 * Diagnostic Page Codes:
 */
#define DIAG_SUPPORTED_PAGES    	0x00    /* Supported diagnostic pages.  */

/*
 * Storage Enclosure Diagnostic Pages:
 *
 * Note: The control pages are also used for status (Send vs. Receive).
 */
#define DIAG_CONFIGURATION_PAGE         0x01
#define DIAG_ENCLOSURE_CONTROL_PAGE     0x02
#define DIAG_HELP_TEXT_PAGE             0x03
#define DIAG_STRING_IN_OUT_PAGE         0x04
#define DIAG_THRESHOLD_IN_OUT_PAGE      0x05
#define DIAG_OBSOLETE_PAGE              0x06
#define DIAG_ELEMENT_DESCRIPTOR_PAGE    0x07
#define DIAG_SHORT_ENCLOSURE_STATUS_PAGE 0x08
#define DIAG_ENCLOSURE_BUSY_PAGE        0x09
#define DIAG_ADDL_ELEMENT_STATUS_PAGE   0x0A
#define DIAG_SUBENCLOSURE_HELP_TEXT_PAGE 0x0B
#define DIAG_SUBENCLOSURE_STRING_IN_OUT_PAGE 0x0C
#define DIAG_SES_DIAGNOSTIC_PAGES_PAGE  0x0D
#define DIAG_DOWNLOAD_MICROCODE_CONTROL_PAGE 0x0E
#define DIAG_SUBENCLOSURE_NICKNAME_CONTROL_PAGE 0x0F

/*
 * Block Device (disk) Page:
 */
#define DIAG_TRANS_ADDR_PAGE    0x40    /* Translate address page.      */

#define DIAG_RESERVED_START     0x30    /* Reserved starting page value */
#define DIAG_RESERVED_END       0x3F    /* Reserved ending page value.  */
#define DIAG_DEVICE_START       0x40    /* Device specific start value. */
#define DIAG_DEVICE_END         0x7F    /* Device specific ending value */
                                        /* 0x40-0x7F is reserved by     */
                                        /*           most device types. */
#define DIAG_VENDOR_START       0x80    /* Vendor specific start value. */
#define DIAG_VENDOR_END         0xFF    /* Vendor specific ending value */

#endif /* defined(SCSI_DIAG_H) */
