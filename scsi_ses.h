#if !defined(SCSI_SES_H)
#define SCSI_SES_H 1
/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2019			    *
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
 * File:	scsi_ses.h						*
 * Date:	December 5th, 2016					*
 * Author:	Robin T. Miller						*
 *									*
 * Description:								*
 *	Definitions for SCSI Enclosure Services (SES).			*
 *									*
 * Modification History:						*
 *									*
 ************************************************************************/

#if !defined(_BITFIELDS_LOW_TO_HIGH_) && !defined(_BITFIELDS_HIGH_TO_LOW_)
#   error "bitfield ordering is NOT defined!"
#endif /* !defined(_BITFIELDS_LOW_TO_HIGH_) || !defined(_BITFIELDS_HIGH_TO_LOW_) */

#include "inquiry.h"

#define ELEMENT_INDEX_OVERALL		-1
#define ELEMENT_INDEX_UNINITIALIZED	ELEMENT_INDEX_OVERALL

typedef enum {
    ELEMENT_STATUS_UNINITIALIZED	= -1,	/* Element status uninitialized. (internal) */
    ELEMENT_STATUS_UNSUPPORTED		= 0x00,	/* Status detection not implemented for this element. */
    ELEMENT_STATUS_OK			= 0x01,	/* Element is installed and no error conditions are known. */
    ELEMENT_STATUS_CRITICAL		= 0x02,	/* Critical condition is detected. */
    ELEMENT_STATUS_NON_CRITICAL		= 0x03,	/* Noncritical condition is detected. */
    ELEMENT_STATUS_UNRECOVERABLE	= 0x04,	/* Unrecoverable condition is detected. */
    ELEMENT_STATUS_NOT_INSTALLED	= 0x05,	/* Element is not installed in enclosure. */
    ELEMENT_STATUS_UNKNOWN		= 0x06,	/* Sensor has failed or element status is not available. */
    ELEMENT_STATUS_NOT_AVAILABLE	= 0x07,	/* Element has not been turned on or set into operation. */
    ELEMENT_STATUS_NO_ACCESS		= 0x08,	/* No access allowed from initiator port. */
    ELEMENT_STATUS_RESERVED_START	= 0x09,	/* Reserved start. */
    ELEMENT_STATUS_RESERVED_END		= 0x0F	/* Reserved end. */
} element_status_t;

typedef enum {
    ELEMENT_TYPE_UNINITIALIZED		= -1,	/* Element type uninitialized. (internal) */
    ELEMENT_TYPE_UNSPECIFIED		= 0x00,	/* Unspecified */
    ELEMENT_TYPE_DEVICE_SLOT		= 0x01,	/* Device Slot */
    ELEMENT_TYPE_POWER_SUPPLY		= 0x02,	/* Power Supply */
    ELEMENT_TYPE_COOLING		= 0x03,	/* Cooling */
    ELEMENT_TYPE_SENSOR_TEMPERATURE	= 0x04,	/* Temperature Sensor */
    ELEMENT_TYPE_DOOR			= 0x05,	/* Door */
    ELEMENT_TYPE_AUDIBLE_ALARM		= 0x06,	/* Audible Alarm */
    ELEMENT_TYPE_ESCE			= 0x07,	/* Enclosure Services Controller Electronics */
    ELEMENT_TYPE_SCC_CTRL_ELECTRONICS	= 0x08,	/* SCC Controller Electrons */
    ELEMENT_TYPE_NONVOLATILE_CACHE	= 0x09,	/* Nonvolatile Cache */
    ELEMENT_TYPE_INVALID_OPER_REASON	= 0x0A,	/* Invalid Operation Reason */
    ELEMENT_TYPE_UNINT_POWER_SUPPLY	= 0x0B,	/* Uninterruptable Power Supply */
    ELEMENT_TYPE_DISPLAY		= 0x0C,	/* Display */
    ELEMENT_TYPE_KEY_PAD_ENTRY		= 0x0D,	/* Key Pad Entry */
    ELEMENT_TYPE_ENCLOSURE		= 0x0E,	/* Enclosure */
    ELEMENT_TYPE_SCSI_PORT_TRANS	= 0x0F,	/* SCSI Port Transceiver */
    ELEMENT_TYPE_LANGUAGE		= 0x10,	/* Language */
    ELEMENT_TYPE_COMMUNICATION_PORT	= 0x11,	/* Communication Port */
    ELEMENT_TYPE_VOLTAGE_SENSOR		= 0x12,	/* Sensor Voltage */
    ELEMENT_TYPE_CURRENT_SENSOR		= 0x13,	/* Current Sensor */
    ELEMENT_TYPE_SCSI_TARGET_PORT	= 0x14,	/* SCSI Target Port */
    ELEMENT_TYPE_SCSI_INITIATOR_PORT	= 0x15,	/* SCSI Initiator Port */
    ELEMENT_TYPE_SIMPLE_SUBENCLOSURE	= 0x16,	/* Simple Enclosure */
    ELEMENT_TYPE_ARRAY_DEVICE_SLOT	= 0x17,	/* Array Device Slot */
    ELEMENT_TYPE_SAS_EXPANDER		= 0x18,	/* SAS Expander */
    ELEMENT_TYPE_SAS_CONNECTOR		= 0x19,	/* SAS Connector */
    ELEMENT_TYPE_RESERVED_START		= 0x1A,	/* Reserved start */
    ELEMENT_TYPE_RESERVED_END		= 0x7F,	/* Reserved end */
    ELEMENT_TYPE_VENDOR_START		= 0x80, /* Vendor specific start */
    ELEMENT_TYPE_VENDOR_END		= 0xFF	/* Vendor specific end */
} element_type_t;

/* ============================================================================================== */
/*
 * SES Configuration Diagnostic Page 1:
 */
typedef struct ses_configuration_page {
    uint8_t	page_code;			/* The page code.			[0] */
    uint8_t	secondary_enclosures;		/* Number of secondary enclosures.	[1] */
    uint8_t	page_length[2];			/* The page length.		      [2-3] */
    uint8_t	generation_number[4];		/* The generation code.	  	      [4-7] */
} ses_configuration_page_t;

#define SES_ENCLOSURE_LOGICAL_IDENTIFIER_LEN	8
#define SES_ENCLOSURE_VENDOR_LEN		INQ_VID_LEN
#define SES_ENCLOSURE_PRODUCT_LEN		INQ_PID_LEN
#define SES_ENCLOSURE_REVISION_LEN		INQ_REV_LEN

/*
 * Enclosure Descriptor:
 */
typedef struct ses_enclosure_descriptor {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					      [0] */
	num_enclosure_services_processes : 3,	/* Number enclosure services processes.    (b0:2) */
	reserved_byte0_b3		 : 1,	/* Reserved.				     (b3) */
	rel_enclosure_services_process_id : 3,	/* Relative enclosure services process ID. (b4:3) */
	reserved_byte0_b7		 : 1;	/* Reserved.				     (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					      [0] */
	reserved_byte0_b7		 : 1,	/* Reserved.				     (b7) */
	rel_enclosure_services_process_id : 3,	/* Relative enclosure services process ID. (b4:6) */
	reserved_byte0_b3		 : 1,	/* Reserved.				     (b3) */
	num_enclosure_services_processes : 3;	/* Number enclosure services processes.    (b0:2) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t subenclosure_identifier;		/* The subenclosure identifier.		      [1] */
    uint8_t num_type_descriptor_headers;	/* The number of type descriptor headers.     [2] */
    uint8_t enclosure_descriptor_length;	/* The enclosure descriptor length.	      [3] */
    uint8_t enclosure_logical_id[SES_ENCLOSURE_LOGICAL_IDENTIFIER_LEN];
						/* The enclosure logicial identifier.	   [4-11] */
    uint8_t enclosure_vendor_id[SES_ENCLOSURE_VENDOR_LEN];
						/* The enclosure vendor ID.		  [12-19] */
    uint8_t enclosure_product_id[SES_ENCLOSURE_PRODUCT_LEN];
						/* The enclosure product ID.		  [20-35] */
    uint8_t enclosure_revision_code[SES_ENCLOSURE_REVISION_LEN];
						/* The enclosure revision code.		  [36-39] */
    //uint8_t vendor_specific_enclosure_information[];
						/* Vendor specific enclosure information. (varies)*/
} ses_enclosure_descriptor_t;

typedef struct ses_type_desc_header {
    uint8_t element_type;			/* The element type.			[0] */
    uint8_t number_elements;			/* Number of possible elements.		[1] */
    uint8_t subenclosure_identifier;		/* Subenclosure identifier.		[2] */
    uint8_t type_descriptor_text_length;	/* Type descriptor text length.		[3] */
} ses_type_desc_header_t;

/* ============================================================================================== */
/*
 * SES Enclosure Control/Status Page 2:
 */
typedef struct ses_enclosure_control_page {
    uint8_t	page_code;			/* The page code.			[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	unrecov			: 1,		/* Unrecoverable condition.	       (b0) */
	crit			: 1,		/* Critical condition.		       (b1) */
	non_crit		: 1,		/* Non-critical condition.	       (b2) */
	info			: 1,		/* Information condition.              (b3) */
	reserved_byte1_b4_7	: 4;		/* Reserved.			     (b4:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
	reserved_byte1_b4_7	: 4,		/* Reserved.			     (b4:7) */
	info			: 1,	        /* Information condition.	       (b3) */
	non_crit		: 1,		/* Non-critical condition.	       (b2) */
	crit			: 1,		/* Critical condition.		       (b1) */
	unrecov			: 1;		/* Unrecoverable condition.	       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	page_length[2];			/* The page length.		      [2-3] */
    uint8_t	generation_number[4];		/* The generation code.	  	      [4-7] */
} ses_enclosure_control_page_t;

typedef struct ses_enclosure_status_page {
    uint8_t	page_code;			/* The page code.			[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	unrecov			: 1,		/* Unrecoverable condition.	       (b0) */
	crit			: 1,		/* Critical condition.		       (b1) */
	non_crit		: 1,		/* Non-critical condition.	       (b2) */
	info			: 1,		/* Information condition.              (b3) */
	invop			: 1,		/* Invalid operation).		       (b4) */
	reserved_byte1_b5_7	: 3;		/* Reserved.			     (b5:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
	reserved_byte1_b5_7	: 3,		/* Reserved.			     (b5:7) */
	invop			: 1,		/* Invalid operation.		       (b4) */
	info			: 1,	        /* Information condition.	       (b3) */
	non_crit		: 1,		/* Non-critical condition.	       (b2) */
	crit			: 1,		/* Critical condition.		       (b1) */
	unrecov			: 1;		/* Unrecoverable condition.	       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	page_length[2];			/* The page length.		      [2-3] */
    uint8_t	generation_number[4];		/* The generation code.	  	      [4-7] */
} ses_enclosure_status_page_t;

/*
 * Common Status for any Status Element.
 */
typedef struct ses_status_common {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[0] */
	element_status_code	: 4,		/* Element status code.		     (b0:3) */
	swap			: 1,		/* Element swapped.		       (b4) */
	disabled		: 1,		/* Element disabled.		       (b5) */
	prdfail			: 1,		/* Predicted failure.		       (b6) */
	reserved_byte0_b7	: 1;		/* Reserved.			       (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
     bitfield_t					/*					[0] */
	reserved_byte0_b7	: 1,		/* Reserved.			       (b7) */
	prdfail			: 1,		/* Predicted failure.		       (b6) */
	disabled		: 1,		/* Element disabled.		       (b5) */
	swap			: 1,		/* Element swapped.		       (b4) */
	element_status_code	: 4;		/* Element status code.		     (b0:3) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_status_common_t;

typedef struct ses_status_element {
    ses_status_common_t sc;			/* The common status fields.		[0] */
    uint8_t element_specific_data[3];		/* The element specific data.	      [1-3] */
} ses_status_element_t;

/*
 * Common Control for any Control Element.
 */
typedef struct ses_control_common {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[0] */
	reserved_byte0_b0_3	: 4,		/* Reserved.			     (b0:3) */
	rst_swap		: 1,		/* Reset swapped.		       (b4) */
	disable			: 1,		/* Disable element.		       (b5) */
	prdfail			: 1,		/* Predicted failure.		       (b6) */
	select			: 1;		/* Select.			       (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
     bitfield_t					/*					[0] */
	select			: 1,		/* Select.			       (b7) */
	prdfail			: 1,		/* Predicted failure.		       (b6) */
	disable			: 1,		/* Disable element.		       (b5) */
	rst_swap		: 1,		/* Reset swapped.		       (b4) */
	reserved_byte0_b0_3	: 4;		/* Reserved.			     (b0:3) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_control_common_t;

typedef struct ses_control_element {
    ses_control_common_t sc;			/* The common control fields.		[0] */
    uint8_t element_specific_data[3];		/* The element specific data.	      [1-3] */
} ses_control_element_t;

#define SES_CONTROL_STATUS_OFFSET	0
#define SES_CONTROL_PREDICTED_FAILURE	0x40
#define SES_CONTROL_SELECT		0x80
#define SES_CONTROL_MASK		SES_CONTROL_PREDICTED_FAILURE

/*
 * Note: Only those elements returned by HGST Enclosures have been defined!
 */
/* ============================================================================================== */
/*
 * SES Control/Status Power Supply Element (0x02).
 */
typedef struct ses_control_power_supply_element {
    ses_control_common_t sc;			/* The common control fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_5	: 6,		/* Reserved.			     (b0:5) */
	do_not_remove		: 1,		/* Do not remove.		       (b6) */
	rqst_ident		: 1;		/* Request identify.		       (b7) */
    uint8_t reserved_byte2;			/* Reserved.                   		[2] */
    bitfield_t					/*          				[3] */
        reserved_byte3_b0_4     : 5,            /* Reserved.                         (b0:4) */
	rqst_on		        : 1,		/* Request on.		               (b5) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	reserved_byte3_b7	: 1;		/* Reserved.		               (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	rqst_ident		: 1,		/* Request identify.		       (b7) */
	do_not_remove		: 1,		/* Do not remove.		       (b6) */
	reserved_byte1_b0_5	: 6;		/* Reserved.			     (b0:5) */
    uint8_t reserved_byte2;			/* Reserved.                   		[2] */
    bitfield_t					/*          				[3] */
	reserved_byte3_b7	: 1,		/* Reserved.		               (b7) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	rqst_on		        : 1,		/* Request on.		               (b5) */
        reserved_byte3_b0_4     : 5;            /* Reserved.                         (b0:4) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_control_power_supply_element_t;

typedef struct ses_status_power_supply_element {
    ses_status_common_t sc;			/* The common status fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_5	: 6,		/* Reserved.			     (b0:5) */
	do_not_remove		: 1,		/* Do not remove.		       (b6) */
	ident			: 1;		/* Identify LED.		       (b7) */
    bitfield_t					/*                         		[2] */
	reserved_byte2_b0	: 1,		/* Reserved.			       (b0) */
	dc_overcurrent		: 1,		/* DC overcurrent.		       (b1) */
	dc_undervoltage		: 1,		/* DC undervoltage.		       (b2) */
	dc_overvoltage		: 1,		/* DC overvoltage.		       (b3) */
	reserved_byte2_b4_7	: 4;		/* Reserved.			     (b4:7) */
    bitfield_t					/*          				[3] */
	dc_fail			: 1,		/* DC fail.			       (b0) */
	ac_fail			: 1,		/* AC fail.			       (b1) */
	temp_warn		: 1,		/* Over temperature warning.	       (b2) */
	over_temp_fail		: 1,		/* Over temperature failure.	       (b3) */
	off			: 1,		/* Power supply off.		       (b4) */
	rqsted_on		: 1,		/* Requested on.		       (b5) */
	fail			: 1,		/* Failure LED.			       (b6) */
	hot_swap		: 1;		/* Hot swap.		               (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	ident			: 1,		/* Identify LED.		       (b7) */
	do_not_remove		: 1,		/* Do not remove.		       (b6) */
	reserved_byte1_b0_5	: 6;		/* Reserved.			     (b0:5) */
    bitfield_t					/*                         		[2] */
	reserved_byte2_b4_7	: 4,		/* Reserved.			     (b4:7) */
	dc_overvoltage		: 1,		/* DC overvoltage.		       (b3) */
	dc_undervoltage		: 1,		/* DC undervoltage.		       (b2) */
	dc_overcurrent		: 1,		/* DC overcurrent.		       (b1) */
	reserved_byte2_b0	: 1;		/* Reserved.			       (b0) */
    bitfield_t					/*          				[3] */
	hot_swap		: 1,		/* Hot swap.		               (b7) */
	fail			: 1,		/* Failure LED.			       (b6) */
	rqsted_on		: 1,		/* Requested on.		       (b5) */
	off			: 1,		/* Power supply off.		       (b4) */
	over_temp_fail		: 1,		/* Over temperature failure.	       (b3) */
	temp_warn		: 1,		/* Over temperature warning.	       (b2) */
	ac_fail			: 1,		/* AC fail.			       (b1) */
	dc_fail			: 1;		/* DC fail.			       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_status_power_supply_element_t;

/* ============================================================================================== */
/*
 * SES Control/Status Cooling Element (0x03).
 */
typedef struct ses_control_cooling_element {
    ses_control_common_t sc;			/* The common control fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
        reserved_byte1_b0_5	: 6,		/* Reserved.			     (b0:5) */
        do_not_remove		: 1,		/* Do not remove.		       (b6) */
        rqst_ident		: 1;		/* Request identify.		       (b7) */
    uint8_t reserved_byte2;			/* Reserved.                   		[2] */
    bitfield_t					/*          				[3] */
	requested_speed_code	: 3,		/* Requested speed code.	     (b0:2) */
	reserved_byte3_b3_4	: 2,		/* Reserved.			     (b3:4) */
	rqst_on	        	: 1,		/* Request on.  		       (b5) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	reserved_byte3_b7	: 1;		/* Reserved.		               (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
        rqst_ident		: 1,		/* Request identify.		       (b7) */
        do_not_remove		: 1,		/* Do not remove.		       (b6) */
        reserved_byte1_b0_5	: 6;		/* Reserved.			     (b0:5) */
    uint8_t reserved_byte2;			/* Reserved.                   		[2] */
    bitfield_t					/*          				[3] */
	reserved_byte3_b7	: 1,		/* Reserved.		               (b7) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	rqst_on	        	: 1,		/* Request on.  		       (b5) */
        reserved_byte3_b3_4	: 2,		/* Reserved.			     (b3:4) */
        requested_speed_code	: 3;		/* Requested speed code.	     (b0:2) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_control_cooling_element_t;

typedef struct ses_status_cooling_element {
    ses_status_common_t sc;			/* The common status fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	actual_fan_speed_msb	: 3,		/* Actual fan speed (MSB).	     (b0:2) */
	reserved_byte1_b3_5	: 3,		/* Reserved.			     (b3:5) */
	do_not_remove		: 1,		/* Do not remove.		       (b6) */
	ident			: 1;		/* Identify LED.		       (b7) */
    uint8_t actual_fan_speed;			/* Actual fan speed (LSB).		[2] */
    bitfield_t					/*          				[3] */
	actual_speed_code	: 3,		/* Actual speed code.		     (b0:2) */
	reserved_byte3_b3	: 1,		/* Reserved.			       (b3) */
	off			: 1,		/* Cooling mechanism off.	       (b4) */
	rqsted_on		: 1,		/* Requested on.		       (b5) */
	fail			: 1,		/* Failure LED.			       (b6) */
	hot_swap		: 1;		/* Hot swap.		               (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	ident			: 1,		/* Identify LED.		       (b7) */
	do_not_remove		: 1,		/* Do not remove.		       (b6) */
	reserved_byte1_b3_5	: 3,		/* Reserved.			     (b3:5) */
	actual_fan_speed_msb	: 3;		/* Actual fan speed (MSB).	     (b0:2) */
    uint8_t actual_fan_speed;			/* Actual fan speed (LSB).		[2] */
    bitfield_t					/*          				[3] */
	hot_swap		: 1,		/* Hot swap.		               (b7) */
	fail			: 1,		/* Failure LED.			       (b6) */
	rqsted_on		: 1,		/* Requested on.		       (b5) */
	off			: 1,		/* Cooling mechanism off.	       (b4) */
	reserved_byte3_b3	: 1,		/* Reserved.			       (b3) */
	actual_speed_code	: 3;		/* Actual speed code.		     (b0:2) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_status_cooling_element_t;

/* ============================================================================================== */
/*
 * SES Control/Status Temperature Element (0x04).
 */
typedef struct ses_control_temperature_element {
    ses_control_common_t sc;			/* The common control fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_5	: 6,		/* Reserved.			     (b0:5) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	rqst_ident		: 1;		/* Request identify.		       (b7) */
    uint8_t reserved_byte2_3[2];		/* Reserved.			      [2-3] */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	rqst_ident		: 1,		/* Request identify.		       (b7) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	reserved_byte1_b0_5	: 6;		/* Reserved.			     (b0:5) */
    uint8_t reserved_byte2_3[2];		/* Reserved.			      [2-3] */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_control_temperature_element_t;

typedef struct ses_status_temperature_element {
    ses_status_common_t sc;			/* The common status fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_5	: 6,		/* Reserved.			     (b0:5) */
	fail			: 1,		/* Failure LED.			       (b6) */
	ident			: 1;		/* Identify LED.		       (b7) */
    uint8_t temperature;			/* Temperature.				[2] */
    bitfield_t					/*          				[3] */
	ut_warning		: 1,		/* Under temperature warning.	       (b0) */
	ut_failure		: 1,		/* Under temperature failure.	       (b1) */
	ot_warning		: 1,		/* Over temperature warning.	       (b2) */
	ot_failure		: 1,		/* Over temperature failure.	       (b3) */
	reserved_byte3_b4_7	: 4;		/* Reserved.			     (b4:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	ident			: 1,		/* Identify LED.		       (b7) */
	fail			: 1,		/* Failure LED.			       (b6) */
	reserved_byte1_b0_5	: 6;		/* Reserved.			     (b0:5) */
    uint8_t temperature;			/* Temperature.				[2] */
    bitfield_t					/*          				[3] */
	reserved_byte3_b4_7	: 4,		/* Reserved.			     (b4:7) */
	ot_failure		: 1,		/* Over temperature failure.	       (b3) */
	ot_warning		: 1,		/* Over temperature warning.	       (b2) */
	ut_failure		: 1,		/* Under temperature failure.	       (b1) */
	ut_warning		: 1;		/* Under temperature warning.	       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_status_temperature_element_t;

/* ============================================================================================== */
/*
 * SES Control/Status Door Element (0x05).
 */
typedef struct ses_control_door_element {
    ses_control_common_t sc;			/* The common control fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_5	: 6,		/* Reserved.			     (b0:5) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	rqst_ident		: 1;		/* Request identify.		       (b7) */
    uint8_t reserved_byte2;     		/* Reserved.			      [2-3] */
    bitfield_t					/*          				[3] */
	unlock		        : 1,		/* Unlock door latch.    	       (b0) */
	reserved_byte3_b1_7	: 7;		/* Reserved.            	     (b1:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	rqst_ident		: 1,		/* Request identify.		       (b7) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	reserved_byte1_b0_5	: 6;		/* Reserved.			     (b0:5) */
    uint8_t reserved_byte2;	        	/* Reserved.			      [2-3] */
    bitfield_t					/*          				[3] */
	reserved_byte3_b1_7	: 7,		/* Reserved.            	     (b1:7) */
	unlock		        : 1;		/* Unlock door latch.    	       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_control_door_element_t;

typedef struct ses_status_door_element {
    ses_status_common_t sc;			/* The common status fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_5	: 6,		/* Reserved.			     (b0:5) */
	fail			: 1,		/* Failure LED.			       (b6) */
	ident			: 1;		/* Identify LED.		       (b7) */
    uint8_t reserved_byte2;	        	/* Reserved.				[2] */
    bitfield_t					/*          				[3] */
	unlocked		: 1,		/* Door unlocked.        	       (b0) */
	open		        : 1,		/* Door open.           	       (b1) */
	reserved_byte3_b2_7	: 6;		/* Reserved.            	     (b2:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	ident			: 1,		/* Identify LED.		       (b7) */
	fail			: 1,		/* Failure LED.			       (b6) */
	reserved_byte1_b0_5	: 6;		/* Reserved.			     (b0:5) */
    uint8_t reserved_byte2;	        	/* Reserved.				[2] */
    bitfield_t					/*          				[3] */
	reserved_byte3_b2_7	: 6,		/* Reserved.            	     (b2:7) */
        open		        : 1,		/* Door open.            	       (b1) */
	unlocked		: 1;		/* Door unlocked.        	       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_status_door_element_t;

/* ============================================================================================== */
/*
 * SES Control/Status Enclosure Services Controller Electronics Element (ESCE) (0x07).
 */
typedef struct ses_control_esce_element {
    ses_control_common_t sc;			/* The common control fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_3	: 4,		/* Reserved.			     (b0:3) */
	rqst_remove		: 1,		/* Request removal.	               (b4) */
	do_not_remove		: 1,		/* Do not remove.		       (b5) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	rqst_ident		: 1;		/* Request identify.		       (b7) */
    bitfield_t					/*					[2] */
	select_element		: 1,		/* Select element.		       (b0) */
	reserved_byte2_b1_7	: 7;		/* Reserved.			     (b1:7) */
    uint8_t reserved_byte3;			/* Reserved.				[3] */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	rqst_ident		: 1,		/* Request identify.		       (b7) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	do_not_remove		: 1,		/* Do not remove.		       (b5) */
	rqst_remove		: 1,		/* Reqeust removal.     	       (b4) */
	reserved_byte1_b0_3	: 4;		/* Reserved.			     (b0:3) */
    bitfield_t					/*					[2] */
	reserved_byte2_b1_7	: 7,		/* Reserved.			     (b1:7) */
	select_element		: 1;		/* Select element.		       (b0) */
    uint8_t reserved_byte3;			/* Reserved.				[3] */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_control_esce_element_t;

typedef struct ses_status_esce_element {
    ses_status_common_t sc;			/* The common status fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_3	: 4,		/* Reserved.			     (b0:3) */
	rmv			: 1,		/* Slot prepared for removal.	       (b4) */
	do_not_remove		: 1,		/* Do not remove.		       (b5) */
	fail			: 1,		/* Failure LED.			       (b6) */
	ident			: 1;		/* Identify LED.		       (b7) */
    bitfield_t					/*					[2] */
	report			: 1,		/* Report.			       (b0) */
	reserved_byte2_b1_7	: 7;		/* Reserved.			     (b1:7) */
    bitfield_t					/*					[3] */
	reserved_byte3_b0_6	: 7,		/* Reserved.			     (b0:6) */
	hot_swap		: 1;		/* Hot Swap.			       (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	ident			: 1,		/* Identify LED.		       (b7) */
	fail			: 1,		/* Failure LED.			       (b6) */
	do_not_remove		: 1,		/* Do not remove.		       (b5) */
	rmv			: 1,		/* Slot prepared for removal.	       (b4) */
	reserved_byte1_b0_3	: 4;		/* Reserved.			     (b0:3) */
    bitfield_t					/*					[2] */
	reserved_byte2_b1_7	: 7,		/* Reserved.			     (b1:7) */
	report			: 1;		/* Report.			       (b0) */
    bitfield_t					/*					[3] */
	hot_swap		: 1,		/* Hot Swap.			       (b7) */
	reserved_byte3_b0_6	: 7;		/* Reserved.			     (b0:6) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_status_esce_element_t;

/* ============================================================================================== */
/*
 * SES Control/Status Enclosure Element (0x0e).
 */
typedef struct ses_control_enclosure_element {
    ses_control_common_t sc;			/* The common control fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_6	: 7,		/* Reserved.			     (b0:6) */
	rqst_ident		: 1;		/* Request identify LED.	       (b7) */
    bitfield_t					/*					[2] */
	power_cycle_delay	: 6,		/* Power cycle delay.		     (b0:5) */
	power_cycle_request	: 2;		/* Power cycle request. 	     (b6:7) */
    bitfield_t					/*					[3] */
	request_warning 	: 1,		/* Request warning.		       (b0) */
	request_failure 	: 1,		/* Request failure.		       (b1) */
	power_off_duration      : 6;	        /* Power off duration.               (b2:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	rqst_ident		: 1,		/* Request identify LED.	       (b7) */
	reserved_byte1_b0_6	: 7;		/* Reserved.			     (b0:6) */
    bitfield_t					/*					[2] */
	power_cycle_request	: 2,		/* Power cycle request. 	     (b6:7) */
	power_cycle_delay	: 6;		/* Power cycle delay.		     (b0:5) */
    bitfield_t					/*					[3] */
	power_off_duration      : 6,	        /* Power off duration.               (b2:7) */
	request_failure 	: 1,		/* Request failure.		       (b1) */
	request_warning 	: 1;		/* Request warning.		       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_control_enclosure_element_t;

typedef struct ses_status_enclosure_element {
    ses_status_common_t sc;			/* The common status fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_6	: 7,		/* Reserved.			     (b0:6) */
	ident			: 1;		/* Identify LED.		       (b7) */
    bitfield_t					/*					[2] */
	warning_indication	: 1,		/* Warning indication.		       (b0) */
	failure_indication	: 1,		/* Failure indication.		       (b1) */
	time_until_power_cycle	: 6;		/* Time until power cycle.	     (b2:7) */
    bitfield_t					/*					[3] */
	warning_requested	: 1,		/* Warning requested.		       (b0) */
	failure_requested	: 1,		/* Failure requested.		       (b1) */
	requested_power_off_duration : 6;	/* Requested power off duration.     (b2:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	ident			: 1,		/* Identify LED.		       (b7) */
	reserved_byte1_b0_6	: 7;		/* Reserved.			     (b0:6) */
    bitfield_t					/*					[2] */
	time_until_power_cycle	: 6,		/* Time until power cycle.	     (b2:7) */
	failure_indication	: 1,		/* Failure indication.		       (b1) */
	warning_indication	: 1;		/* Warning indication.		       (b0) */
    bitfield_t					/*					[3] */
	requested_power_off_duration : 6,	/* Requested power off duration.     (b2:7) */
	failure_requested	: 1,		/* Failure requested.		       (b1) */
	warning_requested	: 1;		/* Warning requested.		       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_status_enclosure_element_t;

/* ============================================================================================== */
/*
 * SES Control/Status Voltage Element (0x12).
 */
typedef struct ses_control_voltage_element {
    ses_control_common_t sc;			/* The common control fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_5	: 6,		/* Reserved.			     (b0:5) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	rqst_ident		: 1;		/* Request identify.		       (b7) */
    uint8_t reserved_byte2_3[2];		/* Reserved.			      [2-3] */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	rqst_ident		: 1,		/* Request identify.		       (b7) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	reserved_byte1_b0_5	: 6;		/* Reserved.			     (b0:5) */
    uint8_t reserved_byte2_3[2];		/* Reserved.			      [2-3] */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_control_voltage_element_t;

typedef struct ses_status_voltage_element {
    ses_status_common_t sc;			/* The common status fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	crit_under		: 1,		/* Critical under voltage.	       (b0) */
	crit_over		: 1,		/* Critical over voltage.	       (b1) */
	warn_under		: 1,		/* Under voltage warnng.	       (b2) */
	warn_over		: 1,		/* Over voltage warning.	       (b3) */
	reserved_byte1_b4_5	: 2,		/* Reserved.			     (b4:5) */
	fail			: 1,		/* Failure LED.			       (b6) */
	ident			: 1;		/* Identify LED.		       (b7) */
    uint8_t voltage[2];				/* Voltage.			      [2-3] */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	ident			: 1,		/* Identify LED.		       (b7) */
	fail			: 1,		/* Failure LED.			       (b6) */
	reserved_byte1_b4_5	: 2,		/* Reserved.			     (b4:5) */
	warn_over		: 1,		/* Over voltage warning.	       (b3) */
	warn_under		: 1,		/* Under voltage warnng.	       (b2) */
	crit_over		: 1,		/* Critical over voltage.	       (b1) */
	crit_under		: 1;		/* Critical under voltage.	       (b0) */
    uint8_t voltage[2];				/* Voltage.			      [2-3] */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_status_voltage_element_t;

/* ============================================================================================== */
/*
 * SES Control/Status Current Element (0x13).
 */
typedef struct ses_control_current_element {
    ses_control_common_t sc;			/* The common control fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_5	: 6,		/* Reserved.			     (b0:5) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	rqst_ident		: 1;		/* Request identify.		       (b7) */
    uint8_t reserved_byte2_3[2];		/* Reserved.			      [2-3] */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	rqst_ident		: 1,		/* Request identify.		       (b7) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	reserved_byte1_b0_5	: 6;		/* Reserved.			     (b0:5) */
    uint8_t reserved_byte2_3[2];		/* Reserved.			      [2-3] */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_control_current_element_t;

typedef struct ses_status_current_element {
    ses_status_common_t sc;			/* The common status fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0	: 1,		/* Reserved.			       (b0) */
	crit_over		: 1,		/* Critical over current.	       (b1) */
	reserved_byte1_b2	: 1,		/* Reserved.			       (b2) */
	warn_over		: 1,		/* Over current warning.	       (b3) */
	reserved_byte1_b4_5	: 2,		/* Reserved.			     (b4:5) */
	fail			: 1,		/* Failure LED.			       (b6) */
	ident			: 1;		/* Identify LED.		       (b7) */
    uint8_t current[2];				/* Current.			      [2-3] */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	ident			: 1,		/* Identify LED.		       (b7) */
	fail			: 1,		/* Failure LED.			       (b6) */
	reserved_byte1_b4_5	: 2,		/* Reserved.			     (b4:5) */
	warn_over		: 1,		/* Over current warning.	       (b3) */
	reserved_byte1_b2	: 1,		/* Reserved.			       (b2) */
	crit_over		: 1,		/* Critical over current.	       (b1) */
	reserved_byte1_b0	: 1;		/* Reserved.			       (b0) */
    uint8_t current[2];				/* Current.			      [2-3] */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_status_current_element_t;

/* ============================================================================================== */
/*
 * SES Control/Status Array Device Element (0x17).
 */
typedef struct ses_control_array_device_element {
    ses_control_common_t sc;			/* The common control fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	rqst_rr_abort		: 1,		/* Request Rebuild/Remap abort.	       (b0) */
	rqst_rebuild_remap	: 1,		/* Request Rebuild/Remap.	       (b1) */
	rqst_in_failed_array	: 1,		/* Request in failed array.	       (b2) */
	rqst_in_crit_array	: 1,		/* Request in critical array.	       (b3) */
	rqst_cons_chk		: 1,		/* Request consistency check in prog.  (b4) */
	rqst_hot_spare		: 1,		/* Request hot spare.		       (b5) */
	rqst_rsvd_device	: 1,		/* Request reserved device.	       (b6) */
	rqst_ok			: 1;		/* Request device okay		       (b7) */
    bitfield_t					/*					[2] */
	reserved_byte2_b0	: 1,		/* Reserved.			       (b0) */
	rqst_ident		: 1,		/* Request identify.		       (b1) */
	rqst_remove		: 1,		/* Request removal.		       (b2) */
	rqst_insert		: 1,		/* Request insert.		       (b3) */
	rqst_missing		: 1,		/* Request missing.		       (b4) */
	reserved_byte2_b5	: 1,		/* Reserved.			       (b5) */
	do_not_remove		: 1,		/* Do not remove.		       (b6) */
	rqst_active		: 1;		/* Request active.		       (b7) */
    bitfield_t					/*					[3] */
	reserved_byte3_b0_1	: 2,		/* Reserved.			     (b0:1) */
	enable_bypass_b		: 1,		/* Enable bypass port B.	       (b2) */
	enable_bypass_a		: 1,		/* Enable bypass port A.	       (b3) */
	device_off		: 1,		/* Device turned off.		       (b4) */
	rqst_fault		: 1,		/* Request fault.		       (b5) */
	reserved_byte3_b6_7	: 2;		/* Reserved.			     (b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	rqst_ok			: 1,		/* Request device okay		       (b7) */
	rqst_rsvd_device	: 1,		/* Request reserved device.	       (b6) */
	rqst_hot_spare		: 1,		/* Request hot spare.		       (b5) */
	rqst_cons_chk		: 1,		/* Request consistency check in prog.  (b4) */
	rqst_in_crit_array	: 1,		/* Request in critical array.	       (b3) */
	rqst_in_failed_array	: 1,		/* Request in failed array.	       (b2) */
	rqst_rebuild_remap	: 1,		/* Request Rebuild/Remap.	       (b1) */
	rqst_rr_abort		: 1;		/* Request Rebuild/Remap abort.	       (b0) */
    bitfield_t					/*					[2] */
	rqst_active		: 1,		/* Request active.		       (b7) */
	do_not_remove		: 1,		/* Do not remove.		       (b6) */
	reserved_byte1_b5	: 1,		/* Reserved.			       (b5) */
	rqst_missing		: 1,		/* Request missing.		       (b4) */
	rqst_insert		: 1,		/* Request insert.		       (b3) */
	rqst_remove		: 1,		/* Request removal.		       (b2) */
	rqst_ident		: 1,		/* Request identify.		       (b1) */
	reserved_byte1_b0	: 1;		/* Reserved.			       (b0) */
    bitfield_t					/*					[3] */
	reserved_byte3_b6_7	: 2,		/* Reserved.			     (b6:7) */
	rqst_fault		: 1,		/* Request fault.		       (b5) */
	device_off		: 1,		/* Device turned off.		       (b4) */
	enable_bypass_a		: 1,		/* Enable bypass port A.	       (b3) */
	enable_bypass_b		: 1,		/* Enable bypass port B.	       (b2) */
	reserved_byte3_b0_1	: 2;		/* Reserved.			     (b0:1) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_control_array_device_element_t;

typedef struct ses_status_array_device_element {
    ses_status_common_t sc;			/* The common status fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	rr_abort		: 1,		/* Rebuild/Remap abort.		       (b0) */
	rebuild_remap		: 1,		/* Rebuild/Remap.		       (b1) */
	in_failed_array		: 1,		/* In failed array.		       (b2) */
	in_crit_array		: 1,		/* In critical array.		       (b3) */
	cons_chk		: 1,		/* Consistency check in progress.      (b4) */
	hot_spare		: 1,		/* Hot spare.			       (b5) */
	rsvd_device		: 1,		/* Reserved device.		       (b6) */
	ok			: 1;		/* Device okay			       (b7) */
    bitfield_t					/*					[2] */
	report			: 1,		/* Report.			       (b0) */
	ident			: 1,		/* Identify LED.		       (b1) */
	rmv			: 1,		/* Slot prepared for removal.	       (b2) */
	ready_to_insert		: 1,		/* Ready to insert.		       (b3) */
	enclosure_bypassed_b	: 1,		/* Enclosure bypassed port B.	       (b4) */
	enclosure_bypassed_a	: 1,		/* Enclosure bypassed port A.	       (b5) */
	do_not_remove		: 1,		/* Do not remove.		       (b6) */
	app_client_bypassed_a	: 1;		/* Application client bypassed port A. (b7) */
    bitfield_t					/*					[3] */
	device_bypassed_b	: 1,		/* Device bypassed port B.	       (b0) */
	device_bypassed_a	: 1,		/* Device bypassed port A.	       (b1) */
	bypassed_b		: 1,		/* Bypassed port B.		       (b2) */
	bypassed_a		: 1,		/* Bypassed Port A.		       (b3) */
	device_off		: 1,		/* Device turned off.		       (b4) */
	fault_reqstd		: 1,		/* Fault requested.		       (b5) */
	fault_sensed		: 1,		/* Fault sensed.		       (b6) */
	app_client_bypassed_b	: 1;		/* Application client bypassed port B. (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	ok			: 1,		/* Device okay			       (b7) */
	rsvd_device		: 1,		/* Reserved device.		       (b6) */
	hot_spare		: 1,		/* Hot spare.			       (b5) */
	cons_chk		: 1,		/* Consistency check in progress.      (b4) */
	in_crit_array		: 1,		/* In critical array.		       (b3) */
	in_failed_array		: 1,		/* In failed array.		       (b2) */
	rebuild_remap		: 1,		/* Rebuild/Remap.		       (b1) */
	rr_abort		: 1;		/* Rebuild/Remap abort.		       (b0) */
    bitfield_t					/*					[2] */
	app_client_bypassed_a	: 1,		/* Application client bypassed port A. (b7) */
	do_not_remove		: 1,		/* Do not remove.		       (b6) */
	enclosure_bypassed_a	: 1,		/* Enclosure bypassed port A.	       (b5) */
	enclosure_bypassed_b	: 1,		/* Enclosure bypassed port B.	       (b4) */
	ready_to_insert		: 1,		/* Ready to insert.		       (b3) */
	rmv			: 1,		/* Slot prepared for removal.	       (b2) */
	ident			: 1,		/* Identify LED.		       (b1) */
	report			: 1;		/* Report.			       (b0) */
    bitfield_t					/*					[3] */
	app_client_bypassed_b	: 1,		/* Application client bypassed port B. (b7) */
	fault_sensed		: 1,		/* Fault sensed.		       (b6) */
	fault_reqstd		: 1,		/* Fault requested.		       (b5) */
	device_off		: 1,		/* Device turned off.		       (b4) */
	bypassed_a		: 1,		/* Bypassed port A.		       (b3) */
	bypassed_b		: 1,		/* Bypassed port B.		       (b2) */
	device_bypassed_a	: 1,		/* Device bypassed port A.	       (b1) */
	device_bypassed_b	: 1;		/* Device bypassed port B.	       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_status_array_device_element_t;

/* ============================================================================================== */
/*
 * SES Control/Status SAS Expander Element (0x18).
 */
typedef struct ses_control_sas_expander_element {
    ses_control_common_t sc;			/* The common control fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_5	: 6,		/* Reserved.			     (b0:5) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	rqst_ident		: 1;		/* Request identify.		       (b7) */
    uint8_t reserved_byte2;			/* Reserved.				[2] */
    uint8_t reserved_byte3;			/* Reserved.				[3] */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	rqst_ident		: 1,		/* Request identify.		       (b7) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	reserved_byte1_b0_5	: 6;		/* Reserved.			     (b0:5) */
    uint8_t reserved_byte2;			/* Reserved.				[2] */
    uint8_t reserved_byte3;			/* Reserved.				[3] */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_control_sas_expander_element_t;

typedef struct ses_status_sas_expander_element {
    ses_status_common_t sc;			/* The common status fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_5	: 6,		/* Reserved.			     (b0:5) */
	fail			: 1,		/* Failure LED.			       (b6) */
	ident			: 1;		/* Identify LED.		       (b7) */
    uint8_t reserved_byte2;			/* Reserved.				[2] */
    uint8_t reserved_byte3;			/* Reserved.				[3] */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	ident			: 1,		/* Identify LED			       (b7) */
	fail			: 1,		/* Failure LED.	       		       (b6) */
	reserved_byte1_b0_5	: 6;		/* Reserved.			     (b0:5) */
    uint8_t reserved_byte2;			/* Reserved.				[2] */
    uint8_t reserved_byte3;			/* Reserved.				[3] */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_status_sas_expander_element_t;

/* ============================================================================================== */
/*
 * SES Control/Status SAS Connector Element (0x19).
 */
typedef struct ses_control_sas_connector_element {
    ses_control_common_t sc;			/* The common control fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_6	: 7,		/* Reserved.    		     (b0:6) */
	rqst_ident		: 1;		/* Request identify.		       (b7) */
    uint8_t reserved_byte2;     		/* Reserved.            		[2] */
    bitfield_t					/*          				[3] */
	reserved_byte3_b0_5	: 6,		/* Reserved.			     (b0:5) */
	rqst_fail		: 1,		/* Request failure.		       (b6) */
	reserved_byte3_b7	: 1;		/* Resevred.                           (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	rqst_ident		: 1,		/* Request identify.		       (b7) */
	reserved_byte1_b0_6	: 7;		/* Reserved.    		     (b0:6) */
    uint8_t reserved_byte2;     		/* Reserved.            		[2] */
    bitfield_t					/*          				[3] */
	reserved_byte3_b7	: 1,		/* Resevred.                           (b7) */
        rqst_fail		: 1,		/* Request failure.		       (b6) */
	reserved_byte3_b0_5	: 6;		/* Reserved.			     (b0:5) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_control_sas_connector_element_t;

typedef struct ses_status_sas_connector_element {
    ses_status_common_t sc;			/* The common status fields.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	connector_type		: 7,		/* Connector type.		     (b0:6) */
	ident			: 1;		/* Identify LED.		       (b7) */
    uint8_t connector_physical_link;		/* Connector physical link.		[2] */
    bitfield_t					/*          				[3] */
	reserved_byte3_b0_4	: 5,		/* Reserved.			     (b0:4) */
	overcurrent		: 1,		/* Overcurrent.			       (b5) */
	fail			: 1,		/* Failure LED.			       (b6) */
	mated			: 1;		/* Mated (mechanically connected).     (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	ident			: 1,		/* Identify LED.		       (b7) */
	connector_type		: 7;		/* Connector type.		     (b0:6) */
    uint8_t connector_physical_link;		/* Connector physical link.		[2] */
    bitfield_t					/*          				[3] */
	mated			: 1,		/* Mated (mechanically connected).     (b7) */
	fail			: 1,		/* Failure LED.			       (b6) */
	overcurrent		: 1,		/* Overcurrent.			       (b5) */
	reserved_byte3_b0_4	: 5;		/* Reserved.			     (b0:4) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} ses_status_sas_connector_element_t;

/* ============================================================================================== */
/*
 * SES String In Page 4:
 */
typedef struct ses_page4_header {
    uint8_t	page_code;			/* Page code.				[0] */
    uint8_t	reserved_byte1;			/* Reserved.				[1] */
    uint8_t	page_length[2];			/* The page length.		      [2-3] */
} ses_page4_header_t;

typedef struct ses_string_in_status_page {
    ses_page4_header_t page_hdr;		/* The page header.		      [0-3] */
    uint8_t	oneSecTickCt[4];		/* Tick counter (seconds).    	      [4-7] */
    uint8_t	monitorRunCt[4];		/* Monitor loop counter.	     [8-11] */
    uint8_t	monitorRunTime[4];		/* Monitor recent latency.	    [12-15] */
    uint8_t	monitorMaxTime[4];		/* Monitor maximum latency.	    [16-19] */
    uint8_t	lockDownReason[4];		/* Offline state reason mask.	    [20-23] */
    uint8_t	powerMode[4];			/* Power state.			    [24-27] */
    uint8_t	acFailCountPsuA[4];		/* PSU-A AC failure counter.        [28-31] */
    uint8_t	acFailCountPsuB[4];		/* PSU-B AC failure counter.        [32-35] */
    uint8_t	lastPhyIdReset[4];		/* Phy reset last ID.		    [36-39] */
    uint8_t	phyResetCnt[4];			/* Phy reset event counter.	    [40-43] */
    uint8_t	bistFailCnt[4];			/* BIST failure event counter.      [44-47] */
    uint8_t	currSafeguard;			/* Current safeguard.		       [48] */
    uint8_t	prevSafeguard;			/* Previous safeguard.		       [49] */
    uint8_t	thermalVote;			/* Last thermal thread vote.	       [50] */
    uint8_t	powerVote;			/* Last power thread vote.	       [51] */
    uint8_t	currDriveBMS;			/* Current drive BMS enable.	       [52] */
    uint8_t	prevDriveBMS;			/* Previous drive BMS enable.	       [53] */
    uint8_t	reserved_byte54;		/* Reserved.			       [54] */
    uint8_t	reserved_byte55;		/* Reserved.			       [55] */
                                                /* Note: Bytes 54-55 == Slot A/B for CP/MM. */
    uint8_t	lastSafeGuardChangeTicks[4];	/* Last safeguard tick count.       [56-59] */
    uint8_t	lastBMSChangeTicks[4];		/* Last drive BMS tick count.	    [60-63] */

    /*
     * The Fan elements vary by enclosure platform. 
     * Then there is enclosure specific information! 
     * Therefore, separate data structures are required! 
     */
    uint8_t	opaque[64];			/* [64-127] */
} ses_string_in_status_page_t;

/* ============================================================================================== */
/*
 * SES Element Descriptor Status Page 7:
 */
typedef struct ses_element_descriptor_page {
    uint8_t	page_code;			/* The page code.			[0] */
    uint8_t	reserved_byte1;			/* Reserved.				[1] */
    uint8_t	page_length[2];			/* The page length.		      [2-3] */
    uint8_t	generation_number[4];		/* The generation code.	  	      [4-7] */
						/* Element descriptor list.		    */
} ses_element_descriptor_page_t;

typedef struct ses_element_descriptor {
    uint8_t	reserved_byte0;			/* Reserved.				[0] */
    uint8_t	reserved_byte1;			/* Reserved.				[1] */
    uint8_t	descriptor_length[2];		/* Descriptor length.		      [2-3] */
						/* Descriptor text (ASCII string).	    */
} ses_element_descriptor_t;

/* ============================================================================================== */
/*
 * SES Additional Element Status Page 0x0A:
 */
typedef struct ses_addl_element_status_page {
    uint8_t	page_code;			/* The page code.			[0] */
    uint8_t	reserved_byte1;			/* Reserved.				[1] */
    uint8_t	page_length[2];			/* The page length.		      [2-3] */
    uint8_t	generation_number[4];		/* The generation code.	  	      [4-7] */
						/* Additional element descriptor list.	    */
} ses_addl_element_status_page_t;

#define SES_PROTOCOL_IDENTIFIER_FC	0x00
#define SES_PROTOCOL_IDENTIFIER_SAS	0x06
#define SES_PROTOCOL_IDENTIFIER_PCIe	0x0B

/* Note: Our enclosures only support EIP=1, so defining this descriptor only! */
typedef struct ses_addl_element_status_descriptor {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[0] */
	protocol_identifier	: 4,		/* The protocol identifier.		    */
	eip			: 1,		/* Element index present (EIP).	       (b4) */
	reserved_byte0_b5_6	: 2,		/* Reserved.			     (b5:6) */
	invalid			: 1;		/* Protocol specific info invalid.     (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[0] */
	invalid			: 1,		/* Protocol specific info invalid.     (b7) */
	reserved_byte0_b5_6	: 2,		/* Reserved.			     (b5:6) */
	eip			: 1,		/* Element index present (EIP).	       (b4) */
	protocol_identifier	: 4;		/* The protocol identifier.		    */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t addl_element_desc_length;		/* Additional element desc length.	[1] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[2] */
	eiioe			: 2,		/* Element index includes overall.   (b0:1) */
	reserved_byte2_b2_7	: 6;		/* Reserved.			     (b2:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[2] */
	reserved_byte2_b2_7	: 6,		/* Reserved.			     (b2:7) */
	eiioe			: 2;		/* Element index includes overall.   (b0:1) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	element_index;			/* Element index.			[3] */
						/* Protocol specific information.	    */
} ses_addl_element_status_descriptor_t;

/*
 * SAS Protocol Identifier Information:
 */
#define SAS_DESCRIPTOR_TYPE0	0x00		/* SAS array device slot elements.	*/
#define SAS_DESCRIPTOR_TYPE1	0x01		/* SAS Expander and ESCE elements.	*/

typedef struct sas_protocol_information {
    uint8_t	descriptor_type_specific_byte0;	/* Descriptor type specific.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	descriptor_type_specific_byte1 : 6,	/* Descriptor type specific.         (b0:5) */
	descriptor_type		: 2;		/* The descriptor type.		     (b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
        descriptor_type		: 2,		/* The descriptor type.		     (b6:7) */
	descriptor_type_specific_byte1 : 6;	/* Descriptor type specific.         (b0:5) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
						/* Descriptor type specific follows...	   */
} sas_protocol_information_t;

/*
 * Protocol Identifier for SAS Array Device Slot Elements. 
 * Note: Valid for Device Slot elements too! 
 */
typedef struct sas_protocol_array {
    uint8_t	number_phy_descriptors;		/* Number of phy descriptors.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	not_all_phys		: 1,		/* Not all phys.		       (b0) */
	reserved_byte1_b1_5	: 5,		/* Reserved.			     (b1:5) */
	descriptor_type		: 2;		/* The descriptor type.		     (b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
        descriptor_type		: 2,		/* The descriptor type.		     (b6:7) */
	reserved_byte1_b1_5	: 5,		/* Reserved.			     (b1:5) */
	not_all_phys		: 1;		/* Not all phys.		       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	reserved_byte2;			/* Reserved.				[2] */
    uint8_t	device_slot_number;		/* Device slot number.			[3] */
} sas_protocol_array_t;

/*
 * For Reference: 
 *      SMP = Serial Management Protocol
 *      STP = Serial ATA Tunneling Protocol
 *      SSP = Serial SCSI Protocol
 */

/*
 * Define the SAS Device Types: (from IDENTIFY address frame transmitted by the phy)
 */
#define SAS_DTYPE_NO_DEVICE_ATTACHED	0x00
#define SAS_DTYPE_END_DEVICE		0x01
#define SAS_DTYPE_EXPANDER_DEVICE	0x02

typedef struct sas_array_phy_descriptor {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[0] */
	reserved_byte0_b0_3	: 4,		/* Reserved.			     (b0:3) */
	device_type		: 3,		/* The device type.		     (b4:6) */
	reserved_byte0_b7	: 1;		/* Reserved.			       (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[0] */
	reserved_byte0_b7	: 1,		/* Reserved.			       (b7) */
	device_type		: 3,		/* The device type.		     (b4:6) */
	reserved_byte0_b0_3	: 4;		/* Reserved.			     (b0:3) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	reserved_byte1;			/* Reserved.				[1] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[2] */
	reserved_byte2_b0	: 1,		/* Reserved.			       (b0) */
	smp_initiator_port	: 1,		/* SMP initiator port.		       (b1) */
	stp_initiator_port	: 1,		/* STP initiator port.		       (b2) */
	ssp_initiator_port	: 1,		/* SSP initiator port.		       (b3) */
	reserved_byte2_b4_4	: 4;		/* Reserved.			     (b4:4) */
    bitfield_t					/*					[3] */
	sata_device		: 1,		/* SATA device.			       (b0) */
	smp_target_port		: 1,		/* SMP target port.		       (b1) */
	stp_target_port		: 1,		/* STP target port.		       (b2) */
	ssp_target_port		: 1,		/* SSP target port.		       (b3) */
	reserved_byte3_b4_6	: 3,		/* Reserved.			     (b4:6) */
	sata_port_selector	: 1;		/* SATA port selector.		       (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[2] */
	reserved_byte2_b4_4	: 4,		/* Reserved.			     (b4:4) */
	ssp_initiator_port	: 1,		/* SSP initiator port.		       (b3) */
	stp_initiator_port	: 1,		/* STP initiator port.		       (b2) */
	smp_initiator_port	: 1,		/* SMP initiator port.		       (b1) */
	reserved_byte2_b0	: 1;		/* Reserved.			       (b0) */
    bitfield_t					/*					[3] */
	sata_port_selector	: 1,		/* SATA port selector.		       (b7) */
	reserved_byte3_b4_6	: 3,		/* Reserved.			     (b4:6) */
	ssp_target_port		: 1,		/* SSP target port.		       (b3) */
	stp_target_port		: 1,		/* STP target port.		       (b2) */
	smp_target_port		: 1,		/* SMP target port.		       (b1) */
	sata_device		: 1;		/* SATA device.			       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	attached_sas_address[8];	/* Attached SAS address.	     [4-11] */
    uint8_t	sas_address[8];			/* The SAS address.		    [12-19] */
    uint8_t	phy_identifier;			/* The phy identifier.		       [20] */
    uint8_t	reserved_byte21;		/* Reserved.			       [21] */
    uint8_t	reserved_byte22;		/* Reserved.			       [22] */
    uint8_t	reserved_byte23;		/* Reserved.			       [23] */
    uint8_t	reserved_byte24;		/* Reserved.			       [24] */
    uint8_t	reserved_byte25;		/* Reserved.			       [25] */
    uint8_t	reserved_byte26;		/* Reserved.			       [26] */
    uint8_t	reserved_byte27;		/* Reserved.			       [27] */
} sas_array_phy_descriptor_t;

/*
 * Protocol Identifier for SAS Enclosure Services Controller Electronics (ESCE)
 */
typedef struct sas_protocol_esce {
    uint8_t	number_phy_descriptors;		/* Number of phy descriptors.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_5	: 6,		/* Reserved.			     (b0:5) */
	descriptor_type		: 2;		/* The descriptor type.		     (b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
        descriptor_type		: 2,		/* The descriptor type.		     (b6:7) */
	reserved_byte1_b0_5	: 6;		/* Reserved.			     (b0:5) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	reserved_byte2;			/* Reserved.				[2] */
	uint8_t	reserved_byte3;			/* Reserved.				[3] */
						/* Phy descriptor list.			    */
} sas_protocol_esce_t;

typedef struct sas_esce_phy_descriptor {
    uint8_t	phy_identifier;			/* The phy identifier.			[0] */
    uint8_t	reserved_byte1;			/* Reserved.				[1] */
    uint8_t	connector_element_index;	/* The connector element index.		[2] */
    uint8_t	other_element_index;		/* The other element index.		[3] */
    uint8_t	sas_address[8];			/* The SAS address.		     [4-11] */
} sas_esce_phy_descriptor_t;

/*
 * Protocol Identifier for SAS Expander:
 * Note: Valid for SCSI Inititor Port and Target Port.
 */
typedef struct sas_protocol_expander {
    uint8_t	number_expander_phy_descriptors;/* Number of expander phy descriptors.	[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	reserved_byte1_b0_5	: 6,		/* Reserved.			     (b0:5) */
	descriptor_type		: 2;		/* The descriptor type.		     (b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
        descriptor_type		: 2,		/* The descriptor type.		     (b6:7) */
	reserved_byte1_b0_5	: 6;		/* Reserved.			     (b0:5) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	reserved_byte2;			/* Reserved.				[2] */
    uint8_t	reserved_byte3;			/* Reserved.				[3] */
    uint8_t	sas_address[8];			/* The SAS address.		     [4-11] */
						/* Expander phy descriptor list.	    */
} sas_protocol_expander_t;

#define PHY_NOT_CONNECTED	0xFF

typedef struct sas_expander_phy_descriptor {
    uint8_t	connector_element_index;	/* The connector element index.		[0] */
    uint8_t	other_element_index;		/* The other element index.		[1] */
} sas_expander_phy_descriptor_t;

/*
 * PCIe Protocol Identifier Information:
 */
#define PCIe_SERIAL_NUMBER_LENGTH	20
#define PCIe_MODEL_NUMBER_LENGTH	40

#define PCIe_NVME	0x01

typedef struct pcie_protocol_information {
    uint8_t	number_of_ports;		/* The number of ports.			[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[1] */
	not_all_ports		: 1,		/* Not all ports.		       (b0) */
	reserved_byte1_b1_4	: 4,		/* Reserved.			     (b1:4) */
	PCIe_protocol_type	: 3;		/* PCIe protocol type.		     (b4:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[1] */
	PCIe_protocol_type	: 3,		/* PCIe protocol type.		     (b4:7) */
	reserved_byte1_b1_4	: 4,		/* Reserved.			     (b1:4) */
	not_all_ports		: 1;		/* Not all ports.		       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	reserved_byte2;			/* Reserved.				[2] */
    uint8_t	device_slot_number;		/* Device slot number.			[3] */
    uint8_t	reserved_byte4;			/* Reserved.				[4] */
    uint8_t	reserved_byte5;			/* Reserved.				[5] */
    uint8_t	pcie_vendor_id[2];		/* The PCIe vendor ID.		      [6-7] */
    uint8_t	serial_number[PCIe_SERIAL_NUMBER_LENGTH];
						/* The serial number.		     [8-27] */
    uint8_t	model_number[PCIe_MODEL_NUMBER_LENGTH];
						/* The model number.		    [28-67] */
						/* Physical port descriptor list.	    */
} pcie_protocol_information_t;

typedef struct nvme_port_descriptor {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[0] */
	cid_valid		: 1,		/* Controller ID valid.		       (b0) */
	bdf_valid		: 1,		/* Bus Device Function valid.	       (b1) */
	psn_valid		: 1,		/* Physical slot number valid.	       (b2) */
	reserved_byte0_b3_7	: 5;		/* Reserved.			     (b3:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[0] */
	reserved_byte0_b3_7	: 5,		/* Reserved.			     (b3:7) */
	psn_valid		: 1,		/* Physical slot number valid.	       (b2) */
	bdf_valid		: 1,		/* Bus Device Function valid.	       (b1) */
	cid_valid		: 1;		/* Controller ID valid.		       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	controller_id[2];		/* The controller ID.		      [1-2] */
    uint8_t	reserved_byte3;			/* Reserved.				[3] */
    uint8_t	bus_number;			/* The bus number.			[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t					/*					[5] */
	function_number		: 3,		/* The function number.		     (b0:2) */
	device_number		: 5;		/* The device number.		     (b3:7) */
    uint8_t	physical_slot_number_lsb;	/* The physical slot number (LSB).	[6] */
    bitfield_t					/*					[7] */
	physical_slot_number_msb : 5,		/* The physical slot number (MSB).   (b0:4) */
	reserved_byte7_b5_7	: 3;		/* Reserved.			     (b5:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t					/*					[5] */
	device_number		: 5,		/* The device number.		     (b3:7) */
	function_number		: 3;		/* The function number.		     (b0:2) */
    uint8_t	physical_slot_number_lsb;	/* The physical slot number (LSB).	[6] */
    bitfield_t					/*					[7] */
	reserved_byte7_b5_7	: 3,		/* Reserved.			     (b5:7) */
	physical_slot_number_msb : 5;		/* The physical slot number (MSB).   (b0:4) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} nvme_port_descriptor_t;

/* ============================================================================================== */
/*
 * SES Download Microcode Status Page 0x0E:
 */
typedef struct ses_download_microcode_page {
    uint8_t	page_code;			/* The page code.			[0] */
    uint8_t	secondary_enclosures;		/* Number of secondary enclosures.	[1] */
    uint8_t	page_length[2];			/* The page length.		      [2-3] */
    uint8_t	generation_number[4];		/* The generation code.	  	      [4-7] */
						/* Download descriptor list.		    */
} ses_download_microcode_page_t;

/*
 * Note: The first descriptor is for the primary subenclosure.
 */
typedef struct ses_download_microcode_descriptor {
    uint8_t	reserved_byte0;			/* Reserved.				[0] */
    uint8_t	subenclosure_identifier;	/* The subenclosure identifier.		[1] */
    uint8_t	download_microcode_status;	/* Download microcode status.		[2] */
    uint8_t	download_additional_status;	/* Download additional status.		[3] */
    uint8_t	microcode_maximim_size[4];	/* Download microcode maximum size.   [4-7] */
    uint8_t	reserved_byte8;			/* Reserved.				[8] */
    uint8_t	reserved_byte9;			/* Reserved.				[9] */
    uint8_t	reserved_byte10;		/* Reserved.			       [10] */
    uint8_t	download_expected_buffer_id;	/* Download expected buffer ID.	       [11] */
    uint8_t	download_expected_buffer_offset[4]; /* Download expected buf offset.[12-15] */
} ses_download_microcode_descriptor_t;

/*
 * Download Microcode Status Codes:
 */
typedef enum {
    /* Interim Status Codes */
    DMS_NO_OPERATION_IN_PROGRESS = 0x00,	/* No download microcode is in progress.	*/
    DMS_OPERATION_IS_IN_PROGRESS = 0x01,	/* A download microcode is in progress.		*/
    DMS_COMPLETE_UPDATE_NONVOLATILE = 0x02,	/* Download complete, updating nonvolatile storage. */
    DMS_UPDATING_NONVOLATILE_DEFERRED_MICROCODE = 0x03,
						/* Updating nonvolatile w/deferred microcode.	 */
    DMS_INTERIM_STATUS_RESERVED_START = 0x04,	/* Interim status codes reserved start.		*/
    DMS_INTERIM_STATUS_RESERVED_END = 0x0F,	/* Interim status codes reserved end.		*/

    /* Completed with No Error Codes */
    DMS_COMPLETE_NO_ERROR_STARTING = 0x10,	/* Download complete, no errorr, start using now. */
    DMS_COMPLETE_NO_ERROR_START_AFTER_RESET_POWER_CYCLE = 0x11,
						/* Download complete, no error, start using after reset or power cycle. */
    DMS_COMPLETE_NO_ERROR_START_AFTER_POWER_CYCLE = 0x12,
						/* Download complete, no error, start using after power cycle. */
    DMS_COMPLETE_NO_ERROR_START_AFTER_ACTIVATE_MC = 0x13,
						/* Download complete, no error, start after activate MC, reset, or power cycle. */
    DMS_COMPLETE_RESERVED_START = 0x14,		/* Download complete reserved start.		*/
    DMS_COMPLETE_RESERVED_END = 0x6F,		/* Download complete reserved end.		*/

    /* Completed with Error Codes */
    DMS_DOWNLOAD_ERROR_MICROCODE_DISCARDED = 0x80,
						/* Download error, microcode discarded.	*/
    DMS_MICROCODE_IMAGE_ERROR_DISCARDED = 0x81,	/* Microcode image error, microcode discarded.	*/
    DMS_DOWNLOAD_TIMEOUT_MICROCODE_DISCARDED = 0x82,
						/* Download timeout, microcode discarded.	*/
    DMS_INTERNAL_ERROR_NEW_MICROCODED_NEEDED = 0x83,
						/* Internal error, new microcode needed before reset. */
    DMS_INTERNAL_ERROR_HARD_RESET_POWER_ON_SAFE = 0x84,
						/* Internal error, hard reset and power on safe. */
    DMS_PROCESSED_ACTIVATE_DEFERRED_MICROCODE = 0x85,
						/* Processed activate deferred microcode.	*/
    DMS_ERROR_RESERVED_START = 0x86,		/* Download error reserved start.		*/
    DMS_ERROR_RESERVED_END = 0xEF,		/* Download error reserved end.			*/

    /* Vendor Specific Codes */
    DMS_VENDOR_START = 0xF0,			/* Vendor specific start code.			*/
    DMS_VENDOR_END = 0xFF			/* Vendor specific end code.			*/

} ses_download_status_t;

#endif /* !defined(SCSI_SES_H) */
