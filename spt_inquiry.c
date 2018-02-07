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
 * Command:	spt_inquiry.c
 * Author:	Robin T. Miller
 * Date:	September 27th, 2006 
 *  
 * Modification History: 

 * November 16th, 2017 by Robin T. Miller
 *      Added support for all supported Inquiry log pages.
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "spt.h"
//#include "libscsi.h"
#include "inquiry.h"
#include "scsi_cdbs.h"

#include "parson.h"

/*
 * Forward References:
 */ 
int standard_inquiry(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp, inquiry_t *inquiry);

char *standard_inquiry_json(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp, inquiry_t *inquiry, char *page_name);

int PrintInquiryPageHeader(scsi_device_t *sdp, int offset,
			   inquiry_header_t *ihdr, vendor_id_t vendor_id);
JSON_Status PrintInquiryPageHeaderJson(scsi_device_t *sdp, JSON_Object *object, inquiry_header_t *ihdr);

/* Page 0x00 */
int inquiry_supported_decode(scsi_device_t *sdp, io_params_t *iop,
			     scsi_generic_t *sgp, inquiry_header_t *ihdr);
char *inquiry_supported_to_json(scsi_device_t *sdp, io_params_t *iop,
				inquiry_header_t *ihdr, char *page_name);

/* Page 0x03 */
int inquiry_ascii_information03_decode(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp, inquiry_header_t *ihdr);
char *inquiry_ascii_information03_decode_json(scsi_device_t *sdp, io_params_t *iop,
					      scsi_generic_t *sgp, inquiry_header_t *ihdr, char *page_name);
static char *get_operating_state(uint32_t operating_state);
static char *get_functional_mode(uint32_t functional_mode);
static char *get_code_mode_type(uint32_t code_mode_type);

/* Page 0x80 */
int inquiry_serial_number_decode(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp, inquiry_header_t *ihdr);
char *inquiry_serial_number_decode_json(scsi_device_t *sdp, io_params_t *iop,
					scsi_generic_t *sgp, inquiry_header_t *ihdr, char *page_name);

char *GetDeviceType(uint8_t device_type);
char *GetPeripheralQualifier(inquiry_t *inquiry, hbool_t fullname);
char *get_inquiry_page_name(uint8_t device_type, uint8_t page_code, uint8_t vendor_id);

/*
 * Short names are preferred, but backwards compatability must be kept.
 */
static struct {
    char *sname;
    char *fname;
} pqual_table[] = {
    { "Device Connected",	"Peripheral Device Connected"      }, /* 0x0 */
    { "Device NOT Connected",	"Peripheral Device NOT Connected"  }, /* 0x1 */
    { "Reserved",		"Reserved"			   }, /* 0x2 */
    { "No Physical Device Support", "No Physical Device Support"   }  /* 0x3 */
};

static struct {
    char *sname;
    char *fname;
} ansi_table[] = {
    { "!ANSI", "May or maynot comply to ANSI-approved standard"}, /* 0x0 */
    { "SCSI-1", "Complies to ANSI X3.131-1986, SCSI-1"	},	  /* 0x1 */
    { "SCSI-2", "Complies to ANSI X3.131-1994, SCSI-2"	},	  /* 0x2 */
    { "SCSI-3", "Complies to ANSI X3.301-1997, SCSI-3"	},	  /* 0x3 */
    { "SPC-2",  "Complies to ANSI INCITS 351-2001, SPC-2" },	  /* 0x4 */
    { "SPC-3",  "Complies to ANSI INCITS 408-2005, SPC-3" },	  /* 0x5 */
    { "SPC-4",  "Complies to ANSI INCITS 513 Revision 37a" }	  /* 0x6 */
};
static int ansi_entries = (sizeof(ansi_table) / sizeof(ansi_table[0]));

/*
 * Operating Definition Parameter Table.
 */
static char *opdef_table[] = {
    "Use Current",						/* 0x00 */
    "SCSI-1",							/* 0x01 */
    "CCS",							/* 0x02 */
    "SCSI-2",							/* 0x03 */
    "SCSI-3",							/* 0x04 */
    "SPC-3",							/* 0x05 */
    "SPC-4"							/* 0x06 */
};
static int opdef_entries = (sizeof(opdef_table) / sizeof(char *));

char *reserved_str = "Reserved";
char *vendor_specific_str = "Vendor Specific";

int
setup_inquiry(scsi_device_t *sdp, scsi_generic_t *sgp, size_t data_length, uint8_t page)
{
    struct Inquiry_CDB *cdb;
    int status = SUCCESS;
    
    cdb = (struct Inquiry_CDB *)sgp->cdb;
    memset(cdb, '\0', sizeof(*cdb));
    sgp->data_dir = scsi_data_read;
    if (sgp->data_length == 0) {
	sgp->data_length = sizeof(inquiry_t);
	sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
    }
    /* Setup to execute a CDB operation. */
    sdp->op_type = SCSI_CDB_OP;
    sdp->encode_flag = True;
    sdp->decode_flag = True;
    cdb->opcode = (uint8_t)SOPC_INQUIRY;
    sgp->cdb_size = GetCdbLength(cdb->opcode);
    if (sdp->page_specified) {
	cdb->evpd = True;
	cdb->page_code = page;
    }
    if (cdb->allocation_length == 0) {
	cdb->allocation_length = sgp->data_length;
    }
    return(status);
}

int
inquiry_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    struct Inquiry_CDB *cdb;
    int status = SUCCESS;
    
    cdb = (struct Inquiry_CDB *)sgp->cdb;
    sgp->data_dir = scsi_data_read;
    if (sgp->data_length == 0) {
	sgp->data_length = sizeof(inquiry_t);
	sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
    }
    if (sdp->page_specified) {
	cdb->evpd = True;
	cdb->page_code = sdp->page_code;
    }
    if (cdb->allocation_length == 0) {
	cdb->allocation_length = sgp->data_length;
    }
    return(status);
}

int
inquiry_decode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    inquiry_t *inquiry = sgp->data_buffer;
    inquiry_header_t *ihdr = sgp->data_buffer;
    struct Inquiry_CDB *cdb;
    int status = SUCCESS;

    if (inquiry == NULL) {
	Fprintf(sdp, "No data buffer, so no data to decode!\n");
	return(FAILURE);
    }

    cdb = (struct Inquiry_CDB *)sgp->cdb;
    if (cdb->evpd == False) {
	status = standard_inquiry(sdp, iop, sgp, inquiry);
	return(status);
    }
    if (iop->first_time) {
	status = get_inquiry_information(sdp, iop, sgp);
	if (status == FAILURE) return(status);
	iop->first_time = False;
    }

    if (ihdr->inq_page_code == INQ_ALL_PAGES) {
	status = inquiry_supported_decode(sdp, iop, sgp, ihdr);			/* Page 0x00 */
    } else if (ihdr->inq_page_code == INQ_SERIAL_PAGE) {
	status = inquiry_serial_number_decode(sdp, iop, sgp, ihdr);		/* Page 0x80 */
    } else {
	sdp->verbose = True;
    }
    return(status);
}

/* ============================================================================================== */

/*
 * Supported Inquiry Pages (page 0x00).
 */
int
inquiry_supported_decode(scsi_device_t *sdp, io_params_t *iop,
			 scsi_generic_t *sgp, inquiry_header_t *ihdr)
{
    inquiry_page_t *inquiry_page = (inquiry_page_t *)ihdr;
    uint8_t device_type = iop->sip->si_inquiry->inq_dtype;
    uint8_t *pages, page_code = ihdr->inq_page_code;
    int page_length = ihdr->inq_page_length;
    char *page_name = NULL;
    int status = SUCCESS;

    if (sdp->output_format == JSON_FMT) {
	char *json_string = NULL;
	json_string = inquiry_supported_to_json(sdp, iop, ihdr, "Supported Inquiry Pages");
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }
    /* Format: <page header><page code>...*/
    pages = (uint8_t *)ihdr + sizeof(*ihdr);

    PrintHeader(sdp, "Inquiry Pages Supported");

    if (sdp->DebugFlag) {
	uint8_t *ucp = (uint8_t *)ihdr;
	int length = page_length;
	int offset = 0;
	offset = PrintHexData(sdp, offset, ucp, length);
    }

    for ( ; page_length-- ; pages++) {
	page_code = *pages;
	page_name = get_inquiry_page_name(device_type, page_code, iop->vendor_id);
	Printf(sdp, "%34.34s Page (Code = 0x%02x)\n", page_name, page_code);
    }
    Printf(sdp, "\n");
    return(status);
}

/*
 * Supported Inquiry Pages (Page 0x00) in JSON Format:
 */
char *
inquiry_supported_to_json(scsi_device_t *sdp, io_params_t *iop,
			  inquiry_header_t *ihdr, char *page_name)
{
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    JSON_Value  *svalue = NULL;
    JSON_Object *sobject = NULL;
    JSON_Value	*enc_value = NULL;
    JSON_Status json_status;
    char *json_string = NULL;
    inquiry_page_t *inquiry_page = (inquiry_page_t *)ihdr;
    uint8_t *pages, page_code = INQ_ALL_PAGES;
    int page_length = ihdr->inq_page_length;
    uint8_t device_type = iop->sip->si_inquiry->inq_dtype;
    char *inq_page_name = NULL;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    uint8_t *ucp = NULL;
    int length = 0;
    int offset = 0;
    int status = SUCCESS;

    root_value  = json_value_init_object();
    if (root_value == NULL) return(NULL);
    root_object = json_value_get_object(root_value);

    value  = json_value_init_object();
    if (value == NULL) {
	json_value_free(root_value);
	return(NULL);
    }
    json_status = json_object_dotset_value(root_object, page_name, value);
    object = json_value_get_object(value);

    ucp = (uint8_t *)ihdr;
    length = page_length;
    if (sdp->report_format == REPORT_FULL) {
	json_status = json_object_set_number(object, "Length", (double)length);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(object, "Offset", (double)offset);
	if (json_status != JSONSuccess) goto finish;
	offset = FormatHexBytes(text, offset, ucp, length);
	json_status = json_object_set_string(object, "Bytes", text);
	if (json_status != JSONSuccess) goto finish;
    }
    (void)sprintf(text, "0x%02x", page_code);
    json_status = json_object_set_string(object, "Page Code", text);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Page Length", (double)page_length);
    if (json_status != JSONSuccess) goto finish;

    /* Format: <page header><page code>...*/
    pages = (uint8_t *)ihdr + sizeof(*ihdr);

    for ( ; page_length-- ; pages++) {
	page_code = *pages;
	inq_page_name = get_inquiry_page_name(device_type, page_code, iop->vendor_id);
	(void)sprintf(text, "Page 0x%02x", page_code);
	json_status = json_object_set_string(object, text, inq_page_name);
	if (json_status != JSONSuccess) break;
    }

finish:
    (void)json_object_set_number(object, "JSON Status", (double)json_status);
    if (sdp->json_pretty) {
	json_string = json_serialize_to_string_pretty(root_value);
    } else {
	json_string = json_serialize_to_string(root_value);
    }
    json_value_free(root_value);
    return(json_string);
}

/* ============================================================================================== */

int
standard_inquiry(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp, inquiry_t *inquiry)
{
    char vid[sizeof(inquiry->inq_vid)+1];
    char pid[sizeof(inquiry->inq_pid)+1];
    char revlevel[sizeof(inquiry->inq_revlevel)+1];
    uint8_t *vup = inquiry->inq_vendor_unique;
    int	addl_len = (int)inquiry->inq_addlen;
    int offset = 0;
    int status = SUCCESS;

    if (sdp->output_format == JSON_FMT) {
	char *json_string = NULL;
	json_string = standard_inquiry_json(sdp, iop, sgp, inquiry, "Inquiry");
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }

    PrintHeader(sdp, "Inquiry Information");

    offset = PrintHexDebug(sdp, offset, (uint8_t *)inquiry, sgp->data_transferred);

    /* Byte 0 */
    PrintHex(sdp, "Peripheral Device Type", inquiry->inq_dtype, DNL);
    Print(sdp, " (%s)\n", GetDeviceType(inquiry->inq_dtype));

    PrintHex(sdp, "Peripheral Qualifier", inquiry->inq_pqual, DNL);
    Print(sdp, " (%s)\n", GetPeripheralQualifier(inquiry, True));

    /* Byte 1 */
    if (inquiry->inq_reserved_byte1_b0_5 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 1, bits 0:5)", inquiry->inq_reserved_byte1_b0_5, PNL);
    }
    PrintBoolean(sdp, False, "Logical Unit Conglomerate", inquiry->inq_lu_cong, PNL);
    PrintBoolean(sdp, False, "Removable Media", inquiry->inq_rmb, PNL);

    /* Byte 2 */
    PrintDecimal(sdp, "ANSI Version", inquiry->inq_ansi_version, DNL);
    Print(sdp, " (%s)\n", (inquiry->inq_ansi_version < ansi_entries)
	    ? ansi_table[inquiry->inq_ansi_version].fname : reserved_str);

    /* Byte 3 */
    PrintDecimal(sdp, "Response Data Format", inquiry->inq_rdf, PNL);
    PrintBoolean(sdp, False, "Historical Support", inquiry->inq_hisup, PNL);
    PrintBoolean(sdp, False, "Normal ACA Support", inquiry->inq_normaca, PNL);
    if (inquiry->inq_reserved_byte3_b6_7 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 3, bits 6:2)", inquiry->inq_reserved_byte3_b6_7, PNL);
    }

    /* Byte 4 */
    PrintDecHex(sdp, "Additional Length", inquiry->inq_addlen, PNL);
    if (--addl_len <= 0) return(status);

    /* Inquiry Byte 5 */
    PrintBoolean(sdp, False, "Supports Protection Information", inquiry->inq_protect, PNL);
    if (inquiry->inq_reserved_byte5_b1_2 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 5, bits 1:2)", inquiry->inq_reserved_byte5_b1_2, PNL);
    }
    PrintBoolean(sdp, False, "Third Party Copy Support", inquiry->inq_3pc, PNL);
    PrintHex(sdp, "Target Port Group Support", inquiry->inq_tpgs, DNL);
    switch (inquiry->inq_tpgs) {
	case 0:
	    Print(sdp, " (ALUA not supported)\n");
	    break;
	case 1:
	    Print(sdp, " (implicit ALUA)\n");
	    break;
	case 2:
	    Print(sdp, " (explicit ALUA)\n");
	    break;
	case 3:
	    Print(sdp, " (explicit & implicit ALUA)\n");
	    break;
    }
    if (inquiry->inq_obsolete_byte5_b6 || sdp->DebugFlag) {
	PrintHex(sdp, "Obsolete (byte 5, bit 6)", inquiry->inq_obsolete_byte5_b6, PNL);
    }
    PrintBoolean(sdp, False, "Storage Controller Components", inquiry->inq_sccs, PNL);
    if (--addl_len == 0) return(status);

    /* Inquiry Byte 6 */
    if (inquiry->inq_obsolete_byte6_b0 || sdp->DebugFlag) {
	PrintHex(sdp, "Obsolete (byte 6, bit 0)", inquiry->inq_obsolete_byte6_b0, PNL);
    }
    if (inquiry->inq_reserved_byte6_b1_2 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 6, bits 1:2)", inquiry->inq_reserved_byte6_b1_2, PNL);
    }
    if (inquiry->inq_obsolete_byte6_b3 || sdp->DebugFlag) {
	PrintHex(sdp, "Obsolete (byte 6, bit 3)", inquiry->inq_obsolete_byte6_b3, PNL);
    }
    PrintBoolean(sdp, False, "Multiple SCSI Ports", inquiry->inq_multip, PNL);
    if ( (inquiry->inq_dtype == DTYPE_DIRECT) &&
	 (strncmp((char *)inquiry->inq_vid, "HGST", 4) == 0) ) {
	PrintDecimal(sdp, "Port Receiving Inquiry", inquiry->inq_port, DNL);
	Print(sdp, " (Port %s)\n", (inquiry->inq_port == 0) ? "A" : "B");
    } else if (inquiry->inq_vs_byte6_b5 || sdp->DebugFlag) {
	PrintBoolean(sdp, False, "Vendor Specific (byte 6, bit 5)", inquiry->inq_vs_byte6_b5, PNL);
    }
    PrintBoolean(sdp, False, "Embedded Enclosure Services", inquiry->inq_encserv, PNL);
    if (inquiry->inq_obsolete_byte6_b7 || sdp->DebugFlag) {
	PrintBoolean(sdp, False, "Obsolete (byte 6, bit 7)", inquiry->inq_obsolete_byte6_b7, PNL);
    }
    if (--addl_len == 0) return(status);

    /* Inquiry Byte 7 */
    if (inquiry->inq_vs_byte7_b0 || sdp->DebugFlag) {
	PrintBoolean(sdp, False, "Vendor Specific (byte 7, bit 0)", inquiry->inq_vs_byte7_b0, PNL);
    }
    PrintBoolean(sdp, False, "Command Queuing Support", inquiry->inq_cmdque, PNL);
    if (inquiry->inq_reserved_byte7_b2 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 7, bit 2)", inquiry->inq_reserved_byte7_b2, PNL);
    }
    if (inquiry->inq_obsolete_byte7_b3_3 || sdp->DebugFlag) {
	PrintHex(sdp, "Obsolete (byte 7, bits 3:3)", inquiry->inq_obsolete_byte7_b3_3, PNL);
    }
    if (inquiry->inq_reserved_byte7_b6 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 7, bit 6)", inquiry->inq_reserved_byte7_b6, PNL);
    }
    if (inquiry->inq_obsolete_byte7_b7 || sdp->DebugFlag) {
	PrintHex(sdp, "Obsolete (byte 7, bit 7)", inquiry->inq_obsolete_byte7_b7, PNL);
    }
    if (--addl_len == 0) return(status);

    strncpy (vid, (char *)inquiry->inq_vid, sizeof(inquiry->inq_vid));
    vid[sizeof(inquiry->inq_vid)] = '\0';
    PrintAscii(sdp, "Vendor Identification", vid, PNL);
    if ( (addl_len -= sizeof(inquiry->inq_vid)) <= 0) return(status);

    strncpy (pid, (char *)inquiry->inq_pid, sizeof(inquiry->inq_pid));
    pid[sizeof(inquiry->inq_pid)] = '\0';
    PrintAscii(sdp, "Product Identification", pid, PNL);
    if ( (addl_len -= sizeof(inquiry->inq_pid)) <= 0) return(status);

    strncpy (revlevel, (char *)inquiry->inq_revlevel, sizeof(inquiry->inq_revlevel));
    revlevel[sizeof(inquiry->inq_revlevel)] = '\0';
    PrintAscii(sdp, "Firmware Revision Level", revlevel, PNL);
    if ( (addl_len -= sizeof(inquiry->inq_revlevel)) <= 0) return(status);

    if (addl_len > 0) {
	PrintDecHex(sdp, "Vendor Data Length", addl_len, PNL);
	PrintAscii(sdp, "Vendor Specific Data", "", DNL);
	PrintHAFields(sdp, vup, addl_len);
    }
    Printf(sdp, "\n");
    return(status);
}

char *
standard_inquiry_json(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp, inquiry_t *inquiry, char *page_name)
{
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    JSON_Status json_status;
    char *json_string = NULL;
    char vid[sizeof(inquiry->inq_vid)+1];
    char pid[sizeof(inquiry->inq_pid)+1];
    char revlevel[sizeof(inquiry->inq_revlevel)+1];
    char text[STRING_BUFFER_SIZE];
    char *str = NULL;
    uint8_t *ucp = NULL;
    int length = 0;
    int offset = 0;
    int	addl_len = (int)inquiry->inq_addlen;

    root_value  = json_value_init_object();
    if (root_value == NULL) return(NULL);
    root_object = json_value_get_object(root_value);

    value  = json_value_init_object();
    if (value == NULL) {
	json_value_free(root_value);
	return(NULL);
    }
    json_status = json_object_dotset_value(root_object, page_name, value);
    object = json_value_get_object(value);

    ucp = (uint8_t *)inquiry;
    length = sgp->data_transferred;
    json_status = json_object_set_number(object, "Length", (double)length);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Offset", (double)offset);
    if (json_status != JSONSuccess) goto finish;
    offset = FormatHexBytes(text, offset, ucp, length);
    json_status = json_object_set_string(object, "Bytes", text);
    if (json_status != JSONSuccess) goto finish;

    /* Byte 0 */
    json_status = json_object_set_number(object, "Peripheral Device Type", (double)inquiry->inq_dtype);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_string(object, "Peripheral Device Type Description", GetDeviceType(inquiry->inq_dtype));
    if (json_status != JSONSuccess) goto finish;

    json_status = json_object_set_number(object, "Peripheral Qualifier", (double)inquiry->inq_pqual);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_string(object, "Peripheral Qualifier Description", GetPeripheralQualifier(inquiry, True));
    if (json_status != JSONSuccess) goto finish;

    /* Byte 1 */
    json_status = json_object_set_number(object, "Reserved (byte 1, bits 0:5)", (double)inquiry->inq_reserved_byte1_b0_5);
    if (json_status != JSONSuccess) goto finish;

    json_status = json_object_set_boolean(object, "Logical Unit Conglomerate", inquiry->inq_lu_cong);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(object, "Removable Media", inquiry->inq_rmb);
    if (json_status != JSONSuccess) goto finish;

    /* Byte 2 */
    json_status = json_object_set_number(object, "ANSI Version", (double)inquiry->inq_ansi_version);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_string(object, "ANSI Version Description",
	 (inquiry->inq_ansi_version < ansi_entries) ? ansi_table[inquiry->inq_ansi_version].fname : reserved_str);
    if (json_status != JSONSuccess) goto finish;

    /* Byte 3 */
    json_status = json_object_set_number(object, "Response Data Format", (double)inquiry->inq_rdf);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(object, "Historical Support", inquiry->inq_hisup);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(object, "Normal ACA Support", inquiry->inq_normaca);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Reserved (byte 3, bits 6:2)", (double)inquiry->inq_reserved_byte3_b6_7);
    if (json_status != JSONSuccess) goto finish;

    /* Byte 4 */
    json_status = json_object_set_number(object, "Additional Length", (double)inquiry->inq_addlen);
    if (json_status != JSONSuccess) goto finish;
    if (--addl_len <= 0) goto finish;

    /* Inquiry Byte 5 */
    json_status = json_object_set_boolean(object, "Supports Protection Information", inquiry->inq_protect);
    if (inquiry->inq_reserved_byte5_b1_2 || sdp->DebugFlag) {
	json_status = json_object_set_number(object, "Reserved (byte 5, bits 1:2)", (double)inquiry->inq_reserved_byte5_b1_2);
    }
    json_status = json_object_set_boolean(object, "Third Party Copy Support", inquiry->inq_3pc);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Target Port Group Support", (double)inquiry->inq_tpgs);
    if (json_status != JSONSuccess) goto finish;
    switch (inquiry->inq_tpgs) {
	case 0:
	    str = "ALUA not supported";
	    break;
	case 1:
	    str = "Implicit ALUA";
	    break;
	case 2:
	    str = "Explicit ALUA";
	    break;
	case 3:
	    str = "Explicit and implicit ALUA";
	    break;
    }
    json_status = json_object_set_string(object, "Target Port Group Support Description", str);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Obsolete (byte 5, bit 6)", (double)inquiry->inq_obsolete_byte5_b6);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(object, "Storage Controller Components", inquiry->inq_sccs);
    if (json_status != JSONSuccess) goto finish;
    if (--addl_len == 0) goto finish;

    /* Inquiry Byte 6 */
    json_status = json_object_set_number(object, "Obsolete (byte 6, bit 0)", (double)inquiry->inq_obsolete_byte6_b0);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Reserved (byte 6, bits 1:2)", (double)inquiry->inq_reserved_byte6_b1_2);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Obsolete (byte 6, bit 3)", (double)inquiry->inq_obsolete_byte6_b3);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(object, "Multiple SCSI Ports", inquiry->inq_multip);
    if (json_status != JSONSuccess) goto finish;

    if ( (inquiry->inq_dtype == DTYPE_DIRECT) &&
	 (strncmp((char *)inquiry->inq_vid, "HGST", 4) == 0) ) {
	str = (inquiry->inq_port == 0) ? "A" : "B";
	json_status = json_object_set_string(object, "Port Receiving Inquiry", str);
	if (json_status != JSONSuccess) goto finish;
    } else if (inquiry->inq_vs_byte6_b5 || sdp->DebugFlag) {
	json_status = json_object_set_boolean(object, "Vendor Specific (byte 6, bit 5)", inquiry->inq_vs_byte6_b5);
	if (json_status != JSONSuccess) goto finish;
    }
    json_status = json_object_set_boolean(object, "Embedded Enclosure Services", inquiry->inq_encserv);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(object, "Obsolete (byte 6, bit 7)", inquiry->inq_obsolete_byte6_b7);
    if (json_status != JSONSuccess) goto finish;
    if (--addl_len == 0) goto finish;

    /* Inquiry Byte 7 */
    json_status = json_object_set_boolean(object, "Vendor Specific (byte 7, bit 0)", inquiry->inq_vs_byte7_b0);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(object, "Command Queuing Support", inquiry->inq_cmdque);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Reserved (byte 7, bit 2)", (double)inquiry->inq_reserved_byte7_b2);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Obsolete (byte 7, bits 3:3)", (double)inquiry->inq_obsolete_byte7_b3_3);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Reserved (byte 7, bit 6)", (double)inquiry->inq_reserved_byte7_b6);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Obsolete (byte 7, bit 7)", (double)inquiry->inq_obsolete_byte7_b7);
    if (json_status != JSONSuccess) goto finish;
    if (--addl_len == 0) goto finish;

    strncpy (vid, (char *)inquiry->inq_vid, sizeof(inquiry->inq_vid));
    vid[sizeof(inquiry->inq_vid)] = '\0';
    json_status = json_object_set_string(object, "Vendor Identification", vid);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Obsolete (byte 7, bit 7)", (double)inquiry->inq_obsolete_byte7_b7);
    if (json_status != JSONSuccess) goto finish;
    if ( (addl_len -= sizeof(inquiry->inq_vid)) <= 0) goto finish;

    strncpy (pid, (char *)inquiry->inq_pid, sizeof(inquiry->inq_pid));
    pid[sizeof(inquiry->inq_pid)] = '\0';
    json_status = json_object_set_string(object, "Product Identification", pid);
    if (json_status != JSONSuccess) goto finish;
    if ( (addl_len -= sizeof(inquiry->inq_pid)) <= 0) goto finish;

    strncpy (revlevel, (char *)inquiry->inq_revlevel, sizeof(inquiry->inq_revlevel));
    revlevel[sizeof(inquiry->inq_revlevel)] = '\0';
    json_status = json_object_set_string(object, "Firmware Revision Level", revlevel);
    if (json_status != JSONSuccess) goto finish;
    if ( (addl_len -= sizeof(inquiry->inq_revlevel)) <= 0) goto finish;

finish:
    (void)json_object_set_number(object, "JSON Status", (double)json_status);
    if (sdp->json_pretty) {
	json_string = json_serialize_to_string_pretty(root_value);
    } else {
	json_string = json_serialize_to_string(root_value);
    }
    json_value_free(root_value);
    return(json_string);
}

/* ============================================================================================== */

int
PrintInquiryPageHeader(scsi_device_t *sdp, int offset,
		       inquiry_header_t *ihdr, vendor_id_t vendor_id)
{
    uint8_t page_code = ihdr->inq_page_code;
    int page_length = ihdr->inq_page_length;
    char *page_name = get_inquiry_page_name(ihdr->inq_dtype, page_code, vendor_id);

    Printf(sdp, "\n");
    Printf(sdp, "%s Page:\n", page_name);
    Printf(sdp, "\n");

    offset = PrintHexDebug(sdp, offset, (uint8_t *)ihdr, sizeof(*ihdr));
    PrintHex(sdp, "Peripheral Device Type", ihdr->inq_dtype, DNL);
    Print(sdp, " (%s)\n", GetDeviceType(ihdr->inq_dtype)); 
    PrintHex(sdp, "Peripheral Qualifier", ihdr->inq_pqual, DNL);
    if (ihdr->inq_pqual & PQUAL_VENDOR_SPECIFIC) {
	Printf(sdp, " (%s)\n", vendor_specific_str);
    } else {
	Print(sdp, " (%s)\n", pqual_table[ihdr->inq_pqual].fname);
    }
    PrintHex(sdp, "Page Code", page_code, PNL);
    if (ihdr->inq_reserved_byte2 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 2)", ihdr->inq_reserved_byte2, PNL);
    }
    PrintDecHex(sdp, "Page Length", page_length, PNL);
    return(offset);
}

JSON_Status
PrintInquiryPageHeaderJson(scsi_device_t *sdp, JSON_Object *object, inquiry_header_t *ihdr)
{
    uint8_t page_code = ihdr->inq_page_code;
    int page_length = ihdr->inq_page_length;
    char text[STRING_BUFFER_SIZE];
    JSON_Status json_status;

    /* Note: The page name is already setup! */
    (void)sprintf(text, "0x%02x", page_code);
    json_status = json_object_set_string(object, "Page Code", text);
    if (json_status != JSONSuccess) return(json_status);
    json_status = json_object_set_number(object, "Page Length", (double)page_length);
    if (json_status != JSONSuccess) return(json_status);
    json_status = json_object_set_number(object, "Peripheral Device Type", (double)ihdr->inq_dtype);
    if (json_status != JSONSuccess) return(json_status);
    json_status = json_object_set_string(object, "Peripheral Device Type Description", GetDeviceType(ihdr->inq_dtype));
    if (json_status != JSONSuccess) return(json_status);
    json_status = json_object_set_number(object, "Peripheral Qualifier", (double)ihdr->inq_pqual);
    if (json_status != JSONSuccess) return(json_status);
    if (ihdr->inq_pqual & PQUAL_VENDOR_SPECIFIC) {
	json_status = json_object_set_string(object, "Peripheral Qualifier Description", vendor_specific_str);
	if (json_status != JSONSuccess) return(json_status);
    } else {
	json_status = json_object_set_string(object, "Peripheral Qualifier Description", pqual_table[ihdr->inq_pqual].fname);
	if (json_status != JSONSuccess) return(json_status);
    }
    json_status = json_object_set_number(object, "Reserved (byte 2)", (double)ihdr->inq_reserved_byte2);
    return(json_status);
}

/* ============================================================================================== */

int
inquiry_serial_number_decode(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp, inquiry_header_t *ihdr)
{
    inquiry_page_t *inquiry_page = (inquiry_page_t *)ihdr;
    uint8_t device_type = iop->sip->si_inquiry->inq_dtype;
    uint8_t page_code = ihdr->inq_page_code;
    int page_length = ihdr->inq_page_length;
    char *page_name = get_inquiry_page_name(device_type, page_code, iop->vendor_id);
    char text[STRING_BUFFER_SIZE];
    uint8_t *ucp = NULL;
    int length = 0;
    int offset = 0;
    int status = SUCCESS;

    if (sdp->output_format == JSON_FMT) {
	char *json_string = NULL;
	json_string = inquiry_serial_number_decode_json(sdp, iop, sgp, ihdr, page_name);
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }
    offset = PrintInquiryPageHeader(sdp, offset, ihdr, iop->vendor_id);

    if (sdp->DebugFlag) {
	Printf(sdp, "\n");
	ucp = (uint8_t *)ihdr + sizeof(*ihdr);
	length = page_length;
	offset = PrintHexDebug(sdp, offset, ucp, length);
    }
    (void)strncpy(text, (char *)inquiry_page->inquiry_page_data, page_length);
    text[page_length] = '\0';

    PrintAscii(sdp, "Product Serial Number", text, PNL);

    Printf(sdp, "\n");
    return(status);
}

char *
inquiry_serial_number_decode_json(scsi_device_t *sdp, io_params_t *iop,
				  scsi_generic_t *sgp, inquiry_header_t *ihdr, char *page_name)
{
    inquiry_page_t *inquiry_page = (inquiry_page_t *)ihdr;
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    JSON_Status json_status;
    char *json_string = NULL;
    int page_length = ihdr->inq_page_length;
    char text[STRING_BUFFER_SIZE];
    uint8_t *ucp = NULL;
    int length = 0;
    int offset = 0;

    root_value  = json_value_init_object();
    if (root_value == NULL) return(NULL);
    root_object = json_value_get_object(root_value);

    value  = json_value_init_object();
    if (value == NULL) {
	json_value_free(root_value);
	return(NULL);
    }
    json_status = json_object_dotset_value(root_object, page_name, value);
    object = json_value_get_object(value);

    ucp = (uint8_t *)ihdr;
    length = page_length + sizeof(*ihdr);
    json_status = json_object_set_number(object, "Length", (double)length);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Offset", (double)offset);
    if (json_status != JSONSuccess) goto finish;
    offset = FormatHexBytes(text, offset, ucp, length);
    json_status = json_object_set_string(object, "Bytes", text);
    if (json_status != JSONSuccess) goto finish;

    json_status = PrintInquiryPageHeaderJson(sdp, object, ihdr);
    if (json_status != JSONSuccess) goto finish;

    (void)strncpy(text, (char *)inquiry_page->inquiry_page_data, page_length);
    text[page_length] = '\0';

    json_status = json_object_set_string(object, "Product Serial Number", text);

finish:
    (void)json_object_set_number(object, "JSON Status", (double)json_status);
    if (sdp->json_pretty) {
	json_string = json_serialize_to_string_pretty(root_value);
    } else {
	json_string = json_serialize_to_string(root_value);
    }
    json_value_free(root_value);
    return(json_string);
}

/* ============================================================================================== */

static struct {
    char *fname;                /* Full device type name.       */
    uint8_t dtype;              /* The Inquiry device type.     */
} dnames[] = {
    { "Direct Access Device",          		DTYPE_DIRECT		},
    { "Sequential Access Device",      		DTYPE_SEQUENTIAL	},
    { "Printer Device",                		DTYPE_PRINTER		},	/* SPC-5 Obsolete */
    { "Processor Device",              		DTYPE_PROCESSOR		},
    { "Write-Once/Read-Many",   		DTYPE_WORM		},	/* SPC-5 Obsolete */
    { "CD/DVD Device",				DTYPE_MULTIMEDIA	},
    { "Scanner Device",                		DTYPE_SCANNER		},	/* SPC-5 Reserved */
    { "Optical Memory Device",         		DTYPE_OPTICAL		},
    { "Media Changer Device",         		DTYPE_CHANGER		},
    { "Communications Device",         		DTYPE_COMMUNICATIONS	},	/* SPC-5 Reserved */
    { "Graphics Pre-press Device",     		DTYPE_PREPRESS_0	},	/* SPC-5 Reserved */
    { "Graphics Pre-press Device",     		DTYPE_PREPRESS_1	},	/* SPC-5 Reserved */
    { "Array Controller Device",       		DTYPE_RAID		},
    { "Enclosure Services Device",      	DTYPE_ENCLOSURE		},
    { "Simplified Direct-Access Device",	DTYPE_SIMPLIFIED_DIRECT	},
    { "Optical Card Reader/Writer Device",	DTYPE_OPTICAL_CARD	},
    { "Object Storage Device",			DTYPE_OBJECT_STORAGE	},
    { "Automation/Drive Interface",		DTYPE_AUTOMATION_DRIVE	},
    { "Host Managed Zoned Block Device",	DTYPE_HOST_MANAGED	},
    { "Well Known Logical Unit",		DTYPE_WELL_KNOWN_LUN	},
    { "Unknown or No Device Type",     		DTYPE_NOTPRESENT	}
};
static int dtype_entries = sizeof(dnames) / sizeof(dnames[0]);

char *
GetDeviceType(uint8_t device_type)
{
    int i;

    for (i = 0; i < dtype_entries; i++) {
	if (dnames[i].dtype == device_type) {
	    return (dnames[i].fname);
	}
    }
    return ("Reserved");
}

char *
GetPeripheralQualifier(inquiry_t *inquiry, hbool_t fullname)
{
    if (fullname) {
	return ( (inquiry->inq_pqual & PQUAL_VENDOR_SPECIFIC) ?
		 vendor_specific_str : pqual_table[inquiry->inq_pqual].fname);
    } else {
	return ( (inquiry->inq_pqual & PQUAL_VENDOR_SPECIFIC) ?
		 vendor_specific_str : pqual_table[inquiry->inq_pqual].sname);
    }
}

/*
 * Inquiry Lookup Table/Functions:
 */
typedef struct inquiry_page_entry {
    uint8_t		page_code;	/* The page code. */
    uint16_t		device_type;	/* THe device type (from Inquiry) */
    vendor_id_t		vendor_id;	/* The vendor ID (internal) */
    char		*page_name;	/* The page name. */
    char		*parse_name;	/* The parse name. */
} inquiry_page_entry_t;

inquiry_page_entry_t inquiry_page_table[] = {
    { INQ_ALL_PAGES, 		ALL_DEVICE_TYPES,	VID_ALL,	"Supported",		"supported"		},
    { INQ_SERIAL_PAGE,		ALL_DEVICE_TYPES,	VID_ALL,	"Serial Number",	"serial"		},
    { INQ_IMPOPR_PAGE,		ALL_DEVICE_TYPES,	VID_ALL,	"Implemented Operating Definitions", "implemented" },
    { INQ_ASCOPR_PAGE,		ALL_DEVICE_TYPES,	VID_ALL,	"ASCII Operating Definitions", "ascii_operating" },
    { INQ_SOFT_INT_ID_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Software Interface Identification", "software_interface" },
    { INQ_MGMT_NET_ADDR_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Management Network Addresses", "mgmt_network"	},
    { INQ_EXTENDED_INQ_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Extended INQUIRY Data", "extended_inquiry"	},
    { INQ_MP_POLICY_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Mode Page Policy",	"mode_page_policy"	},
    { INQ_SCSI_PORTS_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"SCSI Ports",		"scsi_ports"		},
    { INQ_ATA_INFO_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"ATA Information",	"ata_information"	},
    { INQ_POWER_CONDITION,	ALL_DEVICE_TYPES,	VID_ALL,	"Power Condition",	"power_condition"	},
    { INQ_POWER_CONSUMPTION,	ALL_DEVICE_TYPES,	VID_ALL,	"Power Consumption",	"power_consumption"	},
    { INQ_PROTO_LUN_INFO,	ALL_DEVICE_TYPES,	VID_ALL,	"Protocol Logical Unit Information", "protocol_lun_info" },
    { INQ_PROTO_PORT_INFO,	ALL_DEVICE_TYPES,	VID_ALL,	"Protocol Specific Port Information", "protocol_port_info" },
    { INQ_THIRD_PARTY_COPY,	ALL_DEVICE_TYPES,	VID_ALL,	"Third Party Copy",	"third_party_copy"	},
    { INQ_BLOCK_LIMITS_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Block Limits",		"block_limits"		},
    { INQ_BLOCK_CHAR_VPD_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Block Device Characteristics VPD", "block_char_vpd" },
    { INQ_LOGICAL_BLOCK_PROVISIONING_PAGE, ALL_DEVICE_TYPES, VID_ALL,	"Logical Block Provisioning", "logical_block_prov" }
};

int num_inquiry_page_entries = ( sizeof(inquiry_page_table) / sizeof(inquiry_page_entry_t) );

uint8_t
find_inquiry_page_code(scsi_device_t *sdp, char *page_name, int *status)
{
    inquiry_page_entry_t *ipe = inquiry_page_table;
    size_t length = strlen(page_name);
    int i;

    if (length == 0) {
	Printf(sdp, "\n");
	Printf(sdp, "Inquiry Page Codes/Names:\n");
	for (i = 0; i < num_inquiry_page_entries; i++, ipe++) {
	    Printf(sdp, "    0x%02x - %s (%s)\n",
		   ipe->page_code, ipe->page_name, ipe->parse_name);
	}
	Printf(sdp, "\n");
	*status = WARNING;
	return(INQ_PAGE_UNKNOWN);
    }

    /*
     * Note: Need to add device type and vendor ID checks, when implemented.
     */
    for (i = 0; i < num_inquiry_page_entries; i++, ipe++) {
	/* Allow a matching a portion (start of string). */
	if (strncasecmp(ipe->parse_name, page_name, length) == 0) {
	    *status = SUCCESS;
	    return( ipe->page_code );
	}
    }
    *status = FAILURE;
    return(INQ_PAGE_UNKNOWN);
}

char *
get_inquiry_page_name(uint8_t device_type, uint8_t page_code, uint8_t vendor_id)
{
    inquiry_page_entry_t *ipe = inquiry_page_table;
    char *page_name = NULL;
    int i;

    for (i = 0; i < num_inquiry_page_entries; i++, ipe++) {
	if ( ((ipe->device_type == ALL_DEVICE_TYPES) || (ipe->device_type == device_type)) &&
	     (ipe->page_code == page_code) &&
	     ((ipe->vendor_id == VID_ALL) || (ipe->vendor_id == vendor_id)) ) {
	    return( ipe->page_name );
	}
    }
    if ( (page_code >= INQ_ASCIIINFO_START) &&
	 (page_code <= INQ_ASCIIINFO_END) ) {
	page_name = "ASCII Information";
    } else if ( (page_code >= INQ_RESERVED_START) &&
		(page_code <= INQ_RESERVED_END) ) {
	page_name = "Reserved";
    } else if ( (page_code >= INQ_VENDOR_START) &&
		(page_code <= INQ_VENDOR_END) ) {
	page_name = "Vendor Specific";
    } else {
	page_name = "Unknown";
    }
    return(page_name);
}
