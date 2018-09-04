#if !defined(SCSI_LOG_H)
#define SCSI_LOG_H 1
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
 * File:	scsi_log.h						*
 * Date:	February 17th, 2017					*
 * Author:	Robin T. Miller						*
 *									*
 * Description:								*
 *	Definitions for SCSI Log Pages.					*
 *									*
 * Modification History:						*
 *									*
 ************************************************************************/

#if !defined(_BITFIELDS_LOW_TO_HIGH_) && !defined(_BITFIELDS_HIGH_TO_LOW_)
#   error "bitfield ordering is NOT defined!"
#endif /* !defined(_BITFIELDS_LOW_TO_HIGH_) || !defined(_BITFIELDS_HIGH_TO_LOW_) */

/*
 * Log Page Codes: (Note: This list is likely dated!)
 */
#define LOG_ALL_PAGES		0x00	/* Supported log pages.		*/
#define LOG_OVER_UNDER_PAGE	0x01	/* Buffer over-run/under-run.	*/
#define LOG_WRITE_ERROR_PAGE	0x02	/* Write error counter page.	*/
#define LOG_READ_ERROR_PAGE	0x03	/* Read error counter page.	*/
#define LOG_READREV_ERROR_PAGE	0x04	/* Read reverse error counter.	*/
#define LOG_VERIFY_ERROR_PAGE	0x05	/* Verify error counter page.	*/
#define LOG_NONMED_ERROR_PAGE	0x06	/* Non-medium error counter.	*/
#define LOG_LASTN_EVENTS_PAGE	0x07	/* Last n error events page.	*/

#define LOG_FORMAT_STATUS_PAGE	0x08	/* Format Status page.		*/
#define LOG_LASTN_DEFFERED_PAGE 0x0B	/* Last n Deferred or Async Events */
#define LOG_SEQUENTIAL_PAGE	0x0A	/* Sequential-Access Device.	*/
#define LOG_BLOCK_PROVISION_PAGE 0x0C	/* Logical Block Provisioning.	*/
#define LOG_TEMPERATURE_PAGE	0x0D	/* Temperature page.		*/
#define LOG_START_STOP_PAGE	0x0E	/* Start-stop Cycle Counter.	*/
#define LOG_APP_CLIENT_PAGE	0x0F	/* Application Client page.	*/
#define LOG_SELF_TEST_PAGE	0x10	/* Self-Test Results page.	*/
#define LOG_SOLID_STATE_PAGE	0x11	/* Solid State Media page.	*/
#define LOG_BACK_SCAN_PAGE	0x15	/* Background-Scan Results.	*/
#define LOG_NONVOL_CACHE_PAGE	0x17	/* Non-Volatile Cache.		*/
#define LOG_PROTOCOL_SPEC_PAGE	0x18	/* Protocol Specific Port.	*/
#define LOG_STATS_PERF_PAGE	0x19	/* Statistics and Performance.	*/
#define LOG_INFO_EXCEPT_PAGE	0x2F	/* Informational Exceptions.	*/

#define LOG_VENDOR_START	0x30	/* Vendor-specific start value.	*/
#define LOG_VENDOR_END		0x3E	/* Vendor-specific ending value	*/
#define LOG_RESERVED_START	0x3F	/* Reserved starting page value	*/
#define LOG_RESERVED_END	0xFE	/* Reserved ending page value.	*/
#define LOG_LAST_RESERVED	0x3F	/* Last page is also reserved.	*/

#define LOG_PAGE_UNKNOWN	-1

/*
 * Log Page Control Field Defines:
 */
#define LOG_PCF_CURRENT_THRESHOLD  0x00 /* Current threshold values.  */
#define LOG_PCF_CURRENT_CUMULATIVE 0x01 /* Current cumulative values. */
#define LOG_PCF_DEFAULT_THRESHOLD  0x02 /* Default threshold values.  */
#define LOG_PCF_DEFAULT_CUMULATIVE 0x03 /* Default cumulative values. */

typedef struct log_page_header {
    bitfield_t				/*				[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	log_page_code		: 6,	/* The log page code.	     (b0:5) */
	reserved_byte0_b6_7	: 2;	/* Reserved.		     (b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	reserved_byte0_b6_7	: 2,	/* Reserved.		     (b6:7) */
	log_page_code		: 6;	/* The log page code.	     (b0:5) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	log_subpage_code;	/* The subpage code.		[1] */
	uint8_t	log_page_length[2];	/* Log page length.	      [2-3] */
} log_page_header_t;

/*
 * Format and Linking Defintions:
 */
#define BOUNDED_DATA_COUNTER		0x00
#define ASCII_FORMAT_LIST		0x01
#define BOUNDED_UNBOUNDED_DATA_COUNTER	0x02
#define BINARY_FORMAT_LIST		0x03

/*
 * Log Parameter Header proceeding page parameters.
 */
typedef struct log_parameter_header {
    uint8_t	log_parameter_code[2];	/* Log parameter code (MSB).   [0-1] */
    bitfield_t				/*				 [2] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	log_format_linking	: 2,	/* Format and linking.	      (b0:1) */
	obsolete_byte2_b2_4	: 3,	/* Obsolete.		      (b2:4) */
	log_tsd			: 1,	/* Target save disable.		(b5) */
	obsolete_byte2_b6	: 1,	/* Obsolete. 			(b6) */
	log_du			: 1;	/* Disable update.		(b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	log_du			: 1,	/* Disable update.		(b7) */
	obsolete_byte2_b6	: 1,	/* Obsolete. 			(b6) */
	log_tsd			: 1,	/* Target save disable.		(b5) */
	obsolete_byte2_b2_4	: 3,	/* Obsolete.		      (b2:4) */
	log_format_linking	: 2;	/* Format and linking.	      (b0:1) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t  log_parameter_length;	/* The parameter length.	 [3] */
} log_parameter_header_t;

typedef struct log_page {
	log_page_header_t log_hdr;	/* Log page header.		*/
	log_parameter_header_t log_phdr;/* Log parameter header.	*/
	uint8_t log_data[1];		/* Variable length log data.	*/
} log_page_t;

/*
 * Buffer Overrun/Underrun Counter Definitions:
 */
#define LOG_TYPE_OVERRUN	0x01	/* Parameter type is over-run.	*/
					/* 	0 = under-run counter.	*/
/*
 * Cause Field Definitions:
 */
#define CFD_UNDEFINED		0x00	/* Undefined.			*/
#define CFD_SCSI_BUS_BUSY	0x01	/* SCSI bus busy.		*/
#define CFD_XFER_RATE_TOO_SLOW	0x02	/* Transfer rate too slow.	*/
					/* 0x03-0x0F are reserved.	*/
/*
 * Count Basis Definitions:
 */
#define CBD_UNDEFINED		0x00	/* Undefined.			*/
#define CBD_PER_COMMAND		0x01	/* Per command.			*/
#define CBD_PER_FAILED_RECON	0x02	/* Per failed reconnect.	*/
#define CBD_PER_UNIT_OF_TIME	0x03	/* Per unit of time.		*/
					/* 0x04 - 0x07 are reserved.	*/
typedef struct overrun_underrun_params {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    uint8_t	oup_type	: 1,	/* The counter type		(b0) */
	oup_cause	: 4,		/* The cause definition.      (b1:4) */
	oup_basis	: 3;		/* The count basis.	      (b5:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    uint8_t	oup_basis	: 3,	/* The count basis.	      (b5:7) */
	oup_cause	: 4,		/* The cause definition.      (b1:4) */
	oup_type	: 1;		/* The counter type	        (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} overrun_underrun_params_t;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*
 * Note: Page definitions do not include page header and parameter header.
 *       Thus, we essential start at byte 4 if there is no parameter header
 *       or byte 8 if there is a parameter header (both defined above).
 */

/*
 * Temperature Page Parameters:
 */
#define TLP_TEMP_PARAM      0x0000
#define TLP_REF_TEMP_PARAM  0x0001

#define TLP_TEMP_LESS_ZERO  0x00
#define TLP_TEMP_NOT_AVAIL  0xFF

/*
 * Temperature Log Page Parameters:
 */
typedef struct temp_log_param {
    uint8_t  tlp_reserved;               /* Reserved.                    */
    uint8_t  tlp_temperature;            /* Temperature in Celsius.      */
} temp_log_param_t;

typedef struct ref_temp_log_param {
    uint8_t  rtp_reserved;               /* Reserved.                    */
    uint8_t  rtp_ref_temperature;        /* Reference temp in Celsius.   */
} ref_temp_log_param_t;

/* ============================================================================================== */

#define PROTOCOL_PRIMARY_PORT		0x01
#define PROTOCOL_SECONDARY_PORT		0x02

/*
 * Protocol Identifier Definitions:
 */
#define PROTOCOL_ID_Fibre_Channel_Protocol	0x00	/* SCSI FCP-4 */
#define PROTOCOL_ID_Obsolete			0x01
#define PROTOCOL_ID_Serial_Storage_Architecture 0x02	/* SCSI-3 Protocol SSA-S3P */
#define PROTOCOL_ID_Serial_Bus_Protocol		0x03	/* IEEE 1394 SBP-3 */
#define PROTOCOL_ID_SCSI_RDMA_Protocol		0x04	/* SRP */
#define PROTOCOL_ID_Internet_SCSI_iSCSI		0x05	/* iSCSI */
#define PROTOCOL_ID_SAS_Serial_SCSI_Protocol	0x06	/* SPL-4 */
#define PROTOCOL_ID_Automation_Drive_Interface	0x07	/* ADT-2 */
#define PROTOCOL_ID_AT_Attachment_Interface	0x08	/* ACS-2 */
#define PROTOCOL_ID_USB_Attached_SCSI	 	0x09	/* UAS */
#define PROTOCOL_ID_SCSI_over_PCI_Express	0x0A	/* SOP */
#define PROTOCOL_ID_PCI_Express_Protocols	0x0B	/* PCIe */
#define PROTOCOL_ID_RESERVED_0x0C		0x0C
#define PROTOCOL_ID_RESERVED_0x0D		0x0D
#define PROTOCOL_ID_RESERVED_0x0E		0x0E
#define PROTOCOL_ID_No_Specific_Protocol	0x0F

/*
 * Idenitfy Reason Definitions:
 */
#define REASON_Power_On				0x00
#define REASON_Open_Connection_Request		0x01
#define REASON_Hard_Reset			0x02
#define REASON_SMP_PHY_CONTROL_Function		0x03
#define REASON_Loss_of_Dword_Synchronization	0x04
#define REASON_Multiplexing_Sequence_Mixup	0x05
#define REASON_I_T_Nexus_Loss_Timer_Expired	0x06
#define REASON_Break_Timeout_Timer_Expired	0x07
#define REASON_Phy_Test_Function_Stopped	0x08
#define REASON_Expander_Reduced_Functionality	0x09
/* Note: All others reserved! */

/*
 * Negotiated Physical Link Rates:
 */
#define LINK_RATE_UNKNOWN			0x00
#define LINK_RATE_PHY_DISABLED			0x01
#define LINK_RATE_SPEED_NEGOTIATION_FAILED	0x02
#define LINK_RATE_SATA_SPINUP_HOLD_STATE	0x03
#define LINK_RATE_PORT_SELECTOR			0x04
#define LINK_RATE_RESET_IN_PROGRESS		0x05
#define LINK_RATE_UNSUPPORTED_PHY_ATTACHED	0x06
#define LINK_RATE_RESERVED_0x07			0x07
#define LINK_RATE_SPEED_1_5Gbps			0x08
#define LINK_RATE_SPEED_3Gbps			0x09
#define LINK_RATE_SPEED_6Gbps			0x0A
#define LINK_RATE_SPEED_12Gbps			0x0B
#define LINK_RATE_SPEED_22_5Gbps		0x0C
#define LINK_RATE_RESERVED_0x0D			0x0D
#define LINK_RATE_RESERVED_0x0E			0x0E
#define LINK_RATE_RESERVED_0x0F			0x0F

typedef struct sas_phy_log_descriptor {
    uint8_t	reserved_byte0;		/* Reserved.			[0] */
    uint8_t	phy_identifier;		/* Phy identifier.		[1] */
    uint8_t	reserved_byte2;		/* Reserved.			[2] */
    uint8_t	sas_phy_log_descriptor_length;
					/* SAS Phy log descriptor length.[3] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[4] */
	attached_reason		: 4,	/* Attached reason.	     (b0:3) */
	attached_device_type	: 3,	/* Attached device type.     (b4:6) */
	reserved_byte4_b7	: 1;	/* Reserved.		       (b7) */
    bitfield_t				/*				[5] */
	negotiated_physical_link_rate : 4, /* Negotiated phyical link rate. (b0:3) */
	reason			: 4;	/* Last link reset reason.   (b4:7) */
    bitfield_t				/*				[6] */
	reserved_byte6_b0	: 1,	/* Reserved.		       (b0) */
	smp_initiator_port	: 1,	/* SMP initiator port.	       (b1) */
	stp_initiator_port	: 1,	/* STP initiator port.	       (b2) */
	ssp_initiator_port	: 1,	/* SSP initiator port.	       (b3) */
	reserved_byte6_b4_4	: 4;	/* Reserved.		     (b4:4) */
    bitfield_t				/*				[7] */
	reserved_byte7_b0	: 1,	/* SATA device.		       (b0) */
	smp_target_port		: 1,	/* SMP target port.	       (b1) */
	stp_target_port		: 1,	/* STP target port.	       (b2) */
	ssp_target_port		: 1,	/* SSP target port.	       (b3) */
	reserved_byte7_b4_7	: 4;	/* Reserved.		     (b4:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[4] */
	reserved_byte4_b7	: 1,	/* Reserved.		       (b7) */
	attached_device_type	: 3,	/* Attached device type.     (b4:6) */
	attached_reason		: 4;	/* Attached reason.	     (b0:3) */
    bitfield_t				/*				[5] */
	reason			: 4,	/* Last link reset reason.   (b4:7) */
	negotiated_physical_link_rate : 4; /* Negotiated physical link rate. (b0:3) */
    bitfield_t				/*				[6] */
	reserved_byte6_b4_4	: 4,	/* Reserved.		     (b4:4) */
	ssp_initiator_port	: 1,	/* SSP initiator port.	       (b3) */
	stp_initiator_port	: 1,	/* STP initiator port.	       (b2) */
	smp_initiator_port	: 1,	/* SMP initiator port.	       (b1) */
	reserved_byte6_b0	: 1;	/* Reserved.		       (b0) */
    bitfield_t				/*				[7] */
	reserved_byte7_b4_7	: 4,	/* Reserved.		     (b4:7) */
	ssp_target_port		: 1,	/* SSP target port.	       (b3) */
	stp_target_port		: 1,	/* STP target port.	       (b2) */
	smp_target_port		: 1,	/* SMP target port.	       (b1) */
	reserved_byte7_b0	: 1;	/* SATA device.		       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	sas_address[8];		/* The SAS address.	     [8-15] */
    uint8_t	attached_sas_address[8];/* Attached SAS address.    [16-23] */
    uint8_t	attached_phy_identifier;/* Attached Phy identifier.    [24] */
    uint8_t	reserved_byte25;	/* Reserved.		       [25] */
    uint8_t	reserved_byte26;	/* Reserved.		       [26] */
    uint8_t	reserved_byte27;	/* Reserved.		       [27] */
    uint8_t	reserved_byte28;	/* Reserved.		       [28] */
    uint8_t	reserved_byte29;	/* Reserved.		       [29] */
    uint8_t	reserved_byte30;	/* Reserved.		       [30] */
    uint8_t	reserved_byte31;	/* Reserved.		       [31] */

    uint8_t	invalid_dword_count[4];	/* Invalid dword count.	    [32-35] */
    uint8_t	running_disparity_error_count[4];
				/* Running disparity error count.   [36-39] */
    uint8_t	loss_of_dword_synchronization[4];
				/* Loss of dword synchronization.   [40-43] */
    uint8_t	phy_reset_problem[4];	/* Phy reset problem.	    [44-47] */
    uint8_t	reserved_byte48;	/* Reserved.		       [48] */
    uint8_t	reserved_byte49;	/* Reserved.		       [49] */
    uint8_t	phy_event_descriptor_length;
					/* Phy event descriptor length.[50] */
    uint8_t	number_of_event_descriptors;
					/* Number of event descriptors.[51] */
} sas_phy_log_descriptor_t;

/*
 * Phy Event Types:
 */
#define PHY_EVENT_INVALID_DWORD_COUNT		0x01
#define PHY_EVENT_RUNNING_DISPARITY_ERROR_COUNT	0x02
#define PHY_EVENT_LOSS_OF_DWORD_SYNC		0x03
#define PHY_EVENT_PHY_RESET_PROBLEM		0x04

typedef struct phy_event_descriptor {
    uint8_t	reserved_byte0;		/* Reserved.			[0] */
    uint8_t	reserved_byte1;		/* Reserved.			[1] */
    uint8_t	reserved_byte2;		/* Reserved.			[2] */
    uint8_t	phy_event_source;	/* Phy event source.		[3] */
    uint8_t	phy_event[4];		/* The Phy event.	      [4-7] */
    uint8_t	peak_value_detector_threshold[4];
					/* Peak value detector threshold. [8-11] */
} phy_event_descriptor_t;

/*
 * Log Page Protocol Specific Port Parameter for SAS Target Ports:
 */
typedef struct log_protocol_specific {
    log_parameter_header_t phdr;	/* The log parameter header.  [0-3] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[4] */
	protocol_identifier	: 4,	/* Protocol identifier.	     (b0:3) */
	reserved_byte4_b4_4	: 4;	/* Reserved.		     (b4:4) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[4] */
	reserved_byte4_b4_4	: 4,	/* Reserved.		     (b4:4) */
	protocol_identifier	: 4;	/* Protocol identifier.	     (b0:3) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	reserved_byte5;		/* Reserved.			[5] */
    uint8_t	generation_code;	/* Generation code.		[6] */
    uint8_t	number_of_phys;		/* Number of PHYs.		[7] */

    /* SAS Phy Log Descriptors */

    /* Phy Event Descriptors */

} log_protocol_specific_t;

/*
 * External Declarations: 
 */
extern int log_select_encode(void *arg);
extern int log_sense_encode(void *arg);
extern int log_sense_decode(void *arg);

extern uint8_t find_log_page_code(scsi_device_t *sdp, char *page_name, int *status);
extern int setup_log_select(scsi_device_t *sdp, scsi_generic_t *sgp, size_t data_length, uint8_t page);
extern int setup_zero_log(scsi_device_t *sdp, scsi_generic_t *sgp, uint8_t page);
extern int setup_log_sense(scsi_device_t *sdp, scsi_generic_t *sgp, size_t data_length, uint8_t page);

#endif /* !defined(SCSI_LOG_H) */
