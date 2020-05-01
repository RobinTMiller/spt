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
 * File:	inquiry.h
 * Date:	April 9, 1991
 * Author:	Robin T. Miller
 *
 * Description:
 *      SCSI Inquiry definitions.
 *
 * Modification History:
 *
 * December 18th, 2018 by Robin T. Miller
 *      Increase the Inquiry allocation length, introduced in SPC-3.
 */
#if !defined(INQUIRY_INCLUDE)
#define INQUIRY_INCLUDE 1

#if !defined(_BITFIELDS_LOW_TO_HIGH_) && !defined(_BITFIELDS_HIGH_TO_LOW_)
#   error "bitfield ordering is NOT defined!"
#endif /* !defined(_BITFIELDS_LOW_TO_HIGH_) || !defined(_BITFIELDS_HIGH_TO_LOW_) */

#if defined(__IBMC__)
/* IBM aligns bit fields to 32-bits by default! */
#  pragma options align=bit_packed
#endif /* defined(__IBMC__) */

#include "include.h"

/* %Z%%M% %I% %E% 1990 by Robin Miller. */

/*
 * Defined Peripheral Quailifiers:
 */
#define PQUAL_CONNECTED		0x0	/* Device is connected.		*/
#define PQUAL_NOT_CONNECTED	0x1	/* Device is NOT connected.	*/
#define PQUAL_NO_PHYSICAL	0x3	/* No physical device support.	*/
#define PQUAL_VENDOR_SPECIFIC	0x4	/* Vendor specific peripheral.	*/

/*
 * Defined Device Types:
 */
#define	DTYPE_DIRECT		0x00	/* Direct access block device.	*/
#define	DTYPE_SEQUENTIAL	0x01	/* Sequential access device.	*/
#define	DTYPE_PRINTER		0x02	/* Printer device.		*/ /* SPC-5 Obsolete */
#define	DTYPE_PROCESSOR		0x03	/* Processor device.		*/
#define	DTYPE_WORM		0x04	/* Write-Once/Read Many.	*/ /* SPC-5 Obsolete */
#define DTYPE_MULTIMEDIA	0x05	/* CD/DVD device.		*/
#define	DTYPE_SCANNER		0x06	/* Scanner device.		*/ /* SPC-5 Reserved */
#define	DTYPE_OPTICAL		0x07	/* Optical memory device.	*/
#define	DTYPE_CHANGER		0x08	/* Media changer device.	*/
#define DTYPE_COMMUNICATIONS	0x09	/* Communications device.	*/ /* SPC-5 Reserved */
#define DTYPE_PREPRESS_0	0x0A	/* Graphics pre-press device.	*/ /* SPC-5 Reserved */
#define DTYPE_PREPRESS_1	0x0B	/* Graphics pre-press device.	*/ /* SPC-5 Reserved */
#define DTYPE_RAID		0x0C	/* Array controller device.	*/
#define DTYPE_ENCLOSURE		0x0D	/* Storage enclosure services.	*/
#define DTYPE_SIMPLIFIED_DIRECT	0x0E	/* Simplified direct-access.	*/
#define DTYPE_OPTICAL_CARD	0x0F	/* Optical card reader/writer.	*/
#define DTYPE_RESERVED_10	0x10	/* Reserved 0x10		*/ /* SPC-5 Reserved */
#define DTYPE_OBJECT_STORAGE	0x11	/* Object storage device.	*/
#define DTYPE_AUTOMATION_DRIVE	0x12	/* Automation/drive interface.	*/
#define DTYPE_OBSOLETE_13	0x13	/* Obsolete 0x13		*/ /* SPC-5 Obsolete */
#define DTYPE_HOST_MANAGED	0x14	/* Host managed zoned block.	*/
					/* 0x15-0x1D are reserved.	*/
#define DTYPE_WELL_KNOWN_LUN	0x1E	/* Well known logical unit.	*/
#define	DTYPE_NOTPRESENT	0x1F	/* Unknown or no device type.	*/
#define DTYPE_UNKNOWN		0xFF	/* (my) Unknown device type.	*/

/*
 * Device type bitmasks control access to commands and mode pages.
 */
#define ALL_DEVICE_TYPES	0xffffU	/* Supported for all devices.   */

/*
 * Random access devices support many of the commands and pages, so...
 */
#define ALL_RANDOM_DEVICES	(BITMASK(DTYPE_DIRECT) |		\
				 BITMASK(DTYPE_OPTICAL) |		\
				 BITMASK(DTYPE_MULTIMEDIA) |		\
				 BITMASK(DTYPE_WORM))
/*
 * ANSI Approved Versions:
 *
 * NOTE: Many devices are implementing SCSI-3 extensions, but since
 *	 that spec isn't approved by ANSI yet, return SCSI-2 response.
 */
#define ANSI_LEVEL0		0x00	/* May or maynot comply to ANSI	*/
#define ANSI_SCSI1		0x01	/* Complies to ANSI X3.131-1986	*/
#define ANSI_SCSI2		0x02	/* Complies to ANSI X3.131-1994 */
#define ANSI_SCSI3		0x03	/* Complies to ANSI X3.351-1997 */
#define ANSI_SPC	ANSI_SCSI3	/* SCSI Primary Commands 1.	*/
#define ANSI_SPC2		0x04	/* SCSI Primary Commands 2.	*/
#define ANSI_SPC3		0x05	/* SCSI Primary Commands 3.	*/
#define ANSI_SPC4		0x06	/* SCSI Primary Commands 4.	*/
					/* ANSI codes > 6 are reserved.	*/
/*
 * Response Data Formats:
 */
#define	RDF_SCSI1		0x00	/* SCSI-1 inquiry data format.	*/
#define	RDF_CCS			0x01	/* CCS inquiry data format.	*/
#define	RDF_SCSI2		0x02	/* SCSI-2 inquiry data format.	*/
					/* RDF codes > 2 are reserved.	*/
/*
 * The first 36 bytes of inquiry is standard, vendor unique after that.
 */
#define STD_INQ_LEN		36	/* Length of standard inquiry.	*/
#define STD_ADDL_LEN		31	/* Standard additional length.	*/
#define MAX_INQ_LEN		255	/* Maxiumum Inquiry length.	*/

#define INQ_VID_LEN		8	/* The vendor ident length.	*/
#define INQ_PID_LEN		16	/* The Product ident length.	*/
#define INQ_REV_LEN		4	/* The revision level length.	*/

/*
 * Note: These bits fields and union below are historic and allowed us 
 * to access these fields as both a byte or via bit fields. This was 
 * done since older versions of the spec these fields were reserved. 
 */
typedef struct inquiry_sflags {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				 [6] */
	inqf_obsolete_byte6_b0	: 1,	/* Obsolete			(b0) */
	inqf_reserved_byte6_b1_2: 2,	/* Reserved.		      (b1:2) */
	inqf_obsolete_byte6_b3	: 1,	/* Obsolete.			(b3) */
	inqf_multip		: 1,	/* Multiple SCSI ports.		(b4) */
	inqf_vs_byte6_b5	: 1,	/* Vendor specific.		(b5) */
	inqf_encserv		: 1,	/* Enclosure services.		(b6) */
	inqf_obsolete_byte6_b7	: 1;	/* Obsolete.			(b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				 [6] */
	inqf_obsolete_byte6_b7	: 1,	/* Obsolete.			(b7) */
	inqf_encserv		: 1,	/* Enclosure services.		(b6) */
	inqf_vs_byte6_b5	: 1,	/* Vendor specific.		(b5) */
	inqf_multip		: 1,	/* Multiple SCSI ports.		(b4) */
	inqf_obsolete_byte6_b3	: 1,	/* Obsolete.			(b3) */
	inqf_reserved_byte6_b1_2: 2,	/* Reserved.		      (b1:2) */
	inqf_obsolete_byte6_b0	: 1;	/* Obsolete			(b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} inquiry_sflags_t;

typedef struct inquiry_flags {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				 [7] */
	inqf_vs_byte7_b0	: 1,	/* Vendor Specific.	 	(b0) */
	inqf_cmdque		: 1,	/* Command queuing support.	(b1) */
	inqf_reserved_byte7_b2	: 1,	/* Reserved.			(b2) */
	inqf_obsolete_byte7_b3_3: 3,	/* Obsolete.		      (b3:3) */
	inqf_reserved_byte7_b6	: 1,	/* Reserved.			(b6) */
	inqf_obsolete_byte7_b7	: 1;	/* Obsolete.			(b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				 [7] */
	inqf_obsolete_byte7_b7	: 1,	/* Obsolete.			(b7) */
	inqf_reserved_byte7_b6	: 1,	/* Reserved.			(b6) */
	inqf_obsolete_byte7_b3_3: 3,	/* Obsolete.		      (b3:3) */
	inqf_reserved_byte7_b2	: 1,	/* Reserved.			(b2) */
	inqf_cmdque		: 1,	/* Command queuing support.	(b1) */
	inqf_vs_byte7_b0	: 1;	/* Vendor Specific.	 	(b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} inquiry_flags_t;

/*
 * SCSI Inquiry Data Structure:
 */
typedef struct {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				 [0] */
	inq_dtype		: 5,	/* Peripheral device type.    (b0:5) */
	inq_pqual		: 3;	/* Peripheral qualifier.      (b6:3) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				 [0] */
	inq_pqual		: 3,	/* Peripheral qualifier.      (b6:3) */
	inq_dtype		: 5;	/* Peripheral device type.    (b0:5) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*			         [1] */
	inq_reserved_byte1_b0_5	: 6,	/* Reserved.		      (b0:6) */
	inq_lu_cong     	: 1,    /* Logical unit conglomerate.   (b6) */
	inq_rmb         	: 1;    /* Removable media.             (b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				 [1] */
	inq_rmb         	: 1,    /* Removable media.             (b7) */
	inq_lu_cong     	: 1,    /* Logical unit conglomerate.   (b6) */
	inq_reserved_byte1_b0_5	: 6;	/* Reserved.		      (b0:6) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t inq_ansi_version;	/* ANSI version.		 [2] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				 [3] */
	inq_rdf			: 4,	/* Response data format.      (b0:3) */
	inq_hisup		: 1,	/* Historical support.	        (b4) */
	inq_normaca		: 1,	/* Normal ACA supported.	(b5) */
	inq_reserved_byte3_b6_7	: 2;	/* Reserved.		      (b6:2) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				 [3] */
	inq_reserved_byte3_b6_7	: 2,	/* Reserved.		      (b6:2) */
	inq_normaca		: 1,	/* Normal ACA supported.	(b5) */
	inq_hisup		: 1,	/* Historical support.          (b4) */
	inq_rdf			: 4;	/* Response data format.      (b0:3) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	inq_addlen;		/* Additional length.		 [4] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				 [5] */
	inq_protect		: 1,	/* Supports Protection Info.	(b0) */
	inq_reserved_byte5_b1_2	: 2,	/* Reserved.		      (b1:2) */
	inq_3pc			: 1,	/* 3rd Party Copy Support.	(b3) */
	inq_tpgs		: 2,	/* Target Port Group Support. (b4:2) */
	inq_obsolete_byte5_b6	: 1,	/* Reserved.			(b6) */
	inq_sccs		: 1;	/* Storage Controller Components(b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				 [5] */
        inq_sccs		: 1,	/* Storage Controller Components(b7) */
	inq_obsolete_byte5_b6	: 1,	/* Reserved.			(b6) */
	inq_tpgs		: 2,	/* Target Port Group Support. (b4:2) */
	inq_3pc			: 1,	/* 3rd Party Copy Support.	(b3) */ 
	inq_reserved_byte5_b1_2	: 2,	/* Reserved.		      (b1:2) */
	inq_protect		: 1;	/* Supports Protection Info.	(b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	union {
		uint8_t sflags;		/* SCSI-3 capability flags.	[6] */
		struct inquiry_sflags bits;
	} s3un;
#define inq_sflags		s3un.sflags
#define inq_obsolete_byte6_b0	s3un.bits.inqf_obsolete_byte6_b0
#define inq_reserved_byte6_b1_2	s3un.bits.inqf_reserved_byte6_b1_2
#define inq_obsolete_byte6_b3	s3un.bits.inqf_obsolete_byte6_b3
#define inq_multip		s3un.bits.inqf_multip
#define inq_vs_byte6_b5		s3un.bits.inqf_vs_byte6_b5
#define inq_encserv		s3un.bits.inqf_encserv
#define inq_obsolete_byte6_b7	s3un.bits.inqf_obsolete_byte6_b7
	union {
		uint8_t flags;		/* Device capability flags.	[7] */
		struct inquiry_flags bits;
	} un;
#define inq_flags		un.flags
#define inq_vs_byte7_b0		un.bits.inqf_vs_byte7_b0
#define inq_cmdque		un.bits.inqf_cmdque
#define inq_reserved_byte7_b2	un.bits.inqf_reserved_byte7_b2
#define inq_obsolete_byte7_b3_3	un.bits.inqf_obsolete_byte7_b3_3
#define inq_reserved_byte7_b6	un.bits.inqf_reserved_byte7_b6
#define inq_obsolete_byte7_b7	un.bits.inqf_obsolete_byte7_b7
	uint8_t	inq_vid[INQ_VID_LEN];	/* Vendor ID.		     [8-15] */
	uint8_t	inq_pid[INQ_PID_LEN];	/* Product ID.		    [16-31] */
	uint8_t	inq_revlevel[INQ_REV_LEN];/* Revision level.	    [32-35] */
	uint8_t	inq_vendor_unique[MAX_INQ_LEN - STD_INQ_LEN];
} inquiry_t;

/*
 * Inquiry Flag Bits:
 */
#define INQ_EVPD	0x01		/* Enable vital product data.	*/
#define INQ_CMDDT	0x02		/* Command support data.	*/

/*
 * Inquiry Page Codes:
 */
#define INQ_ALL_PAGES		0x00	/* Supported vital product data	*/
#define INQ_SERIAL_PAGE		0x80	/* Unit serial number page.	*/
#define INQ_IMPOPR_PAGE		0x81	/* Implemented opr defs page.	*/
#define INQ_ASCOPR_PAGE		0x82	/* ASCII operating defs page.	*/
#define INQ_DEVICE_PAGE		0x83	/* Device Identification page.	*/

#define INQ_SOFT_INT_ID_PAGE	0x84	/* Software Interface Identification. */
#define INQ_MGMT_NET_ADDR_PAGE	0x85	/* Management Network Addresses.*/
#define INQ_EXTENDED_INQ_PAGE	0x86	/* Extended INQUIRY Data.	*/
#define INQ_MP_POLICY_PAGE	0x87	/* Mode Page Policy.		*/
#define INQ_SCSI_PORTS_PAGE	0x88	/* SCSI Ports.			*/
#define INQ_ATA_INFO_PAGE	0x89	/* ATA Information.		*/
#define INQ_POWER_CONDITION     0x8A    /* Power condition.             */
#define INQ_POWER_CONSUMPTION	0x8D	/* Power consumption.		*/
#define INQ_PROTO_LUN_INFO      0x90    /* Protocol Logical Unit Info.  */
#define INQ_PROTO_PORT_INFO     0x91    /* Protocol Specific Port Info. */
#define INQ_THIRD_PARTY_COPY    0x8F    /* Third party copy.            */
#define INQ_BLOCK_LIMITS_PAGE	0xB0	/* Block limits.		*/
#define INQ_LOGICAL_BLOCK_PROVISIONING_PAGE 0xB2
					/* Logical block provisioning.	*/
#define INQ_BLOCK_CHAR_VPD_PAGE 0xB1    /* Block Device Characteristics VPD. */

#define INQ_ASCIIINFO_START	0x01	/* ASCII Info starting page.	*/
#define INQ_ASCIIINFO_END	0x07	/* ASCII Info ending page value	*/
#define INQ_ASCIIINFO_PAGE01	0x01
#define INQ_ASCIIINFO_PAGE02	0x02
#define INQ_ASCIIINFO_PAGE03	0x03
#define INQ_ASCIIINFO_PAGE04	0x04
#define INQ_ASCIIINFO_PAGE05	0x05
#define INQ_ASCIIINFO_PAGE06	0x06
#define INQ_ASCIIINFO_PAGE07	0x07

#define INQ_RESERVED_START	0x84	/* Reserved starting page value	*/
#define INQ_RESERVED_END	0xBF	/* Reserved ending page value.	*/
#define INQ_VENDOR_START	0xC0	/* Vendor-specific start value.	*/
#define INQ_VENDOR_END		0xFF	/* Vendor-specific ending value	*/
#define MAX_INQUIRY_PAGE	0xFF	/* Maximum inquiry page code.	*/

#define INQ_PAGE_UNKNOWN	-1

/*
 * Declarations/Definitions for Inquiry Command:
 */
#define InquiryName           "Inquiry"
#define InquiryOpcode         0x12
#define InquiryCdbSize        6
#define InquiryTimeout        ScsiDefaultTimeout

typedef struct inquiry_header {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	uint8_t	inq_dtype	: 5,	/* Peripheral device type.	[0] */
		inq_pqual	: 3;	/* Peripheral qualifier.	    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	uint8_t	inq_pqual	: 3,	/* Peripheral qualifier.	    */
		inq_dtype	: 5;	/* Peripheral device type.	[0] */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	inq_page_code;		/* The inquiry page code.	[1] */
	uint8_t	inq_page_length[2];	/* The page code length.      [2-3] */
					/* Variable length data follows.    */
} inquiry_header_t;

#define MAX_INQ_PAGE_LENGTH	(MAX_INQ_LEN - sizeof(inquiry_header_t))

typedef struct inquiry_page {
	inquiry_header_t inquiry_hdr;
	uint8_t inquiry_page_data[MAX_INQ_PAGE_LENGTH];
} inquiry_page_t;

/*
 * Operating Definition Parameter Values:
 */
#define OPDEF_CURRENT	0x00		/* Use current operating def.	*/
#define OPDEF_SCSI1	0x01		/* SCSI-1 operating definition.	*/
#define OPDEF_CCS	0x02		/* CCS operating definition.	*/
#define OPDEF_SCSI2	0x03		/* SCSI-2 operating definition.	*/
#define OPDEF_SCSI3	0x04		/* SCSI-3 operating definition.	*/
#define OPDEF_MAX	0x05		/* Number of operating defs.	*/

struct opdef_param {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	uint8_t	opdef	: 7,		/* Operating definition.	*/
		savimp	: 1;		/* Operating def can be saved.	*/
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	uint8_t	savimp	: 1,		/* Operating def can be saved.	*/
		opdef	: 7;		/* Operating definition.	*/
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
};

/*
 * Implemented Operating Definition Page.
 */
typedef struct inquiry_opdef_page {
	inquiry_header_t inquiry_header;
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	uint8_t	current_opdef	: 7,	/* Current operating definition */
				: 1;	/* Reserved.			*/
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	uint8_t			: 1,	/* Reserved.			*/
		current_opdef	: 7;	/* Current operating definition */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
	uint8_t	default_opdef	: 7,	/* Default operating definition	*/
		default_savimp	: 1;	/* Operating def can be saved.	*/
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
	uint8_t	default_savimp	: 1,	/* Operating def can be saved.	*/
		default_opdef	: 7;	/* Default operating definition	*/
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
	uint8_t	support_list[10];	/* Supported definition list.	*/
} inquiry_opdef_page_t;

/*
 * Device Identification Page Definitions:
 */
#define IID_CODE_SET_RESERVED	0x00	/* Reserved.			*/
#define IID_CODE_SET_BINARY	0x01	/* Identifier field is binary.	*/
#define IID_CODE_SET_ASCII	0x02	/* Identifier field is ASCII.	*/
#define IID_CODE_SET_ISO_IEC    0x03    /* Contains ISO/IEC identifier. */
					/* 0x04-0x0F are reserved.	*/
/*
 * Designator Type Definitions:
 */
#define IID_ID_TYPE_VS		0x0	/* ID type is vendor specific.	*/
#define IID_ID_TYPE_T10_VID	0x1	/* T10 vendor ID based. 	*/
#define IID_ID_TYPE_EUI64	0x2	/* EUI-64 based identifier.	*/
#define IID_ID_TYPE_NAA 	0x3	/* Name Address Authority (NAA) */
#define IID_ID_TYPE_RELTGTPORT  0x4     /* Relative target port ident.  */
#define IID_ID_TYPE_TGTPORTGRP  0x5     /* Target port group identifier */
#define IID_ID_TYPE_LOGUNITGRP  0x6     /* Logical unit group ident.    */
#define IID_ID_TYPE_MD5LOGUNIT  0x7     /* MD5 logical unit identifier. */
#define IID_ID_TYPE_SCSI_NAME   0x8     /* SCSI name string identifier. */
#define IID_ID_TYPE_PROTOPORT	0x9	/* Protocol specific port ID.	*/
#define IID_ID_TYPE_UUID	0xA	/* UUID identifier.		*/
					/* 0xB-0xF are reserved.	*/
/*
 * Association Definitions:
 */
#define IID_ASSOC_LOGICAL_UNIT  0x0     /* Associated w/logical unit.   */
#define IID_ASSOC_TARGET_PORT   0x1     /* Associated w/target port.    */
#define IID_ASSOC_TARGET_DEVICE 0x2     /* Associated w/target device.  */
#define IID_ASSOC_RESERVED	0x3	/* Reserved.			*/

/*
 * Name Address Authority (NAA) Definitions:
 */
#define NAA_IEEE_EXTENDED       0x2	/* IEEE Extended.	        */
#define NAA_LOCALLY_ASSIGNED	0x3	/* Locally Assigned.		*/
#define NAA_IEEE_REGISTERED     0x5	/* IEEE Registered.		*/
#define NAA_IEEE_REG_EXTENDED   0x6	/* IEEE Registered Extended.	*/
                                        /* All other values reserved.	*/

/* Note: SPC-4 and beyond now call this the Designation descriptor! */
typedef struct inquiry_ident_descriptor {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[0] */
	iid_code_set		: 4,    /* The code set.		    */
	iid_proto_ident		: 4;    /* Protocol identifier.             */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[0] */
	iid_proto_ident		: 4,    /* Protocol identifier.		    */
	iid_code_set		: 4;    /* The code set.		    */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[1] */
	iid_ident_type		: 4,    /* The identifier type.		    */
	iid_association		: 2,    /* Association.			    */
	iid_reserved_byte1_b6	: 1,	/* Reserved.                        */
	iid_proto_valid		: 1;    /* Protocol identifier valid.       */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[1] */
	iid_proto_valid		: 1,    /* Protocol identifier valid.       */
	iid_reserved_byte1_b6	: 1,	/* Reserved.                        */
	iid_association		: 2,    /* Association.			    */
	iid_ident_type		: 4;    /* The identifier type.		    */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t	iid_reserved_byte2;	/* Reserved.			[2] */
    uint8_t	iid_ident_length;	/* The identifier length.	[3] */
					/* Variable length identifier.	[4] */
} inquiry_ident_descriptor_t;

typedef struct inquiry_deviceid_page {
	inquiry_header_t inquiry_header;
	inquiry_ident_descriptor_t ident_descriptor;
} inquiry_deviceid_page_t;

/* Note: Defined without the Inquiry page header! */
typedef struct inquiry_network_service_page {
    uint8_t association_service_type;
    uint8_t reserved;
    uint8_t address_length[2];
    uint8_t address[1];
} inquiry_network_service_page_t;

/* 
 * Block Limits Data Structure: 
 */
typedef struct inquiry_block_limits_page {
    inquiry_header_t inquiry_header;
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t				/*				[4] */
	wsnz			: 1,    /* Write same no zero.	       (b0) */
	reserved_byte4_b1_7	: 7;    /* Reserved.		     (b1:7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t				/*				[4] */
	reserved_byte4_b1_7	: 7,    /* Reserved.		     (b1:7) */
	wsnz			: 1;    /* Write same no zero.	       (b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t max_caw_len;		/* Max compare & write length.	[5] */
    uint8_t opt_xfer_len_granularity[2];/* Optimal transfer granularity.[6-7] */
    uint8_t max_xfer_len[4];		/* Maximum transfer length.	[8-11] */
    uint8_t opt_xfer_len[4];		/* Optimal transfer length.	[12-15] */
    uint8_t max_prefetch_xfer_len[4];	/* Max prefetch xfer length.	[16-19] */
    uint8_t max_unmap_lba_count[4];	/* Max unmap LBA count.		[20-23] */
    uint8_t max_unmap_descriptor_count[4]; /* Max descriptor count.	[24-27] */
    uint8_t optimal_unmap_granularity[4]; /* Optimal unmap granularity.	[28-31] */
#define UGAVALID_BIT	0x80000000UL	/* Unmap granularity alignment valid. */
    uint8_t unmap_granularity_alignment[4]; /* Max unmap granularity.	[32-35] */
    uint8_t max_write_same_len[8];	/* Max write same length.	[36-43] */
    uint8_t reserved_bytes44_63[20];	/* Reserved.			[44-63] */
} inquiry_block_limits_page_t;

#if defined(__IBMC__)
#  pragma options align=reset
#endif /* defined(__IBMC__) */

#endif /* !defined(INQUIRY_INCLUDE) */
