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
 * File:	spt_ses.c
 * Author:	Robin T. Miller
 * Date:	December 3rd, 2016
 *
 * Descriptions:
 *	SCSI Enclosure Services (SES) support functions.
 *
 * Modification History:
 * 
 * October 6th, 2017 by Robin T. Miller
 *      In Help page, ensure the ASCII data is null terminated.
 * 
 * February 1st, 2017 by Robin T. Miller
 *      For SES page 7 (element descriptor) add the element location, which
 * is basically the element descriptor number, used by our automation.
 * 
 * January 27th, 2017 by Robin T. Miller
 *      Fix bug in configuration decode to properly adjust for vendor bytes.
 *      Display lengths in both decimal and hexadecimal for ASCII output.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spt.h"
#include "libscsi.h"

#include "spt_ses.h"
#include "scsi_ses.h"
#include "scsi_cdbs.h"
#include "scsi_diag.h"

#include "parson.h"

/*
 * Forward References:
 */ 
int supported_receive_diagnostic_decode(scsi_device_t *sdp, io_params_t *iop,
					scsi_generic_t *sgp, diagnostic_page_header_t *dph);
char *diagnostic_supported_to_json(scsi_device_t *sdp, io_params_t *iop,
				   diagnostic_page_header_t *dph, char *page_name);

/* SES Page 0x01 */
int ses_config_receive_diagnostic_decode(scsi_device_t *sdp, io_params_t *iop,
					 scsi_generic_t *sgp, diagnostic_page_header_t *dph);
char *ses_config_to_json(scsi_device_t *sdp, ses_configuration_page_t *scp, char *page_name);

/* SES Page 0x02 */
int ses_enc_control_send_diagnostic(scsi_device_t *sdp, io_params_t *iop,
				    scsi_generic_t *sgp, diagnostic_page_header_t *dph);
int ses_element_type_control(scsi_device_t *sdp, element_type_t element_type, ses_control_element_t *cep);

int ses_enc_status_receive_diagnostic_decode(scsi_device_t *sdp, io_params_t *iop,
					     scsi_generic_t *sgp, diagnostic_page_header_t *dph);
int ses_process_element_descriptor(scsi_device_t *sdp,
				   ses_type_desc_header_t *tdp, ses_status_element_t *sep,
				   char *element_text, int element_index, int offset);
void ses_common_element_status(scsi_device_t *sdp, ses_status_common_t *scp);
int ses_element_type_status(scsi_device_t *sdp, element_type_t element_type, ses_status_element_t *sep);

char *ses_enc_status_to_json(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
			     ses_enclosure_status_page_t *esp, char *page_name);
JSON_Status ses_common_element_status_json(scsi_device_t *sdp, JSON_Object *dobject, ses_status_common_t *scp);
JSON_Status ses_element_type_status_json(scsi_device_t *sdp, JSON_Object *dobject,
					 element_type_t element_type, ses_status_element_t *sep);

/* SES Page 0x07 */
int ses_element_descriptor_receive_diagnostic_decode(scsi_device_t *sdp, io_params_t *iop,
						     scsi_generic_t *sgp, diagnostic_page_header_t *dph);
char *ses_element_descriptor_to_json(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
				     ses_element_descriptor_page_t *sedp, char *page_name);

/* SES Page 0x0A */
int ses_addl_element_status_receive_diagnostic_decode(scsi_device_t *sdp, io_params_t *iop,
						      scsi_generic_t *sgp, diagnostic_page_header_t *dph);
char *find_ses_protocol_identifier(uint8_t protocol_identifier);
char *find_sas_device_type(uint8_t device_type);
hbool_t valid_addl_element_types(uint8_t element_type);
int ses_protocol_specific_information(scsi_device_t *sdp, ses_addl_element_status_descriptor_t *aedp, element_type_t element_type);
char *ses_addl_element_status_to_json(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
				      ses_addl_element_status_page_t *aesp, char *page_name);
JSON_Status ses_protocol_specific_information_json(scsi_device_t *sdp, JSON_Object *dobject,
						   ses_addl_element_status_descriptor_t *aedp, element_type_t element_type);

/* SES Page 0x0E */
int ses_download_microcode_receive_diagnostic_decode(scsi_device_t *sdp, io_params_t *iop,
						     scsi_generic_t *sgp, diagnostic_page_header_t *dph);
char *get_download_microcode_status(ses_download_status_t microcode_status);
char *ses_download_microcode_status_to_json(scsi_device_t *sdp, io_params_t *iop,
					    ses_download_microcode_page_t *dmp, char *page_name);

char *get_diagnostic_page_name(uint8_t device_type, uint8_t page_code, uint8_t vendor_id);

char *get_element_type(element_type_t element_type);
char *get_element_status(element_status_t element_status);
char *get_element_status_desc(element_status_t element_status);
char *get_connector_type(uint8_t connector_type);
char *get_cooling_actual_speed(int actual_speed_code);

/*
 * Note: To be rewritten! For reference only!
 */
#define NUMBER_ELEMENT_TYPES 	ELEMENT_TYPE_SAS_CONNECTOR + 1
#define NUMBER_ELEMENT_BYTES	sizeof(ses_control_element_t)

/*
 * When setting (updating) element status descriptors, these four bytes are
 * used to clear bits returned by status page, but must be cleared prior to 
 * sending a control page with the SELECT set for desired element descriptors. 
 */
static uint8_t ses3_element_control_mask_array[NUMBER_ELEMENT_TYPES][NUMBER_ELEMENT_BYTES] = {
                                            /* Element type names. */
    {SES_CONTROL_MASK, 0xff, 0xff, 0xff},   /* ELEMENT_TYPE_UNSPECIFIED */
    {SES_CONTROL_MASK, 0x00, 0x4e, 0x3c},   /* ELEMENT_TYPE_DEVICE_SLOT */
    {SES_CONTROL_MASK, 0x80, 0x00, 0x60},   /* ELEMENT_TYPE_POWER_SUPPLY */
    {SES_CONTROL_MASK, 0x80, 0x00, 0x60},   /* ELEMENT_TYPE_COOLING */
    {SES_CONTROL_MASK, 0xc0, 0x00, 0x00},   /* ELEMENT_TYPE_SENSOR_TEMPERATURE */
    {SES_CONTROL_MASK, 0xc0, 0x00, 0x01},   /* ELEMENT_TYPE_DOOR */
    {SES_CONTROL_MASK, 0xc0, 0x00, 0x5f},   /* ELEMENT_TYPE_AUDIBLE_ALARM */
    {SES_CONTROL_MASK, 0xc0, 0x01, 0x00},   /* ELEMENT_TYPE_ESCE */
    {SES_CONTROL_MASK, 0xc0, 0x00, 0x00},   /* ELEMENT_TYPE_SCC_CTRL_ELECTRONICS */
    {SES_CONTROL_MASK, 0xc0, 0x00, 0x00},   /* ELEMENT_TYPE_NONVOLATILE_CACHE */
    {SES_CONTROL_MASK, 0x00, 0x00, 0x00},   /* ELEMENT_TYPE_INVALID_OPER_REASON */
    {SES_CONTROL_MASK, 0x00, 0x00, 0xc0},   /* ELEMENT_TYPE_UNINT_POWER_SUPPLY */
    {SES_CONTROL_MASK, 0xc0, 0xff, 0xff},   /* ELEMENT_TYPE_UNINT_POWER_SUPPLY */
    {SES_CONTROL_MASK, 0xc3, 0x00, 0x00},   /* ELEMENT_TYPE_KEY_PAD_ENTRY */
    {SES_CONTROL_MASK, 0x80, 0x00, 0xff},   /* ELEMENT_TYPE_ENCLOSURE */
    {SES_CONTROL_MASK, 0xc0, 0x00, 0x10},   /* ELEMENT_TYPE_SCSI_PORT_TRANS */
    {SES_CONTROL_MASK, 0x80, 0xff, 0xff},   /* ELEMENT_TYPE_LANGUAGE */
    {SES_CONTROL_MASK, 0xc0, 0x00, 0x01},   /* ELEMENT_TYPE_COMMUNICATION_PORT */
    {SES_CONTROL_MASK, 0xc0, 0x00, 0x00},   /* ELEMENT_TYPE_VOLTAGE_SENSOR */
    {SES_CONTROL_MASK, 0xc0, 0x00, 0x00},   /* ELEMENT_TYPE_CURRENT_SENSOR */
    {SES_CONTROL_MASK, 0xc0, 0x00, 0x01},   /* ELEMENT_TYPE_SCSI_TARGET_PORT */
    {SES_CONTROL_MASK, 0xc0, 0x00, 0x01},   /* ELEMENT_TYPE_SCSI_INITIATOR_PORT */
    {SES_CONTROL_MASK, 0xc0, 0x00, 0x00},   /* ELEMENT_TYPE_SIMPLE_SUBENCLOSURE */
    {SES_CONTROL_MASK, 0xff, 0x4e, 0x3c},   /* ELEMENT_TYPE_ARRAY_DEVICE_SLOT */
    {SES_CONTROL_MASK, 0xc0, 0x00, 0x00},   /* ELEMENT_TYPE_SAS_EXPANDER */
    {SES_CONTROL_MASK, 0x80, 0x00, 0x40}    /* ELEMENT_TYPE_SAS_CONNECTOR */
};

/* ============================================================================================== */

/*
 * parse_ses_args() - Parse the expected SES keywords.
 *
 * Inputs:
 *	string = The string to parse.
 *	sdp = The SCSI device information.
 *
 * Outputs:
 *	string = Updated input argument pointer.
 *
 * Return Value:
 *	Returns SUCCESS / FAILURE
 */ 
int
parse_ses_args(char *string, scsi_device_t *sdp)
{
    if (match(&string, "clear=")) {
	sdp->cmd_type = CMD_TYPE_CLEAR;
    } else if (match(&string, "set=")) {
	sdp->cmd_type = CMD_TYPE_SET;
    } else {
	Eprintf(sdp, "Invalid SES keyword found: %s\n", string);
	Printf(sdp, "Valid SES keywords are: clear=, set=, or reset=\n");
	return(FAILURE);
    }
    if (match(&string, "devoff")) {
	sdp->cgs_type = CGS_TYPE_DEVOFF;
    } else if ( match(&string, "fail") || match(&string, "fault")) {
	sdp->cgs_type = CGS_TYPE_FAULT;
    } else if ( match(&string, "ident") || match(&string, "locate") ) {
	sdp->cgs_type = CGS_TYPE_IDENT;
    } else if ( match(&string, "unlock") ) {
	sdp->cgs_type = CGS_TYPE_UNLOCK;
    } else {
	Eprintf(sdp, "Invalid SES keyword found: %s\n", string);
	Printf(sdp, "Valid SES keywords are: devoff, fail/fault, ident/locate, unlock\n");
	return(FAILURE);
    }
    return(SUCCESS);
}

/* ============================================================================================== */

int
setup_receive_diagnostic(scsi_device_t *sdp, scsi_generic_t *sgp, size_t data_length, uint8_t page)
{
    receive_diagnostic_cdb_t *cdb;

    cdb = (receive_diagnostic_cdb_t *)sgp->cdb;
    memset(cdb, '\0', sizeof(*cdb));
    cdb->pcv = sdp->page_code_valid;
    cdb->page_code = page;
    sgp->data_dir = scsi_data_read; /* Receiving parameter data from device. */
    sgp->data_length = (unsigned int)data_length;
    sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
    if (sgp->data_buffer == NULL) return (FAILURE);
    /* Setup to execute a CDB operation. */
    sdp->op_type = SCSI_CDB_OP;
    sdp->encode_flag = True;
    sdp->decode_flag = True;
    sgp->cdb[0] = (uint8_t)SOPC_RECEIVE_DIAGNOSTIC;
    sgp->cdb_size = GetCdbLength(sgp->cdb[0]);
    return(SUCCESS);
}

int
receive_diagnostic_page(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp, void **data, uint8_t page)
{
    scsi_generic_t *rsgp = Malloc(sdp, sizeof(*sgp));
    size_t data_length = RECEIVE_DIAGNOSTIC_MAX;
    int status;

    if (rsgp == NULL) return(FAILURE);
    *rsgp = *sgp;
    *data = NULL;
    /* Setup for receive diagnostic. */
    status = setup_receive_diagnostic(sdp, rsgp, data_length, page);
    if (status == SUCCESS) {
	receive_diagnostic_cdb_t *cdb = (receive_diagnostic_cdb_t *)rsgp->cdb;
	HtoS(cdb->allocation_length, rsgp->data_length);
	status = receive_diagnostic_encode(sdp);
	if (status == SUCCESS) {
	    rsgp->cdb_name = "Receive Diagnostic";
	    status = libExecuteCdb(rsgp);
	}
    }
    if (status == SUCCESS) {
	*data = rsgp->data_buffer;	/* Caller must free when finished! */
    } else if (rsgp->data_buffer) {
	free_palign(sdp, rsgp->data_buffer);
    }
    free(rsgp);
    return(status);
}

int
receive_diagnostic_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    receive_diagnostic_cdb_t *cdb;
    int status = SUCCESS;

    cdb = (receive_diagnostic_cdb_t *)sgp->cdb;
    cdb->pcv = sdp->page_code_valid;
    if (sdp->page_code) {
	cdb->page_code = sdp->page_code;
    }
    HtoS(cdb->allocation_length, sgp->data_length);
    sgp->data_dir = iop->sop->data_dir;
    return(status);
}

int
receive_diagnostic_decode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    diagnostic_page_header_t *dph = sgp->data_buffer;
    int status = SUCCESS;

    if (dph == NULL) return(status);

    if ( (dph->page_code == DIAG_SUPPORTED_PAGES) ||					/* Page 0x00 */
	 (dph->page_code == DIAG_SES_DIAGNOSTIC_PAGES_PAGE) ) {				/* Page 0x0D */
	status = supported_receive_diagnostic_decode(sdp, iop, sgp, dph);
    } else if (dph->page_code == DIAG_CONFIGURATION_PAGE) {				/* Page 0x01 */
	status = ses_config_receive_diagnostic_decode(sdp, iop, sgp, dph);
    } else if (dph->page_code == DIAG_ENCLOSURE_CONTROL_PAGE) {				/* Page 0x02 */
	if (sdp->cmd_type == CMD_TYPE_NONE) {
	    status = ses_enc_status_receive_diagnostic_decode(sdp, iop, sgp, dph);
	} else {
	    status = ses_enc_control_send_diagnostic(sdp, iop, sgp, dph);
	}
    } else if (dph->page_code == DIAG_ELEMENT_DESCRIPTOR_PAGE) {			/* Page 0x07 */
	status = ses_element_descriptor_receive_diagnostic_decode(sdp, iop, sgp, dph);
    } else if (dph->page_code == DIAG_ADDL_ELEMENT_STATUS_PAGE) {			/* Page 0x0A */
	status = ses_addl_element_status_receive_diagnostic_decode(sdp, iop, sgp, dph);
    } else if (dph->page_code == DIAG_DOWNLOAD_MICROCODE_CONTROL_PAGE) {		/* Page 0x0E */
	status = ses_download_microcode_receive_diagnostic_decode(sdp, iop, sgp, dph);
    } else {
	sdp->verbose = True;
    }
    return(status);
}

/* ============================================================================================== */
/*
 * Supported Diagnostic Pages (Page 0x00):
 */
int
supported_receive_diagnostic_decode(scsi_device_t *sdp, io_params_t *iop,
				    scsi_generic_t *sgp, diagnostic_page_header_t *dph)
{
    int page_length;
    uint8_t *pages, page_code;
    char *diag_page_name;
    uint8_t device_type = 0;
    int status = SUCCESS;

    if (iop->first_time) {
	status = get_inquiry_information(sdp, iop, sgp);
	if (status == FAILURE) return(status);
	iop->first_time = False;
    }

    if (sdp->output_format == JSON_FMT) {
	char *json_string;
	json_string = diagnostic_supported_to_json(sdp, iop, dph, "Diagnostic Pages");
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }

    /* Format: <page header><page code>...*/
    pages = (uint8_t *)dph + sizeof(*dph);
    page_length = (int)StoH(dph->page_length);
    device_type = iop->sip->si_inquiry->inq_dtype;

    if (iop->first_time) {
	status = get_inquiry_information(sdp, iop, sgp);
	if (status == FAILURE) return(status);
	iop->first_time = False;
    }

    PrintHeader(sdp, "Diagnostic Pages Supported");

    if (sdp->DebugFlag) {
	uint8_t *ucp = (uint8_t *)dph;
	int length = page_length + sizeof(*dph);
	int offset = 0;
	offset = PrintHexData(sdp, offset, ucp, length);
    }

    for ( ; page_length-- ; pages++) {
	page_code = *pages;
	diag_page_name = get_diagnostic_page_name(device_type, page_code, iop->vendor_id);
	Printf(sdp, "%34.34s Page (Code = 0x%02x)\n", diag_page_name, page_code);
    }
    Printf(sdp, "\n");
    return(status);
}

/*
 * Supported Diagnostic Pages (Page 0x00) in JSON Format:
 */
char *
diagnostic_supported_to_json(scsi_device_t *sdp, io_params_t *iop, diagnostic_page_header_t *dph, char *page_name)
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
    int	page_length = (int)StoH(dph->page_length);
    uint8_t *pages, page_code = DIAG_SUPPORTED_PAGES;
    char *diag_page_name;
    uint8_t device_type = 0;
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

    ucp = (uint8_t *)dph;
    length = page_length + sizeof(*dph);
    json_status = json_object_set_number(object, "Length", (double)length);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Offset", (double)offset);
    if (json_status != JSONSuccess) goto finish;
    offset = FormatHexBytes(text, offset, ucp, length);
    json_status = json_object_set_string(object, "Bytes", text);
    if (json_status != JSONSuccess) goto finish;

    (void)sprintf(text, "0x%02x", page_code);
    json_status = json_object_set_string(object, "Page Code", text);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Page Length", (double)page_length);
    if (json_status != JSONSuccess) goto finish;

    /* Format: <page header><page code>...*/
    pages = (uint8_t *)dph + sizeof(*dph);
    device_type = iop->sip->si_inquiry->inq_dtype;

    for ( ; page_length-- ; pages++) {
	page_code = *pages;
	diag_page_name = get_diagnostic_page_name(device_type, page_code, iop->vendor_id);
	(void)sprintf(text, "Page 0x%02x", page_code);
	json_status = json_object_set_string(object, text, diag_page_name);
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
/*
 * SES Configuration Diagnostic Page 0x01:
 */
int
ses_config_receive_diagnostic_decode(scsi_device_t *sdp, io_params_t *iop,
				     scsi_generic_t *sgp, diagnostic_page_header_t *dph)
{
    ses_configuration_page_t *scp = (ses_configuration_page_t *)dph;
    ses_enclosure_descriptor_t *edp = NULL;
    ses_type_desc_header_t *tdp = NULL;
    char vid[sizeof(edp->enclosure_vendor_id)+1];
    char pid[sizeof(edp->enclosure_product_id)+1];
    char revlevel[sizeof(edp->enclosure_revision_code)+1];
    int	page_length = (int)StoH(scp->page_length);
    uint32_t generation_number = 0;
    uint64_t enclosure_logical_id = 0;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    int offset = 0;
    int toffset = 0;
    int i, addl_len = 0;
    int total_element_types = 0;
    int status = SUCCESS;

    if (sdp->output_format == JSON_FMT) {
	char *json_string;
	json_string = ses_config_to_json(sdp, scp, "Configuration");
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }

    PrintHeader(sdp, "Configuration Diagnostic Page");

    if (sdp->DebugFlag) {
	uint8_t *ucp = (uint8_t *)scp;
	int length = sizeof(*scp);
	offset = PrintHexData(sdp, offset, ucp, length);
    }
    PrintHex(sdp, "Page Code", scp->page_code, PNL); 
    PrintDecimal(sdp, "Number of Secondary Enclosures", scp->secondary_enclosures, PNL);
    PrintDecHex(sdp, "Page Length", page_length, PNL);
    generation_number = (uint32_t)StoH(scp->generation_number);
    PrintHex(sdp, "Generation Number", generation_number, PNL);

    edp = (ses_enclosure_descriptor_t *)((uint8_t *)scp + sizeof(*scp));

    /*
     * Display the primary and secondary enclosure information.
     */
    for (i = 0; i < (scp->secondary_enclosures + 1); i++) {
	Printf(sdp, "\n");
	if (sdp->DebugFlag) {
	    uint8_t *ucp = (uint8_t *)edp;
	    int length = (edp->enclosure_descriptor_length + 4);
	    offset = PrintHexData(sdp, offset, ucp, length);
	}
	PrintAscii(sdp, "Enclosure Descriptor List", "", PNL);
	PrintDecimal(sdp, "Number Enclosure Services Processes", edp->num_enclosure_services_processes, PNL);
	if (edp->reserved_byte0_b3 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 0, bit 3)", edp->reserved_byte0_b3, PNL);
	}
	PrintDecimal(sdp, "Relative Enclosure Services Process ID", edp->rel_enclosure_services_process_id, PNL);
	if (edp->reserved_byte0_b7 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 0, bit 7)", edp->reserved_byte0_b7, PNL);
	}
     
	PrintLongDec(sdp, "Subenclosure Identifier", edp->subenclosure_identifier, DNL);
	if (edp->subenclosure_identifier == 0) {
	    Print(sdp, " (Primary)\n");
	} else {
	    Print(sdp, " (Secondary)\n");
	}
	total_element_types += edp->num_type_descriptor_headers;
	PrintDecimal(sdp, "Number of Type Descriptor Headers", edp->num_type_descriptor_headers, PNL);
	PrintDecHex(sdp, "Enclosure Descriptor Length", edp->enclosure_descriptor_length, PNL);

	enclosure_logical_id = StoH(edp->enclosure_logical_id);
	PrintLongHex(sdp, "Enclosure Logical Identifier", enclosure_logical_id, PNL);

	strncpy (vid, (char *)edp->enclosure_vendor_id, sizeof(edp->enclosure_vendor_id));
	vid[sizeof(edp->enclosure_vendor_id)] = '\0';
	PrintAscii(sdp, "Vendor Identification", vid, PNL);

	strncpy (pid, (char *)edp->enclosure_product_id, sizeof(edp->enclosure_product_id));
	pid[sizeof(edp->enclosure_product_id)] = '\0';
	PrintAscii(sdp, "Product Identification", pid, PNL);

	strncpy (revlevel, (char *)edp->enclosure_revision_code, sizeof(edp->enclosure_revision_code));
	revlevel[sizeof(edp->enclosure_revision_code)] = '\0';
	PrintAscii(sdp, "Product Revision Level", revlevel, PNL);

	addl_len = (edp->enclosure_descriptor_length - sizeof(*edp));
	if (addl_len > 0) {
	    uint8_t *vendor_data = (uint8_t *)edp + sizeof(*edp);
	    PrintAscii(sdp, "Vendor Specific Data", "", DNL);
	    PrintHAFields(sdp, vendor_data, addl_len);
	}
	edp = (ses_enclosure_descriptor_t *)((uint8_t *)edp + edp->enclosure_descriptor_length + 4);
    }
    /* 
     * Decode and display the type descriptor headers and text. 
     *  
     * Layout: 
     * < element type descriptors > 
     * < element type text strings > 
     */
    tdp = (ses_type_desc_header_t *)edp;
    tp = (char *)tdp + (total_element_types * sizeof(*tdp));
    toffset = (int)(tp - (char *)scp);
    for (i = 0; (i < total_element_types); i++, tdp++) {
	char *element_name = get_element_type(tdp->element_type);
	uint8_t text_length = tdp->type_descriptor_text_length;

	if ( (sdp->ses_element_type > 0) && (tdp->element_type != sdp->ses_element_type) ) {
	    tp += text_length;
	    offset += sizeof(*tdp);
	    toffset += text_length;
	    continue;
	}
	Printf(sdp, "\n");
	if (sdp->DebugFlag) {
	    uint8_t *ucp = (uint8_t *)tdp;
	    int length = sizeof(*tdp);
	    offset = PrintHexData(sdp, offset, ucp, length);
	}
	PrintAscii(sdp, "Element Type", element_name, DNL);
	Print(sdp, " (0x%02x)\n", tdp->element_type);
	PrintDecimal(sdp, "Number of Elements", tdp->number_elements, PNL);
	PrintDecimal(sdp, "Subenclosure Identifier", tdp->subenclosure_identifier, DNL);
	if (tdp->subenclosure_identifier == 0) {
	    Print(sdp, " (Primary)\n");
	} else {
	    Print(sdp, " (Secondary)\n");
	}
	(void)FormatQuotedText(text, tp, text_length);
	PrintDecHex(sdp, "Text Length", text_length, PNL);
	if (sdp->DebugFlag) {
	    toffset = PrintAsciiData(sdp, toffset, tp, text_length);
	}
	PrintAscii(sdp, "Element Text", text, PNL);
	tp += text_length;
    }
    Printf(sdp, "\n");
    return(status);
}

/*
 * SES Configuration Diagnostic Page 0x01 in JSON Format:
 */
char *
ses_config_to_json(scsi_device_t *sdp, ses_configuration_page_t *scp, char *page_name)
{
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    JSON_Value  *svalue = NULL;
    JSON_Object *sobject = NULL;
    JSON_Array	*enc_array;
    JSON_Value	*enc_value = NULL;
    JSON_Status json_status;
    char *json_string = NULL;
    ses_enclosure_descriptor_t *edp = NULL;
    ses_type_desc_header_t *tdp = NULL;
    char vid[sizeof(edp->enclosure_vendor_id)+1];
    char pid[sizeof(edp->enclosure_product_id)+1];
    char revlevel[sizeof(edp->enclosure_revision_code)+1];
    int	page_length = (int)StoH(scp->page_length);
    uint32_t generation_number = 0;
    uint64_t enclosure_logical_id = 0;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    uint8_t *ucp = NULL;
    int length = 0;
    int offset = 0;
    int toffset = 0;
    int i, addl_len = 0;
    int total_element_types = 0;
    int status = SUCCESS;

    root_value  = json_value_init_object();
    if (root_value == NULL) return(NULL);
    root_object = json_value_get_object(root_value);

    value = json_value_init_object();
    if (value == NULL) {
	json_value_free(root_value);
	return(NULL);
    }
    json_status = json_object_dotset_value(root_object, page_name, value);
    if (json_status != JSONSuccess) goto finish;
    object = json_value_get_object(value);

    ucp = (uint8_t *)scp;
    length = sizeof(*scp);
    json_status = json_object_set_number(object, "Length", (double)length);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Offset", (double)offset);
    if (json_status != JSONSuccess) goto finish;
    offset = FormatHexBytes(text, offset, ucp, length);
    json_status = json_object_set_string(object, "Bytes", text);
    if (json_status != JSONSuccess) goto finish;

    (void)sprintf(text, "0x%02x", scp->page_code);
    json_status = json_object_set_string(object, "Page Code", text);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Secondary Enclosures", (double)scp->secondary_enclosures);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Page Length", (double)page_length);
    if (json_status != JSONSuccess) goto finish;
    generation_number = (uint32_t)StoH(scp->generation_number);
    json_status = json_object_set_number(object, "Generation Number", (double)generation_number);
    if (json_status != JSONSuccess) goto finish;

    edp = (ses_enclosure_descriptor_t *)((uint8_t *)scp + sizeof(*scp));

    /*
     * Display the primary and secondary enclosure information.
     */
    for (i = 0; i < (scp->secondary_enclosures + 1); i++) {
	if (enc_value == NULL) {
	    enc_value = json_value_init_array();
	    enc_array = json_value_get_array(enc_value);
	}
	if (svalue == NULL) {
	    svalue  = json_value_init_object();
	    sobject = json_value_get_object(svalue);
	}

	ucp = (uint8_t *)edp;
	length = (edp->enclosure_descriptor_length + 4);
	json_status = json_object_set_number(sobject, "Length", (double)length);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(sobject, "Offset", (double)offset);
	if (json_status != JSONSuccess) goto finish;
	offset = FormatHexBytes(text, offset, ucp, length);
	json_status = json_object_set_string(sobject, "Bytes", text);
	if (json_status != JSONSuccess) goto finish;

	json_status = json_object_set_number(sobject, "Enclosure Services Processes", (double)edp->num_enclosure_services_processes);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(sobject, "Reserved (byte 0, bit 3)", (double)edp->reserved_byte0_b3);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(sobject, "Reserved (byte 0, bit 7)", (double)edp->reserved_byte0_b7);
	if (json_status != JSONSuccess) goto finish;

	json_status = json_object_set_number(sobject, "Subenclosure Identifier", (double)edp->subenclosure_identifier);
	if (json_status != JSONSuccess) goto finish;

	total_element_types += edp->num_type_descriptor_headers;
	json_status = json_object_set_number(sobject, "Type Descriptor Headers", (double)edp->num_type_descriptor_headers);
	if (json_status != JSONSuccess) goto finish;

	json_status = json_object_set_number(sobject, "Enclosure Descriptor Length", (double)edp->enclosure_descriptor_length);
	if (json_status != JSONSuccess) goto finish;

	enclosure_logical_id = StoH(edp->enclosure_logical_id);
	(void)sprintf(text, LXF, enclosure_logical_id);
	json_status = json_object_set_string(sobject, "Enclosure Logical Identifier", text);
	if (json_status != JSONSuccess) goto finish;

	strncpy (vid, (char *)edp->enclosure_vendor_id, sizeof(edp->enclosure_vendor_id));
	vid[sizeof(edp->enclosure_vendor_id)] = '\0';
	json_status = json_object_set_string(sobject, "Vendor Identification", vid);
	if (json_status != JSONSuccess) goto finish;

	strncpy (pid, (char *)edp->enclosure_product_id, sizeof(edp->enclosure_product_id));
	pid[sizeof(edp->enclosure_product_id)] = '\0';
	json_status = json_object_set_string(sobject, "Product Identification", pid);
	if (json_status != JSONSuccess) goto finish;

	strncpy (revlevel, (char *)edp->enclosure_revision_code, sizeof(edp->enclosure_revision_code));
	revlevel[sizeof(edp->enclosure_revision_code)] = '\0';
	json_status = json_object_set_string(sobject, "Product Revision Level", revlevel);
	if (json_status != JSONSuccess) goto finish;

	addl_len = (edp->enclosure_descriptor_length - sizeof(*edp));
	if (addl_len > 0) {
	    uint8_t *ucp = (uint8_t *)edp + sizeof(*edp);
	    int length = addl_len;
	    offset = FormatHexBytes(text, offset, ucp, length);
	    json_status = json_object_set_string(sobject, "Vendor Specific Data", text);
	    if (json_status != JSONSuccess) goto finish;
	}
	edp = (ses_enclosure_descriptor_t *)((uint8_t *)edp + edp->enclosure_descriptor_length + 4);
	json_array_append_value(enc_array, svalue);
	svalue = NULL;
    }
    if (enc_value) {
	json_object_set_value(object, "Enclosure Descriptor List", enc_value);
	enc_value = NULL;
    }
    /* 
     * Decode and display the type descriptor headers and text. 
     *  
     * Layout: 
     * < element type descriptors > 
     * < element type text string > 
     */
    tdp = (ses_type_desc_header_t *)edp;
    tp = (char *)tdp + (total_element_types * sizeof(*tdp));
    toffset = (int)(tp - (char *)scp);
    for (i = 0; (i < total_element_types); i++, tdp++) {
	char *element_name = get_element_type(tdp->element_type);
	uint8_t text_length = tdp->type_descriptor_text_length;

	if ( (sdp->ses_element_type > 0) && (tdp->element_type != sdp->ses_element_type) ) {
	    tp += text_length;
	    offset += sizeof(*tdp);
	    toffset += text_length;
	    continue;
	}
	if (svalue == NULL) {
	    svalue  = json_value_init_object();
	    sobject = json_value_get_object(svalue);
	}
	ucp = (uint8_t *)tdp;
	length = sizeof(*tdp);
	json_status = json_object_set_number(sobject, "Length", (double)length);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(sobject, "Offset", (double)offset);
	if (json_status != JSONSuccess) goto finish;
	offset = FormatHexBytes(text, offset, ucp, length);
	json_status = json_object_set_string(sobject, "Bytes", text);
	if (json_status != JSONSuccess) goto finish;

	json_status = json_object_set_number(sobject, "Element Type", (double)tdp->element_type);
	if (json_status != JSONSuccess) goto finish;

	json_status = json_object_set_number(sobject, "Number of Elements", (double)tdp->number_elements);
	if (json_status != JSONSuccess) goto finish;

	json_status = json_object_set_number(sobject, "Subenclosure Identifier", (double)tdp->subenclosure_identifier);
	if (json_status != JSONSuccess) goto finish;

	json_status = json_object_set_number(sobject, "Text Offset", (double)toffset);
	if (json_status != JSONSuccess) goto finish;
	toffset = FormatHexBytes(text, toffset, tp, text_length);
	json_status = json_object_set_string(sobject, "Text Bytes", text);
	if (json_status != JSONSuccess) goto finish;

	(void)strncpy(text, tp, text_length);
	text[text_length] = '\0';
	json_status = json_object_set_number(sobject, "Text Length", (double)text_length);
	if (json_status != JSONSuccess) goto finish;

	json_status = json_object_set_string(sobject, "Element Text", text);
	if (json_status != JSONSuccess) goto finish;

	json_status = json_object_dotset_value(object, element_name, svalue);
	if (json_status != JSONSuccess) goto finish;
	svalue = NULL;
	tp += text_length;
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
/*
 * SES Enclosure Control Page 0x02:
 */
int
ses_enc_control_send_diagnostic(scsi_device_t *sdp, io_params_t *iop,
				scsi_generic_t *sgp, diagnostic_page_header_t *dph)
{
    ses_enclosure_control_page_t *ecp = (ses_enclosure_control_page_t *)dph;
    ses_control_element_t *cep = NULL;
    ses_configuration_page_t *scp = NULL;
    ses_enclosure_descriptor_t *edp = NULL;
    ses_type_desc_header_t *tdp = NULL;
    int	page_length = (int)StoH(ecp->page_length);
    uint32_t generation_number = 0;
    int i;
    int total_element_types = 0;
    int status = SUCCESS;

    ecp->reserved_byte1_b4_7 = 0;

    status = receive_diagnostic_page(sdp, iop, sgp, (void **)&scp, DIAG_CONFIGURATION_PAGE);
    if (status == FAILURE) return(status);

    edp = (ses_enclosure_descriptor_t *)((uint8_t *)scp + sizeof(*scp));
    for (i = 0; i < (scp->secondary_enclosures + 1); i++) {
	total_element_types += edp->num_type_descriptor_headers;
	edp = (ses_enclosure_descriptor_t *)((uint8_t *)edp + edp->enclosure_descriptor_length + 4);
    }
    tdp = (ses_type_desc_header_t *)edp;
    cep = (ses_control_element_t *)((uint8_t *)ecp + sizeof(*ecp));

    for (i = 0; (i < total_element_types); i++, tdp++) {
	int element_index = 0;

	if ( (sdp->ses_element_type > 0) && (tdp->element_type != sdp->ses_element_type) ) {
	    cep += (tdp->number_elements + 1);
	    continue;
	}
	/* 
	 * Note: The first descriptor is for the overall status.
	 */
	for (element_index = ELEMENT_INDEX_OVERALL;
	       (element_index < tdp->number_elements); element_index++, cep++) {

	    if ( (sdp->ses_element_flag == True) && (sdp->ses_element_index != element_index) ) {
		continue;
	    }
	    status = ses_element_type_control(sdp, tdp->element_type, cep);
	    if (status == SUCCESS) {
		status = send_diagnostic_page(sdp, iop, sgp, ecp, page_length + 4, ecp->page_code);
	    }
	}
    }
    if (scp) {
	free_palign(sdp, scp);
    }
    return(status);
}

int
ses_element_type_control(scsi_device_t *sdp, element_type_t element_type, ses_control_element_t *cep)
{
    int status = SUCCESS;
    uint8_t *ucp = (uint8_t *)cep;
    int byte = 0;

    /*
     * Note: We only get here if the element type and element index was found. 
     * The first step for all element types, is to mask status bits that are 
     * not valid for the control page. We accomplish this via a table indexed 
     * by element type, which contains 4 mask bytes to apply. These masks vary 
     * via element type, since sadly we don't have consistentcy for each type. 
     */
    if (element_type <= NUMBER_ELEMENT_TYPES) {
	for (byte = 0; byte < (int)sizeof(*cep); byte++) {
	    ucp[byte] &= ses3_element_control_mask_array[element_type][byte];
	}
    } else {
	ucp[SES_CONTROL_STATUS_OFFSET] = SES_CONTROL_MASK;
    }
    switch (element_type) {
	case ELEMENT_TYPE_POWER_SUPPLY: {
	    ses_control_power_supply_element_t *psp;
	    psp = (ses_control_power_supply_element_t *)cep;

	    switch (sdp->cgs_type) {
		case CGS_TYPE_FAULT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			psp->rqst_fail = 0;
		    } else {
			psp->rqst_fail = 1;
		    }
		    psp->sc.select = 1;
		    break;
		case CGS_TYPE_IDENT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			psp->rqst_ident = 0;
		    } else {
			psp->rqst_ident = 1;
		    }
		    psp->sc.select = 1;
		    break;
	    }
	    break;
	}
	case ELEMENT_TYPE_COOLING: {
	    ses_control_cooling_element_t *coep;
	    coep = (ses_control_cooling_element_t *)cep;

	    switch (sdp->cgs_type) {
		case CGS_TYPE_FAULT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			coep->rqst_fail = 0;
		    } else {
			coep->rqst_fail = 1;
		    }
		    coep->sc.select = 1;
		    break;
		case CGS_TYPE_IDENT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			coep->rqst_ident = 0;
		    } else {
			coep->rqst_ident = 1;
		    }
		    coep->sc.select = 1;
		    break;
	    }
	    break;
	}
	case ELEMENT_TYPE_SENSOR_TEMPERATURE: {
	    ses_control_temperature_element_t *tep;
	    tep = (ses_control_temperature_element_t *)cep;

	    switch (sdp->cgs_type) {
		case CGS_TYPE_FAULT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			tep->rqst_fail = 0;
		    } else {
			tep->rqst_fail = 1;
		    }
		    tep->sc.select = 1;
		    break;
		case CGS_TYPE_IDENT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			tep->rqst_ident = 0;
		    } else {
			tep->rqst_ident = 1;
		    }
		    tep->sc.select = 1;
		    break;
	    }
	    break;
	}
	case ELEMENT_TYPE_DOOR: {
	    ses_control_door_element_t *dep;
	    dep = (ses_control_door_element_t *)cep;

	    switch (sdp->cgs_type) {
		case CGS_TYPE_FAULT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			dep->rqst_fail = 0;
		    } else {
			dep->rqst_fail = 1;
		    }
		    dep->sc.select = 1;
		    break;
		case CGS_TYPE_IDENT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			dep->rqst_ident = 0;
		    } else {
			dep->rqst_ident = 1;
		    }
		    dep->sc.select = 1;
		    break;
		case CGS_TYPE_UNLOCK:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			dep->unlock = 0;
		    } else {
			dep->unlock = 1;
		    }
		    dep->sc.select = 1;
		    break;
	    }
	    break;
	}
	case ELEMENT_TYPE_ESCE: {
	    ses_control_esce_element_t *ecp;
	    ecp = (ses_control_esce_element_t *)cep;

	    switch (sdp->cgs_type) {
		case CGS_TYPE_FAULT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			ecp->rqst_fail = 0;
		    } else {
			ecp->rqst_fail = 1;
		    }
		    ecp->sc.select = 1;
		    break;
		case CGS_TYPE_IDENT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			ecp->rqst_ident = 0;
		    } else {
			ecp->rqst_ident = 1;
		    }
		    ecp->sc.select = 1;
		    break;
	    }
	    break;
	}
	case ELEMENT_TYPE_ENCLOSURE: {
	    ses_control_enclosure_element_t *eep;
	    eep = (ses_control_enclosure_element_t *)cep;

	    switch (sdp->cgs_type) {
		case CGS_TYPE_FAULT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			eep->request_failure = 0;
		    } else {
			eep->request_failure = 1;
		    }
		    eep->sc.select = 1;
		    break;
		case CGS_TYPE_IDENT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			eep->rqst_ident = 0;
		    } else {
			eep->rqst_ident = 1;
		    }
		    eep->sc.select = 1;
		    break;
	    }
	    break;
	}
	case ELEMENT_TYPE_VOLTAGE_SENSOR: {
	    ses_control_voltage_element_t *vep;
	    vep = (ses_control_voltage_element_t *)cep;

	    switch (sdp->cgs_type) {
		case CGS_TYPE_FAULT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			vep->rqst_fail = 0;
		    } else {
			vep->rqst_fail = 1;
		    }
		    vep->sc.select = 1;
		    break;
		case CGS_TYPE_IDENT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			vep->rqst_ident = 0;
		    } else {
			vep->rqst_ident = 1;
		    }
		    vep->sc.select = 1;
		    break;
	    }
	    break;
	}
	case ELEMENT_TYPE_CURRENT_SENSOR: {
	    ses_control_current_element_t *curep;
	    curep = (ses_control_current_element_t *)cep;

	    switch (sdp->cgs_type) {
		case CGS_TYPE_FAULT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			curep->rqst_fail = 0;
		    } else {
			curep->rqst_fail = 1;
		    }
		    curep->sc.select = 1;
		    break;
		case CGS_TYPE_IDENT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			curep->rqst_ident = 0;
		    } else {
			curep->rqst_ident = 1;
		    }
		    curep->sc.select = 1;
		    break;
	    }
	    break;
	}
	case ELEMENT_TYPE_ARRAY_DEVICE_SLOT: {
	    ses_control_array_device_element_t *adp;
	    adp = (ses_control_array_device_element_t *)cep;

	    switch (sdp->cgs_type) {
		case CGS_TYPE_DEVOFF:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			adp->device_off = 0;
		    } else {
			adp->device_off = 1;
		    }
		    adp->sc.select = 1;
		    break;
		case CGS_TYPE_FAULT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			adp->rqst_fault = 0;
		    } else {
			adp->rqst_fault = 1;
		    }
		    adp->sc.select = 1;
		    break;
		case CGS_TYPE_IDENT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			adp->rqst_ident = 0;
		    } else {
			adp->rqst_ident = 1;
		    }
		    adp->sc.select = 1;
		    break;
	    }
	    break;
	}
	case ELEMENT_TYPE_SAS_EXPANDER: {
	    ses_control_sas_expander_element_t  *sasep;
	    sasep = (ses_control_sas_expander_element_t *)cep;

	    switch (sdp->cgs_type) {
		case CGS_TYPE_FAULT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			sasep->rqst_fail = 0;
		    } else {
			sasep->rqst_fail = 1;
		    }
		    sasep->sc.select = 1;
		    break;
		case CGS_TYPE_IDENT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			sasep->rqst_ident = 0;
		    } else {
			sasep->rqst_ident = 1;
		    }
		    sasep->sc.select = 1;
		    break;
	    }
	    break;
	}
	case ELEMENT_TYPE_SAS_CONNECTOR: {
	    ses_control_sas_connector_element_t *sascp;
	    sascp = (ses_control_sas_connector_element_t *)cep;

	    switch (sdp->cgs_type) {
		case CGS_TYPE_FAULT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			sascp->rqst_fail = 0;
		    } else {
			sascp->rqst_fail = 1;
		    }
		    sascp->sc.select = 1;
		    break;
		case CGS_TYPE_IDENT:
		    if (sdp->cmd_type == CMD_TYPE_CLEAR) {
			sascp->rqst_ident = 0;
		    } else {
			sascp->rqst_ident = 1;
		    }
		    sascp->sc.select = 1;
		    break;
	    }
	    break;
	}
	default: {
	    char *element_name = get_element_type(element_type);
	    Wprintf(sdp, "Element type %s (0x%02x), is NOT implemented yet!\n",
		    element_name, element_type);
	    status = WARNING;
	    break;
	}
    }
    return(status);
}

/* ============================================================================================== */
/*
 * SES Enclosure Status Page 0x02:
 */
int
ses_enc_status_receive_diagnostic_decode(scsi_device_t *sdp, io_params_t *iop,
					 scsi_generic_t *sgp, diagnostic_page_header_t *dph)
{
    ses_enclosure_status_page_t *esp = (ses_enclosure_status_page_t *)dph;
    ses_status_element_t *sep = NULL;
    ses_configuration_page_t *scp = NULL;
    ses_enclosure_descriptor_t *edp = NULL;
    ses_type_desc_header_t *tdp = NULL;
    int	page_length = (int)StoH(esp->page_length);
    uint32_t generation_number = 0;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    int i, offset = 0;
    int total_element_types = 0;
    int status = SUCCESS;

    if (sdp->output_format == JSON_FMT) {
	char *json_string;
	json_string = ses_enc_status_to_json(sdp, iop, sgp, esp, "Enclosure Status");
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }

    PrintHeader(sdp, "Enclosure Status Diagnostic Page");

    if (sdp->DebugFlag) {
	uint8_t *ucp = (uint8_t *)esp;
	int length = sizeof(*esp);
	offset = PrintHexData(sdp, offset, ucp, length);
    } else {
	offset += sizeof(*esp);
    }
    PrintHex(sdp, "Page Code", esp->page_code, PNL); 
    PrintBoolean(sdp, False, "Unrecoverable Condition", esp->unrecov, PNL);
    PrintBoolean(sdp, False, "Critical Condition", esp->crit, PNL);
    PrintBoolean(sdp, False, "Non-Critical Condition", esp->non_crit, PNL);
    PrintBoolean(sdp, False, "Information Condition", esp->info, PNL);
    PrintBoolean(sdp, False, "Invalid Operation", esp->invop, PNL);
    if (esp->reserved_byte1_b5_7 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 1, bits 5:7)", esp->reserved_byte1_b5_7, PNL);
    }
    PrintDecHex(sdp, "Page Length", page_length, PNL);
    generation_number = (uint32_t)StoH(esp->generation_number);
    PrintHex(sdp, "Generation Number", generation_number, PNL);

    /*
     * The designers of SES page 2, decided to omit the element type, and instead 
     * order elements according to what the configuration page reports. So be it! 
     * Therefore, we must request the configuration page to decode page 2 elements! 
     */
    status = receive_diagnostic_page(sdp, iop, sgp, (void **)&scp, DIAG_CONFIGURATION_PAGE);
    if (status == FAILURE) return(status);

    edp = (ses_enclosure_descriptor_t *)((uint8_t *)scp + sizeof(*scp));
    for (i = 0; i < (scp->secondary_enclosures + 1); i++) {
	total_element_types += edp->num_type_descriptor_headers;
	edp = (ses_enclosure_descriptor_t *)((uint8_t *)edp + edp->enclosure_descriptor_length + 4);
    }
    tdp = (ses_type_desc_header_t *)edp;
    tp = (char *)tdp + (total_element_types * sizeof(*tdp));

    sep = (ses_status_element_t *)((uint8_t *)esp + sizeof(*esp));

    for (i = 0; (i < total_element_types); i++, tdp++) {
	char *element_name = get_element_type(tdp->element_type);
	char element_text[STRING_BUFFER_SIZE];
	uint8_t text_length = tdp->type_descriptor_text_length;
	int element_index = 0;

	if ( (sdp->ses_element_type > 0) && (tdp->element_type != sdp->ses_element_type) ) {
	    tp += text_length;
	    sep += (tdp->number_elements + 1);
	    offset += (tdp->number_elements + 1) * sizeof(*tdp);
	    continue;
	}
	(void)strncpy(element_text, tp, text_length);
	element_text[text_length] = '\0';

	Printf(sdp, "\n");
	(void)sprintf(text, "%s (0x%02x)", element_name, tdp->element_type);
	PrintAscii(sdp, "Element Type", text, PNL);
	PrintDecimal(sdp, "Number of Elements", tdp->number_elements, PNL);
	PrintDecimal(sdp, "Subenclosure Identifier", tdp->subenclosure_identifier, DNL);
	if (tdp->subenclosure_identifier == 0) {
	    Print(sdp, " (Primary)\n");
	} else {
	    Print(sdp, " (Secondary)\n");
	}
	PrintAscii(sdp, "Element Text", element_text, PNL);

	/* 
	 * Note: The first descriptor is for the overall status.
	 */
	for (element_index = ELEMENT_INDEX_OVERALL;
	       (element_index < tdp->number_elements); element_index++, sep++) {

	    if ( (sdp->ses_element_flag == True) && (sdp->ses_element_index != element_index) ) {
		offset += sizeof(*sep);
		continue;
	    }
	    if ( (sdp->ses_element_status > 0) &&
		 (sdp->ses_element_status != sep->sc.element_status_code) ) {
		offset += sizeof(*sep);
		continue;
	    }
	    Printf(sdp, "\n");
	    if (sdp->DebugFlag) {
		uint8_t *ucp = (uint8_t *)sep;
		int length = sizeof(*sep);
		offset = PrintHexData(sdp, offset, ucp, length);
	    }
	    if (element_index == ELEMENT_INDEX_OVERALL) {
		PrintAscii(sdp, "Overall Status Descriptor", "", PNL);
	    } else {
		PrintDecimal(sdp, "Element Descriptor", element_index, PNL);
	    }
	    PrintAscii(sdp, "Element Text", element_text, PNL);
	    status = ses_element_type_status(sdp, tdp->element_type, sep);
	    /* 
	     * Display offset and hex bytes, if we did not decode this element.
	     */
	    if ( (status == WARNING) && (sdp->DebugFlag == False) ) {
		uint8_t *ucp = (uint8_t *)sep;
		int length = sizeof(*sep);
		offset = PrintHexData(sdp, offset, ucp, length);
	    } else if (sdp->DebugFlag == False) {
		offset += sizeof(*sep);
	    }
	}
	tp += text_length;
    }

    if (scp) {
	free_palign(sdp, scp);
    }
    Printf(sdp, "\n");
    return(status);
}

/*
 * SES Enclosure Status Page 0x02 in JSON Format:
 */
void
ses_common_element_status(scsi_device_t *sdp, ses_status_common_t *scp)
{
    char text[STRING_BUFFER_SIZE];
    char *element_status = get_element_status(scp->element_status_code);
    char *element_desc = get_element_status_desc(scp->element_status_code);

    (void)sprintf(text, "%s (0x%02x)", element_status, scp->element_status_code);
    PrintAscii(sdp, "Element Status Code", text, PNL);
    PrintAscii(sdp, "Element Status Description", element_desc, PNL);
    PrintBoolean(sdp, False, "Element Swapped", scp->swap, PNL);
    PrintBoolean(sdp, False, "Element Disabled", scp->disabled, PNL);
    PrintBoolean(sdp, False, "Predicted Failure", scp->prdfail, PNL);
    if (scp->reserved_byte0_b7 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 0, bit 7)", scp->reserved_byte0_b7, PNL);
    }
    return;
}

int
ses_element_type_status(scsi_device_t *sdp, element_type_t element_type, ses_status_element_t *sep)
{
    char text[STRING_BUFFER_SIZE];
    int status = SUCCESS;

    ses_common_element_status(sdp, &sep->sc);

    switch (element_type) {

	case ELEMENT_TYPE_POWER_SUPPLY: {
	    ses_status_power_supply_element_t *psp;
	    psp = (ses_status_power_supply_element_t *)sep;

	    Printf(sdp, "\n");
	    if (psp->reserved_byte1_b0_5 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1, bits 0:5)", psp->reserved_byte1_b0_5, PNL);
	    }
	    PrintBoolean(sdp, False, "Do Not Remove", psp->do_not_remove, PNL);
	    PrintOnOff(sdp, False, "Identify LED", psp->ident, PNL);

	    if (psp->reserved_byte2_b0 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 2, bit 0)", psp->reserved_byte2_b0, PNL);
	    }
	    PrintBoolean(sdp, False, "DC Overcurrent", psp->dc_overcurrent, PNL);
	    PrintBoolean(sdp, False, "DC Undervoltage", psp->dc_undervoltage, PNL);
	    PrintBoolean(sdp, False, "DC Overvoltage", psp->dc_overvoltage, PNL);
	    if (psp->reserved_byte2_b4_7 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 2, bits 4:7)", psp->reserved_byte2_b4_7, PNL);
	    }

	    PrintBoolean(sdp, False, "DC Fail", psp->dc_fail, PNL);
	    PrintBoolean(sdp, False, "AC Fail", psp->ac_fail, PNL);
	    PrintBoolean(sdp, False, "Over Temperature Warning", psp->temp_warn, PNL);
	    PrintBoolean(sdp, False, "Over Temperature Failure", psp->over_temp_fail, PNL);
	    PrintBoolean(sdp, False, "Power Supply Off", psp->off, PNL);
	    PrintBoolean(sdp, False, "Requested On", psp->rqsted_on, PNL);
	    PrintOnOff(sdp, False, "Failure LED", psp->fail, PNL);
	    PrintBoolean(sdp, False, "Hot Swap", psp->hot_swap, PNL);
	    break;
	}
	case ELEMENT_TYPE_COOLING: {
	    ses_status_cooling_element_t *cep;
	    cep = (ses_status_cooling_element_t *)sep;
	    int actual_fan_speed;
	    char *actual_speed_name;

	    Printf(sdp, "\n");
	    if (cep->reserved_byte1_b3_5 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1, bits 3:5)", cep->reserved_byte1_b3_5, PNL);
	    }
	    PrintBoolean(sdp, False, "Do Not Remove", cep->do_not_remove, PNL);
	    PrintOnOff(sdp, False, "Identify LED", cep->ident, PNL);

	    actual_fan_speed = (cep->actual_fan_speed_msb << 8) + cep->actual_fan_speed;
	    actual_fan_speed *= 10;	/* Convert to RPM's. */
	    /* Format into a ingle string for (future) message passing and JSON output. */
	    (void)sprintf(text, "%d rpm", actual_fan_speed);
	    PrintAscii(sdp, "Actual Fan Speed", text, PNL);

	    PrintHex(sdp, "Actual Speed Code", cep->actual_speed_code, PNL);
	    actual_speed_name = get_cooling_actual_speed(cep->actual_speed_code);
	    PrintAscii(sdp, "Actual Speed Description", actual_speed_name, PNL);
	    if (cep->reserved_byte3_b3 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 3, bit 3)", cep->reserved_byte3_b3, PNL);
	    }
	    PrintBoolean(sdp, False, "Power Supply Off", cep->off, PNL);
	    PrintBoolean(sdp, False, "Requested On", cep->rqsted_on, PNL);
	    PrintOnOff(sdp, False, "Failure LED", cep->fail, PNL);
	    PrintBoolean(sdp, False, "Hot Swap", cep->hot_swap, PNL);
	    break;
	}
	case ELEMENT_TYPE_SENSOR_TEMPERATURE: {
	    ses_status_temperature_element_t *tep;
	    tep = (ses_status_temperature_element_t *)sep;
	    int temperature, temperature_offset = 20;

	    Printf(sdp, "\n");
	    if (tep->reserved_byte1_b0_5 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1, bits 0:5)", tep->reserved_byte1_b0_5, PNL);
	    }
	    PrintOnOff(sdp, False, "Failure LED", tep->fail, PNL);
	    PrintOnOff(sdp, False, "Identify LED", tep->ident, PNL);

	    /* Temperature - offset gives us -90C to 235C. */
	    temperature = (int)tep->temperature;
	    if (temperature) {
		temperature -= temperature_offset;
		(void)sprintf(text, "%d Celsius", temperature);
	    } else {
		(void)sprintf(text, "%d (reserved)", temperature);
	    }
	    PrintAscii(sdp, "Temperature", text, PNL);

	    PrintBoolean(sdp, False, "Under Temperature Warning", tep->ut_warning, PNL);
	    PrintBoolean(sdp, False, "Under Temperature Failure", tep->ut_failure, PNL);
	    PrintBoolean(sdp, False, "Over Temperature Warning", tep->ot_warning, PNL);
	    PrintBoolean(sdp, False, "Over Temperature Failure", tep->ot_failure, PNL);
	    if (tep->reserved_byte3_b4_7 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 3, bits 4:7)", tep->reserved_byte3_b4_7, PNL);
	    }
	    break;
	}
	case ELEMENT_TYPE_DOOR: {
	    ses_status_door_element_t *dep;
	    dep = (ses_status_door_element_t *)sep;

	    Printf(sdp, "\n");
	    if (dep->reserved_byte1_b0_5 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1, bits 0:5)", dep->reserved_byte1_b0_5, PNL);
	    }
	    PrintOnOff(sdp, False, "Failure LED", dep->fail, PNL);
	    PrintOnOff(sdp, False, "Identify LED", dep->ident, PNL);

	    if (dep->reserved_byte2 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 2)", dep->reserved_byte2, PNL);
	    }

	    PrintBoolean(sdp, False, "Door unlocked", dep->unlocked, PNL);
	    PrintBoolean(sdp, False, "Door open", dep->open, PNL);
	    if (dep->reserved_byte3_b2_7 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 3, bits 2:7)", dep->reserved_byte1_b0_5, PNL);
	    }
	    break;
	}
	case ELEMENT_TYPE_ESCE: {
	    ses_status_esce_element_t *esp;
	    esp = (ses_status_esce_element_t *)sep;

	    Printf(sdp, "\n");
	    if (esp->reserved_byte1_b0_3 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1, bits 0:3)", esp->reserved_byte1_b0_3, PNL);
	    }
	    PrintBoolean(sdp, False, "Slot Prepared For Removal", esp->rmv, PNL);
	    PrintBoolean(sdp, False, "Do Not Remove", esp->do_not_remove, PNL);
	    PrintOnOff(sdp, False, "Failure LED", esp->fail, PNL);
	    PrintOnOff(sdp, False, "Identify LED", esp->ident, PNL);

	    PrintBoolean(sdp, False, "Report", esp->report, PNL);
	    if (esp->reserved_byte2_b1_7 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 2, bits 1:7)", esp->reserved_byte2_b1_7, PNL);
	    }

	    if (esp->reserved_byte3_b0_6 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 3, bits 0:6)", esp->reserved_byte3_b0_6, PNL);
	    }
	    PrintBoolean(sdp, False, "Hot Swap", esp->hot_swap, PNL);
	    break;
	}
	case ELEMENT_TYPE_ENCLOSURE: {
	    ses_status_enclosure_element_t *eep;
	    eep = (ses_status_enclosure_element_t *)sep;
	    char *power_cycle_description = NULL;

	    Printf(sdp, "\n");
	    if (eep->reserved_byte1_b0_6 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1, bits 0:6)", eep->reserved_byte1_b0_6, PNL);
	    }
	    PrintOnOff(sdp, False, "Identify LED", eep->ident, PNL);

	    PrintBoolean(sdp, False, "Warning Indication", eep->warning_indication, PNL);
	    PrintBoolean(sdp, False, "Failure Indication", eep->failure_indication, PNL);
	    if (eep->time_until_power_cycle == 0) {
		power_cycle_description = "(No power cycle scheduled)";
	    } else if ( (eep->time_until_power_cycle >= 1) && (eep->time_until_power_cycle <= 60) ) {
		power_cycle_description = "(Power cycle after indicated minutes)";
	    } else if ( (eep->time_until_power_cycle >= 61) && (eep->time_until_power_cycle <= 62) ) {
		power_cycle_description = "(Reserved)";
	    } else if (eep->time_until_power_cycle == 63) {
		power_cycle_description = "(Power cycle after zero minutes)";
	    } else {
		power_cycle_description = "";
	    }
	    (void)sprintf(text, "%d %s", eep->time_until_power_cycle, power_cycle_description);
	    PrintAscii(sdp, "Time until power cycle", text, PNL);

	    PrintBoolean(sdp, False, "Warning Requested", eep->warning_requested, PNL);
	    PrintBoolean(sdp, False, "Failure Requested", eep->failure_requested, PNL);
	    if (eep->requested_power_off_duration == 0) {
		power_cycle_description = "(No power cycle scheduled or to be kept off less than one minute)";
	    } else if ( (eep->requested_power_off_duration >= 1) && (eep->requested_power_off_duration <= 60) ) {
		power_cycle_description = "(Power scheduled to be off for indicated minutes)";
	    } else if ( (eep->requested_power_off_duration >= 61) && (eep->requested_power_off_duration <= 62) ) {
		power_cycle_description = "(Reserved)";
	    } else if (eep->requested_power_off_duration == 63) {
		power_cycle_description = "(Power to be kept off until manually restored)";
	    } else {
		power_cycle_description = "";
	    }
	    (void)sprintf(text, "%d %s", eep->requested_power_off_duration, power_cycle_description);
	    PrintAscii(sdp, "Requested Power Off Duration", text, PNL);
	    break;
	}
	case ELEMENT_TYPE_VOLTAGE_SENSOR: {
	    ses_status_voltage_element_t *vep;
	    vep = (ses_status_voltage_element_t *)sep;
	    int voltage;

	    Printf(sdp, "\n");
	    PrintBoolean(sdp, False, "Critical Under Voltage", vep->crit_under, PNL);
	    PrintBoolean(sdp, False, "Critical Over Voltage", vep->crit_over, PNL);
	    PrintBoolean(sdp, False, "Under Voltage Warning", vep->warn_under, PNL);
	    PrintBoolean(sdp, False, "Over Voltage Warning", vep->warn_over, PNL);
	    if (vep->reserved_byte1_b4_5 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1, bits 4:5)", vep->reserved_byte1_b4_5, PNL);
	    }
	    PrintOnOff(sdp, False, "Failure LED", vep->fail, PNL);
	    PrintOnOff(sdp, False, "Identify LED", vep->ident, PNL);

	    voltage = (int)(short)StoH(vep->voltage);
	    (void)sprintf(text, "%.2f volts", ((float)voltage / 100.0));
	    PrintAscii(sdp, "Voltage", text, PNL);
	    break;
	}
	case ELEMENT_TYPE_CURRENT_SENSOR: {
	    ses_status_current_element_t *cep;
	    cep = (ses_status_current_element_t *)sep;
	    int current;

	    Printf(sdp, "\n");
	    if (cep->reserved_byte1_b0 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1, bit 0)", cep->reserved_byte1_b0, PNL);
	    }
	    PrintBoolean(sdp, False, "Critical Over Current", cep->crit_over, PNL);
	    if (cep->reserved_byte1_b2 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1, bit 2)", cep->reserved_byte1_b2, PNL);
	    }
	    PrintBoolean(sdp, False, "Over Current Warning", cep->warn_over, PNL);
	    if (cep->reserved_byte1_b4_5 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1, bits 4:5)", cep->reserved_byte1_b4_5, PNL);
	    }
	    PrintOnOff(sdp, False, "Failure LED", cep->fail, PNL);
	    PrintOnOff(sdp, False, "Identify LED", cep->ident, PNL);

	    current = (int)(short)StoH(cep->current);
	    (void)sprintf(text, "%.2f amps", ((float)current / 100.0));
	    PrintAscii(sdp, "Current", text, PNL);
	    break;
	}
	case ELEMENT_TYPE_ARRAY_DEVICE_SLOT: {
	    ses_status_array_device_element_t *adp;
	    adp = (ses_status_array_device_element_t *)sep;

	    Printf(sdp, "\n");
	    PrintBoolean(sdp, False, "Rebuild/Remap Abort", adp->rr_abort, PNL);
	    PrintBoolean(sdp, False, "Rebuild/Remap", adp->rebuild_remap, PNL);
	    PrintBoolean(sdp, False, "In Failed Array", adp->in_failed_array, PNL);
	    PrintBoolean(sdp, False, "In Critical Array", adp->in_crit_array, PNL);
	    PrintBoolean(sdp, False, "Consistency Check In Progress", adp->cons_chk, PNL);
	    PrintBoolean(sdp, False, "Hot Spare", adp->hot_spare, PNL);
	    PrintBoolean(sdp, False, "Reserved Device", adp->rsvd_device, PNL);
	    PrintBoolean(sdp, False, "Device Okay", adp->ok, PNL);

	    Printf(sdp, "\n");
	    PrintBoolean(sdp, False, "Report", adp->report, PNL);
	    PrintOnOff(sdp, False, "Identify LED", adp->ident, PNL);
	    PrintBoolean(sdp, False, "Slot Prepared For Removal", adp->rmv, PNL);
	    PrintBoolean(sdp, False, "Ready to Insert", adp->ready_to_insert, PNL);
	    PrintBoolean(sdp, False, "Enclosure Bypassed Port B", adp->enclosure_bypassed_b, PNL);
	    PrintBoolean(sdp, False, "Enclosure Bypassed Port A", adp->enclosure_bypassed_a, PNL);
	    PrintBoolean(sdp, False, "Do Not Remove", adp->do_not_remove, PNL);
	    PrintBoolean(sdp, False, "Application Client Bypassed Port A", adp->app_client_bypassed_a, PNL);

	    Printf(sdp, "\n");
	    PrintBoolean(sdp, False, "Device Bypassed Port B", adp->device_bypassed_b, PNL);
	    PrintBoolean(sdp, False, "Device Bypassed Port A", adp->device_bypassed_a, PNL);
	    PrintBoolean(sdp, False, "Bypassed Port B", adp->bypassed_b, PNL);
	    PrintBoolean(sdp, False, "Bypassed Port A", adp->bypassed_a, PNL);
	    PrintBoolean(sdp, False, "Device Turned Off", adp->device_off, PNL);
	    PrintBoolean(sdp, False, "Fault Requested", adp->fault_reqstd, PNL);
	    PrintBoolean(sdp, False, "Fault Sensed", adp->fault_sensed, PNL);
	    PrintBoolean(sdp, False, "Application Client Bypassed Port B", adp->app_client_bypassed_b, PNL);
	    break;
	}
	case ELEMENT_TYPE_SAS_EXPANDER: {
	    ses_status_sas_expander_element_t  *sasep;
	    sasep = (ses_status_sas_expander_element_t *)sep;

	    //Printf(sdp, "\n");
	    if (sasep->reserved_byte1_b0_5 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1, bits 0:5)", sasep->reserved_byte1_b0_5, PNL);
	    }
	    PrintOnOff(sdp, False, "Failure LED", sasep->fail, PNL);
	    PrintOnOff(sdp, False, "Identify LED", sasep->ident, PNL);

	    if (sasep->reserved_byte2 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 2)", sasep->reserved_byte2, PNL);
	    }
	    if (sasep->reserved_byte3 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 3)", sasep->reserved_byte3, PNL);
	    }
	    break;
	}
	case ELEMENT_TYPE_SAS_CONNECTOR: {
	    ses_status_sas_connector_element_t *sascp;
	    sascp = (ses_status_sas_connector_element_t *)sep;
	    char *connector_name = NULL;

	    Printf(sdp, "\n");
	    PrintHex(sdp, "Connector Type", sascp->connector_type, PNL);
	    connector_name = get_connector_type(sascp->connector_type);
	    PrintAscii(sdp, "Connector Description", connector_name, PNL);
	    PrintOnOff(sdp, False, "Identify LED", sascp->ident, PNL);

	    PrintHex(sdp, "Connector Physical Link", sascp->connector_physical_link, PNL);

	    if (sascp->reserved_byte3_b0_4 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 3, bits 0:4)", sascp->reserved_byte3_b0_4, PNL);
	    }
	    PrintBoolean(sdp, False, "Mated", sascp->mated, PNL);
	    PrintOnOff(sdp, False, "Failure LED", sascp->fail, PNL);
	    PrintBoolean(sdp, False, "Overcurrent", sascp->overcurrent, PNL);
	    break;
	}
	default: {
	    char *element_name = get_element_type(element_type);
	    Wprintf(sdp, "Element type %s (0x%02x), is NOT implemented yet!\n",
		    element_name, element_type);
	    status = WARNING;
	    break;
	}
    }
    return(status);
}

/*
 * SES Enclosure Status Page 0x02 in JSON Format.
 */
char *
ses_enc_status_to_json(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
		       ses_enclosure_status_page_t *esp, char *page_name)
{
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    JSON_Value  *tvalue = NULL;
    JSON_Object *tobject = NULL;
    JSON_Value  *dvalue = NULL;
    JSON_Object *dobject = NULL;
    JSON_Array	*desc_array;
    JSON_Value	*desc_value = NULL;
    JSON_Status json_status;
    char *json_string = NULL;
    ses_status_element_t *sep = NULL;
    ses_configuration_page_t *scp = NULL;
    ses_enclosure_descriptor_t *edp = NULL;
    ses_type_desc_header_t *tdp = NULL;
    int	page_length = (int)StoH(esp->page_length);
    uint32_t generation_number = 0;
    uint64_t enclosure_logical_id = 0;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    uint8_t *ucp = NULL;
    int length = 0;
    int offset = 0;
    int toffset = 0;
    int i, addl_len = 0;
    int total_element_types = 0;
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

    ucp = (uint8_t *)esp;
    length = sizeof(*esp);
    json_status = json_object_set_number(object, "Length", (double)length);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Offset", (double)offset);
    if (json_status != JSONSuccess) goto finish;
    offset = FormatHexBytes(text, offset, ucp, length);
    json_status = json_object_set_string(object, "Bytes", text);
    if (json_status != JSONSuccess) goto finish;

    (void)sprintf(text, "0x%02x", esp->page_code);
    json_status = json_object_set_string(object, "Page Code", text);
    if (json_status != JSONSuccess) goto finish;

    json_status = json_object_set_boolean(object, "Unrecoverable Condition", esp->unrecov);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(object, "Critical Condition", esp->crit);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(object, "Non-Critical Condition", esp->non_crit);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(object, "Information Condition", esp->info);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(object, "Invalid Operation", esp->invop);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Reserved (byte 1, bits 5:7)", (double)esp->reserved_byte1_b5_7);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Page Length", (double)page_length);
    if (json_status != JSONSuccess) goto finish;
    generation_number = (uint32_t)StoH(esp->generation_number);
    json_status = json_object_set_number(object, "Generation Number", (double)generation_number);
    if (json_status != JSONSuccess) goto finish;

    /*
     * Request the configuration page, requird to decode the enclosure status page.
     */
    status = receive_diagnostic_page(sdp, iop, sgp, (void **)&scp, DIAG_CONFIGURATION_PAGE);
    if (status == FAILURE) goto finish;

    edp = (ses_enclosure_descriptor_t *)((uint8_t *)scp + sizeof(*scp));
    for (i = 0; i < (scp->secondary_enclosures + 1); i++) {
	total_element_types += edp->num_type_descriptor_headers;
	edp = (ses_enclosure_descriptor_t *)((uint8_t *)edp + edp->enclosure_descriptor_length + 4);
    }
    tdp = (ses_type_desc_header_t *)edp;
    tp = (char *)tdp + (total_element_types * sizeof(*tdp));

    sep = (ses_status_element_t *)((uint8_t *)esp + sizeof(*esp));

    for (i = 0; (i < total_element_types); i++, tdp++) {
	char *element_name = get_element_type(tdp->element_type);
	char element_text[STRING_BUFFER_SIZE];
	uint8_t text_length = tdp->type_descriptor_text_length;
	int element_index = 0;

	if ( (sdp->ses_element_type > 0) && (tdp->element_type != sdp->ses_element_type) ) {
	    tp += text_length;
	    sep += (tdp->number_elements + 1);
	    offset += (tdp->number_elements + 1) * sizeof(*tdp);
	    continue;
	}
	if (tvalue == NULL) {
	    tvalue  = json_value_init_object();
	    tobject = json_value_get_object(tvalue);
	}
	(void)strncpy(element_text, tp, text_length);
	element_text[text_length] = '\0';

	json_status = json_object_set_string(tobject, "Element Type", element_name);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(tobject, "Element Type Code", tdp->element_type);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(tobject, "Number of Elements", tdp->number_elements);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(tobject, "Subenclosure Identifier", (double)tdp->subenclosure_identifier);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_string(tobject, "Element Text", element_text);
	if (json_status != JSONSuccess) goto finish;

	/* 
	 * Note: The first descriptor is for the overall status.
	 */
	for (element_index = ELEMENT_INDEX_OVERALL;
	       (element_index < tdp->number_elements); element_index++, sep++) {

	    if ( (sdp->ses_element_flag == True) && (sdp->ses_element_index != element_index) ) {
		offset += sizeof(*sep);
		continue;
	    }
	    if ( (sdp->ses_element_status > 0) &&
		 (sdp->ses_element_status != sep->sc.element_status_code) ) {
		offset += sizeof(*sep);
		continue;
	    }
	    if (dvalue == NULL) {
		dvalue  = json_value_init_object();
		dobject = json_value_get_object(dvalue);
	    }
	    if (desc_value == NULL) {
		desc_value = json_value_init_array();
		desc_array = json_value_get_array(desc_value);
	    }

	    ucp = (uint8_t *)sep;
	    length = sizeof(*sep);
	    json_status = json_object_set_number(dobject, "Length", (double)length);
	    if (json_status != JSONSuccess) goto finish;
	    json_status = json_object_set_number(dobject, "Offset", (double)offset);
	    if (json_status != JSONSuccess) goto finish;
	    offset = FormatHexBytes(text, offset, ucp, length);
	    json_status = json_object_set_string(dobject, "Bytes", text);
	    if (json_status != JSONSuccess) goto finish;

	    if (element_index == ELEMENT_INDEX_OVERALL) {
		json_status = json_object_set_number(dobject, "Overall Status Descriptor", element_index);
		if (json_status != JSONSuccess) break;
	    } else {
		json_status = json_object_set_number(dobject, "Element Descriptor", element_index);
		if (json_status != JSONSuccess) break;
	    }
	    json_status = json_object_set_string(dobject, "Element Text", element_text);
	    if (json_status != JSONSuccess) break;

	    json_status = ses_element_type_status_json(sdp, dobject, tdp->element_type, sep);
	    if (json_status != JSONSuccess) break;

	    offset += sizeof(*sep);

	    json_array_append_value(desc_array, dvalue);
	    dvalue = NULL;
	}
	/* Finish this element type. */
	if (desc_value) {
	    json_object_set_value(tobject, "Descriptor List", desc_value);
	    desc_value = NULL;
	}
	json_status = json_object_dotset_value(object, element_name, tvalue);
	if (json_status != JSONSuccess) goto finish;
	tvalue = NULL;
	tp += text_length;
    }

finish:
    if (scp) {
	free_palign(sdp, scp);
    }
    (void)json_object_set_number(object, "JSON Status", (double)json_status);
    if (sdp->json_pretty) {
	json_string = json_serialize_to_string_pretty(root_value);
    } else {
	json_string = json_serialize_to_string(root_value);
    }
    json_value_free(root_value);
    return(json_string);
}

JSON_Status
ses_common_element_status_json(scsi_device_t *sdp, JSON_Object *dobject, ses_status_common_t *scp)
{
    char *element_status = get_element_status(scp->element_status_code);
    char *element_desc = get_element_status_desc(scp->element_status_code);
    JSON_Status json_status;

    json_status = json_object_set_number(dobject, "Element Status Code", (double)scp->element_status_code);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_string(dobject, "Element Status", element_status);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_string(dobject, "Element Status Description", element_desc);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(dobject, "Element Swapped", scp->swap);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(dobject, "Element Disabled", scp->disabled);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(dobject, "Predicted Failure", scp->prdfail);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(dobject, "Reserved (byte 0, bit 7)", (double)scp->reserved_byte0_b7);

finish:
    return(json_status);
}

int
ses_element_type_status_json(scsi_device_t *sdp, JSON_Object *dobject,
			     element_type_t element_type, ses_status_element_t *sep)
{
    char text[STRING_BUFFER_SIZE];
    JSON_Status json_status;

    json_status = ses_common_element_status_json(sdp, dobject, &sep->sc);
    if (json_status != JSONSuccess) return(json_status);

    switch (element_type) {

	case ELEMENT_TYPE_POWER_SUPPLY: {
	    ses_status_power_supply_element_t *psp;
	    psp = (ses_status_power_supply_element_t *)sep;

	    json_status = json_object_set_number(dobject, "Reserved (byte 1, bits 0:5)", (double)psp->reserved_byte1_b0_5);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Do Not Remove", psp->do_not_remove);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Identify LED", psp->ident);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_number(dobject, "Reserved (byte 2, bit 0)", (double)psp->reserved_byte2_b0);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "DC Overcurrent", psp->dc_overcurrent);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "DC Undervoltage", psp->dc_undervoltage);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "DC Overvoltage", psp->dc_overvoltage);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 2, bits 4:7)", (double)psp->reserved_byte2_b4_7);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_boolean(dobject, "DC Fail", psp->dc_fail);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "AC Fail", psp->ac_fail);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Over Temperature Warning", psp->temp_warn);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Over Temperature Failure", psp->over_temp_fail);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Power Supply Off", psp->off);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Requested On", psp->rqsted_on);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Failure LED", psp->fail);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Hot Swap", psp->hot_swap);
	    break;
	}
	case ELEMENT_TYPE_COOLING: {
	    ses_status_cooling_element_t *cep;
	    cep = (ses_status_cooling_element_t *)sep;
	    int actual_fan_speed;
	    char *actual_speed_name;

	    json_status = json_object_set_number(dobject, "Reserved (byte 1, bits 3:5)", (double)cep->reserved_byte1_b3_5);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Do Not Remove", cep->do_not_remove);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Identify LED", cep->ident);
	    if (json_status != JSONSuccess) break;

	    actual_fan_speed = (cep->actual_fan_speed_msb << 8) + cep->actual_fan_speed;
	    actual_fan_speed *= 10;	/* Convert to RPM's. */
	    /* Format into a ingle string for (future) message passing and JSON output. */
	    (void)sprintf(text, "%d rpm", actual_fan_speed);
	    json_status = json_object_set_string(dobject, "Actual Fan Speed", text);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_number(dobject, "Actual Speed Code", (double)cep->actual_speed_code);
	    if (json_status != JSONSuccess) break;
	    actual_speed_name = get_cooling_actual_speed(cep->actual_speed_code);
	    json_status = json_object_set_string(dobject, "Actual Speed Description", actual_speed_name);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 3, bit 3)", (double)cep->reserved_byte3_b3);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Power Supply Off", cep->off);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Requested On", cep->rqsted_on);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Failure LED", cep->fail);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Hot Swap", cep->hot_swap);
	    break;
	}
	case ELEMENT_TYPE_SENSOR_TEMPERATURE: {
	    ses_status_temperature_element_t *tep;
	    tep = (ses_status_temperature_element_t *)sep;
	    int temperature, temperature_offset = 20;

	    json_status = json_object_set_number(dobject, "Reserved (byte 1, bits 0:5)", (double)tep->reserved_byte1_b0_5);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Failure LED", tep->fail);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Identify LED", tep->ident);
	    if (json_status != JSONSuccess) break;

	    /* Temperature - offset gives us -90C to 235C. */
	    temperature = (int)tep->temperature;
	    if (temperature) {
		temperature -= temperature_offset;
		(void)sprintf(text, "%d Celsius", temperature);
	    } else {
		(void)sprintf(text, "%d (reserved)", temperature);
	    }
	    json_status = json_object_set_string(dobject, "Temperature", text);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_boolean(dobject, "Under Temperature Warning", tep->ut_warning);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Under Temperature Failure", tep->ut_failure);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Over Temperature Warning", tep->ot_warning);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Over Temperature Failure", tep->ot_failure);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 3, bits 4:7)", (double)tep->reserved_byte3_b4_7);
	    break;
	}
	case ELEMENT_TYPE_DOOR: {
	    ses_status_door_element_t *dep;
	    dep = (ses_status_door_element_t *)sep;

	    json_status = json_object_set_number(dobject, "Reserved (byte 1, bits 0:5)", (double)dep->reserved_byte1_b0_5);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Failure LED", dep->fail);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Identify LED", dep->ident);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_number(dobject, "Reserved (byte 2)", (double)dep->reserved_byte2);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_boolean(dobject, "Door unlocked", dep->unlocked);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Door open", dep->open);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 3, bits 2:7)", (double)dep->reserved_byte1_b0_5);
	    if (json_status != JSONSuccess) break;
	    break;
	}
	case ELEMENT_TYPE_ESCE: {
	    ses_status_esce_element_t *esp;
	    esp = (ses_status_esce_element_t *)sep;

	    json_status = json_object_set_number(dobject, "Reserved (byte 1, bits 0:3)", (double)esp->reserved_byte1_b0_3);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Slot Prepared For Removal", esp->rmv);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Do Not Remove", esp->do_not_remove);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Failure LED", esp->fail);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Identify LED", esp->ident);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_boolean(dobject, "Report", esp->report);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 2, bits 1:7)", (double)esp->reserved_byte2_b1_7);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_number(dobject, "Reserved (byte 3, bits 0:6)", (double)esp->reserved_byte3_b0_6);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Hot Swap", esp->hot_swap);
	    break;
	}
	case ELEMENT_TYPE_ENCLOSURE: {
	    ses_status_enclosure_element_t *eep;
	    eep = (ses_status_enclosure_element_t *)sep;
	    char *power_cycle_description = NULL;

	    json_status = json_object_set_number(dobject, "Reserved (byte 1, bits 0:6)", (double)eep->reserved_byte1_b0_6);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Identify LED", eep->ident);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_boolean(dobject, "Warning Indication", eep->warning_indication);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Failure Indication", eep->failure_indication);
	    if (json_status != JSONSuccess) break;
	    if (eep->time_until_power_cycle == 0) {
		power_cycle_description = "(No power cycle scheduled)";
	    } else if ( (eep->time_until_power_cycle >= 1) && (eep->time_until_power_cycle <= 60) ) {
		power_cycle_description = "(Power cycle after indicated minutes)";
	    } else if ( (eep->time_until_power_cycle >= 61) && (eep->time_until_power_cycle <= 62) ) {
		power_cycle_description = "(Reserved)";
	    } else if (eep->time_until_power_cycle == 63) {
		power_cycle_description = "(Power cycle after zero minutes)";
	    } else {
		power_cycle_description = "";
	    }
	    (void)sprintf(text, "%d %s", eep->time_until_power_cycle, power_cycle_description);
	    json_status = json_object_set_string(dobject, "Time until power cycle", text);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_boolean(dobject, "Warning Requested", eep->warning_requested);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Failure Requested", eep->failure_requested);
	    if (json_status != JSONSuccess) break;
	    if (eep->requested_power_off_duration == 0) {
		power_cycle_description = "(No power cycle scheduled or to be kept off less than one minute)";
	    } else if ( (eep->requested_power_off_duration >= 1) && (eep->requested_power_off_duration <= 60) ) {
		power_cycle_description = "(Power scheduled to be off for indicated minutes)";
	    } else if ( (eep->requested_power_off_duration >= 61) && (eep->requested_power_off_duration <= 62) ) {
		power_cycle_description = "(Reserved)";
	    } else if (eep->requested_power_off_duration == 63) {
		power_cycle_description = "(Power to be kept off until manually restored)";
	    } else {
		power_cycle_description = "";
	    }
	    (void)sprintf(text, "%d %s", eep->requested_power_off_duration, power_cycle_description);
	    json_status = json_object_set_string(dobject, "Requested Power Off Duration", text);
	    break;
	}
	case ELEMENT_TYPE_VOLTAGE_SENSOR: {
	    ses_status_voltage_element_t *vep;
	    vep = (ses_status_voltage_element_t *)sep;
	    int voltage;

	    json_status = json_object_set_boolean(dobject, "Critical Under Voltage", vep->crit_under);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Critical Over Voltage", vep->crit_over);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Under Voltage Warning", vep->warn_under);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Over Voltage Warning", vep->warn_over);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 1, bits 4:5)", (double)vep->reserved_byte1_b4_5);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Failure LED", vep->fail);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Identify LED", vep->ident);
	    if (json_status != JSONSuccess) break;

	    voltage = (int)(short)StoH(vep->voltage);
	    (void)sprintf(text, "%.2f volts", ((float)voltage / 100.0));
	    json_status = json_object_set_string(dobject, "Voltage", text);
	    break;
	}
	case ELEMENT_TYPE_CURRENT_SENSOR: {
	    ses_status_current_element_t *cep;
	    cep = (ses_status_current_element_t *)sep;
	    int current;

	    json_status = json_object_set_number(dobject, "Reserved (byte 1, bit 0)", (double)cep->reserved_byte1_b0);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Critical Over Current", cep->crit_over);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 1, bit 2)", (double)cep->reserved_byte1_b2);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Over Current Warning", cep->warn_over);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 1, bits 4:5)", (double)cep->reserved_byte1_b4_5);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Failure LED", cep->fail);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Identify LED", cep->ident);
	    if (json_status != JSONSuccess) break;

	    current = (int)(short)StoH(cep->current);
	    (void)sprintf(text, "%.2f amps", ((float)current / 100.0));
	    json_status = json_object_set_string(dobject, "Current", text);
	    break;
	}
	case ELEMENT_TYPE_ARRAY_DEVICE_SLOT: {
	    ses_status_array_device_element_t *adp;
	    adp = (ses_status_array_device_element_t *)sep;

	    json_status = json_object_set_boolean(dobject, "Rebuild/Remap Abort", adp->rr_abort);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Rebuild/Remap", adp->rebuild_remap);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "In Failed Array", adp->in_failed_array);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "In Critical Array", adp->in_crit_array);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Consistency Check In Progress", adp->cons_chk);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Hot Spare", adp->hot_spare);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Reserved Device", adp->rsvd_device);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Device Okay", adp->ok);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_boolean(dobject, "Report", adp->report);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Identify LED", adp->ident);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Slot Prepared For Removal", adp->rmv);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Ready to Insert", adp->ready_to_insert);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Enclosure Bypassed Port B", adp->enclosure_bypassed_b);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Enclosure Bypassed Port A", adp->enclosure_bypassed_a);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Do Not Remove", adp->do_not_remove);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Application Client Bypassed Port A", adp->app_client_bypassed_a);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_boolean(dobject, "Device Bypassed Port B", adp->device_bypassed_b);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Device Bypassed Port A", adp->device_bypassed_a);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Bypassed Port B", adp->bypassed_b);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Bypassed Port A", adp->bypassed_a);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Device Turned Off", adp->device_off);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Fault Requested", adp->fault_reqstd);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Fault Sensed", adp->fault_sensed);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Application Client Bypassed Port B", adp->app_client_bypassed_b);
	    break;
	}
	case ELEMENT_TYPE_SAS_EXPANDER: {
	    ses_status_sas_expander_element_t  *sasep;
	    sasep = (ses_status_sas_expander_element_t *)sep;

	    json_status = json_object_set_number(dobject, "Reserved (byte 1, bits 0:5)", (double)sasep->reserved_byte1_b0_5);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Failure LED", sasep->fail);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Identify LED", sasep->ident);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_number(dobject, "Reserved (byte 2)", (double)sasep->reserved_byte2);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 3)", (double)sasep->reserved_byte3);
	    if (json_status != JSONSuccess) break;
	    break;
	}
	case ELEMENT_TYPE_SAS_CONNECTOR: {
	    ses_status_sas_connector_element_t *sascp;
	    sascp = (ses_status_sas_connector_element_t *)sep;
	    char *connector_name = NULL;

	    json_status = json_object_set_number(dobject, "Connector Type", (double)sascp->connector_type);
	    if (json_status != JSONSuccess) break;
	    connector_name = get_connector_type(sascp->connector_type);
	    json_status = json_object_set_string(dobject, "Connector Description", connector_name);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Identify LED", sascp->ident);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_number(dobject, "Connector Physical Link", (double)sascp->connector_physical_link);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_number(dobject, "Reserved (byte 3, bits 0:4)", (double)sascp->reserved_byte3_b0_4);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Mated", sascp->mated);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Failure LED", sascp->fail);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Overcurrent", sascp->overcurrent);
	    break;
	}
	default: {
	    /* Note: Multiple warnings will create invalid JSON! */
	    char *element_name = get_element_type(element_type);
	    (void)sprintf(text, "Element type %s (0x%02x), is NOT implemented yet!",
			  element_name, element_type);
	    json_status = json_object_set_string(dobject, "Warning", text);
	    break;
	}
    }
    return(json_status);
}

/* ============================================================================================== */
/*
 * SES Element Descriptor Status Page 0x07.
 */
int
ses_element_descriptor_receive_diagnostic_decode(scsi_device_t *sdp, io_params_t *iop,
						 scsi_generic_t *sgp, diagnostic_page_header_t *dph)
{
    ses_element_descriptor_page_t *sedp = (ses_element_descriptor_page_t *)dph;
    ses_element_descriptor_t *sep = NULL;
    ses_configuration_page_t *scp = NULL;
    ses_enclosure_descriptor_t *edp = NULL;
    ses_type_desc_header_t *tdp = NULL;
    int	page_length = (int)StoH(sedp->page_length);
    int descriptor_length = 0;
    uint32_t generation_number = 0;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    int i, offset = 0;
    int location = 0;
    int total_element_types = 0;
    int status = SUCCESS;

    if (sdp->output_format == JSON_FMT) {
	char *json_string;
	json_string = ses_element_descriptor_to_json(sdp, iop, sgp, sedp, "Element Descriptor");
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }

    PrintHeader(sdp, "Element Descriptor Diagnostic Page");

    if (sdp->DebugFlag) {
	uint8_t *ucp = (uint8_t *)sedp;
	int length = sizeof(*sedp);
	offset = PrintHexData(sdp, offset, ucp, length);
    } else {
	offset += sizeof(*sedp);
    }
    PrintHex(sdp, "Page Code", sedp->page_code, PNL); 
    if (sedp->reserved_byte1 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 1)", sedp->reserved_byte1, PNL);
    }
    PrintDecHex(sdp, "Page Length", page_length, PNL);
    generation_number = (uint32_t)StoH(sedp->generation_number);
    PrintHex(sdp, "Generation Number", generation_number, PNL);

    /*
     * The designers of SES page 2, decided to omit the element type, and instead 
     * order elements according to what the configuration page reports. So be it! 
     * Therefore, we must request the configuration page to decode page 2 elements! 
     */
    status = receive_diagnostic_page(sdp, iop, sgp, (void **)&scp, DIAG_CONFIGURATION_PAGE);
    if (status == FAILURE) return(status);

    edp = (ses_enclosure_descriptor_t *)((uint8_t *)scp + sizeof(*scp));
    for (i = 0; i < (scp->secondary_enclosures + 1); i++) {
	total_element_types += edp->num_type_descriptor_headers;
	edp = (ses_enclosure_descriptor_t *)((uint8_t *)edp + edp->enclosure_descriptor_length + 4);
    }
    tdp = (ses_type_desc_header_t *)edp;
    tp = (char *)tdp + (total_element_types * sizeof(*tdp));

    sep = (ses_element_descriptor_t *)((uint8_t *)sedp + sizeof(*sedp));

    for (i = 0; (i < total_element_types); i++, tdp++) {
	char *element_name = get_element_type(tdp->element_type);
	char element_text[STRING_BUFFER_SIZE];
	uint8_t text_length = tdp->type_descriptor_text_length;
	int element_index = 0;

	if ( (sdp->ses_element_type > 0) && (tdp->element_type != sdp->ses_element_type) ) {
	    tp += text_length;
	    /* Note: The element descriptors may be variable length! */
	    for (element_index = ELEMENT_INDEX_OVERALL;
		   (element_index < tdp->number_elements); element_index++) {
		location++;
		descriptor_length = (int)StoH(sep->descriptor_length);
		offset += sizeof(*sep) + descriptor_length;
		sep = (ses_element_descriptor_t *)((uint8_t *)sep + sizeof(*sep) + descriptor_length);
	    }
	    continue;
	}
	(void)strncpy(element_text, tp, text_length);
	element_text[text_length] = '\0';

	Printf(sdp, "\n");
	(void)sprintf(text, "%s (0x%02x)", element_name, tdp->element_type);
	PrintAscii(sdp, "Element Type", text, PNL);
	PrintDecimal(sdp, "Number of Elements", tdp->number_elements, PNL);
	PrintDecimal(sdp, "Subenclosure Identifier", tdp->subenclosure_identifier, DNL);
	if (tdp->subenclosure_identifier == 0) {
	    Print(sdp, " (Primary)\n");
	} else {
	    Print(sdp, " (Secondary)\n");
	}
	PrintAscii(sdp, "Element Text", element_text, PNL);

	/* 
	 * Note: The first descriptor is for the overall status.
	 */
	for (element_index = ELEMENT_INDEX_OVERALL;
	       (element_index < tdp->number_elements); element_index++) {

	    descriptor_length = (int)StoH(sep->descriptor_length);
	    if ( (sdp->ses_element_flag == True) && (sdp->ses_element_index != element_index) ) {
		location++;
		offset += sizeof(*sep) + descriptor_length;
		sep = (ses_element_descriptor_t *)((uint8_t *)sep + sizeof(*sep) + descriptor_length);
		continue;
	    }
	    Printf(sdp, "\n");
	    if (sdp->DebugFlag) {
		uint8_t *ucp = (uint8_t *)sep;
		int length = sizeof(*sep);
		PrintDecimal(sdp, "Element Location", location, PNL);
		/* Element Descriptor */
		offset = PrintHexData(sdp, offset, ucp, length);
	    }
	    if (element_index == ELEMENT_INDEX_OVERALL) {
		PrintAscii(sdp, "Overall Status Descriptor", "", PNL);
	    } else {
		PrintDecimal(sdp, "Element Descriptor", element_index, PNL);
	    }
	    PrintAscii(sdp, "Element Text", element_text, PNL);
	    if (sep->reserved_byte0 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 0)", sep->reserved_byte0, PNL);
	    }
	    if (sep->reserved_byte1 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1)", sep->reserved_byte1, PNL);
	    }
	    PrintDecHex(sdp, "Descriptor Length", descriptor_length, PNL);

	    if (sdp->DebugFlag) {
		uint8_t *ucp = (uint8_t *)sep + sizeof(*sep);
		/* Element Descriptor Text */
		offset = PrintAsciiData(sdp, offset, ucp, descriptor_length);
	    }
	    (void)FormatQuotedText(text, (char *)sep + sizeof(*sep), descriptor_length);
	    PrintAscii(sdp, "Descriptor Text", text, PNL);

	    if (sdp->DebugFlag == False) {
		offset += sizeof(*sep) + descriptor_length;
	    }
	    location++;
	    sep = (ses_element_descriptor_t *)((uint8_t *)sep + sizeof(*sep) + descriptor_length);
	}
	tp += text_length;
    }

    if (scp) {
	free_palign(sdp, scp);
    }
    Printf(sdp, "\n");
    return(status);
}

/*
 * SES Element Descriptor Page 0x07 in JSON Format.
 */
char *
ses_element_descriptor_to_json(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
			       ses_element_descriptor_page_t *sedp, char *page_name)
{
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    JSON_Value  *tvalue = NULL;
    JSON_Object *tobject = NULL;
    JSON_Value  *dvalue = NULL;
    JSON_Object *dobject = NULL;
    JSON_Array	*desc_array;
    JSON_Value	*desc_value = NULL;
    JSON_Status json_status;
    char *json_string = NULL;
    ses_element_descriptor_t *sep = NULL;
    ses_configuration_page_t *scp = NULL;
    ses_enclosure_descriptor_t *edp = NULL;
    ses_type_desc_header_t *tdp = NULL;
    int	page_length = (int)StoH(sedp->page_length);
    int descriptor_length = 0;
    uint32_t generation_number = 0;
    uint64_t enclosure_logical_id = 0;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    uint8_t *ucp = NULL;
    int length = 0;
    int location = 0;
    int offset = 0;
    int toffset = 0;
    int i, addl_len = 0;
    int total_element_types = 0;
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

    ucp = (uint8_t *)sedp;
    length = sizeof(*sedp);
    json_status = json_object_set_number(object, "Length", (double)length);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Offset", (double)offset);
    if (json_status != JSONSuccess) goto finish;
    offset = FormatHexBytes(text, offset, ucp, length);
    json_status = json_object_set_string(object, "Bytes", text);
    if (json_status != JSONSuccess) goto finish;

    (void)sprintf(text, "0x%02x", sedp->page_code);
    json_status = json_object_set_string(object, "Page Code", text);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Reserved (byte 1)", (double)sedp->reserved_byte1);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Page Length", (double)page_length);
    if (json_status != JSONSuccess) goto finish;
    generation_number = (uint32_t)StoH(sedp->generation_number);
    json_status = json_object_set_number(object, "Generation Number", (double)generation_number);
    if (json_status != JSONSuccess) goto finish;

    /*
     * Request the configuration page, requird to decode the enclosure status page.
     */
    status = receive_diagnostic_page(sdp, iop, sgp, (void **)&scp, DIAG_CONFIGURATION_PAGE);
    if (status == FAILURE) goto finish;

    edp = (ses_enclosure_descriptor_t *)((uint8_t *)scp + sizeof(*scp));
    for (i = 0; i < (scp->secondary_enclosures + 1); i++) {
	total_element_types += edp->num_type_descriptor_headers;
	edp = (ses_enclosure_descriptor_t *)((uint8_t *)edp + edp->enclosure_descriptor_length + 4);
    }
    tdp = (ses_type_desc_header_t *)edp;
    tp = (char *)tdp + (total_element_types * sizeof(*tdp));

    sep = (ses_element_descriptor_t *)((uint8_t *)sedp + sizeof(*sedp));

    for (i = 0; (i < total_element_types); i++, tdp++) {
	char *element_name = get_element_type(tdp->element_type);
	char element_text[STRING_BUFFER_SIZE];
	uint8_t text_length = tdp->type_descriptor_text_length;
	int element_index = 0;

	if ( (sdp->ses_element_type > 0) && (tdp->element_type != sdp->ses_element_type) ) {
	    tp += text_length;
	    /* Note: The element descriptors may be variable length! */
	    for (element_index = ELEMENT_INDEX_OVERALL;
		   (element_index < tdp->number_elements); element_index++) {
		location++;
		descriptor_length = (int)StoH(sep->descriptor_length);
		offset += sizeof(*sep) + descriptor_length;
		sep = (ses_element_descriptor_t *)((uint8_t *)sep + sizeof(*sep) + descriptor_length);
	    }
	    continue;
	}
	if (tvalue == NULL) {
	    tvalue  = json_value_init_object();
	    tobject = json_value_get_object(tvalue);
	}
	(void)strncpy(element_text, tp, text_length);
	element_text[text_length] = '\0';

	json_status = json_object_set_string(tobject, "Element Type", element_name);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(tobject, "Element Type Code", tdp->element_type);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(tobject, "Number of Elements", tdp->number_elements);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(tobject, "Subenclosure Identifier", (double)tdp->subenclosure_identifier);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_string(tobject, "Element Text", element_text);
	if (json_status != JSONSuccess) goto finish;

	/* 
	 * Note: The first descriptor is for the overall status.
	 */
	for (element_index = ELEMENT_INDEX_OVERALL;
	       (element_index < tdp->number_elements); element_index++) {

	    descriptor_length = (int)StoH(sep->descriptor_length);
	    if ( (sdp->ses_element_flag == True) && (sdp->ses_element_index != element_index) ) {
		location++;
		offset += sizeof(*sep) + descriptor_length;
		sep = (ses_element_descriptor_t *)((uint8_t *)sep + sizeof(*sep) + descriptor_length);
		continue;
	    }
	    if (dvalue == NULL) {
		dvalue  = json_value_init_object();
		dobject = json_value_get_object(dvalue);
	    }
	    if (desc_value == NULL) {
		desc_value = json_value_init_array();
		desc_array = json_value_get_array(desc_value);
	    }

	    json_status = json_object_set_number(dobject, "Element Location", (double)location);
	    if (json_status != JSONSuccess) goto finish;
	    ucp = (uint8_t *)sep;
	    length = sizeof(*sep) + descriptor_length;
	    json_status = json_object_set_number(dobject, "Length", (double)length);
	    if (json_status != JSONSuccess) goto finish;
	    json_status = json_object_set_number(dobject, "Offset", (double)offset);
	    if (json_status != JSONSuccess) goto finish;
	    offset = FormatHexBytes(text, offset, ucp, length);
	    json_status = json_object_set_string(dobject, "Bytes", text);
	    if (json_status != JSONSuccess) goto finish;

	    if (element_index == ELEMENT_INDEX_OVERALL) {
		json_status = json_object_set_number(dobject, "Overall Status Descriptor", (double)element_index);
		if (json_status != JSONSuccess) break;
	    } else {
		json_status = json_object_set_number(dobject, "Element Descriptor", (double)element_index);
		if (json_status != JSONSuccess) break;
	    }
	    json_status = json_object_set_string(dobject, "Element Text", element_text);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_number(object, "Reserved (byte 0)", (double)sep->reserved_byte0);
	    if (json_status != JSONSuccess) goto finish;
	    json_status = json_object_set_number(object, "Reserved (byte 1)", (double)sep->reserved_byte1);
	    if (json_status != JSONSuccess) goto finish;
	    json_status = json_object_set_number(object, "Descriptor Length", (double)descriptor_length);
	    if (json_status != JSONSuccess) goto finish;
	    (void)strncpy(text, (char *)sep + sizeof(*sep), descriptor_length);
	    text[descriptor_length] = '\0';
	    json_status = json_object_set_string(dobject, "Descriptor Text", text);
	    if (json_status != JSONSuccess) break;

	    location++;
	    sep = (ses_element_descriptor_t *)((uint8_t *)sep + sizeof(*sep) + descriptor_length);

	    json_array_append_value(desc_array, dvalue);
	    dvalue = NULL;
	}
	/* Finish this element type. */
	if (desc_value) {
	    json_object_set_value(tobject, "Descriptor List", desc_value);
	    desc_value = NULL;
	}
	json_status = json_object_dotset_value(object, element_name, tvalue);
	if (json_status != JSONSuccess) goto finish;
	tvalue = NULL;
	tp += text_length;
    }

finish:
    if (scp) {
	free_palign(sdp, scp);
    }
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
/*
 * SES Additional Element Status Page 0x0A.
 */
int
ses_addl_element_status_receive_diagnostic_decode(scsi_device_t *sdp, io_params_t *iop,
						  scsi_generic_t *sgp, diagnostic_page_header_t *dph)
{
    ses_addl_element_status_page_t *aesp = (ses_addl_element_status_page_t *)dph;
    ses_addl_element_status_descriptor_t *aedp = NULL;
    ses_configuration_page_t *scp = NULL;
    ses_enclosure_descriptor_t *edp = NULL;
    ses_type_desc_header_t *tdp = NULL;
    int	page_length = (int)StoH(aesp->page_length);
    int descriptor_length = 0;
    uint32_t generation_number = 0;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    int i, offset = 0;
    int total_element_types = 0;
    int status = SUCCESS;

    if (sdp->output_format == JSON_FMT) {
	char *json_string;
	json_string = ses_addl_element_status_to_json(sdp, iop, sgp, aesp, "Additional Element Status");
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }

    PrintHeader(sdp, "Additional Element Status Diagnostic Page");

    if (sdp->DebugFlag) {
	uint8_t *ucp = (uint8_t *)aesp;
	int length = sizeof(*aesp);
	offset = PrintHexData(sdp, offset, ucp, length);
    } else {
	offset += sizeof(*aesp);
    }
    PrintHex(sdp, "Page Code", aesp->page_code, PNL); 
    if (aesp->reserved_byte1 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 1)", aesp->reserved_byte1, PNL);
    }
    PrintDecHex(sdp, "Page Length", page_length, PNL);
    generation_number = (uint32_t)StoH(aesp->generation_number);
    PrintHex(sdp, "Generation Number", generation_number, PNL);

    status = receive_diagnostic_page(sdp, iop, sgp, (void **)&scp, DIAG_CONFIGURATION_PAGE);
    if (status == FAILURE) return(status);

    edp = (ses_enclosure_descriptor_t *)((uint8_t *)scp + sizeof(*scp));
    for (i = 0; i < (scp->secondary_enclosures + 1); i++) {
	total_element_types += edp->num_type_descriptor_headers;
	edp = (ses_enclosure_descriptor_t *)((uint8_t *)edp + edp->enclosure_descriptor_length + 4);
    }
    tdp = (ses_type_desc_header_t *)edp;
    tp = (char *)tdp + (total_element_types * sizeof(*tdp));

    aedp = (ses_addl_element_status_descriptor_t *)((uint8_t *)aesp + sizeof(*aesp));

    /*
     * Format: 
     *  Additional Element Status Descriptor
     *  Protocol Specific Information Descriptor
     */
    for (i = 0; (i < total_element_types); i++, tdp++) {
	char *element_name = get_element_type(tdp->element_type);
	char element_text[STRING_BUFFER_SIZE];
	uint8_t text_length = tdp->type_descriptor_text_length;
	int element_index = 0;

	if (valid_addl_element_types(tdp->element_type) == False) {
	    tp += text_length;
	    continue;
	}

	if ( (sdp->ses_element_type > 0) && (tdp->element_type != sdp->ses_element_type) ) {
	    tp += text_length;
	    /* Note: The element descriptors may be variable length! */
	    for (element_index = 0; (element_index < tdp->number_elements); element_index++) {
		descriptor_length = (int)aedp->addl_element_desc_length - 2;
		offset += sizeof(*aedp) + descriptor_length;
		aedp = (ses_addl_element_status_descriptor_t *)((uint8_t *)aedp + sizeof(*aedp) + descriptor_length);
	    }
	    continue;
	}
	(void)strncpy(element_text, tp, text_length);
	element_text[text_length] = '\0';

	Printf(sdp, "\n");
	(void)sprintf(text, "%s (0x%02x)", element_name, tdp->element_type);
	PrintAscii(sdp, "Element Type", text, PNL);
	PrintDecimal(sdp, "Number of Elements", tdp->number_elements, PNL);
	PrintDecimal(sdp, "Subenclosure Identifier", tdp->subenclosure_identifier, DNL);
	if (tdp->subenclosure_identifier == 0) {
	    Print(sdp, " (Primary)\n");
	} else {
	    Print(sdp, " (Secondary)\n");
	}
	PrintAscii(sdp, "Element Text", element_text, PNL);

	for (element_index = 0; (element_index < tdp->number_elements); element_index++) {

	    descriptor_length = (int)aedp->addl_element_desc_length - 2;
	    if ( (sdp->ses_element_flag == True) && (sdp->ses_element_index != element_index) ) {
		offset += sizeof(*aedp) + descriptor_length;
		aedp = (ses_addl_element_status_descriptor_t *)((uint8_t *)aedp + sizeof(*aedp) + descriptor_length);
		continue;
	    }
	    Printf(sdp, "\n");
	    offset = PrintHexDebug(sdp, offset, (uint8_t *)aedp, sizeof(*aedp));
	    PrintDecimal(sdp, "Element Descriptor", element_index, PNL);
	    PrintAscii(sdp, "Element Text", element_text, PNL);
	    PrintHex(sdp, "The Protocol Identifier", aedp->protocol_identifier, DNL);
	    Print(sdp, " (%s)\n", find_ses_protocol_identifier(aedp->protocol_identifier));
	    /* Note: The descriptor size is shorter is EIP=0, which I'm *not* implementing! */
	    PrintBoolean(sdp, False, "Element Index Present (EIP)", aedp->eip, PNL);
	    if (aedp->reserved_byte0_b5_6 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 0, bits 5:6)", aedp->reserved_byte0_b5_6, PNL);
	    }
	    PrintBoolean(sdp, False, "Protocol Specific Information Invalid", aedp->invalid, PNL);
	    PrintHexDec(sdp, "Additional Element Descriptor Length", descriptor_length, PNL);
	    PrintHex(sdp, "Element Index Includes Overall (EIIOE)", aedp->eiioe, PNL);
	    if (aedp->reserved_byte2_b2_7 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 2, bits 2:7)", aedp->reserved_byte2_b2_7, PNL);
	    }
	    PrintDecimal(sdp, "Element Index", aedp->element_index, PNL);

	    offset = PrintHexDebug(sdp, offset, (uint8_t *)aedp + sizeof(*aedp), descriptor_length);
	    if (aedp->invalid == False) {
		status = ses_protocol_specific_information(sdp, aedp, tdp->element_type);
		/* 
		 * Display offset and hex bytes, if we did not decode this element.
		 */
		if ( (status == WARNING) && (sdp->DebugFlag == False) ) {
		    uint8_t *ucp = (uint8_t *)aedp;
		    offset = PrintHexData(sdp, offset, ucp, descriptor_length);
		}
	    }
	    aedp = (ses_addl_element_status_descriptor_t *)((uint8_t *)aedp + sizeof(*aedp) + descriptor_length);
	}
	tp += text_length;
    }

    if (scp) {
	free_palign(sdp, scp);
    }
    Printf(sdp, "\n");
    return(status);
}

char *
find_ses_protocol_identifier(uint8_t protocol_identifier)
{
    if (protocol_identifier == SES_PROTOCOL_IDENTIFIER_FC) {
	return("Fibre Channel");
    } else if (protocol_identifier == SES_PROTOCOL_IDENTIFIER_SAS) {
	return("SAS");
    } else if (protocol_identifier == SES_PROTOCOL_IDENTIFIER_PCIe) {
	return("PCIe");
    } else {
	return("<unknown>");
    }
}

char *
find_sas_device_type(uint8_t device_type)
{
    switch (device_type) {
        case SAS_DTYPE_NO_DEVICE_ATTACHED:
	    return("No device attached");
	case SAS_DTYPE_END_DEVICE:
	    return("End device");
	case SAS_DTYPE_EXPANDER_DEVICE:
	    return("Expander device");
	default:
	    return("reserved");
    }
}

/*
 * Check for valid element types returned by Additional Element Status Page. 
 * Again, Mickey Mouse crap due to these pages being poorly designed| (sigh)
 */
hbool_t
valid_addl_element_types(uint8_t element_type)
{
    switch (element_type) {
        case ELEMENT_TYPE_DEVICE_SLOT:
	case ELEMENT_TYPE_ESCE:
	case ELEMENT_TYPE_SCSI_TARGET_PORT:
	case ELEMENT_TYPE_SCSI_INITIATOR_PORT:
	case ELEMENT_TYPE_ARRAY_DEVICE_SLOT:
	case ELEMENT_TYPE_SAS_EXPANDER:
	    return(True);
	default:
	    return(False);
    }
}

int
ses_protocol_specific_information(scsi_device_t *sdp, ses_addl_element_status_descriptor_t *aedp, element_type_t element_type)
{
    char *element_name = get_element_type(element_type);
    char *protocol_name = find_ses_protocol_identifier(aedp->protocol_identifier);
    int status = SUCCESS;

    switch (aedp->protocol_identifier) {

	//case SES_PROTOCOL_IDENTIFIER_FC:
	//    break;

	case SES_PROTOCOL_IDENTIFIER_SAS: {
	    sas_protocol_information_t *spi = (sas_protocol_information_t *)((uint8_t *)aedp + sizeof(*aedp));

	    if ( (element_type == ELEMENT_TYPE_ARRAY_DEVICE_SLOT) && (spi->descriptor_type == SAS_DESCRIPTOR_TYPE0) ) {
		sas_protocol_array_t *spa = (sas_protocol_array_t *)spi;

		Printf(sdp, "\n");
		PrintDecimal(sdp, "Number of Phy Descriptors", spa->number_phy_descriptors, PNL);
		PrintBoolean(sdp, False, "Not All Phys", spa->not_all_phys, PNL);
		if (spa->reserved_byte1_b1_5 || sdp->DebugFlag) {
		    PrintHex(sdp, "Reserved (byte 1, bits 0:5)", spa->reserved_byte1_b1_5, PNL);
		}
		PrintDecimal(sdp, "The Descriptor Type", spa->descriptor_type, PNL);
		if (spa->reserved_byte2 || sdp->DebugFlag) {
		    PrintHex(sdp, "Reserved (byte 2)", spa->reserved_byte2, PNL);
		}
		PrintDecimal(sdp, "Device Slot Number", spa->device_slot_number, PNL);
		if (spa->number_phy_descriptors) {
		    sas_array_phy_descriptor_t *apd = (sas_array_phy_descriptor_t *)((uint8_t *)spa + sizeof(*spa));
		    char *sas_device_type = NULL;
		    int phy;

		    for (phy = 0; phy < spa->number_phy_descriptors; phy++, apd++) {
			Printf(sdp, "\n");
			PrintDecimal(sdp, "Phy Descriptor", phy, PNL);

			if (apd->reserved_byte0_b0_3 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 0, bits 0:3)", apd->reserved_byte0_b0_3, PNL);
			}
			PrintHex(sdp, "The Device Type", apd->device_type, DNL);
			sas_device_type = find_sas_device_type(apd->device_type);
			Print(sdp, " (%s)\n", sas_device_type);
			if (apd->reserved_byte0_b7 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 0, bit 7)", apd->reserved_byte0_b7, PNL);
			}
			if (apd->reserved_byte1 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 1)", apd->reserved_byte1, PNL);
			}
			if (apd->reserved_byte2_b0 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 2, bit 0)", apd->reserved_byte2_b0, PNL);
			}
			PrintBoolean(sdp, False, "SMP Initiator Port", apd->smp_initiator_port, PNL);
			PrintBoolean(sdp, False, "STP Initiator Port", apd->stp_initiator_port, PNL);
			PrintBoolean(sdp, False, "SSP Initiator Port", apd->ssp_initiator_port, PNL);
			if (apd->reserved_byte2_b4_4 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 2, bits 4:4)", apd->reserved_byte2_b4_4, PNL);
			}

			PrintBoolean(sdp, False, "SATA Device", apd->sata_device, PNL);
			PrintBoolean(sdp, False, "SMP Target Port", apd->smp_target_port, PNL);
			PrintBoolean(sdp, False, "STP Target Port", apd->stp_target_port, PNL);
			PrintBoolean(sdp, False, "SSP Target Port", apd->ssp_target_port, PNL);
			if (apd->reserved_byte3_b4_6 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 3, bits 4:6)", apd->reserved_byte3_b4_6, PNL);
			}
			PrintBoolean(sdp, False, "SATA Port Selector", apd->sata_port_selector, PNL);

			PrintLongHexP(sdp, "Attached SAS Address", StoH(apd->attached_sas_address), PNL);
			PrintLongHexP(sdp, "The SAS Address", StoH(apd->sas_address), PNL);
			PrintDecimal(sdp, "The Phy Identifier", apd->phy_identifier, PNL);
			if (apd->reserved_byte21 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 21)", apd->reserved_byte21, PNL);
			}
			if (apd->reserved_byte22 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 22)", apd->reserved_byte22, PNL);
			}
			if (apd->reserved_byte23 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 23)", apd->reserved_byte23, PNL);
			}
			if (apd->reserved_byte24 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 24)", apd->reserved_byte24, PNL);
			}
			if (apd->reserved_byte25 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 25)", apd->reserved_byte25, PNL);
			}
			if (apd->reserved_byte26 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 26)", apd->reserved_byte26, PNL);
			}
			if (apd->reserved_byte27 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 27)", apd->reserved_byte27, PNL);
			}
		    }
		}
		return(status);
	    } else if ( (element_type == ELEMENT_TYPE_ESCE) && (spi->descriptor_type == SAS_DESCRIPTOR_TYPE1) ) {
		sas_protocol_esce_t *spe = (sas_protocol_esce_t *)spi;

		Printf(sdp, "\n");
		PrintDecimal(sdp, "Number of Phy Descriptors", spe->number_phy_descriptors, PNL);
		if (spe->reserved_byte1_b0_5 || sdp->DebugFlag) {
		    PrintHex(sdp, "Reserved (byte 1, bits 0:5)", spe->reserved_byte1_b0_5, PNL);
		}
		PrintDecimal(sdp, "The Descriptor Type", spe->descriptor_type, PNL);
		if (spe->reserved_byte2 || sdp->DebugFlag) {
		    PrintHex(sdp, "Reserved (byte 2)", spe->reserved_byte2, PNL);
		}
		if (spe->reserved_byte3 || sdp->DebugFlag) {
		    PrintHex(sdp, "Reserved (byte 3)", spe->reserved_byte3, PNL);
		} 
		if (spe->number_phy_descriptors) {
		    sas_esce_phy_descriptor_t *epd = (sas_esce_phy_descriptor_t *)((uint8_t *)spe + sizeof(*spe));
		    int phy;
		    for (phy = 0; phy < spe->number_phy_descriptors; phy++, epd++) {
			Printf(sdp, "\n");
			PrintDecimal(sdp, "Phy Descriptor", phy, PNL);
			PrintDecimal(sdp, "The Phy Identifier", epd->phy_identifier, PNL);
			if (epd->reserved_byte1 || sdp->DebugFlag) {
			    PrintHex(sdp, "Reserved (byte 1)", epd->reserved_byte1, PNL);
			} 
			PrintDecimal(sdp, "Connector Element Index", epd->connector_element_index, DNL);
			if (epd->connector_element_index == PHY_NOT_CONNECTED) {
			    Print(sdp, " (not attached to a connector)\n");
			} else {
			    Print(sdp, "\n");
			}
			PrintDecimal(sdp, "Other Element Index", epd->other_element_index, DNL);
			if (epd->other_element_index == PHY_NOT_CONNECTED) {
			    Print(sdp, " (not attached to a connector)\n");
			} else {
			    Print(sdp, "\n");
			}
			PrintLongHexP(sdp, "The SAS Address", StoH(epd->sas_address), PNL);
		    }
		}
		return(status);
	    } else if ( (element_type == ELEMENT_TYPE_SAS_EXPANDER) && (spi->descriptor_type == SAS_DESCRIPTOR_TYPE1) ) {
		sas_protocol_expander_t *spe = (sas_protocol_expander_t *)spi;

		Printf(sdp, "\n");
		PrintDecimal(sdp, "Number of Expander Phy Descriptors", spe->number_expander_phy_descriptors, PNL);
		if (spe->reserved_byte1_b0_5 || sdp->DebugFlag) {
		    PrintHex(sdp, "Reserved (byte 1, bits 0:5)", spe->reserved_byte1_b0_5, PNL);
		}
		PrintDecimal(sdp, "The Descriptor Type", spe->descriptor_type, PNL);
		if (spe->reserved_byte2 || sdp->DebugFlag) {
		    PrintHex(sdp, "Reserved (byte 2)", spe->reserved_byte2, PNL);
		}
		if (spe->reserved_byte3 || sdp->DebugFlag) {
		    PrintHex(sdp, "Reserved (byte 3)", spe->reserved_byte3, PNL);
		} 
		PrintLongHexP(sdp, "The SAS Address", StoH(spe->sas_address), PNL);
		if (spe->number_expander_phy_descriptors) {
		    sas_expander_phy_descriptor_t *epd = (sas_expander_phy_descriptor_t *)((uint8_t *)spe + sizeof(*spe));
		    int phy;

		    for (phy = 0; phy < spe->number_expander_phy_descriptors; phy++, epd++) {
			Printf(sdp, "\n");
			PrintDecimal(sdp, "Expander Phy Descriptor", phy, PNL);
			PrintDecimal(sdp, "Connector Element Index", epd->connector_element_index, DNL);
			if (epd->connector_element_index == PHY_NOT_CONNECTED) {
			    Print(sdp, " (not attached to a connector)\n");
			} else {
			    Print(sdp, "\n");
			}
			PrintDecimal(sdp, "Other Element Index", epd->other_element_index, DNL);
			if (epd->other_element_index == PHY_NOT_CONNECTED) {
			    Print(sdp, " (not attached to a connector)\n");
			} else {
			    Print(sdp, "\n");
			}
		    }
		}
		return(status);
	    }
	    break;
	}
	case SES_PROTOCOL_IDENTIFIER_PCIe: {
	    pcie_protocol_information_t *ppi = (pcie_protocol_information_t *)((uint8_t *)aedp + sizeof(*aedp));
	    nvme_port_descriptor_t *npd = (nvme_port_descriptor_t *)((uint8_t *)ppi + sizeof(*ppi));
	    char serial_number[PCIe_SERIAL_NUMBER_LENGTH + 1];
	    char model_number[PCIe_MODEL_NUMBER_LENGTH + 1];
	    int port = 0;

	    Printf(sdp, "\n");
	    PrintDecimal(sdp, "Number of Ports", ppi->number_of_ports, PNL);
	    PrintDecimal(sdp, "Not All Ports", ppi->not_all_ports, PNL);
	    if (ppi->reserved_byte1_b1_4 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1, bits 1:4)", ppi->reserved_byte1_b1_4, PNL);
	    }
	    PrintDecimal(sdp, "PCIe Protocol Type", ppi->PCIe_protocol_type, DNL);
	    if (ppi->PCIe_protocol_type == PCIe_NVME) {
		Print(sdp, " (NVMe)\n");
	    } else {
		Print(sdp, " (unknown)\n");
	    }
	    if (ppi->reserved_byte2 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 2)", ppi->reserved_byte2, PNL);
	    }
	    PrintDecimal(sdp, "Device Slot Number", ppi->device_slot_number, PNL);
	    if (ppi->reserved_byte4 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 4)", ppi->reserved_byte4, PNL);
	    }
	    if (ppi->reserved_byte5 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 5)", ppi->reserved_byte5, PNL);
	    }
	    PrintHex(sdp, "PCIe Vendor ID", (uint32_t)StoH(ppi->pcie_vendor_id), PNL);

	    strncpy (serial_number, (char *)ppi->serial_number, PCIe_SERIAL_NUMBER_LENGTH);
	    serial_number[PCIe_SERIAL_NUMBER_LENGTH] = '\0';
	    PrintAscii(sdp, "Serial Number", serial_number, PNL);

	    strncpy (model_number, (char *)ppi->model_number, PCIe_MODEL_NUMBER_LENGTH);
	    model_number[PCIe_MODEL_NUMBER_LENGTH] = '\0';
	    PrintAscii(sdp, "Product Number", model_number, PNL);

	    for (port = 0; port < ppi->number_of_ports; port++) {
		Printf(sdp, "\n");
		PrintBoolean(sdp, False, "Controller ID Valid", npd->cid_valid, PNL);
		PrintBoolean(sdp, False, "Bus Device Function Valid", npd->bdf_valid, PNL);
		PrintBoolean(sdp, False, "Physical Slot Number Valid", npd->psn_valid, PNL);
		if (npd->reserved_byte0_b3_7 || sdp->DebugFlag) {
		    PrintHex(sdp, "Reserved (byte 0, bits 3:7)", npd->reserved_byte0_b3_7, PNL);
		}
		if (npd->cid_valid || sdp->DebugFlag) {
		    PrintDecimal(sdp, "The Controller ID", (uint16_t)StoH(npd->controller_id), PNL);
		}
		if (npd->reserved_byte3 || sdp->DebugFlag) {
		    PrintHex(sdp, "Reserved (byte 3)", npd->reserved_byte3, PNL);
		}
		if (npd->bdf_valid || sdp->DebugFlag) {
		    PrintDecimal(sdp, "The Bus Number", npd->bus_number, PNL);
		    PrintDecimal(sdp, "The Function Number", npd->function_number, PNL);
		    PrintDecimal(sdp, "The Device Number", npd->device_number, PNL);
		}
		if (npd->psn_valid || sdp->DebugFlag) {
		    uint16_t physical_slot_number = (npd->physical_slot_number_msb << 8) || npd->physical_slot_number_lsb;
		    PrintDecimal(sdp, "The Physical Slot Number", physical_slot_number, PNL);
		    if (npd->reserved_byte7_b5_7 || sdp->DebugFlag) {
			PrintHex(sdp, "Reserved (byte 7, bits 5:7)", npd->reserved_byte7_b5_7, PNL);
		    }
		}
	    }
	    return(status);
	}
	default:
	    break;
    }
    Wprintf(sdp, "Element type %s, protocol identifier %s (0x%02x), is NOT implemented yet!\n",
	    element_name, protocol_name, aedp->protocol_identifier);
    status = WARNING;
    return(status);
}

/*
 * SES Additional Element Status Page 0x0A in JSON Format.
 */
char *
ses_addl_element_status_to_json(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
				ses_addl_element_status_page_t *aesp, char *page_name)
{
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    JSON_Value  *tvalue = NULL;
    JSON_Object *tobject = NULL;
    JSON_Value  *dvalue = NULL;
    JSON_Object *dobject = NULL;
    JSON_Array	*desc_array;
    JSON_Value	*desc_value = NULL;
    JSON_Status json_status;
    char *json_string = NULL;
    ses_configuration_page_t *scp = NULL;
    ses_enclosure_descriptor_t *edp = NULL;
    ses_type_desc_header_t *tdp = NULL;
    ses_addl_element_status_descriptor_t *aedp = NULL;
    int	page_length = (int)StoH(aesp->page_length);
    int descriptor_length = 0;
    uint32_t generation_number = 0;
    uint64_t enclosure_logical_id = 0;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    uint8_t *ucp = NULL;
    int length = 0;
    int offset = 0;
    int toffset = 0;
    int i, addl_len = 0;
    int total_element_types = 0;
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

    ucp = (uint8_t *)aesp;
    length = sizeof(*aesp);
    json_status = json_object_set_number(object, "Length", (double)length);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Offset", (double)offset);
    if (json_status != JSONSuccess) goto finish;
    offset = FormatHexBytes(text, offset, ucp, length);
    json_status = json_object_set_string(object, "Bytes", text);
    if (json_status != JSONSuccess) goto finish;

    (void)sprintf(text, "0x%02x", aesp->page_code);
    json_status = json_object_set_string(object, "Page Code", text);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Reserved (byte 1)", (double)aesp->reserved_byte1);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Page Length", (double)page_length);
    if (json_status != JSONSuccess) goto finish;
    generation_number = (uint32_t)StoH(aesp->generation_number);
    json_status = json_object_set_number(object, "Generation Number", (double)generation_number);
    if (json_status != JSONSuccess) goto finish;

    /*
     * Request the configuration page, requird to decode the enclosure status page.
     */
    status = receive_diagnostic_page(sdp, iop, sgp, (void **)&scp, DIAG_CONFIGURATION_PAGE);
    if (status == FAILURE) goto finish;

    edp = (ses_enclosure_descriptor_t *)((uint8_t *)scp + sizeof(*scp));
    for (i = 0; i < (scp->secondary_enclosures + 1); i++) {
	total_element_types += edp->num_type_descriptor_headers;
	edp = (ses_enclosure_descriptor_t *)((uint8_t *)edp + edp->enclosure_descriptor_length + 4);
    }
    tdp = (ses_type_desc_header_t *)edp;
    tp = (char *)tdp + (total_element_types * sizeof(*tdp));

    aedp = (ses_addl_element_status_descriptor_t *)((uint8_t *)aesp + sizeof(*aesp));

    /*
     * Format: 
     *  Additional Element Status Descriptor
     *  Protocol Specific Information Descriptor
     */
    for (i = 0; (i < total_element_types); i++, tdp++) {
	char *element_name = get_element_type(tdp->element_type);
	char element_text[STRING_BUFFER_SIZE];
	uint8_t text_length = tdp->type_descriptor_text_length;
	int element_index = 0;

	if (valid_addl_element_types(tdp->element_type) == False) {
	    tp += text_length;
	    continue;
	}

	if ( (sdp->ses_element_type > 0) && (tdp->element_type != sdp->ses_element_type) ) {
	    tp += text_length;
	    /* Note: The element descriptors may be variable length! */
	    for (element_index = 0; (element_index < tdp->number_elements); element_index++) {
		descriptor_length = (int)aedp->addl_element_desc_length - 2;
		offset += sizeof(*aedp) + descriptor_length;
		aedp = (ses_addl_element_status_descriptor_t *)((uint8_t *)aedp + sizeof(*aedp) + descriptor_length);
	    }
	    continue;
	}
	if (tvalue == NULL) {
	    tvalue  = json_value_init_object();
	    tobject = json_value_get_object(tvalue);
	}

	(void)strncpy(element_text, tp, text_length);
	element_text[text_length] = '\0';

	json_status = json_object_set_string(tobject, "Element Type", element_name);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(tobject, "Element Type Code", tdp->element_type);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(tobject, "Number of Elements", tdp->number_elements);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(tobject, "Subenclosure Identifier", (double)tdp->subenclosure_identifier);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_string(tobject, "Element Text", element_text);
	if (json_status != JSONSuccess) goto finish;

	for (element_index = 0; (element_index < tdp->number_elements); element_index++) {

	    descriptor_length = (int)aedp->addl_element_desc_length - 2;
	    if ( (sdp->ses_element_flag == True) && (sdp->ses_element_index != element_index) ) {
		offset += sizeof(*aedp) + descriptor_length;
		aedp = (ses_addl_element_status_descriptor_t *)((uint8_t *)aedp + sizeof(*aedp) + descriptor_length);
		continue;
	    }
	    if (dvalue == NULL) {
		dvalue  = json_value_init_object();
		dobject = json_value_get_object(dvalue);
	    }
	    if (desc_value == NULL) {
		desc_value = json_value_init_array();
		desc_array = json_value_get_array(desc_value);
	    }

	    ucp = (uint8_t *)aedp;
	    length = sizeof(*aedp) + descriptor_length;
	    json_status = json_object_set_number(dobject, "Length", (double)length);
	    if (json_status != JSONSuccess) goto finish;
	    json_status = json_object_set_number(dobject, "Offset", (double)offset);
	    if (json_status != JSONSuccess) goto finish;
	    offset = FormatHexBytes(text, offset, ucp, length);
	    json_status = json_object_set_string(dobject, "Bytes", text);
	    if (json_status != JSONSuccess) goto finish;

	    json_status = json_object_set_number(dobject, "Element Descriptor", (double)element_index);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_string(dobject, "Element Text", element_text);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_number(dobject, "Protocol Identifier", (double)aedp->protocol_identifier);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_string(dobject, "Protocol Identifier Description",
						 find_ses_protocol_identifier(aedp->protocol_identifier));
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Element Index Present", aedp->eip);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 0, bits 5:6)", (double)aedp->reserved_byte0_b5_6);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Protocol Specific Information Invalid", aedp->invalid);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Additional Element Descriptor Length", (double)descriptor_length);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_boolean(dobject, "Element Index Includes Overall", aedp->eiioe);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 2, bits 2:7)", (double)aedp->reserved_byte2_b2_7);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Element Index", (double)aedp->element_index);
	    if (json_status != JSONSuccess) break;

	    if (aedp->invalid == False) {
		json_status = ses_protocol_specific_information_json(sdp, dobject, aedp, tdp->element_type);
		if (json_status != JSONSuccess) break;
	    }
	    aedp = (ses_addl_element_status_descriptor_t *)((uint8_t *)aedp + sizeof(*aedp) + descriptor_length);
	    json_array_append_value(desc_array, dvalue);
	    dvalue = NULL;
	}
	/* Finish this element type. */
	if (desc_value) {
	    json_object_set_value(tobject, "Descriptor List", desc_value);
	    desc_value = NULL;
	}
	json_status = json_object_dotset_value(object, element_name, tvalue);
	if (json_status != JSONSuccess) goto finish;
	tvalue = NULL;
	tp += text_length;
    }

finish:
    if (scp) {
	free_palign(sdp, scp);
    }
    (void)json_object_set_number(object, "JSON Status", (double)json_status);
    if (sdp->json_pretty) {
	json_string = json_serialize_to_string_pretty(root_value);
    } else {
	json_string = json_serialize_to_string(root_value);
    }
    json_value_free(root_value);
    return(json_string);
}

JSON_Status
ses_protocol_specific_information_json(scsi_device_t *sdp, JSON_Object *dobject,
				       ses_addl_element_status_descriptor_t *aedp, element_type_t element_type)
{
    char *element_name = get_element_type(element_type);
    char *protocol_name = find_ses_protocol_identifier(aedp->protocol_identifier);
    char text[STRING_BUFFER_SIZE];
    JSON_Status json_status = JSONSuccess;

    switch (aedp->protocol_identifier) {

	//case SES_PROTOCOL_IDENTIFIER_FC:
	//    break;

	case SES_PROTOCOL_IDENTIFIER_SAS: {
	    sas_protocol_information_t *spi = (sas_protocol_information_t *)((uint8_t *)aedp + sizeof(*aedp));

	    if ( (element_type == ELEMENT_TYPE_ARRAY_DEVICE_SLOT) && (spi->descriptor_type == SAS_DESCRIPTOR_TYPE0) ) {
		sas_protocol_array_t *spa = (sas_protocol_array_t *)spi;

		json_status = json_object_set_number(dobject, "Phy Descriptors", (double)spa->number_phy_descriptors);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_boolean(dobject, "Not All Phys", spa->not_all_phys);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Reserved (byte 1, bits 0:5)", (double)spa->reserved_byte1_b1_5);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Descriptor Type", (double)spa->descriptor_type);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Reserved (byte 2)", (double)spa->reserved_byte2);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Device Slot Number", (double)spa->device_slot_number);
		if (json_status != JSONSuccess) break;

		if (spa->number_phy_descriptors) {
		    sas_array_phy_descriptor_t *apd = (sas_array_phy_descriptor_t *)((uint8_t *)spa + sizeof(*spa));
		    char *sas_device_type = NULL;
		    JSON_Value  *jvalue = NULL;
		    JSON_Object *jobject = NULL;
		    JSON_Array	*phy_array;
		    JSON_Value	*phy_value = NULL;
		    int phy;

		    for (phy = 0; phy < spa->number_phy_descriptors; phy++, apd++) {
			if (jvalue == NULL) {
			    jvalue  = json_value_init_object();
			    jobject = json_value_get_object(jvalue);
			}
			if (phy_value == NULL) {
			    phy_value = json_value_init_array();
			    phy_array = json_value_get_array(phy_value);
			}

			json_status = json_object_set_number(jobject, "Phy Descriptor", (double)phy);
			if (json_status != JSONSuccess) break;

			json_status = json_object_set_number(jobject, "Reserved (byte 0, bits 0:3)", (double)apd->reserved_byte0_b0_3);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Device Type", (double)apd->device_type);
			if (json_status != JSONSuccess) break;
			sas_device_type = find_sas_device_type(apd->device_type);
			json_status = json_object_set_string(jobject, "Device Type Description", sas_device_type);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Reserved (byte 0, bit 7)", (double)apd->reserved_byte0_b7);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Reserved (byte 1)", (double)apd->reserved_byte1);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Reserved (byte 2, bit 0)", (double)apd->reserved_byte2_b0);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_boolean(jobject, "SMP Initiator Port", apd->smp_initiator_port);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_boolean(jobject, "STP Initiator Port", apd->stp_initiator_port);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_boolean(jobject, "SSP Initiator Port", apd->ssp_initiator_port);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Reserved (byte 2, bits 4:4)", (double)apd->reserved_byte2_b4_4);
			if (json_status != JSONSuccess) break;

			json_status = json_object_set_boolean(jobject, "SATA Device", apd->sata_device);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_boolean(jobject, "SMP Target Port", apd->smp_target_port);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_boolean(jobject, "STP Target Port", apd->stp_target_port);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_boolean(jobject, "SSP Target Port", apd->ssp_target_port);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Reserved (byte 3, bits 4:6)", (double)apd->reserved_byte3_b4_6);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_boolean(jobject, "SATA Port Selector", apd->sata_port_selector);
			if (json_status != JSONSuccess) break;

			(void)sprintf(text, LLFXFMT, StoH(apd->attached_sas_address));
			json_status = json_object_set_string(jobject, "Attached SAS Address", text);
			if (json_status != JSONSuccess) break;

			(void)sprintf(text, LLFXFMT, StoH(apd->sas_address));
			json_status = json_object_set_string(jobject, "SAS Address", text);
			if (json_status != JSONSuccess) break;

			json_status = json_object_set_number(jobject, "Phy Identifier", (double)apd->phy_identifier);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Reserved (byte 21)", (double)apd->reserved_byte21);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Reserved (byte 22)", (double)apd->reserved_byte22);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Reserved (byte 23)", (double)apd->reserved_byte23);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Reserved (byte 24)", (double)apd->reserved_byte24);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Reserved (byte 25)", (double)apd->reserved_byte25);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Reserved (byte 26)", (double)apd->reserved_byte26);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Reserved (byte 27)", (double)apd->reserved_byte27);
			if (json_status != JSONSuccess) break;

			json_array_append_value(phy_array, jvalue);
			jvalue = NULL;
		    }
		    if (phy_value) {
			json_object_set_value(dobject, "Descriptor List", phy_value);
			phy_value = NULL;
		    }
		}
		return(json_status);
	    } else if ( (element_type == ELEMENT_TYPE_ESCE) && (spi->descriptor_type == SAS_DESCRIPTOR_TYPE1) ) {
		sas_protocol_esce_t *spe = (sas_protocol_esce_t *)spi;

		json_status = json_object_set_number(dobject, "Phy Descriptors", (double)spe->number_phy_descriptors);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Reserved (byte 1, bits 0:5)", (double)spe->reserved_byte1_b0_5);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Descriptor Type", (double)spe->descriptor_type);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Reserved (byte 2)", (double)spe->reserved_byte2);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Reserved (byte 3)", (double)spe->reserved_byte3);
		if (json_status != JSONSuccess) break;

		if (spe->number_phy_descriptors) {
		    sas_esce_phy_descriptor_t *epd = (sas_esce_phy_descriptor_t *)((uint8_t *)spe + sizeof(*spe));
		    JSON_Value  *jvalue = NULL;
		    JSON_Object *jobject = NULL;
		    JSON_Array	*phy_array;
		    JSON_Value	*phy_value = NULL;
		    int phy;

		    for (phy = 0; phy < spe->number_phy_descriptors; phy++, epd++) {
			if (jvalue == NULL) {
			    jvalue  = json_value_init_object();
			    jobject = json_value_get_object(jvalue);
			}
			if (phy_value == NULL) {
			    phy_value = json_value_init_array();
			    phy_array = json_value_get_array(phy_value);
			}
			json_status = json_object_set_number(jobject, "Phy Descriptor", (double)phy);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Phy Identifier", (double)epd->phy_identifier);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Reserved (byte 1)", (double)epd->reserved_byte1);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Connector Element Index", (double)epd->connector_element_index);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Other Element Index", (double)epd->other_element_index);
			if (json_status != JSONSuccess) break;
			(void)sprintf(text, LLFXFMT, StoH(epd->sas_address));
			json_status = json_object_set_string(jobject, "SAS Address", text);
			if (json_status != JSONSuccess) break;

			json_array_append_value(phy_array, jvalue);
			jvalue = NULL;
		    }
		    if (phy_value) {
			json_object_set_value(dobject, "Descriptor List", phy_value);
			phy_value = NULL;
		    }
		}
		return(json_status);
	    } else if ( (element_type == ELEMENT_TYPE_SAS_EXPANDER) && (spi->descriptor_type == SAS_DESCRIPTOR_TYPE1) ) {
		sas_protocol_expander_t *spe = (sas_protocol_expander_t *)spi;

		json_status = json_object_set_number(dobject, "Expander Phy Descriptors", (double)spe->number_expander_phy_descriptors);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Reserved (byte 1, bits 0:5)", (double)spe->reserved_byte1_b0_5);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Descriptor Type", (double)spe->descriptor_type);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Reserved (byte 2)", (double)spe->reserved_byte2);
		if (json_status != JSONSuccess) break;
		if (spe->reserved_byte3 || sdp->DebugFlag) {
		    json_status = json_object_set_number(dobject, "Reserved (byte 3)", (double)spe->reserved_byte3);
		    if (json_status != JSONSuccess) break;
		} 
		(void)sprintf(text, LLFXFMT, StoH(spe->sas_address));
		json_status = json_object_set_string(dobject, "SAS Address", text);
		if (json_status != JSONSuccess) break;

		if (spe->number_expander_phy_descriptors) {
		    sas_expander_phy_descriptor_t *epd = (sas_expander_phy_descriptor_t *)((uint8_t *)spe + sizeof(*spe));
		    JSON_Value  *jvalue = NULL;
		    JSON_Object *jobject = NULL;
		    JSON_Array	*phy_array;
		    JSON_Value	*phy_value = NULL;
		    int phy;

		    for (phy = 0; phy < spe->number_expander_phy_descriptors; phy++, epd++) {
			if (jvalue == NULL) {
			    jvalue  = json_value_init_object();
			    jobject = json_value_get_object(jvalue);
			}
			if (phy_value == NULL) {
			    phy_value = json_value_init_array();
			    phy_array = json_value_get_array(phy_value);
			}
			json_status = json_object_set_number(jobject, "Expander Phy Descriptor", (double)phy);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Connector Element Index", (double)epd->connector_element_index);
			if (json_status != JSONSuccess) break;
			json_status = json_object_set_number(jobject, "Other Element Index", (double)epd->other_element_index);
			if (json_status != JSONSuccess) break;

			json_array_append_value(phy_array, jvalue);
			jvalue = NULL;
		    }
		    if (phy_value) {
			json_object_set_value(dobject, "Descriptor List", phy_value);
			phy_value = NULL;
		    }
		}
		return(json_status);
	    }
	    break;
	}
	case SES_PROTOCOL_IDENTIFIER_PCIe: {
	    pcie_protocol_information_t *ppi = (pcie_protocol_information_t *)((uint8_t *)aedp + sizeof(*aedp));
	    nvme_port_descriptor_t *npd = (nvme_port_descriptor_t *)((uint8_t *)ppi + sizeof(*ppi));
	    char serial_number[PCIe_SERIAL_NUMBER_LENGTH + 1];
	    char model_number[PCIe_MODEL_NUMBER_LENGTH + 1];
	    int port = 0;

	    json_status = json_object_set_number(dobject, "Number of Ports", ppi->number_of_ports);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Not All Ports", ppi->not_all_ports);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 1, bits 1:4)", (double)ppi->reserved_byte1_b1_4);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "PCIe Protocol Type", (double)ppi->PCIe_protocol_type);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_string(dobject, "PCIe Protocol Type Description",
						 (ppi->PCIe_protocol_type == PCIe_NVME) ? "NVMe": "unknown");
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 2)", (double)ppi->reserved_byte2);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Device Slot Number", (double)ppi->device_slot_number);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 4)", (double)ppi->reserved_byte4);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "Reserved (byte 5)", ppi->reserved_byte5);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(dobject, "PCIe Vendor ID", (uint32_t)StoH(ppi->pcie_vendor_id));
	    if (json_status != JSONSuccess) break;

	    strncpy (serial_number, (char *)ppi->serial_number, PCIe_SERIAL_NUMBER_LENGTH);
	    serial_number[PCIe_SERIAL_NUMBER_LENGTH] = '\0';
	    json_status = json_object_set_string(dobject, "Serial Number", serial_number);
	    if (json_status != JSONSuccess) break;

	    strncpy (model_number, (char *)ppi->model_number, PCIe_MODEL_NUMBER_LENGTH);
	    model_number[PCIe_MODEL_NUMBER_LENGTH] = '\0';
	    json_status = json_object_set_string(dobject, "Product Number", model_number);
	    if (json_status != JSONSuccess) break;

	    for (port = 0; port < ppi->number_of_ports; port++) {
		json_status = json_object_set_boolean(dobject, "Controller ID Valid", npd->cid_valid);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_boolean(dobject, "Bus Device Function Valid", npd->bdf_valid);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_boolean(dobject, "Physical Slot Number Valid", npd->psn_valid);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Reserved (byte 0, bits 3:7)", (double)npd->reserved_byte0_b3_7);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "The Controller ID", (double)StoH(npd->controller_id));
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Reserved (byte 3)", (double)npd->reserved_byte3);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "The Bus Number", (double)npd->bus_number);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "The Function Number", (double)npd->function_number);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "The Device Number", (double)npd->device_number);
		if (json_status != JSONSuccess) break;
		uint16_t physical_slot_number = (npd->physical_slot_number_msb << 8) || npd->physical_slot_number_lsb;
		json_status = json_object_set_number(dobject, "The Physical Slot Number", (double)physical_slot_number);
		if (json_status != JSONSuccess) break;
		json_status = json_object_set_number(dobject, "Reserved (byte 7, bits 5:7)", (double)npd->reserved_byte7_b5_7);
		if (json_status != JSONSuccess) break;
	    }
	    return(json_status);
	}
	default:
	    break;
    }
    /* If we decoded, we should never reach here except for a JSON error! */
    if (json_status == JSONSuccess) {
	/* Note: Multiple warnings will create invalid JSON! */
	(void)sprintf(text, "Element type %s, protocol identifier %s (0x%02x), is NOT implemented yet!",
		      element_name, protocol_name, aedp->protocol_identifier);
	json_status = json_object_set_string(dobject, "Warning", text);
    }
    return(json_status);
}

/* ============================================================================================== */
/*
 * SES Download Microcode Status Page 0x0E:
 */
int
ses_download_microcode_receive_diagnostic_decode(scsi_device_t *sdp, io_params_t *iop,
						 scsi_generic_t *sgp, diagnostic_page_header_t *dph)
{
    ses_download_microcode_page_t *dmp = (ses_download_microcode_page_t *)dph;
    ses_download_microcode_descriptor_t *dmdp = NULL;
    int	page_length = (int)StoH(dmp->page_length);
    uint32_t generation_number = 0;
    char *msg = NULL;
    char *tp = NULL;
    uint32_t value32 = 0;
    int i, offset = 0;
    int status = SUCCESS;

    if (sdp->output_format == JSON_FMT) {
	char *json_string = NULL;
	json_string = ses_download_microcode_status_to_json(sdp, iop, dmp, "Download Microcode Status");
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }

    PrintHeader(sdp, "Download Microcode Status Diagnostic Page");

    offset = PrintHexDebug(sdp, offset, (uint8_t *)dmp, sizeof(*dmp));
    PrintHex(sdp, "Page Code", dmp->page_code, PNL); 
    PrintDecimal(sdp, "Number of Secondary Enclosures", dmp->secondary_enclosures, PNL);
    PrintDecHex(sdp, "Page Length", page_length, PNL);
    generation_number = (uint32_t)StoH(dmp->generation_number);
    PrintHex(sdp, "Generation Number", generation_number, PNL);

    dmdp = (ses_download_microcode_descriptor_t *)((uint8_t *)dmp + sizeof(*dmp));

    /*
     * Display the primary and secondary enclosure information.
     */
    for (i = 0; i < (dmp->secondary_enclosures + 1); i++) {
	Printf(sdp, "\n");
	offset = PrintHexDebug(sdp, offset, (uint8_t *)dmdp, sizeof(*dmdp));
	PrintAscii(sdp, "Download Microcode Descriptor List", "", PNL);
	PrintLongDec(sdp, "Subenclosure Identifier", dmdp->subenclosure_identifier, DNL);
	if (dmdp->subenclosure_identifier == 0) {
	    Print(sdp, " (Primary)\n");
	} else {
	    Print(sdp, " (Secondary)\n");
	}
	PrintHex(sdp, "Download Microcode Status", dmdp->download_microcode_status, PNL);
	msg = get_download_microcode_status( (ses_download_status_t)dmdp->download_microcode_status );
	PrintAscii(sdp, "Download Microcode Status Message", msg, PNL);
	PrintHex(sdp, "Download Additional Status", dmdp->download_additional_status, PNL);
	PrintDecimal(sdp, "Download Microcode Maximum Size", (uint32_t)StoH(dmdp->microcode_maximim_size), DNL);
	Print(sdp, " bytes\n");
	if (dmdp->reserved_byte8 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 8)", dmdp->reserved_byte8, PNL);
	}
	if (dmdp->reserved_byte9 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 9)", dmdp->reserved_byte9, PNL);
	}
	if (dmdp->reserved_byte10 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 10)", dmdp->reserved_byte10, PNL);
	}
	PrintHex(sdp, "Download Expected Buffer ID", dmdp->download_expected_buffer_id, PNL);
	PrintDecimal(sdp, "Download Expected Buffer Offset", (uint32_t)StoH(dmdp->download_expected_buffer_offset), PNL);
	dmdp++;
    }
    Printf(sdp, "\n");
    return(status);
}

typedef struct microcode_status_entry {
    ses_download_status_t microcode_status;
    char *dms_message;
} microcode_status_entry_t;

microcode_status_entry_t microcode_status_table[] =
{
    /* Interim Status Codes */
    { DMS_NO_OPERATION_IN_PROGRESS,
      "No download microcode is in progress." },
    { DMS_OPERATION_IS_IN_PROGRESS,
      "A download microcode is in progress." },
    { DMS_COMPLETE_UPDATE_NONVOLATILE,
      "Download complete, updating nonvolatile storage." },
    { DMS_UPDATING_NONVOLATILE_DEFERRED_MICROCODE,
     "Updating nonvolatile w/deferred microcode." },

    /* Completed with No Error Codes */
    { DMS_COMPLETE_NO_ERROR_STARTING,
      "Download complete, no errorr, start using now." },
    { DMS_COMPLETE_NO_ERROR_START_AFTER_RESET_POWER_CYCLE,
      "Download complete, no error, start using after reset or power cycle." },
    { DMS_COMPLETE_NO_ERROR_START_AFTER_POWER_CYCLE,
      "Download complete, no error, start using after power cycle."  },
    { DMS_COMPLETE_NO_ERROR_START_AFTER_ACTIVATE_MC,
      "Download complete, no error, start after activate MC, reset, or power cycle." },

    /* Completed with Error Codes */
    { DMS_DOWNLOAD_ERROR_MICROCODE_DISCARDED,
      "Download error, microcode discarded." },
    { DMS_MICROCODE_IMAGE_ERROR_DISCARDED,
      "Microcode image error, microcode discarded." },
    { DMS_DOWNLOAD_TIMEOUT_MICROCODE_DISCARDED,
      "Download timeout, microcode discarded." },
    { DMS_INTERNAL_ERROR_NEW_MICROCODED_NEEDED,
      "Internal error, new microcode needed before reset." },
    { DMS_INTERNAL_ERROR_HARD_RESET_POWER_ON_SAFE,
      "Internal error, hard reset and power on safe." },
    { DMS_PROCESSED_ACTIVATE_DEFERRED_MICROCODE,
      "Processed activate deferred microcode." }
};
int num_microcode_status_entries = ( sizeof(microcode_status_table) / sizeof(microcode_status_entry_t) );

char *
get_download_microcode_status(ses_download_status_t microcode_status)
{
    microcode_status_entry_t *mse = microcode_status_table;
    int i;

    for (i = 0; i < num_microcode_status_entries; i++, mse++) {
	if (mse->microcode_status == microcode_status) {
	    return( mse->dms_message);
	}
    }
    return ( "<reserved or vendor unique microcode state>");
}

/* Note: Needs updated, cloned string-in! */
char *
ses_download_microcode_status_to_json(scsi_device_t *sdp, io_params_t *iop,
				      ses_download_microcode_page_t *dmp, char *page_name)
{
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    JSON_Value  *svalue = NULL;
    JSON_Object *sobject = NULL;
    JSON_Array	*enc_array;
    JSON_Value	*enc_value = NULL;
    JSON_Status json_status;
    char *json_string = NULL;
    ses_download_microcode_descriptor_t *dmdp = NULL;
    int	page_length = (int)StoH(dmp->page_length);
    uint32_t generation_number = 0;
    char *msg = NULL;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    uint8_t *ucp = NULL;
    int i;
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

    /*
     * Note: Since we cannot have duplicate keys, format all hex bytes.
     */
    ucp = (uint8_t *)dmp; 
    length = sizeof(*dmp);
    json_status = json_object_set_number(object, "Length", (double)length);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Offset", (double)offset);
    if (json_status != JSONSuccess) goto finish;
    offset = FormatHexBytes(text, offset, ucp, length);
    json_status = json_object_set_string(object, "Bytes", text);
    if (json_status != JSONSuccess) goto finish;

    (void)sprintf(text, "0x%02x", dmp->page_code);
    json_status = json_object_set_string(object, "Page Code", text);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Secondary Enclosures", (double)dmp->secondary_enclosures);
    if (json_status != JSONSuccess) goto finish;
    generation_number = (uint32_t)StoH(dmp->generation_number);
    json_status = json_object_set_number(object, "Page Length", (double)page_length);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Generation Number", (double)generation_number);
    if (json_status != JSONSuccess) goto finish;

    dmdp = (ses_download_microcode_descriptor_t *)((uint8_t *)dmp + sizeof(*dmp));

    /*
     * Display the primary and secondary enclosure information.
     */
    for (i = 0; i < (dmp->secondary_enclosures + 1); i++) {
	if (enc_value == NULL) {
	    enc_value = json_value_init_array();
	    enc_array = json_value_get_array(enc_value);
	}
	if (svalue == NULL) {
	    svalue  = json_value_init_object();
	    sobject = json_value_get_object(svalue);
	}

	ucp = (uint8_t *)dmdp;
	length = sizeof(*dmdp);
	json_status = json_object_set_number(sobject, "Length", (double)length);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(sobject, "Offset", (double)offset);
	if (json_status != JSONSuccess) break;
	offset = FormatHexBytes(text, offset, ucp, length);
	json_status = json_object_set_string(sobject, "Bytes", text);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(sobject, "Subenclosure Identifier", (double)dmdp->subenclosure_identifier);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(sobject, "Download Microcode Status", (double)dmdp->download_microcode_status);
	if (json_status != JSONSuccess) break;

	msg = get_download_microcode_status( (ses_download_status_t)dmdp->download_microcode_status );
	json_status = json_object_set_string(sobject, "Download Microcode Status Message", msg);

	json_status = json_object_set_number(sobject, "Download Additional Status", (double)dmdp->download_additional_status);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(sobject, "Download Microcode Maximum Size", (double)StoH(dmdp->microcode_maximim_size));
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(sobject, "Reserved (byte 8)", (double)dmdp->reserved_byte8);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(sobject, "Reserved (byte 9)", (double)dmdp->reserved_byte9);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(sobject, "Reserved (byte 10)", (double)dmdp->reserved_byte10);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(sobject, "Download Expected Buffer ID", (double)dmdp->download_expected_buffer_id);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(sobject, "Download Expected Buffer Offset", (double)StoH(dmdp->download_expected_buffer_offset));
	if (json_status != JSONSuccess) break;

	dmdp++;
	json_array_append_value(enc_array, svalue);
	svalue = NULL;
    }
    if (enc_value) {
	json_object_set_value(object, "Download Microcode Descriptor List", enc_value);
	enc_value = NULL;
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

/*
 * This function sets up the initial send diagnostic parameters. It's expected 
 * the user will specify the page and parameter out bytes to send. 
 */
int
setup_send_diagnostic(scsi_device_t *sdp, scsi_generic_t *sgp, uint8_t page)
{
    sdp->page_code = page;
    sgp->data_length = 0;
    /* Setup to execute a CDB operation. */
    sdp->op_type = SCSI_CDB_OP;
    sdp->encode_flag = True;
    sgp->cdb[0] = (uint8_t)SOPC_SEND_DIAGNOSTIC;
    sgp->cdb_size = GetCdbLength(sgp->cdb[0]);
    sgp->data_dir = scsi_data_write; /* Sending parameter data to device. */
    return(SUCCESS);
}

int
send_diagnostic_page(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
		     void *data_buffer, size_t data_length, uint8_t page)
{
    scsi_generic_t *ssgp = Malloc(sdp, sizeof(*sgp));
    send_diagnostic_cdb_t *cdb;
    diagnostic_page_header_t *dph;
    int status;

    if (ssgp == NULL) return(FAILURE);
    *ssgp = *sgp;
    /* Setup for send diagnostic. */
    cdb = (send_diagnostic_cdb_t *)ssgp->cdb;
    memset(cdb, '\0', sizeof(*cdb));
    cdb->opcode = (uint8_t)SOPC_SEND_DIAGNOSTIC;
    cdb->pf = sdp->page_format;
    ssgp->cdb_size = GetCdbLength(cdb->opcode);
    ssgp->data_dir = scsi_data_write;
    ssgp->data_buffer = data_buffer;
    ssgp->data_length = (unsigned int)data_length;
    HtoS(cdb->parameter_length, data_length);
    dph = (diagnostic_page_header_t *)data_buffer;
    dph->page_code = page;
    if (data_length) {
	uint16_t data_size = (uint16_t)data_length;
	/* The page length does not include the page header. */
	data_size -= sizeof(*dph);
	HtoS(dph->page_length, data_size);
    }
    ssgp->cdb_name = "Send Diagnostic";
    status = libExecuteCdb(ssgp);
    free(ssgp);
    return(status);
}

int
send_diagnostic_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    send_diagnostic_cdb_t *cdb;
    diagnostic_page_header_t *dph;
    int status = SUCCESS;

    /*
     * The first time, we will allocate and initialize the page 
     * header, so the user does not need to specify these bytes. 
     */
    if (iop->first_time) {
	char *data = sgp->data_buffer;
	size_t dsize = sgp->data_length;
	size_t data_size = dsize + sizeof(*dph);
	void *data_buffer;

	data_buffer = malloc_palign(sdp, data_size, 0);
	if (data_buffer == NULL) {
	    return(FAILURE);
	}
	sgp->data_dir = iop->sop->data_dir;
	sgp->data_length = (unsigned int)data_size;
	sgp->data_buffer = data_buffer;
	/* Copy user data to after the diagnostic header. */
	memcpy((char *)data_buffer+sizeof(*dph), data, dsize);
	free_palign(sdp, data);
	iop->first_time = False;
    }

    /* 
     * Note to Self:
     * Sending requires the page header plus the page data. 
     * The CDB length is this page header plus the page data. 
     * The page header contains the page code and data length. 
     */
    cdb = (send_diagnostic_cdb_t *)sgp->cdb;
    cdb->pf = sdp->page_format;
    HtoS(cdb->parameter_length, sgp->data_length);
    dph = (diagnostic_page_header_t *)sgp->data_buffer;
    if (sdp->page_code) {
	dph->page_code = sdp->page_code;
    }
    if (sgp->data_length) {
	uint16_t data_size = sgp->data_length;
	/* The page length does not include the page header. */
	data_size -= sizeof(*dph);
	HtoS(dph->page_length, data_size);
    }
    return (status);
}

int
send_diagnostic_decode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    int status = SUCCESS;

    return(status);
}

/* ============================================================================================== */

/*
 * Utility Functions:
 */

/*
 * Diagnostic Page Lookup Functions:
 */
typedef struct diagnostic_page_entry {
    uint8_t		page_code;	/* The page code. */
    uint16_t		device_type;	/* THe device type (from Inquiry) */
    vendor_id_t		vendor_id;	/* The vendor ID (internal) */
    char		*page_name;	/* The page name. */
    char		*parse_name;	/* The parse name. */
} diagnostic_page_entry_t;

diagnostic_page_entry_t diagnostic_page_table[] = {
    { DIAG_SUPPORTED_PAGES, 		ALL_DEVICE_TYPES,	VID_ALL,	"Supported",		"supported"		},
    { DIAG_CONFIGURATION_PAGE,		DTYPE_ENCLOSURE,	VID_ALL,	"Configuration",	"configuration"		},
    { DIAG_ENCLOSURE_CONTROL_PAGE,	DTYPE_ENCLOSURE,	VID_ALL,	"Enclosure Control/Status", "enclosure"		},
    { DIAG_HELP_TEXT_PAGE,		DTYPE_ENCLOSURE,	VID_ALL,	"Help",			"help"			},
    { DIAG_STRING_IN_OUT_PAGE,		DTYPE_ENCLOSURE,	VID_ALL,	"String In/Out",	"string"		},
    { DIAG_THRESHOLD_IN_OUT_PAGE,	DTYPE_ENCLOSURE,	VID_ALL,	"Threshold In/Out",	"threshold"		},
    { DIAG_ELEMENT_DESCRIPTOR_PAGE,	DTYPE_ENCLOSURE,	VID_ALL,	"Element Descriptor",	"element"		},
    { DIAG_SHORT_ENCLOSURE_STATUS_PAGE,	DTYPE_ENCLOSURE,	VID_ALL,	"Short Enclosure Status", "short_enclosure"	},
    { DIAG_ENCLOSURE_BUSY_PAGE,		DTYPE_ENCLOSURE,	VID_ALL,	"Enclosure Busy",	"enclosure_busy"	},
    { DIAG_ADDL_ELEMENT_STATUS_PAGE,	DTYPE_ENCLOSURE,	VID_ALL,	"Additional Element Status", "addl_element_status" },
    { DIAG_SUBENCLOSURE_HELP_TEXT_PAGE,	DTYPE_ENCLOSURE,	VID_ALL,	"Subenclosure Help Text", "subenc_help_text"	},
    { DIAG_SUBENCLOSURE_STRING_IN_OUT_PAGE, DTYPE_ENCLOSURE,	VID_ALL,	"Subenclosure String In/Out", "subenc_string"	},
    { DIAG_SES_DIAGNOSTIC_PAGES_PAGE,	DTYPE_ENCLOSURE,	VID_ALL,	"SES Diagnostic Pages",	"ses_diagnostic"	},
    { DIAG_DOWNLOAD_MICROCODE_CONTROL_PAGE, DTYPE_ENCLOSURE,	VID_ALL,	"Download Microcode Control/Status", "download"	},
    { DIAG_SUBENCLOSURE_NICKNAME_CONTROL_PAGE, DTYPE_ENCLOSURE,	VID_ALL,	"Subenclosure Nickname Control/Status", "subenc_nickname" },
    /*
     * Celestica Vendor Specific Enclosure Diagnostic Pages:
     */
    { DIAG_CLI_OVER_SES_CONTROL_PAGE,	DTYPE_ENCLOSURE,	VID_CELESTICA,	"CLI Over SES Control/Status", "cls_cli"	},
    { DIAG_TIMESTAMP_GET_SET_PAGE,	DTYPE_ENCLOSURE,	VID_CELESTICA,	"Timestamp Get/Set",	"cls_timestamp"		},
    { DIAG_VPD_CONTROL_PAGE,		DTYPE_ENCLOSURE,	VID_CELESTICA,	"VPD Control/Status",	"cls_vpd"		},
    { DIAG_LOG_CONTROL_PAGE,		DTYPE_ENCLOSURE,	VID_CELESTICA,	"Log Control/status",	"cls_log"		},
    { DIAG_PHY_CONTROL_PAGE,		DTYPE_ENCLOSURE,	VID_CELESTICA,	"Phy Control/Status",	"cls_phy"		},
    { DIAG_ERROR_INJECTION_CONTROL_PAGE, DTYPE_ENCLOSURE,	VID_CELESTICA,	"Error Injection Control/Status", "cls_error"	},
    { DIAG_STATE_PRESERVATION_STATUS_PAGE, DTYPE_ENCLOSURE,	VID_CELESTICA,	"State Preservation Status", "cls_preservation"	},
    /*
     * Direct Access (disk) Diagnostic Pages:
     */
    //{ DIAG_TRANS_ADDR_PAGE,		DTYPE_OPTICAL,		VID_ALL,	"Translate Address",	"translate"		},
    { DIAG_TRANS_ADDR_PAGE,		DTYPE_DIRECT,		VID_ALL,	"Translate Address",	"translate"		}
};
int num_diagnostic_page_entries = ( sizeof(diagnostic_page_table) / sizeof(diagnostic_page_entry_t) );

uint8_t
find_diagnostic_page_code(scsi_device_t *sdp, char *page_name, int *status)
{
    diagnostic_page_entry_t *dpe = diagnostic_page_table;
    size_t length = strlen(page_name);
    int i;

    if (length == 0) {
	Printf(sdp, "\n");
	Printf(sdp, "Diagnostic Page Codes/Names:\n");
	for (i = 0; i < num_diagnostic_page_entries; i++, dpe++) {
	    Printf(sdp, "    0x%02x - %s (%s)\n",
		   dpe->page_code, dpe->page_name, dpe->parse_name);
	}
	Printf(sdp, "\n");
	*status = WARNING;
	return(DIAGNOSTIC_PAGE_UNKNOWN);
    }

    /*
     * Note: Need to add device type and vendor ID checks, when implemented.
     */
    for (i = 0; i < num_diagnostic_page_entries; i++, dpe++) {
	/* Allow a matching a portion (start of string). */
	if (strncasecmp(dpe->parse_name, page_name, length) == 0) {
	    *status = SUCCESS;
	    return( dpe->page_code );
	}
    }
    *status = FAILURE;
    return(DIAGNOSTIC_PAGE_UNKNOWN);
}

char *
get_diagnostic_page_name(uint8_t device_type, uint8_t page_code, uint8_t vendor_id)
{
    diagnostic_page_entry_t *dpe = diagnostic_page_table;
    char *page_name = NULL;
    int i;

    for (i = 0; i < num_diagnostic_page_entries; i++, dpe++) {
	if ( ((dpe->device_type == ALL_DEVICE_TYPES) || (dpe->device_type == device_type)) &&
	     (dpe->page_code == page_code) &&
	     ((dpe->vendor_id == VID_ALL) || (dpe->vendor_id == vendor_id)) ) {
	    return( dpe->page_name );
	}
    }
    if (page_code < DIAG_RESERVED_START) {
	page_name = "Unknown";
    } else if (page_code >= DIAG_VENDOR_START) {
	page_name = "Vendor Specific";
    } else {
	page_name = "Reserved";
    }
    return(page_name);
}

/*
 * Element Type Lookup Functions:
 */
typedef struct element_type_entry {
    element_type_t	element_type;
    char		*element_name;
    char		*parse_name;
} element_type_entry_t;

element_type_entry_t element_type_table[] = {
    { ELEMENT_TYPE_UNSPECIFIED, 	"Unspecified",		"unspecified"			},
    { ELEMENT_TYPE_DEVICE_SLOT, 	"Device Slot",		"device_slot"			},
    { ELEMENT_TYPE_POWER_SUPPLY, 	"Power Supply",		"power_supply"			},
    { ELEMENT_TYPE_COOLING, 		"Cooling",		"cooling"			},
    { ELEMENT_TYPE_SENSOR_TEMPERATURE, 	"Temperature Sensor",	"temperature"			},
    { ELEMENT_TYPE_DOOR, 		"Door",			"door"				},
    { ELEMENT_TYPE_AUDIBLE_ALARM, 	"Audible Alarm",	"audible_alarm"			},
    { ELEMENT_TYPE_ESCE, 		"Enclosure Services Controller Electronics", "esce"	},
    { ELEMENT_TYPE_SCC_CTRL_ELECTRONICS,"SCC Controller Electrons", "scc_controller"		},
    { ELEMENT_TYPE_NONVOLATILE_CACHE, 	"Nonvolatile Cache",	"nonvolotile_cache"		},
    { ELEMENT_TYPE_INVALID_OPER_REASON, "Invalid Operation Reason", "invalid_operation"		},
    { ELEMENT_TYPE_UNINT_POWER_SUPPLY, 	"Uninterruptable Power Supply", "unint_power_supply"	},
    { ELEMENT_TYPE_DISPLAY, 		"Display",		"display"			},
    { ELEMENT_TYPE_KEY_PAD_ENTRY, 	"Key Pad Entry",	"keypad_entry"			},
    { ELEMENT_TYPE_ENCLOSURE, 		"Enclosure",		"enclosure"			},
    { ELEMENT_TYPE_SCSI_PORT_TRANS,	"SCSI Port Transceiver","scsi_port_transceiver"		},
    { ELEMENT_TYPE_LANGUAGE, 		"Language",		"language"			},
    { ELEMENT_TYPE_COMMUNICATION_PORT, 	"Communication Port",	"communication_port"		},
    { ELEMENT_TYPE_VOLTAGE_SENSOR, 	"Voltage Sensor",	"voltage_sensor"		},
    { ELEMENT_TYPE_CURRENT_SENSOR, 	"Current Sensor",	"current_sensor"		},
    { ELEMENT_TYPE_SCSI_TARGET_PORT, 	"SCSI Target Port",	"scsi_target_port"		},
    { ELEMENT_TYPE_SCSI_INITIATOR_PORT, "SCSI Initiator Port",	"scsi_initiator_port"		},
    { ELEMENT_TYPE_SIMPLE_SUBENCLOSURE, "Simple Enclosure",	"simple_enclosure"		},
    { ELEMENT_TYPE_ARRAY_DEVICE_SLOT, 	"Array Device Slot",	"array_device_slot"		},
    { ELEMENT_TYPE_SAS_EXPANDER, 	"SAS Expander",		"sas_expander"			},
    { ELEMENT_TYPE_SAS_CONNECTOR, 	"SAS Connector",	"sas_connector"			}
};
int num_element_type_entries = ( sizeof(element_type_table) / sizeof(element_type_entry_t) );

char *
get_element_type(element_type_t element_type)
{
    if ((int)element_type < num_element_type_entries) {
	return( element_type_table[element_type].element_name );
    } else if (element_type <= ELEMENT_TYPE_RESERVED_END) {
	return( "Reserved" );
    } else if (element_type <= ELEMENT_TYPE_VENDOR_END) {
	return( "Vendor specific" );
    }
    return(NULL);
}

element_type_t
find_element_type(scsi_device_t *sdp, char *element_type, int *status)
{
    element_type_entry_t *etp = element_type_table;
    size_t length = strlen(element_type);
    int i;

    if (length == 0) {
	Printf(sdp, "\n");
	Printf(sdp, "Element Type Codes/Names:\n");
	for (i = 0; i < num_element_type_entries; i++, etp++) {
	    Printf(sdp, "    0x%02x - %s (%s)\n",
		   etp->element_type, etp->element_name, etp->parse_name);
	}
	Printf(sdp, "\n");
	*status = WARNING;
	return(ELEMENT_TYPE_UNINITIALIZED);
    }

    for (i = 0; i < num_element_type_entries; i++, etp++) {
	/* Allow a matching a portion (start of string). */
	// (strncasecmp(etp->element_name, element_type, length) == 0) ) {
	if (strncasecmp(etp->parse_name, element_type, length) == 0) {
	    *status = SUCCESS;
	    return( etp->element_type );
	}
    }
    *status = FAILURE;
    return(ELEMENT_TYPE_UNINITIALIZED);
}

/*
 * Element Status Lookup Functions: 
 */
typedef struct element_status_entry {
    element_status_t	element_status;
    char		*element_status_name;
    char		*element_status_desc;
} element_status_entry_t;

element_status_entry_t element_status_table[] = {
    { ELEMENT_STATUS_UNSUPPORTED,	"Unsupported",
	"Status detection not implemented for this element."		},
    { ELEMENT_STATUS_OK,		"OK",
	"Element is installed and no error conditions are known."	},
    { ELEMENT_STATUS_CRITICAL,		"Critical",
	"Critical condition is detected."				},
    { ELEMENT_STATUS_NON_CRITICAL,	"Non-Critical",
	"Noncritical condition is detected."				},
    { ELEMENT_STATUS_UNRECOVERABLE,	"Unrecoverable",
	"Unrecoverable condition is detected."				},
    { ELEMENT_STATUS_NOT_INSTALLED,	"Not Installed",
	"Element is not installed in enclosure."			},
    { ELEMENT_STATUS_UNKNOWN,		"Unknown",
	"Sensor has failed or element status is not available."		},
    { ELEMENT_STATUS_NOT_AVAILABLE,	"Not Available",
	"Element has not been turned on or set into operation."		},
    { ELEMENT_STATUS_NO_ACCESS,		"No Access",
	"No access allowed from initiator port."			},
};
int num_element_status_entries = ( sizeof(element_status_table) / sizeof(element_status_entry_t) );

char *
get_element_status(element_status_t element_status)
{
    if ((int)element_status < num_element_status_entries) {
	return( element_status_table[element_status].element_status_name );
    } else if (element_status <= ELEMENT_STATUS_RESERVED_END) {
	return( "Reserved" );
    } else {
	return( "<unknown>" );
    }
}

char *
get_element_status_desc(element_status_t element_status)
{
    if ((int)element_status < num_element_status_entries) {
	return( element_status_table[element_status].element_status_desc );
    } else if (element_status <= ELEMENT_STATUS_RESERVED_END) {
	return( "Reserved" );
    } else {
	return( "<unknown>" );
    }
}

element_status_t
find_element_status(scsi_device_t *sdp, char *element_status, int *status)
{
    element_status_entry_t *esp = element_status_table;
    size_t length = strlen(element_status);
    int i;

    if (length == 0) {
	Printf(sdp, "\n");
	Printf(sdp, "Element Status Codes/Names:\n");
	for (i = 0; i < num_element_status_entries; i++, esp++) {
	    Printf(sdp, "    0x%02x - %s\n", esp->element_status, esp->element_status_name);
	}
	Printf(sdp, "\n");
	*status = WARNING;
	return(ELEMENT_STATUS_UNINITIALIZED);
    }

    for (i = 0; i < num_element_status_entries; i++, esp++) {
	/* Allow a matching a portion (start of string). */
	if ( strncasecmp(esp->element_status_name, element_status, length) == 0 ) {
	    *status = SUCCESS;
	    return( esp->element_status );
	}
    }
    *status = FAILURE;
    return(ELEMENT_STATUS_UNINITIALIZED);
}

/*
 * Connector Type Lookup Function:
 */
typedef struct connector_type_entry {
    uint8_t	connector_type;
    char	*connector_name;
    int		max_links;
} connector_type_entry_t;

connector_type_entry_t connector_type_table[] = {
    { 0x00, "No information", -1								},
    /* External Connectors: */
    { 0x01, "SAS 4x receptacle (SFF-8470) (max 4 physical links)", 4				},
    { 0x02, "Mini SAS 4x receptacle (SFF-8088) (max 8 physical links)", 4			},
    { 0x03, "QSFP+ receptacle (SFF-8436) (max 4 physical links)", 4				},
    { 0x04, "Mini SAS 4x active receptacle (SFF-8088) (max 4 physical links)", 4		},
    { 0x05, "Mini SAS HD 4x receptacle (SFF-8644) (max 4 physical links)", 4			},
    { 0x06, "Mini SAS HD 8x receptacle (SFF-8644) (max 8 physical links)", 8			},
    { 0x07, "Mini SAS HD 16x receptacle (SFF-8644) (max 16 physical links)", 16			},
    // 0x08 to 0x0E Reserved for external connectors
    { 0x0F, "Vendor specific external connector", -1			},
    /* Internal Wide Connectors: */
    { 0x10, "SAS 4i plug (SFF-8484) (max 4 physical links)", 4					},
    { 0x11, "Mini SAS 4i receptacle (SFF-8087) (max 4 physical links)", 4			},
    { 0x12, "Mini SAS HD 4i receptacle (SFF-8643) (max 4 physical links)", 4			},
    { 0x13, "Mini SAS HD 8i receptacle (SFF-8643) (max 8 physical links)", 8			},
    // 0x14 to 1Fh Reserved for internal wide connectors
    /* Internal Connectors to End Devices: */
    { 0x20, "SAS Drive backplane receptacle (SFF-8482) (max 2 physical links)", 2		},
    { 0x21, "SATA host plug (max 1 physical links)", 1						},
    { 0x22, "SAS Drive plug (SFF-8482) (max 2 physical links)", 2				},
    { 0x23, "SATA device plug (max 1 physical links)", 1					},
    { 0x24, "Micro SAS receptacle (max 2 physical links)", 2					},
    { 0x25, "Micro SATA device plug (max 1 physical links)", 1					},
    { 0x26, "Micro SAS plug (SFF-8486) (max 2 physical links)", 2				},
    { 0x27, "Micro SAS/SATA plug (SFF-8486) (max 2 physical links)", 2				},
    { 0x28, "12 Gbit/s SAS Drive backplane receptacle (SFF-8680) (max 2 physical links)", 2	},
    { 0x29, "12 Gbit/s SAS Drive Plug (SFF-8680) (max 2 physical links)", 2			},
    { 0x2A, "Multifunction 12 Gbit/s 6x Unshielded receptacle connector receptacle (SFF-8639) (max 6 physical links)", 6	},
    { 0x2B, "Multifunction 12 Gbit/s 6x Unshielded receptable connector plug (SFF-8639) (max 6 physical links)", 6		},
    // 0x2C, to 2Eh Reserved for internal connectors to end devices
    { 0x2F, "SAS virtual connector (max physical links 1)", 1					},
    /* Internal Connectors: */
    // 0x30, to 3Eh Reserved for internal connectors
    { 0x3F, "Vendor specific internal connector", -1			}
    /* Other: */
    //{ 0x40 to 6Fh Reserved
    //{ 0x70 to 7Fh Vendor specific
};
int num_connector_type_entries = ( sizeof(connector_type_table) / sizeof(connector_type_entry_t) );

char *
get_connector_type(uint8_t connector_type)
{
    connector_type_entry_t *cte = connector_type_table;
    char *connector_name = "unknown";
    int i;

    for (i = 0; (i < num_connector_type_entries); i++, cte++) {
	if (cte->connector_type == connector_type) {
	    return(cte->connector_name);
	}
    }
    if ( (connector_type >= 0x08) && (connector_type <= 0x0E) ) {
	connector_name = "Reserved for external connectors";
    } else if ( (connector_type >= 0x14) && (connector_type <= 0x1F) ) {
	connector_name = "Reserved for internal wide connectors";
    } else if ( (connector_type >= 0x2C) && (connector_type <= 0x2E) ) {
	connector_name = "Reserved for internal connectors to end devices";
    } else if ( (connector_type >= 0x30) && (connector_type <= 0x3E) ) {
	connector_name = "Reserved for internal connectors";
    } else if ( (connector_type >= 0x40) && (connector_type <= 0x6F) ) {
	connector_name = "Reserved";
    } else if ( (connector_type >= 0x70) && (connector_type <= 0x7F) ) {
	connector_name = "Vendor specific";
    }
    return(connector_name);
}

/*
 * Cooling Speed Lookup Function:
 */
typedef struct cooling_actual_speed_entry {
    uint8_t	actual_speed_code;
    char	*actual_speed_name;
} cooling_actual_speed_entry_t;

cooling_actual_speed_entry_t cooling_actual_speed_table[] = {
    { /* 000b */ 0x00, "Cooling mechanism is stopped"				},
    { /* 001b */ 0x01, "Cooling mechanism is at its lowest speed"		},
    { /* 010b */ 0x02, "Cooling mechanism is at its second lowest speed"	},
    { /* 011b */ 0x03, "Cooling mechanism is at its third lowest speed"		},
    { /* 100b */ 0x04, "Cooling mechanism is at its intermediate speed"		},
    { /* 101b */ 0x05, "Cooling mechanism is at its third highest speed"	},
    { /* 110b */ 0x06, "Cooling mechanism is at its second highest speed"	},
    { /* 111b */ 0x07, "Cooling mechanism is at its highest speed"		}
};
int num_cooling_actual_speed_entries = ( sizeof(cooling_actual_speed_table) / sizeof(cooling_actual_speed_entry_t) );

char *
get_cooling_actual_speed(int actual_speed_code)
{
    if (actual_speed_code < num_cooling_actual_speed_entries) {
	return( cooling_actual_speed_table[actual_speed_code].actual_speed_name );
    } else {
	return( "<unknown>" );
    }
}
