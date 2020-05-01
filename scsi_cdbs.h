#if !defined(SCSI_CDBS_INCLUDE)
#define SCSI_CDBS_INCLUDE 1
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
 * File:	scsi_cdbs.h
 * Date:	April 9, 1991
 * Author:	Robin T. Miller
 *
 * Description:
 *      SCSI Command Block Descriptor definitions.
 *
 * Modification History:
 * 
 * January 5th, 2019 by Robin T. Miller
 *      Adding Get LBA Status, Unmap, and Report LUNs definitions.
 * 
 * December 18th, 2018 by Robin T. Miller
 *      Increase the Inquiry allocation length, introduced in SPC-3.
 * 
 * June 7th, 2018 by Robin T. Miller
 *      Add ATA Pass-Through 16 byte CDB definition.
 * 
 * March 19th, 2018 by Robin T. Miller
 *      Update Format Unit CDB, woefully out of date.
 *      Updates based on SCSI Block Commands SBC-4, 20 May 2014.
 *
 * October 6th, 2015 by Robin T. Miller
 * 	Adding CDB's from libscsi.c to here.
 * 	Updating bitfields for native AIX compiler.
 * 	AIX does NOT like uint8_t or unnamed bit fields!
 * 
 * July 31st, 2015 by Robin T. Miller
 * 	Modify/add new SCSI CDB's (16-byte, etc)
 */

#if defined(__IBMC__)
/* IBM aligns bit fields to 32-bits by default! */
#  pragma options align=bit_packed
#endif /* defined(__IBMC__) */

/*
 * MACRO for converting a longword to a byte
 *	args - the longword
 *	     - which byte in the longword you want
 */
//#define	LTOB(a,b)	(uint8_t)((a>>(b*8))&0xff)

/************************************************************************
 *									*
 *			  Generic SCSI Commands				*
 *									*
 ************************************************************************/

/* 
 * Test Unit Ready Command Descriptor Block:
 */
struct TestUnitReady_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/* ============================================================================================== */

/*
 * Inquiry Command Descriptor Block:
 */
struct Inquiry_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    evpd		: 1,	/* Enable Vital Product Data.	[1] */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    evpd		: 1;	/* Enable Vital Product Data.	[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	page_code;		/* EVPD Page Code.		[2] */
	uint8_t	allocation_length[2];	/* Allocation Length.	      [3-4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/* ============================================================================================== */

#define ATA_PASSTHROUGH_OPCODE	0x85
#define ATA_IDENTIFY_COMMAND	0xEC

#define IDENTIFY_SERIAL_OFFSET	20
#define IDENTIFY_SERIAL_LENGTH	20

#define IDENTIFY_FW_OFFSET	46
#define IDENTIFY_FW_LENGTH	8

#define IDENTIFY_MODEL_OFFSET	64
#define IDENTIFY_MODEL_LENGTH	40

#define IDENTIFY_DATA_LENGTH	512
#define IDENTIFY_SECTOR_COUNT	1

#define PROTOCOL_HARD_RESET	0
#define PROTOCOL_SRST		1
#define PROTOCOL_NON_DATA	3
#define PROTOCOL_PIO_DATA_IN	4
#define PROTOCOL_PIO_DATA_OUT	5
#define PROTOCOL_DMA		6
#define PROTOCOL_DMA_QUEUED	7
#define PROTOCOL_DIAGNOSTIC	8
#define PROTOCOL_DEVICE_RESET	9
#define PROTOCOL_UDMA_DATA_IN	10
#define PROTOCOL_UDMA_DATA_OUT	11
#define PROTOCOL_FPDMA		12
#define PROTOCOL_RESPONSE_INFO	15

#define BYT_BLOK_TRANSFER_BYTES	 0
#define BYT_BLOK_TRANSFER_BLOCKS 1

#define T_DIR_TO_ATA_DEVICE	0
#define T_DIR_FROM_ATA_DEVICE	1

#define T_LENGTH_NO_DATA	0x00	/* No data is transferred. */
#define T_LENGTH_FEATURE_FIELD	0x01	/* Transfer count specified in the feature field. */
#define T_LENGTH_SECTOR_COUNT	0x02	/* Transfer count specified in sector count field. */
#define T_LENGTH_STPSIU		0x03	/* Transfer count specified in STPSIU. */

/*
 * ATA Pass-Through 16 Command Descriptor Block.
 */
typedef struct AtaPassThrough16_CDB {
    uint8_t	opcode;			/* Operation code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[1] */
	extend			: 1,	/*	 		     (b0:1) */
	protocol		: 4,	/*			     (b2:4) */
	multiple_count		: 3;	/*			     (b5:3) */
    bitfield_t				/*				[2] */
	t_length		: 2,	/*			     (b0:1) */
	byt_blok		: 1,	/* 			       (b2) */
	t_dir			: 1,	/*			       (b3) */
	reserved_byte1_b4	: 1,	/* Reserved.		       (b4) */
	ck_cond			: 1,	/* 			       (b5) */
	off_line		: 2;	/*			     (b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[1] */
	multiple_count		: 3,	/*			     (b5:3) */
	protocol		: 4,	/*			     (b2:4) */
	extend			: 2;	/*	 		     (b0:1) */
    bitfield_t				/*				[2] */
	off_line		: 2,	/*			     (b6:7) */
	ck_cond			: 1,	/* 			       (b5) */
	reserved_byte1_b4	: 1,	/* Reserved.		       (b4) */
	t_dir			: 1,	/*			       (b3) */
	byt_blok		: 1,	/* 			       (b2) */
	t_length		: 2;	/*			     (b0:1) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	features_high;		/*				[3] */
    uint8_t	features_low;		/*				[4] */
    uint8_t	sector_count_high;	/*				[5] */
    uint8_t	sector_count_low;	/*				[6] */
    uint8_t	lba_low[2];		/*			      [7-8] */
    uint8_t	lba_mid[2];		/*			     [9-10] */
    uint8_t	lba_high[2];		/*			    [11-12] */
    uint8_t	device;			/*			       [13] */
    uint8_t	command;		/*			       [14] */
    uint8_t	control;		/*			       [15] */
} AtaPassThrough16_CDB_t;

/* ============================================================================================== */
/* Note: The page control field values are defined in scsi_log.h */

#define LOG_SELECT_LENGTH_MAX	0xffff

/* 
 * Log Select Command Descriptor Block:
 */
typedef struct LogSelect_CDB {
    uint8_t	opcode;			/* Operation code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[1] */
	save_parameters		: 1,	/* Save parameters.	       (b0) */
	pcr			: 1,	/* Parameter code reset.       (b1) */
	reserved_byte1_b2_7	: 6;	/* Reserved.		     (b2:7) */
    bitfield_t				/*				[2] */
	page_code		: 6,	/* Page code.		     (b0:5) */
	page_control		: 2;	/* Page control.	     (b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[1] */
	reserved_byte1_b2_7	: 6,	/* Reserved.		     (b2:7) */
	pcr			: 1,	/* Parameter code reset.       (b1) */
	save_parameters		: 1;	/* Save parameters.	       (b0) */
    bitfield_t				/*				[2] */
	page_control		: 2,	/* Page control. 	     (b6:7) */
	page_code		: 6;	/* Page code.		     (b0:5) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	subpage_code;		/* Subpage code.		[3] */
    uint8_t	reserved_byte4_6;	/* Reserved.		      [4-6] */
    uint8_t	parameter_length[2];	/* Parameter length.	      [7-8] */
    uint8_t	control;		/* Control.			[9] */
} LogSelect_CDB_t;

/* ============================================================================================== */

#define LOG_SENSE_LENGTH_MAX	0xffff

/* 
 * Log Sense Command Descriptor Block:
 */
typedef struct LogSense_CDB {
    uint8_t	opcode;			/* Operation code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[1] */
	save_parameters		: 1,	/* Save parameters.	       (b0) */
	obsolete		: 1,	/* Obsolete.		       (b1) */
	reserved_byte1_b2_7	: 6;	/* Reserved.		     (b2:7) */
    bitfield_t				/*				[2] */
	page_code		: 6,	/* Page code.		     (b0:5) */
	page_control		: 2;	/* Page control.	     (b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[1] */
	reserved_byte1_b2_7	: 6,	/* Reserved.		     (b2:7) */
	obsolete		: 1,	/* Obsolete.		       (b1) */
	save_parameters		: 1;	/* Save parameters.	       (b0) */
    bitfield_t				/*				[2] */
	page_control		: 2,	/* Page control. 	     (b6:7) */
	page_code		: 6;	/* Page code.		     (b0:5) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	subpage_code;		/* Subpage code.		[3] */
    uint8_t	reserved_byte4;		/* Reserved.			[4] */
    uint8_t	parameter_pointer[2];	/* Parameter pointer.	      [5-6] */
    uint8_t	allocation_length[2];	/* Allocation length.	      [7-8] */
    uint8_t	control;		/* Control.			[9] */
} LogSense_CDB_t;

/* ============================================================================================== */
/*
 * Mode Sense Command Descriptor Block:
 */
struct ModeSense_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_3	: 3,	/* Reserved.			[1] */
	    dbd			: 1,	/* Disable Block Descriptors.	    */
	    res_byte1_b4_1	: 1,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b4_1	: 1,	/* Reserved.			    */
	    dbd			: 1,	/* Disable Block Descriptors.	    */
	    res_byte1_b0_3	: 3;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    	pgcode	: 6,		/* Page code.			[2] */
		pcf	: 2;		/* Page Control Field.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		pcf	: 2,		/* Page Control Field.		    */
		pgcode	: 6;		/* Page code.			[2] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	alclen;			/* Allocation Length.		[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Mode Select Command Descriptor Block:
 */
struct ModeSelect_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    sp			: 1,	/* Save Parameters.		[1] */
	    res_byte1_b1_3	: 3,	/* Reserved.			    */
	    pf			: 1,	/* Page Format.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    pf			: 1,	/* Page Format.			    */
	    res_byte1_b1_3	: 3,	/* Reserved.			    */
	    sp			: 1;	/* Save Parameters.		[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	pll;			/* Parameter List Length.	[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

#define RECEIVE_DIAGNOSTIC_MAX	0xffff

/*
 * Receive Diagnostic Result Command Descriptor Block:
 */
typedef struct receive_diagnostic_cdb {
	uint8_t	opcode;			/* 1c hex Operation Code.	[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    	pcv	: 1,		/* Page code valid.		[1] */
		res_byte1_b1_7 : 7;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		res_byte1_b1_7 : 7,	/* Reserved.			    */
		pcv	: 1;		/* Page code valid.		[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	page_code;		/* Page code.			[2] */
	uint8_t	allocation_length[2];	/* Allocation length.	      [3-4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		link	: 1,		/* Link.			[5] */
		flag	: 1,		/* Flag.			    */
		naca	: 1,		/* Normal ACA.			    */
		res_byte5_b3_5	: 3,	/* Reserved.			    */
		vendor	: 2;		/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		vendor	: 2,		/* Vendor Unique.		    */
		res_byte5_b3_5	: 3,	/* Reserved.			    */
		naca	: 1,		/* Normal ACA.			    */
		flag	: 1,		/* Flag.			    */
		link	: 1;		/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} receive_diagnostic_cdb_t;

typedef struct diagnostic_page_header {
	uint8_t	page_code;		/* Page code.			[0] */
	uint8_t	page_code_specific;	/* Page code specific.		[1] */
	uint8_t	page_length[2];		/* Page length.		      [2-3] */
} diagnostic_page_header_t;

/*
 * Send Diagnostic Command Descriptor Block:
 */
typedef struct send_diagnostic_cdb {
	uint8_t	opcode;			/* 0x1d Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    	unit_offline : 1,	/* Unit offline.		[1] */
		dev_offline  : 1,	/* Device offline.		    */
		self_test    : 1,	/* Self test (default).		    */
		res_byte1_b3 : 1,	/* Reserved.			    */
		pf           : 1,	/* Page format.			    */
		self_test_code : 3;	/* Self test code.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		self_test_code : 3,	/* Self test code.		    */
		pf           : 1,	/* Page format.			    */
		res_byte1_b3 : 1,	/* Reserved.			    */
		self_test    : 1,	/* Self test (default).		    */
		dev_offline  : 1,	/* Device offline.		    */
	    	unit_offline : 1;	/* Unit offline.		[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	reserved_byte2;		/* Reserved.			[2] */
	uint8_t	parameter_length[2];	/* Parameter length.	      [3-4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    	link	: 1,		/* Link.			[5] */
		flag	: 1,		/* Flag.			    */
		naca	: 1,		/* Normal ACA.			    */
		res_byte5_b3_5	: 3,	/* Reserved.			    */
		vendor	: 2;		/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    	vendor	: 2,		/* Vendor Unique.		    */
		res_byte5_b3_5	: 3,	/* Reserved.			    */
		naca	: 1,		/* Normal ACA.			    */
		flag	: 1,		/* Flag.			    */
		link	: 1;		/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} send_diagnostic_cdb_t;

/*
 * Send Diagnostic Self Test Codes:
 */
#define BackgroundShortSelfTest		0x01
#define BackgroundExtendedSelfTest	0x02
#define AbortBackgroundSelfTest		0x04
#define ForgroundShortSelfTest		0x05
#define ForgroundExtendedSelfTest	0x06

/*
 * Sense Key Codes:
 */
#define SKV_NOSENSE		0x0	/* No error or no sense info.	*/
#define SKV_RECOVERED		0x1	/* Recovered error (success).	*/
#define SKV_NOT_READY		0x2	/* Unit is not ready.		*/
#define SKV_MEDIUM_ERROR	0x3	/* Nonrecoverable error.	*/
#define SKV_HARDWARE_ERROR	0x4	/* Nonrecoverable hardware err.	*/
#define SKV_ILLEGAL_REQUEST	0x5	/* Illegal CDB parameter.	*/
#define SKV_UNIT_ATTENTION	0x6	/* Target has been reset.	*/
#define SKV_DATA_PROTECT	0x7	/* Unit is write protected.	*/
#define SKV_BLANK_CHECK		0x8	/* A no-data condition occured.	*/
#define SKV_COPY_ABORTED	0xA	/* Copy command aborted.	*/
#define SKV_ABORTED_CMD		0xB	/* Target aborted cmd, retry.	*/
#define SKV_EQUAL		0xC	/* Vendor unique, not used.	*/
#define SKV_VOLUME_OVERFLOW	0xD	/* Physical end of media detect	*/
#define SKV_MISCOMPARE		0xE	/* Source & medium data differ.	*/
#define SKV_RESERVED		0xF	/* This sense key is reserved.	*/

/* Note: Use definitions in libscsi.h now! */
#if 0
/*
 * Request Sense Data Format.
 */
struct RequestSenseData {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    error_code		: 4,	/* Error code.			[0] */
	    error_class		: 3,	/* Error class.			    */
	    valid		: 1;	/* Information fields valid.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    valid		: 1,	/* Information fields valid.	    */
	    error_class		: 3,	/* Error class.			    */
	    error_code		: 4;	/* Error code.			[0] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	segment_number;		/* Segment number.		[1] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    sense_key		: 4,	/* Sense key.			[2] */
	    res_byte2_b4	: 1,	/* Reserved.			    */
	    illegal_length	: 1,	/* Illegal length indicator.	    */
	    end_of_medium	: 1,	/* End of medium.		    */
	    file_mark		: 1;	/* Tape filemark detected.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    file_mark		: 1,	/* Tape filemark detected.	    */
	    end_of_medium	: 1,	/* End of medium.		    */
	    illegal_length	: 1,	/* Illegal length indicator.	    */
	    res_byte2_b4	: 1,	/* Reserved.			    */
	    sense_key		: 4;	/* Sense key.			[2] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	info_byte3;		/* Information byte (MSB).	[3] */
	uint8_t	info_byte2;		/*      "       "		[4] */
	uint8_t	info_byte1;		/*      "       "		[5] */
	uint8_t	info_byte0;		/* Information byte (LSB).	[6] */
	uint8_t	addl_sense_length;	/* Additional sense length.	[7] */
	uint8_t	cmd_spec_info3;		/* Command specific info (MSB).	[8] */
	uint8_t	cmd_spec_info2;		/*    "       "      "		[9] */
	uint8_t	cmd_spec_info1;		/*    "       "      "		[10]*/
	uint8_t	cmd_spec_info0;		/* Command specific info (LSB).	[11]*/
	uint8_t	addl_sense_code;	/* Additional sense code.	[12]*/
	uint8_t	addl_sense_qual;	/* Additional sense qualifier.	[13]*/
	uint8_t	fru_code;		/* Field replaceable unit.	[14]*/
	uint8_t	addl_sense_bytes[10];	/* Additional sense bytes.	[15]*/
};

/*
 * Additional Sense Bytes Format for "ILLEGAL REQUEST" Sense Key:
 */
struct sense_field_pointer {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	uint8_t	bit_ptr		: 3,	/* Bit pointer.			[15]*/
		bpv		: 1,	/* Bit pointer valid.		    */
				: 2,	/* Reserved.			    */
		cmd_data	: 1,	/* Command/data (1=CDB, 0=Data)	    */
		sksv		: 1;	/* Sense key specific valid.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	uint8_t	sksv		: 1,	/* Sense key specific valid.	    */
		cmd_data	: 1,	/* Command/data (1=CDB, 0=Data)	    */
				: 2,	/* Reserved.			    */
		bpv		: 1,	/* Bit pointer valid.		    */
		bit_ptr		: 3;	/* Bit pointer.			[15]*/
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	field_ptr1;		/* Field pointer (MSB byte).	[16]*/
	uint8_t	field_ptr0;		/* Field pointer (LSB byte).	[17]*/
};

#endif /* 0 */

/*
 * Additional Sense Bytes Format for "RECOVERED ERROR", "HARDWARE ERROR",
 * or "MEDIUM ERROR" Sense Keys:
 */
typedef struct sense_retry_count {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte15_b0_7	: 7,	/* Reserved.			[15]*/
	    sksv		: 1;	/* Sense key specific valid.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    sksv		: 1,	/* Sense key specific valid.	    */
	    res_byte15_b0_7	: 7;	/* Reserved.			[15]*/
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	retry_count[2];		/* Retry count.	             [16-17] */
} sense_retry_count_t;

#if 0
/*
 * Additional Sense Bytes Format for "NOT READY" Sense Key, with 
 * asc/ascq = (0x4, 0x4) - Logical unit not ready, format in progress
 */
typedef struct sense_format_progress {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte15_b0_7	: 7,	/* Reserved.			[15]*/
	    sksv		: 1;	/* Sense key specific valid.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    sksv		: 1,	/* Sense key specific valid.	    */
	    res_byte15_b0_7	: 7;	/* Reserved.			[15]*/
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	progress_indicator[2];	/* Progress indicator.	    [16-17] */
} sense_format_progress_t;
#endif /* #if 0 */

/*
 * Write Buffer Command Descriptor Block:
 */
typedef struct WriteBuffer_CDB {
	uint8_t opcode;		 	/* Operation Code. 	    	 [0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
		mode    : 3,    	/* Mode field.			 [1] */
			: 2,    	/* Reserved.    		     */
		lun     : 3;    	/* Logical Unit Number. 	     */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
		lun     : 3,    	/* Logical Unit Number. 	     */
			: 2,    	/* Reserved.    		     */
		mode    : 3;    	/* Mode field.  		 [1] */
#else
#       error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t  id;    		/* Buffer ID.	  		 [2] */
	uint8_t  offset[3];       	/* Buffer offset.	       [3-5] */
	uint8_t  parameter_length[3];  	/* Parameter length.	       [6-8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	uint8_t	link    : 1, 	  	/* Link.    	   		[9] */
		flag    : 1,    	/* Flag.			    */
		naca    : 1,    	/* Normal ACA.  		    */
			: 3,    	/* Reserved.    		    */
		vendor  : 2;    	/* Vendor Unique.       	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	uint8_t vendor  : 2,   	 	/* Vendor Unique.  	    	    */
			: 3,    	/* Reserved.    		    */
		naca    : 1,    	/* Normal ACA.  		    */
		flag    : 1,    	/* Flag.			    */
		link    : 1;    	/* Link.			[9] */
#else
#       error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} WriteBuffer_CDB_t;

/************************************************************************
 *									*
 *			     Direct I/O Commands			*
 *									*
 ************************************************************************/

/*
 * Format Unit Command Descriptor Block:
 */
typedef struct FormatUnit_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t			/*				[1] */
	    dlf		: 3,		/* Defect List Format.	     (b0:2) */
	    cmplst	: 1,		/* Complete List.	       (b3) */
	    fmtdat	: 1,		/* Format Data.		       (b4) */
	    long_list	: 1,		/* Long list.		       (b5) */
	    fmtpinfo	: 2;		/* Format Protection Info.   (b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t			/*				[1] */
	    fmtpinfo	: 2,		/* Format Protection Info.   (b6:7) */
	    long_list	: 1,		/* Long list.		       (b5) */
	    fmtdat	: 1,		/* Format Data.		       (b4) */
	    cmplst	: 1,		/* Complete List.	       (b3) */
	    dlf		: 3;		/* Defect List Format.	     (b0:2) */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	vu_byte2;		/* Vendor unique.		[2] */
	uint8_t	obsolete[2];		/* Obsolete.		      [3-4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} FormatUnit_CDB_t;

/*
 * Prevent/Allow Medium Removal Command Descriptor Block:
 */
struct PreventAllow_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    prevent		: 1,	/* Prevent = 1, Allow = 0.	[4] */
	    res_byte4_b1_7	: 7;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    res_byte4_b1_7	: 7,	/* Reserved.			    */
	    prevent		: 1;	/* Prevent = 1, Allow = 0.	[4] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Read Capacity(10) Command Descriptor Block:
 */
typedef struct ReadCapacity10_CDB {
    uint8_t opcode;			/* Operation code.			[0] */
    uint8_t reserved_byte1;		/* Reserved.				[1] */
    uint8_t obsolete_byte2_5[4];	/* Obsolete in SPC4r02.		      [2-5] */
    uint8_t reserved_byte6_8[3];	/* Reserved.			      [6-8] */
    uint8_t control;			/* Control.				[9] */
} ReadCapacity10_CDB_t;

typedef struct ReadCapacity10_data {
    uint8_t last_block[4];		/* LBA of last block.		      [0-3] */
    uint8_t block_length[4];		/* Logical block length (in bytes).   [4-7] */
} ReadCapacity10_data_t;

/*
 * Read Capacity(16) Command Descriptor Block:
 */
typedef struct ReadCapacity16_CDB {
    uint8_t opcode;
    uint8_t service_action;
    uint8_t lba[8];
    uint8_t allocation_length[4];
    uint8_t flags;
    uint8_t control;
} ReadCapacity16_CDB_t;

typedef struct ReadCapacity16_data {
    uint8_t last_block[8];
    uint8_t block_length[4];
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
	prot_en		: 1,		/* Protection enabled.			[12][b0]   */
	p_type		: 3,		/* Protection type			    (b1:3) */
	reserved_b4_7	: 4;		/* Reserved.				    (b4:7) */
    bitfield_t
	lbppbe		: 4,		/* Logical blocks per physical exponent.[13](b0:4) */
	p_i_exponent	: 4;		/* Protection information exponent.	    (b4:4) */
    bitfield_t
	lowest_aligned_msb : 6,		/* Lowest aligned logical block (MSB).	[14](b0:6) */
	lbprz		: 1,		/* Thin provisioning read zeroes.	     (b6)  */
	lbpme		: 1;		/* Thin provisioning enabled (1 = True).     (b7)  */
    uint8_t lowest_aligned_lsb;		/* Lowest aligned logical block (LSB).	[15]       */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
	reserved_b4_7	: 4,		/* Reserved.				    (b4:7) */
	p_type		: 3,		/* Protection type			    (b1:3) */
	prot_en		: 1;		/* Protection enabled.			[12][b0]   */
    bitfield_t
	p_i_exponent	: 4,		/* Protection information exponent.	    (b4:4) */
	lbppbe		: 4;		/* Logical blocks per physical exponent.[13](b0:4) */
    bitfield_t
	lbpme		: 1,		/* Thin provisioning enabled (1 = True).     (b7)  */
	lbprz		: 1,		/* Thin provisioning read zeroes.	     (b6)  */
	lowest_aligned_msb : 6;		/* Lowest aligned logical block (MSB)	[14](b0:6) */
    uint8_t lowest_aligned_lsb;		/* Lowest aligned logical block (LSB).	[15]       */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t reserved_byte16_31[16];	/* Reserved bytes.			[16-31]    */
} ReadCapacity16_data_t;

/*
 * Reassign Blocks Command Descriptor Block:
 */
struct ReassignBlocks_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

typedef struct CompareWrite16_CDB {
    uint8_t opcode;			/* Operation code.		[0] */
    uint8_t flags;			/* Flags.			[1] */
    uint8_t lba[8];			/* Logical block address.     [2-9] */
    uint8_t reserved_byte10_12[3];	/* Reserved.		    [10-12] */
    uint8_t blocks;			/* Number of blocks.	       [13] */
    uint8_t group_number; 		/* Group number.	       [14] */
    uint8_t control;			/* Control flags.	       [15] */
} CompareWrite16_CDB_t;

/*
 * Disk Read / Write / Seek CDB's.
 */
typedef struct DirectRW6_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
	uint8_t	lba[3];			/* Logical Block Address.     [1-3] */
	uint8_t	length;			/* Transfer Length.		[4] */
	uint8_t	control;		/* Various control flags.	[5] */
} DirectRW6_CDB_t;

#define SCSI_DIR_RDWR_10_DPO		0x10
#define SCSI_DIR_RDWR_10_FUA            0x08
#define SCSI_DIR_RDWR_10_RELADR         0x01

typedef struct DirectRW10_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
	uint8_t	flags;			/* Various flags.		[1] */
	uint8_t	lba[4];			/* Logical Block Address.     [2-5] */
	uint8_t	reserved_byte6;		/* Reserved.			[6] */
	uint8_t	length[2];		/* Transfer Length.    	      [7-8] */
	uint8_t	control;		/* Various control flags.	[9] */
} DirectRW10_CDB_t;

#define SCSI_DIR_RDWR_16_DPO            0x10
#define SCSI_DIR_RDWR_16_FUA            0x08
#define SCSI_DIR_RDWR_16_RELADR         0x01

typedef struct DirectRW16_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
	uint8_t	flags;			/* Various flags.		[1] */
	uint8_t	lba[8];			/* Logical block address.     [2-9] */
	uint8_t	length[4];		/* Transfer Length.    	    [10-13] */
	uint8_t	reserved_byte14;	/* Reserved.		       [14] */
	uint8_t	control;		/* Various control flags.      [15] */
} DirectRW16_CDB_t;

/*
 * Read Defect Data Command Descriptor Block:
 */
struct ReadDefectData_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    dlf			: 3,	/* Defect List Format.		[2] */
	    grown		: 1,	/* Grown Defect List.		    */
	    manuf		: 1,	/* Manufacturers Defect List.	    */
	    res_byte2_5_3	: 3;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    res_byte2_5_3	: 3,	/* Reserved.			    */
	    manuf		: 1,	/* Manufacturers Defect List.	    */
	    grown		: 1,	/* Grown Defect List.		    */
	    dlf			: 3;	/* Defect List Format.		[2] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
	uint8_t	res_byte5;		/* Reserved.			[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	alclen1;		/* Allocation Length.		[7] */
	uint8_t	alclen0;		/* Allocation Length.		[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Seek(10) LBA Command Descriptor Block:
 */
typedef struct Seek10_CDB {
        uint8_t opcode;		/* Operation Code.			[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
        bitfield_t
	    res_byte1_b0_5	: 5, /* 5 bits reserved                 [1] */
	    lun  	   	: 3; /* Logical unit number                 */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
        bitfield_t
	    lun			: 3, /* Logical unit number             [1] */
	    res_byte1_b0_5	: 5; /* 5 bits reserved                     */
#else
#       error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	lba[4];		/* The logical block address.	      [2-5] */
	uint8_t	reserved[3];	/* Reserved.			      [6-8] */
        uint8_t control;        /* The control byte                     [9] */
} Seek10_CDB_t;

/*
 * Start/Stop Unit Command Descriptor Block:
 */
struct StartStopUnit_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    immed		: 1,	/* Immediate.			[1] */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    immed		: 1;	/* Immediate.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    start		: 1,	/* Start = 1, Stop = 0.		[4] */
	    loej		: 1,	/* Load/Eject = 1, 0 = No Affect.   */
	    res_byte4_b2_6    	: 6;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    ires_byte4_b2_6	: 6,	/* Reserved.			    */
	    loej		: 1,	/* Load/Eject = 1, 0 = No Affect.   */
	    start		: 1;	/* Start = 1, Stop = 0.		[4] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/* 
 * Verify Data Command Descriptor Block:
 */
struct VerifyDirect_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    reladr 		: 1,	/* Relative Address.		[1] */
	    bytchk		: 1,	/* Byte Check.			    */
	    res_byte1_b2_3    	: 3,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b2_3	: 3,	/* Reserved.			    */
	    bytchk		: 1,	/* Byte Check.			    */
	    reladr 		: 1;	/* Relative Address.		[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	lbaddr3;		/* Logical Block Address 	[2] */
	uint8_t	lbaddr2;		/*    "      "      "		[3] */
	uint8_t	lbaddr1;		/*    "      "      "		[4] */
	uint8_t	lbaddr0;		/*    "      "      "		[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	verflen1;		/* Verification Length.		[7] */
	uint8_t	verflen0;		/* Verification Length.		[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/************************************************************************
 *									*
 *			 Sequential I/O Commands			*
 *									*
 ************************************************************************/

/*
 * Erase Tape Command Descriptor Block:
 */
struct EraseTape_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    longe		: 1,	/* Long Erase (1 = Entire Tape)	[1] */
	    res_byte1_b1_4 	: 4,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    longe		: 1;	/* Long Erase (1 = Entire Tape)	[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Load / Unload / Retention Tape Command Descriptor Block:
 */
struct LoadUnload_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    immed		: 1,	/* Immediate.			[1] */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    immed		: 1;	/* Immediate.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    load		: 1,	/* Load.			[4] */
	    reten		: 1,	/* Retention.			    */
	    eot			: 1,	/* End Of Tape.			    */
	    res_byte4_5		: 5;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    res_byte4_5		: 5,	/* Reserved.			    */
	    eot			: 1,	/* End Of Tape.			    */
	    reten		: 1,	/* Retention.			    */
	    load		: 1;	/* Load.			[4] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Rewind Tape Command Descriptor Block:
 */
struct RewindTape_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    immed		: 1,	/* Immediate.			[1] */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b1_4	: 4,	/* Reserved.			    */
	    immed		: 1;	/* Immediate.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Space Operation Codes.
 */
#define SPACE_BLOCKS		0	/* Space blocks (or records).	*/
#define SPACE_FILE_MARKS	1	/* Space file marks.		*/
#define SPACE_SEQ_FILE_MARKS	2	/* Space sequential file marks.	*/
#define SPACE_END_OF_DATA	3	/* Space to end of media.	*/
#define SPACE_SETMARKS		4	/* Space setmarks.		*/
#define SPACE_SEQ_SET_MARKS	5	/* Space sequential setmarks.	*/

/*
 * Space Tape Command Descriptor Block:
 */
struct SpaceTape_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    code		: 3,	/* Space Blocks/Filemarks/EOM.	[1] */
	    res_byte1_b3_2    	: 2,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b3_2	: 2,	/* Reserved.			    */
	    code		: 3;	/* Space Blocks/Filemarks/EOM.	[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	count2;			/* Count (MSB).			[2] */
	uint8_t	count1;			/* Count.			[3] */
	uint8_t	count0;			/* Count (LSB).			[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 1,	/* Vendor Unique.		    */
	    fast		: 1;	/* Fast Space Algorithm.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    fast		: 1,	/* Fast Space Algorithm.	    */
	    vendor		: 1,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Write Filemarks Command Descriptor Block:
 */
struct WriteFileMark_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	fmcount2;		/* Number of Filemarks (MSB).	[2] */
	uint8_t	fmcount1;		/* Number of Filemarks.		[3] */
	uint8_t	fmcount0;		/* Number of Filemarks (LSB).	[4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[5] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 1,	/* Vendor Unique.		    */
	    fast		: 1;	/* Fast Space Algorithm.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    fast		: 1,	/* Fast Space Algorithm.	    */
	    vendor		: 1,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[5] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/************************************************************************
 *									*
 *			  CD-ROM Audio Commands				*
 *									*
 ************************************************************************/

/*
 * CD-ROM Pause/Resume Command Descriptor Block:
 */
struct CdPauseResume_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
	uint8_t	res_byte5;		/* Reserved.			[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	res_byte7;		/* Reserved.			[7] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    resume		: 1,	/* Resume = 1, Pause = 0.	[8] */
	    res_byte8_b1_7	: 7;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    res_byte8_b1_7	: 7,	/* Reserved.			    */
	    resume		: 1;	/* Resume = 1, Pause = 0.	[8] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Play Audio LBA Command Descriptor Block:
 */
struct CdPlayAudioLBA_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    reladr		: 1,	/* Relative Address bit		[1] */
	    res_byte1_b1_4	: 4,	/* Reserved			    */
	    lun			: 3;	/* Logical Unit Number		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number		    */
	    res_byte1_b1_4	: 4,	/* Reserved			    */
	    reladr		: 1;	/* Relative Address bit		[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	lbaddr3;		/* Logical Block Address	[2] */
	uint8_t	lbaddr2;		/* Logical Block Address	[3] */
	uint8_t	lbaddr1;		/* Logical Block Address	[4] */
	uint8_t	lbaddr0;		/* Logical Block Address	[5] */
	uint8_t	res_byte6;		/* Reserved			[6] */
	uint8_t	xferlen1;		/* Transfer Length    		[7] */
	uint8_t	xferlen0;		/* Transfer Length    		[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Play Audio MSF Command Descriptor Block:
 */
struct CdPlayAudioMSF_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	starting_M_unit;	/* Starting M-unit.		[3] */
	uint8_t	starting_S_unit;	/* Starting S-unit.		[4] */
	uint8_t	starting_F_unit;	/* Starting F-unit.		[5] */
	uint8_t	ending_M_unit;		/* Ending M-unit.		[6] */
	uint8_t	ending_S_unit;		/* Ending S-unit.		[7] */
	uint8_t	ending_F_unit;		/* Ending F-unit.		[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Play Audio Track/Index Command Descriptor Block:
 */
struct CdPlayAudioTI_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	starting_track;		/* Starting Track.		[4] */
	uint8_t	starting_index;		/* Starting Index.		[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	ending_track;		/* Ending Track.		[7] */
	uint8_t	ending_index;		/* Ending Index			[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Play Audio Track Relative Command Descriptor Block:
 */
struct CdPlayAudioTR_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	lbaddr3;		/* Logical Block Address	[2] */
	uint8_t	lbaddr2;		/* Logical Block Address.	[3] */
	uint8_t	lbaddr1;		/* Logical Block Address.	[4] */
	uint8_t	lbaddr0;		/* Logical Block Address.	[5] */
	uint8_t	starting_track;		/* Starting Track.		[6] */
	uint8_t	xfer_len1;		/* Transfer Length    		[7] */
	uint8_t	xfer_len0;		/* Transfer Length    		[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Read TOC Command Descriptor Block:
 */
struct CdReadTOC_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0	: 1,	/* Reserved.			[1] */
	    msf			: 1,	/* Report address in MSF format.    */
	    res_byte1_b2_3	: 3,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b2_3	: 3,	/* Reserved.			    */
	    msf			: 1,	/* Report address in MSF format.    */
	    res_byte1_b0	: 1;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
	uint8_t	res_byte5;		/* Reserved.			[5] */
	uint8_t	starting_track;		/* Reserved.			[6] */
	uint8_t	alloc_len1;		/* Allocation length (MSB).	[7] */
	uint8_t	alloc_len0;		/* Allocation length (LSB).	[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Read Sub-Channel Command Descriptor Block:
 */
struct CdReadSubChannel_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0	: 1,	/* Reserved.			[1] */
	    msf			: 1,	/* Report address in MSF format.    */
	    res_byte1_b2_4	: 3,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b2_4	: 3,	/* Reserved.			    */
	    msf			: 1,	/* Report address in MSF format.    */
	    res_byte1_b0	: 1;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte2_b0_5	: 6,	/* Reserved.			[2] */
	    subQ		: 1,	/* Sub-Q Channel Data.		    */
	    res_byte2_b7	: 1;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    res_byte2_b7	: 1,	/* Reserved.			    */
	    subQ		: 1,	/* Sub-Q Channel Data.		    */
	    res_byte2_b0_5	: 6;	/* Reserved.			[2] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	data_format;		/* Data Format Code.		[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
	uint8_t	res_byte5;		/* Reserved.			[5] */
	uint8_t	track_number;		/* Reserved.			[6] */
	uint8_t	alloc_len1;		/* Allocation length (MSB).	[7] */
	uint8_t	alloc_len0;		/* Allocation length (LSB).	[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_6	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_6	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Read Header Command Descriptor Block:
 */
struct CdReadHeader_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0	: 1,	/* Reserved.			[1] */
	    msf			: 1,	/* Report address in MSF format.    */
	    res_byte1_b2_3	: 3,	/* Reserved.			    */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b2_3	: 3,	/* Reserved.			    */
	    msf			: 1,	/* Report address in MSF format.    */
	    res_byte1_b0	: 1;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	lbaddr3;		/* Logical Block Address	[2] */
	uint8_t	lbaddr2;		/* Logical Block Address.	[3] */
	uint8_t	lbaddr1;		/* Logical Block Address.	[4] */
	uint8_t	lbaddr0;		/* Logical Block Address.	[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	alloc_len1;		/* Allocation Length MSB.	[7] */
	uint8_t	alloc_len0;		/* Allocation Length LSB.	[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Play Track Command Descriptor Block:
 */
struct CdPlayTrack_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	starting_track;		/* Starting track.		[4] */
	uint8_t	starting_index;		/* Starting index.		[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	res_byte7;		/* Reserved.			[7] */
	uint8_t	number_indexes;		/* Number of indexes.		[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Playback Control/Status Command Descriptor Block:
 */
struct CdPlayback_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
	uint8_t	res_byte5;		/* Reserved.			[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	alloc_len1;		/* Allocation length (MSB).	[7] */
	uint8_t	alloc_len0;		/* Allocation length (LSB).	[8] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * CD-ROM Set Address Format Command Descriptor Block:
 */
struct CdSetAddressFormat_CDB {
	uint8_t	opcode;			/* Operation Code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    res_byte1_b0_5	: 5,	/* Reserved.			[1] */
	    lun			: 3;	/* Logical Unit Number.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    lun			: 3,	/* Logical Unit Number.		    */
	    res_byte1_b0_5	: 5;	/* Reserved.			[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	res_byte2;		/* Reserved.			[2] */
	uint8_t	res_byte3;		/* Reserved.			[3] */
	uint8_t	res_byte4;		/* Reserved.			[4] */
	uint8_t	res_byte5;		/* Reserved.			[5] */
	uint8_t	res_byte6;		/* Reserved.			[6] */
	uint8_t	res_byte7;		/* Reserved.			[7] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    lbamsf		: 1,	/* Address Format 0/1 = LBA/MSF	[8] */
	    res_byte8_b1_7	: 7;	/* Reserved.			    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    res_byte8_b1_7	: 7,	/* Reserved.			    */
	    lbamsf		: 1;	/* Address Format 0/1 = LBA/MSF	[8] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	bitfield_t
	    link		: 1,	/* Link.			[9] */
	    flag		: 1,	/* Flag.			    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    vendor		: 2;	/* Vendor Unique.		    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	bitfield_t
	    vendor		: 2,	/* Vendor Unique.		    */
	    res_byte5_b2_4	: 4,	/* Reserved.			    */
	    flag		: 1,	/* Flag.			    */
	    link		: 1;	/* Link.			[9] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/* ======================================================================== */

typedef struct get_lba_status_cdb {
    uint8_t opcode;			/* Operation code.		[0] */
    uint8_t service_action;		/* Service action.		[1] */
    uint8_t start_lba[8];		/* Starting logicial block.   [2-9] */
    uint8_t allocation_length[4];	/* Allocation length.	    [10-13] */
    uint8_t reserved_byte14;		/* Reserved.		       [14] */
    uint8_t control;			/* Control.		       [15] */
} get_lba_status_cdb_t;

typedef struct get_lba_status_param_data {
    uint8_t parameter_data_length[4];	/* Parameter data length.     [0-3] */
    uint8_t reserved_bytes4_7[4];	/* Reserved.		      [4-7] */
} get_lba_status_param_data_t;

#define MAX_LBA_STATUS_DESC     650

typedef struct lba_status_descriptor {
    uint8_t start_lba[8];		/* Starting logical block.	[0-7] */
    uint8_t extent_length[4];		/* Extent length.	       [8-12] */
    uint8_t provisioning_status;	/* Provisioning status.	         [13] */
    uint8_t reserved_bytes14_16[3];	/* Reserved.		      [14-16] */
} lba_status_descriptor_t;

#define SCSI_PROV_STATUS_MAPPED 0x0
#define SCSI_PROV_STATUS_HOLE	0x1

/* ======================================================================== */
/* Report LUN Definitions: */

#define SCSI_PeripheralDeviceAddressing		0x0
#define SCSI_FlatSpaceAddressing		0x1
#define SCSI_LogicalUnitAddressing		0x2
#define SCSI_ExtendedLogicalUnitAddressing	0x3

#define SCSIT_REPORT_ALL_LUNS   0x0
#define SCSIT_REPORT_WELL_KNOWN_LUNS 0x1
#define SCSIT_REPORT_2          0x2
#define SCSIT_REPORT_INDEPENDENT_LUS 0xFE
#define SCSIT_REPORT_BOUND_VVOLS 0xFF

typedef struct report_luns_cdb {
    uint8_t opcode;
    uint8_t reserved0;
    uint8_t select_report;
    uint8_t reserved2;
    uint8_t reserved3;
    uint8_t reserved4;
    uint8_t length[4];
    uint8_t reserved5;
    uint8_t control;
} report_luns_cdb_t;

typedef struct report_luns_header {
    uint8_t list_len[4];
    uint8_t reserved[4];
} report_luns_header_t;

typedef struct report_luns_entry {
    uint8_t lun_entry[8];
} report_luns_entry_t;

#define SCSI_BusIdentifierLun		0
#define SCSI_BusIdentifierDomain	1

typedef struct PeripheralDeviceAddressing {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[0] */
	bus_identifier : 6,		/* Bus identifier.	     (b0:5) */
	address_method : 2;		/* The address method.	     (b6:7) */
    uint8_t target_or_lun[7];		/* Target or LUN.		[1] */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[0] */
	address_method : 2,		/* The address method.	     (b6:7) */
    	bus_identifier : 6;		/* Bus identifier.	     (b0:5) */
    uint8_t target_or_lun[7];		/* Target or LUN.		[1] */
#else
#	error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} PeripheralDeviceAddressing_t;

/* ======================================================================== */

/*
 * Maintenance In Definitions:
 */
typedef struct maintenance_in_cdb {
    uint8_t opcode;			/* Operation code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[1] */
	service_action		: 5,    /* Service action.	     (b0:4) */
	reserved_byte1_b5_7	: 3;    /* Reserved.		     (b5:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[1] */
	reserved_byte1_b5_7	: 3,    /* Reserved.		     (b5:7) */
	service_action		: 5;    /* Service action.	     (b0:4) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t mgmt_protocol;		/* Management protocol.		[2] */
    uint8_t mgmt_protocol_specific[3];	/* Mgmt protocol specific.    [3-5] */
    uint8_t allocation_length[4];	/* Allocation length.	      [6-9] */
    uint8_t control;			/* Control.		       [10] */
} maintenance_in_cdb_t;

/*
 * Report Target Group Definitions:
 */
typedef enum target_alua_port_group_states {
    SCSI_TGT_ALUA_ACTIVE_OPTIMIZED     = 0x0,
    SCSI_TGT_ALUA_ACTIVE_NON_OPTIMIZED = 0x1,
    SCSI_TGT_ALUA_STANDBY              = 0x2,
    SCSI_TGT_ALUA_UNAVAILABLE          = 0x3,
    SCSI_TGT_ALUA_OFFLINE              = 0xE,
    SCSI_TGT_ALUA_TRANSITIONING        = 0xF,
    SCSI_TGT_ALUA_NO_STATE             = 0xFF,
} target_alua_port_group_states_t;

typedef struct {
    uint8_t length[4];			/* Returned data length.      [0-3] */
} rtpg_header_t;

typedef struct rtpg_desc_extended_header {
    uint8_t length[4];			/* Returned data length.		[0-3]     */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    uint8_t reserved_0_3	: 4,	/* Reserved,				[4](b0:3) */
	    format_type		: 3,	/* The format type.		`	   (b4:3) */
	    reserved_7		: 1;	/* Reserved.				   (b7:1) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    uint8_t reserved_7		: 1,	/* Reserved.				[4](b7:1) */
	    format_type		: 3,	/* The format type.		`	   (b4:3) */
	    reserved_0_3	: 4;	/* Reserved.				   (b0:3) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */    
    uint8_t implicit_transition_time;	/* Implicit transition time.		[5]       */
    uint8_t reserved_6_7[2];		/* Reserved.				[6-7]     */
					/* First target port group descriptor.		  */
} rtpg_desc_extended_header_t;

typedef struct report_target_port_group_desc {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    uint8_t alua_state		: 4,	/* ALUA state.				[0](0:3) */
	    reserved_0_4_3	: 3,	/* Reserved.				   (4:3) */
	    pref		: 1;	/* Preferred TPG to access LUN.	   	   (7:1) */
    uint8_t ao_sup		: 1,	/* Active/optimized supported.		[1](b0)  */
            an_sup		: 1,	/* Active/non-optimized supported.	   (b1)  */
            s_sup		: 1,	/* Standby supported.			   (b2)  */
            u_sup		: 1,	/* Unavailable supported.		   (b3)  */
            reserved_1_4_3	: 3,	/* Reserved.				   (4:3) */
            t_sup		: 1;	/* Transition supported.		   (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    uint8_t pref		: 1,	/* Preferred TPG to access LUN.		[0](7:1) */
	    reserved_0_4_3	: 3,	/* Reserved.				   (4:3) */
	    alua_state		: 4;	/* ALUA state.				   (0:3) */
    uint8_t t_sup		: 1,	/* Transition supported.		[1](b7)  */
	    reserved_1_4_3	: 3,	/* Reserved.				   (4:3) */
	    u_sup		: 1,	/* Unavailable supported.		   (b3)  */
	    s_sup		: 1,	/* Standby supported.			   (b2)  */
	    an_sup		: 1,	/* Active/non-optimized supported.	   (b1)  */
	    ao_sup		: 1;	/* Active/optimized supported.		   (b0)  */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t target_port_group[2];	/* Target port group.			[2-3]   */
    uint8_t reserved_byte4;		/* Reserved.				[4]     */
    uint8_t status_code;		/* Status code.				[5]     */
    uint8_t vendor_specific;		/* Vendor specific.			[6]     */
    uint8_t target_port_count;		/* Target port count.			[7]     */
					/* First target port descriptor.		*/
} report_target_port_group_desc_t;

typedef struct target_port_desc {
    uint8_t obsolete[2];		/* Obsolete.				[0-1] */
    uint8_t relative_target_port_id[2];	/* Relative target port identifier.	[2-3] */
} target_port_desc_t;

/* ======================================================================== */

/*
 * Unmap Definitions:
 */
typedef struct unmap_cdb {
    uint8_t opcode;			/* Operation code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[1] */
	anchor			: 1,    /* LBA deallocated or anchored.(b0) */
	reserved_byte1_b1_7	: 7;    /* Reserved.		     (b1:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[1] */
	reserved_byte1_b1_7	: 7,    /* Reserved.		     (b1:7) */
	anchor			: 1;    /* LBA deallocated or anchored.(b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t reserved_byte2_5[4];	/* Reserved.		      [2-5] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[6] */
	group_number		: 5,    /* Group number.	     (b0:4) */
	reserved_byte6_b5_7	: 3;    /* Reserved.		     (b5:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[6] */
	reserved_byte6_b5_7	: 3,    /* Reserved.		     (b5:7) */
	group_number		: 5;    /* Group number.	     (b0:4) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t parameter_list_length[2];	/* Parameter list length.     [7-8] */
    uint8_t control;			/* Control.			[9] */
} unmap_cdb_t;

typedef struct unmap_parameter_list_header {
    uint8_t data_length[2];
    uint8_t block_descriptor_length[2];
    uint8_t reserved[4];
} unmap_parameter_list_header_t;

typedef struct unmap_block_descriptor {
    uint8_t lba[8];
    uint8_t length[4];
    uint8_t reserved[4];
} unmap_block_descriptor_t;

/* ======================================================================== */

/*
 * Extended Copy and Token Based Copy Definitions:
 */
/* 
 * Extended Copy Defintions:
 */
typedef struct xcopy_cdb {
    uint8_t opcode;			/* Operation code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[1] */
	service_action		: 5,    /* Service action.	     (b0:4) */
	reserved_byte1_b5_7	: 3;    /* Reserved.		     (b5:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[1] */
	reserved_byte1_b5_7	: 3,    /* Reserved.		     (b5:7) */
	service_action		: 5;    /* Service action.	     (b0:4) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t reserved[8];		/* Reserved.		      [2-9] */
    uint8_t parameter_list_length[4];	/* Parameter list.	    [10-13] */
    uint8_t reserved_byte14;		/* Reserved.		       [14] */
    uint8_t control;			/* Control.	               [15] */
} xcopy_cdb_t;

/*
 * Extended Copy LID1 Parameters:
 */
typedef struct xcopy_lid1_parameter_list {
    uint8_t list_identifier;		/* List Identifier.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[1] */
	priority        : 3,		/* Priority.		  	(b0:2) */
	nlid            : 1,		/* No List Identifier.	   	(b3)   */
	nrcr		: 1,		/* No Receive Copy Results	(b4)   */
	str		: 1,		/* Sequential Striped.		(b5)   */
	reserved_6_7    : 2;		/* Reserved bits.		(b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[1] */
	reserved_6_7    : 2,		/* Reserved bits.		(b6:7) */
	str		: 1,		/* Sequential Striped.		(b5)   */
	nrcr	    	: 1,		/* No Receive Copy Results	(b4)   */
	nlid            : 1,		/* No List Identifier.	   	(b3)   */
	priority        : 3;		/* Priority.		  	(b0:2) */
#else
#       error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t cscd_desc_list_length[2];	/* CSCD desc list length.     [2-3] */
    uint8_t reserved_4_7[4];		/* Reserved.		      [4-8] */
    uint8_t seg_desc_list_length[4];	/* Segment desc list length. [8-11] */
    uint8_t inline_data_length[4];	/* Inline data length.	    [12-15] */
    //uint8_t desc_data[0];		/* CSCD and segment descriptors. */
} xcopy_lid1_parameter_list_t;

typedef struct xcopy_type_spec_params {
    uint8_t byte1;
    uint8_t disk_block_length[3];
} xcopy_type_spec_params_t;

/* 
 * CSCD Type Codes Descriptors:
 */
#define XCOPY_CSCD_TYPE_CODE_FC_N_PORT_NAME	0xE0	/* Fibre Channel N_Port_Name.	*/
#define XCOPY_CSCD_TYPE_CODE_FC_N_PORT_ID	0xE1	/* Fibre Channel N_Port_ID.	*/
#define XCOPY_CSCD_TYPE_CODE_FC_N_PORT_ID_NAME	0xE2	/* Fibre Channel N_Port_ID w/N_Port_Name checking. */
#define XCOPY_CSCD_TYPE_CODE_PARALLEL_INT_T_L	0xE3	/* Parallel Interface T_L.	*/
#define XCOPY_CSCD_TYPE_CODE_IDENTIFICATION	0xE4	/* Identification Descriptor.	*/
#define XCOPY_CSCD_TYPE_CODE_IPV4		0xE5	/* IPv4.			*/
#define XCOPY_CSCD_TYPE_CODE_ALIAS		0xE6	/* Alias.			*/
#define XCOPY_CSCD_TYPE_CODE_RDMA		0xE7	/* RDMA.			*/
#define XCOPY_CSCD_TYPE_CODE_IEEE_EUI_64	0xE8	/* IEEE 1394 EUI-64.		*/
#define XCOPY_CSCD_TYPE_CODE_SAS_SERIAL_SCSI	0xE9	/* SAS Serial SCSI Protocol.	*/
#define XCOPY_CSCD_TYPE_CODE_IPV6		0xEA	/* IPv6 CSCD descriptor.	*/
#define XCOPY_CSCD_TYPE_CODE_COPY_SERVICE	0xEB	/* IP Copy Service.		*/
// 0xEC to FDh Reserved for CSCD descriptors
// 0xFE ROD CSCD descriptor

#define XCOPY_ASSOCIATION_SHIFT		6

/* CSCD Descriptor Type Codes Definitions. (NAA?) */
typedef struct xcopy_id_cscd_desc {
    uint8_t desc_type_code;		/* Descriptor type code.	[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[1] */
	device_type	: 5,		/* Device type (from Inquiry).	(b0:4) */
	obsolete_b1_b5	: 1,		/* Obsolete.		   	(b5)   */
	lu_id_type	: 2;		/* LU ID Type (ignored).	(b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[1] */
	lu_id_type	: 2,		/* LU ID Type (ignored).	(b6:7) */
	obsolete_b1_b5	: 1,		/* Obsolete.		   	(b5)   */
	device_type	: 5;		/* Device type (from Inquiry).	(b0:4) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t relative_init_port_id[2];	/* Relative initiator port ID.	[2-3] */
    /* CSCD Descriptor Parameters */
    uint8_t codeset;			/* Code set.			[4] */
    uint8_t designator_type;		/* Designator type (VPD 0x83)	[5] */
    uint8_t reserved_byte6;		/* Reserved.			[6] */
    uint8_t designator_length;		/* Designator length.		[7] */
    uint8_t designator[16];		/* Desigator (VPD page 0x83).	[8] */
    uint8_t reserved_24_27[4];		/* Reserved.			[9-12] */
    xcopy_type_spec_params_t type_spec_params; /* Type specific parameters. */
} xcopy_id_cscd_desc_t;

/* CSCD Identification Descriptor Type Code Definitions. */
typedef struct xcopy_id_cscd_ident_desc {
    uint8_t desc_type_code;		/* Descriptor type code.	[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[1] */
	device_type	: 5,		/* Device type (from Inquiry).	(b0:4) */
	obsolete_b1_b5	: 1,		/* Obsolete.		   	(b5)   */
	lu_id_type	: 2;		/* LU ID Type (ignored).	(b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[1] */
	lu_id_type	: 2,		/* LU ID Type (ignored).	(b6:7) */
	obsolete_b1_b5	: 1,		/* Obsolete.		   	(b5)   */
	device_type	: 5;		/* Device type (from Inquiry).	(b0:4) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t relative_init_port_id[2];	/* Relative initiator port ID.	[2-3] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[4] */
	codeset		: 4,		/* Device type (from Inquiry).	(b0:3) */
	reserved_b4_4_7	: 4;		/* Reserved.		        (b4:7) */
    bitfield_t				/*				[5] */
	designator_type	: 4,		/* Designator type (VPD 0x83)	(b0:3) */
        association	: 2,		/* Association (form VPD 0x83)  (b4:5) */
        reserved_b5_6_7	: 2;		/* Reserved.			(b6:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[4] */
	reserved_b4_4_7	: 4,		/* Reserved.		        (b4:7) */
	codeset		: 4;		/* Device type (from Inquiry).	(b0:3) */
    bitfield_t				/*				[5] */
	reserved_b5_6_7	: 2,		/* Reserved.			(b6:7) */
	association	: 2,		/* Association (form VPD 0x83)  (b4:5) */
	designator_type	: 4;		/* Designator type (VPD 0x83)	(b0:3) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t reserved_byte6;		/* Reserved.			[6] */
    uint8_t designator_length;		/* Designator length.		[7] */
    uint8_t designator[20];		/* Desigator (VPD page 0x83).	[8-27] */
    //uint8_t device_specific_params[4];/* Device specific parameters.	[28-31] */
    xcopy_type_spec_params_t type_spec_params; /* Type specific parameters. */
} xcopy_id_cscd_ident_desc_t;

/*
 * Segment Descriptor Definitions:
 */
#define XCOPY_DESC_TYPE_CODE_BLOCK_TO_BLOCK_SEG_DESC	0x02

/*
 * Block to Block Segment Descriptor:
 */
typedef struct xcopy_b2b_seg_desc {
    uint8_t desc_type_code;		/* Descriptor type code.	[0] */
    uint8_t reserved_byte1;		/* Reserved.			[1] */
    uint8_t desc_length[2];		/* Descriptor length.		[2-3] */
    uint8_t src_cscd_desc_idx[2];       /* Source descriptor index.	[4-5] */
    uint8_t dst_cscd_desc_idx[2];	/* Destination descriptor index.[6-7] */
    uint8_t reserved_bytes_8_9[2];	/* Reserved.			[8-9] */
    uint8_t block_device_num_of_blocks[2]; /* Number of blocks.		[10-11] */
    uint8_t src_block_device_lba[8];	/* Source device logical block.	[12-19] */
    uint8_t dst_block_device_lba[8];	/* Destination logical block.	[20-27] */
} xcopy_b2b_seg_desc_t;

#define XCOPY_B2B_SEGMENT_LENGTH (sizeof(xcopy_b2b_seg_desc_t) - 4)

/* ======================================================================== */

/*
 * Token Based Extended Copy Definitions: (aka ODX - Offload Data Transfer)
 */

typedef struct populate_token_cdb {
    uint8_t opcode;			/* Operation code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[1] */
	service_action		: 5,    /* Service action.	     (b0:4) */
	reserved_byte1_b5_7	: 3;    /* Reserved.		     (b5:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[1] */
	reserved_byte1_b5_7	: 3,    /* Reserved.		     (b5:7) */
	service_action		: 5;    /* Service action.	     (b0:4) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t reserved_byte2_5[4];	/* Reserved.		      [2-5] */
    uint8_t list_identifier[4];		/* List identifier.	      [6-9] */
    uint8_t parameter_list_length[4];	/* List parameter length.   [10-13] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*			       [15] */
	group_number		: 5,    /* Group number.	     (b0:4) */
	reserved_byte15_b5_7	: 3;    /* Reserved.		     (b5:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*			       [15] */
	reserved_byte15_b5_7	: 3,    /* Reserved.		     (b5:7) */
	group_number		: 5;    /* Group number.	     (b0:4) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t control;			/* Control.	               [15] */
} populate_token_cdb_t;

typedef struct populate_token_parameter_list {
    uint8_t data_length[2];		/* Data length.		      [0-1] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[2] */
	immed			: 1,   	/* Immediate.		       (b0) */
        rtv			: 1,	/* ROD type valid.	       (b1) */
	reserved_byte2_b2_7	: 6;    /* Reserved.		     (b2:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[2] */
	reserved_byte2_b2_7	: 6,    /* Reserved.		     (b2:7) */
        rtv			: 1,	/* ROD type valid.	       (b1) */
	immed			: 1;   	/* Immediate.		       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t reserved_byte3;		/* Reserved.			[3] */
    uint8_t inactivity_timeout[4];	/* Inactivity timeout.	      [4-7] */
    uint8_t rod_type[4];		/* ROD type.		     [8-11] */
    uint8_t reserved_byte_12_13[2];	/* Reserved.		    [12-13] */
    uint8_t range_descriptor_list_length[2]; /* Range desc length.  [14-15] */
} populate_token_parameter_list_t;

typedef struct range_descriptor {
    uint8_t lba[8];			/* Starting logicl block.      [0-7] */
    uint8_t length[4];			/* Number of blocks.	     [8-11] */
    uint8_t reserved_byte_12_15[4];	/* Reserved.		    [12-15] */
} range_descriptor_t;

typedef struct write_using_token_cdb {
    uint8_t opcode;			/* Operation code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[1] */
	service_action		: 5,    /* Service action.	     (b0:4) */
	reserved_byte1_b5_7	: 3;    /* Reserved.		     (b5:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[1] */
	reserved_byte1_b5_7	: 3,    /* Reserved.		     (b5:7) */
	service_action		: 5;    /* Service action.	     (b0:4) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t reserved_byte2_5[4];	/* Reserved.		      [2-5] */
    uint8_t list_identifier[4];		/* List identifier.	      [6-9] */
    uint8_t parameter_list_length[4];	/* List parameter length.   [10-13] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*			       [15] */
	group_number		: 5,    /* Group number.	     (b0:4) */
	reserved_byte15_b5_7	: 3;    /* Reserved.		     (b5:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*			       [15] */
	reserved_byte15_b5_7	: 3,    /* Reserved.		     (b5:7) */
	group_number		: 5;    /* Group number.	     (b0:4) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t control;			/* Control.	               [15] */
} write_using_token_cdb_t;

typedef struct wut_parameter_list {
    uint8_t data_length[2];		/* Data length.		      [0-1] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[2] */
	immed			: 1,   	/* Immediate.		       (b0) */
        del_tkn			: 1,	/* Delete token.	       (b1) */
	reserved_byte2_b2_7	: 6;    /* Reserved.		     (b2:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[2] */
	reserved_byte2_b2_7	: 6,    /* Reserved.		     (b2:7) */
        del_tkn			: 1,	/* Delete token.	       (b1) */
	immed			: 1;   	/* Immediate.		       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t reserved_byte3_7[5];	/* Reserved.		      [3-7] */
    uint8_t offset_into_rod[8];		/* Offset into ROD.	     [8-15] */
} wut_parameter_list_t;

/* Note: This value is currently hard coded in WUT parameter list. */
#define ROD_TOKEN_OFFSET	sizeof(wut_parameter_list_t)
#define ROD_TOKEN_LENGTH	512 

typedef struct wut_parameter_list_runt {
    uint8_t reserved[6];			/*  [0-5]	*/
    uint8_t range_descriptor_list_length[2];	/*  [6-7]	*/
} wut_parameter_list_runt_t;

#define WUT_PARAM_SIZE		sizeof(wut_parameter_list_t) +		\
			    	ROD_TOKEN_LENGTH +			\
			    	sizeof(wut_parameter_list_runt_t)

/* A valid WUT parmater size includes a range descriptor. */
#define WUT_VALID_PARAM_SIZE	SCSI_WUT_PARAM_SIZE +			\
				sizeof(range_descriptor_t)

#define WUT_MIN_PARAM_SIZE	sizeof(wut_parameter_list_t) +		\
				sizeof(wut_parameter_list_runt_t) +	\
				sizeof(range_descriptor_t)

#define ZERO_ROD_TOKEN_TYPE	0xFFFF0001
#define ZERO_ROD_TOKEN_LENGTH	0x1F8

typedef struct rod_token {
    uint8_t	type[4];
    uint8_t	reserved[2];
    uint8_t	length[2];
} rod_token_t;

#define RECEIVE_COPY_RESULTS_SVACT_OPERATING_PARAMETERS      	0x03
#define RECEIVE_ROD_TOKEN_INFORMATION				0x07

/* 
 * Receive Copy Results Definitions: (used by token based copy - ODX method)
 */
typedef struct receive_copy_results_cdb {
    uint8_t opcode;			/* Operation code.		[0] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[1] */
	service_action		: 5,    /* Service action.	     (b0:4) */
	reserved_byte1_b5_7	: 3;    /* Reserved.		     (b5:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[1] */
	reserved_byte1_b5_7	: 3,    /* Reserved.		     (b5:7) */
	service_action		: 5;    /* Service action.	     (b0:4) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t list_identifier[4];		/* List identifier.	      [2-5] */
    uint8_t reserved_byte_6_9[4];	/* Reserved.		      [6-9] */
    uint8_t allocation_length[4];	/* Allocation length.       [10-13] */
    uint8_t reserved_byte14;		/* Reserved.		       [14] */
    uint8_t control;			/* Control.	               [15] */
} receive_copy_results_cdb_t;

typedef enum copy_status {
    COPY_STATUS_UNINIT              = 0x00,
    COPY_STATUS_SUCCESS             = 0x01,
    COPY_STATUS_FAIL                = 0x02,
    COPY_STATUS_SUCCESS_RESID       = 0x03,
    COPY_STATUS_FOREGROUND          = 0x11,
    COPY_STATUS_BACKGROUND          = 0x12,
    COPY_STATUS_TERMINATED          = 0xE0,
} copy_status_t;

/*
 * Receive ROD Token Information Parameter Data:
 */
typedef struct rrti_parameter_data {
    uint8_t available_data[4];		/* Available data.	      [0-3] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[4] */
	response_to_service_action : 5, /* Response to service action(b0:4) */
	reserved_byte4_b5_7	: 3;    /* Reserved.		     (b5:7) */
    bitfield_t				/*				[5] */
	copy_operation_status	: 5,    /* Service operation status. (b0:6) */
	reserved_byte5_b7	: 3;    /* Reserved.		       (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[4] */
	reserved_byte4_b5_7	: 3,    /* Reserved.		     (b5:7) */
	response_to_service_action : 5; /* Response to service action(b0:4) */
    bitfield_t				/*				[5] */
	copy_operation_status	: 5,    /* Service operation status. (b0:6) */
	reserved_byte5_b7	: 3;    /* Reserved.		       (b7) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t operation_counter[2];	/* Operation counter.	      [6-7] */
    uint8_t estimated_status_update_delay[4]; /* Status update delay.[8-11] */
    uint8_t extended_copy_completion_status;  /* Completion status.    [12] */
    uint8_t sense_data_field_length;	/* Sense data filed length.    [13] */
    uint8_t sense_data_length;		/* Sense data length.	       [14] */
    uint8_t transfer_count_units;	/* Transfer count units.       [15] */
    uint8_t transfer_count[8];		/* Transfer count.	    [16-23] */
    uint8_t segments_processed[2];	/* Segments processed.	    [24-25] */
    uint8_t reserved_byte_26_31[6];	/* Reserved.		    [26-31] */
} rrti_parameter_data_t;

/*
 * Receive ROD Token Response Service Actions.
 */
#define SCSI_RRTI_PT 	0x10
#define SCSI_RRTI_WUT 	0x11

typedef struct rod_token_parameter_data {
    uint8_t rod_token_descriptors_length[4];	/* ROD token desc. length. [0-3] */
    uint8_t restricted_byte_4_5[2];		/* Restricted.		   [4-5] */
} rod_token_parameter_data_t;

#define RRTI_PT_DATA_SIZE	sizeof(rrti_parameter_data_t) +	\
				sizeof(rod_token_parameter_data_t) + \
				ROD_TOKEN_LENGTH

#if defined(__IBMC__)
#  pragma options align=reset
#endif /* defined(__IBMC__) */

#endif /* !defined(SCSI_CDBS_INCLUDE) */
