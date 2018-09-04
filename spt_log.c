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
 * File:	spt_log.c
 * Author:	Robin T. Miller
 * Date:	February 17th, 2017
 *
 * Descriptions:
 *	SCSI Log Page Functions.
 *
 * Modification History:
 * 
 * May 25th, 2018 by Robin T. Miller
 *      Add support for log page counters and known log pages (hex dump).
 * 
 * November 16th, 2017 by Robin T. MIller
 *      Add support for report format of brief versus full, where brief format
 * excludes reporting of log page and parameter headers.
 * 
 * November 14th, 2017 by Robin T. Miller
 *      Added support for all supported log pages and temperature page.
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spt.h"
#include "libscsi.h"

#include "scsi_cdbs.h"
#include "scsi_log.h"

#include "parson.h"

/*
 * External References:
 */
extern char *find_sas_device_type(uint8_t device_type);

/*
 * Forward References:
 */
int PrintLogPageHeader(scsi_device_t *sdp, log_page_header_t *hdr, char *page_name, int offset);
JSON_Status PrintLogPageHeaderJson(scsi_device_t *sdp, JSON_Object *object, log_page_header_t *hdr);

int PrintLogParameterHeader(scsi_device_t *sdp, log_page_header_t *hdr, log_parameter_header_t *phdr, int offset);
JSON_Status PrintLogParameterHeaderJson(scsi_device_t *sdp, JSON_Object *object,
					log_page_header_t *hdr, log_parameter_header_t *phdr);
/* Log Page 0x00 */
int log_sense_supported_decode(scsi_device_t *sdp, io_params_t *iop,
			       scsi_generic_t *sgp, log_page_t *log_page);
char *log_sense_supported_to_json(scsi_device_t *sdp, io_params_t *iop,
				  log_page_header_t *hdr, char *page_name);
/* Log Page 0x0D */
int log_sense_temperature_decode(scsi_device_t *sdp, io_params_t *iop,
				 scsi_generic_t *sgp, log_page_t *log_page);
char *log_sense_temperature_to_json(scsi_device_t *sdp, io_params_t *iop,
				    log_page_t *log_page, char *page_name);
/* Log Page 0x18 */
int log_sense_protocol_specific_decode(scsi_device_t *sdp, io_params_t *iop,
				       scsi_generic_t *sgp, log_page_t *log_page);
char *log_sense_protocol_specific_to_json(scsi_device_t *sdp, io_params_t *iop,
					  log_page_t *log_page, char *page_name);
/* Counter and Other Log Pages. */
hbool_t isErrorCounterPage(uint8_t page_code);
int log_page_decode(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp, log_page_t *log_page);
char *log_page_decode_to_json(scsi_device_t *sdp, io_params_t *iop, log_page_t *log_page, char *page_name);
void PrintLogParameter(scsi_device_t *sdp, log_parameter_header_t *phdr, uint16_t param_code,
		       void *param_data, uint32_t param_length, char *param_str);
JSON_Status PrintLogParameterJson(scsi_device_t *sdp, JSON_Object *object,
				  log_parameter_header_t *phdr, uint16_t param_code,
				  void *param_data, uint32_t param_length, char *param_str);
/*
 * Utility Functions:
 */
char *find_protocol_identifier(uint8_t protocol_identifier);
char *find_identify_reason(uint8_t identify_reason);
char *find_link_rate(uint8_t link_rate);
char *find_phy_event_source(uint8_t phy_event_source);
char *get_log_page_name(uint8_t device_type, uint8_t page_code, uint8_t vendor_id);

/*
 * Local Declarations:
 */
static char *parameter_str =		"Parameter";
static char *parameter_code_str =	"Parameter Code";
static char *paramater_data_str =	"Parameter Data";
static char *counter_value_str =	"Counter Value";

static char *log_pcf_table[] = {
    "Current Threshold",				/* 0x00 */
    "Current Cumulative",				/* 0x01 */
    "Default Threshold",				/* 0x02 */
    "Default Cumulative"				/* 0x03 */
};
static int log_pcf_table_size = (sizeof(log_pcf_table) / sizeof(char *));

static char *format_linking_table[] = {
    "Bounded data counter",				/* 0x00 */
    "ASCII format list",				/* 0x01 */
    "Bounded data counter or unbounded data counter",	/* 0x02 */
    "Binary format list"				/* 0x03 */
};

/*
 * Types of Error Counters:
 */
static char *error_counter_types[] = {
    "Errors Corrected w/o Substantial Delay",		/* 0x00 */
    "Errors Corrected with Possible Delays",		/* 0x01 */
    "Total re-Reads or re-Writes",			/* 0x02 */
    "Total Errors Corrected",				/* 0x03 */
    "Times Correction Algorithm Processed",		/* 0x04 */
    "Total Bytes Processed",				/* 0x05 */
    "Total Uncorrected Errors"				/* 0x06 */
};
static int num_error_types = (sizeof(error_counter_types) / sizeof(char *));

/*
 * Overrun/Underrun Cause Fields:
 */
static char *cause_field_table[] = {
	"Undefined",						/* 0x00 */
	"SCSI bus busy",					/* 0x01 */
	"Transfer rate too slow"				/* 0x02 */
};
static int num_cause_field = (sizeof(cause_field_table) / sizeof(char *));

/*
 * Overrun/Underrun Count Basis:
 */
static char *count_basis_table[] = {
	"Undefined",						/* 0x00 */
	"Per command",						/* 0x01 */
	"Per failed reconnect",					/* 0x02 */
	"Per unit of time"					/* 0x03 */
};
static int num_count_basis = (sizeof(count_basis_table) / sizeof(char *));

/* ============================================================================================== */
/* Note: The page code and page control will be overwritten in the decode function if non-zero. */
int
setup_log_select(scsi_device_t *sdp, scsi_generic_t *sgp, size_t data_length, uint8_t page)
{
    LogSelect_CDB_t *cdb;

    cdb = (LogSelect_CDB_t *)sgp->cdb;
    memset(cdb, '\0', sizeof(*cdb));
    cdb->opcode = (uint8_t)SOPC_LOG_SELECT;
    cdb->page_code = page;
    cdb->page_control = sdp->page_control;
    sgp->data_dir = scsi_data_read;
    sgp->data_length = (unsigned int)data_length;
    sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
    if (sgp->data_buffer == NULL) return (FAILURE);
    /* Setup to execute a CDB operation. */
    sdp->op_type = SCSI_CDB_OP;
    sdp->encode_flag = True;
    sgp->cdb_size = GetCdbLength(sgp->cdb[0]);
    return(SUCCESS);
}

int
setup_zero_log(scsi_device_t *sdp, scsi_generic_t *sgp, uint8_t page)
{
    LogSelect_CDB_t *cdb;

    cdb = (LogSelect_CDB_t *)sgp->cdb;
    memset(cdb, '\0', sizeof(*cdb));
    cdb->opcode = (uint8_t)SOPC_LOG_SELECT;
    cdb->pcr = 1;
    cdb->page_code = page;
    cdb->page_control = sdp->page_control;
    sgp->data_dir = scsi_data_none;
    /* Setup to execute a CDB operation. */
    sdp->op_type = SCSI_CDB_OP;
    sdp->bypass = True;
    sdp->encode_flag = True;
    sgp->cdb_size = GetCdbLength(sgp->cdb[0]);
    return(SUCCESS);
}

int
log_select_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    LogSelect_CDB_t *cdb;
    int status = SUCCESS;

    cdb = (LogSelect_CDB_t *)sgp->cdb;
    if (sdp->page_code) {
	cdb->page_code = sdp->page_code;
    }
    if (sdp->page_control) {
	cdb->page_control = sdp->page_control;
    }
    /* Note: When zeroing log pages, there is no parameter data. */
    if (sgp->data_length) {
	HtoS(cdb->parameter_length, sgp->data_length);
	sgp->data_dir = iop->sop->data_dir;
    } else {
	sgp->data_dir = scsi_data_none;
    }
    return(status);
}

/* ============================================================================================== */

int
setup_log_sense(scsi_device_t *sdp, scsi_generic_t *sgp, size_t data_length, uint8_t page)
{
    LogSense_CDB_t *cdb;

    cdb = (LogSense_CDB_t *)sgp->cdb;
    memset(cdb, '\0', sizeof(*cdb));
    cdb->opcode = (uint8_t)SOPC_LOG_SENSE; 
    cdb->page_code = page;
    cdb->page_control = sdp->page_control;
    sgp->data_dir = scsi_data_read;
    sgp->data_length = (unsigned int)data_length;
    sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
    if (sgp->data_buffer == NULL) return (FAILURE);
    /* Setup to execute a CDB operation. */
    sdp->op_type = SCSI_CDB_OP;
    sdp->encode_flag = True;
    sdp->decode_flag = True;
    sgp->cdb_size = GetCdbLength(sgp->cdb[0]);
    return(SUCCESS);
}

int
log_sense_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    LogSense_CDB_t *cdb;
    int status = SUCCESS;

    cdb = (LogSense_CDB_t *)sgp->cdb;
    if (sdp->page_code) {
	cdb->page_code = sdp->page_code;
    }
    if (sdp->page_control) {
	cdb->page_control = sdp->page_control;
    }
    /* No parameter pointer right now! */
    HtoS(cdb->allocation_length, sgp->data_length);
    sgp->data_dir = iop->sop->data_dir;
    return(status);
}

int
log_sense_decode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    log_page_t *log_page = sgp->data_buffer;
    log_page_header_t *hdr = &log_page->log_hdr;
    log_parameter_header_t *phdr = &log_page->log_phdr;
    int status = SUCCESS;

    if (iop->first_time) {
	status = get_inquiry_information(sdp, iop, sgp);
	if (status == FAILURE) return(status);
	iop->first_time = False;
    }

    if (hdr->log_page_code == LOG_ALL_PAGES) {
	status = log_sense_supported_decode(sdp, iop, sgp, log_page);	            /* Page 0x00 */
    } else if (hdr->log_page_code == LOG_TEMPERATURE_PAGE) {
	status = log_sense_temperature_decode(sdp, iop, sgp, log_page);	 	    /* Page 0x0D */
    } else if (hdr->log_page_code == LOG_PROTOCOL_SPEC_PAGE) {
	status = log_sense_protocol_specific_decode(sdp, iop, sgp, log_page);	    /* Page 0x18 */
    } else {
	status = log_page_decode(sdp, iop, sgp, log_page);	/* Counters and other pages. */
	//sdp->verbose = True;
    }
    return(status);
}

int
PrintLogPageHeader(scsi_device_t *sdp, log_page_header_t *hdr, char *page_name, int offset)
{
    uint8_t page_code = hdr->log_page_code;
    uint16_t page_length = (uint16_t)StoH(hdr->log_page_length);

    Printf(sdp, "\n");
    Printf(sdp, "%s Parameters (Page 0x%x - %s Values):\n",
	   page_name, hdr->log_page_code, log_pcf_table[sdp->page_control]);
    Printf(sdp, "\n");
    if (sdp->report_format != REPORT_FULL) {
	return(offset + sizeof(*hdr));
    }
    offset = PrintHexDebug(sdp, offset, (uint8_t *)hdr, sizeof(*hdr));
    PrintHex(sdp, "Page Code", page_code, PNL);
    if (hdr->reserved_byte0_b6_7 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 0, bits 6:7)", hdr->reserved_byte0_b6_7, PNL);
    }
    PrintHex(sdp, "Subpage Code", hdr->log_subpage_code, PNL);
    PrintDecHex(sdp, "Page Length", page_length, PNL);
    return(offset);
}

JSON_Status
PrintLogPageHeaderJson(scsi_device_t *sdp, JSON_Object *object, log_page_header_t *hdr)
{
    uint8_t page_code = hdr->log_page_code;
    uint16_t page_length = (uint16_t)StoH(hdr->log_page_length);
    char text[STRING_BUFFER_SIZE];
    JSON_Status json_status = JSONSuccess;

    if (sdp->report_format != REPORT_FULL) {
	return(json_status);
    }
    /* Note: The page name is already setup! */
    (void)sprintf(text, "0x%02x", page_code);
    json_status = json_object_set_string(object, "Page Code", text);
    if (json_status != JSONSuccess) return(json_status);
    json_status = json_object_set_number(object, "Page Control", (double)sdp->page_control);
    if (json_status != JSONSuccess) return(json_status);
    json_status = json_object_set_string(object, "Page Control Descripption", log_pcf_table[sdp->page_control]);
    if (json_status != JSONSuccess) return(json_status);

    json_status = json_object_set_number(object, "Reserved (byte 0, bits 6:7)", (double)hdr->reserved_byte0_b6_7);
    if (json_status != JSONSuccess) return(json_status);
    json_status = json_object_set_number(object, "Subpage Code", (double)hdr->log_subpage_code);
    if (json_status != JSONSuccess) return(json_status);
    json_status = json_object_set_number(object, "Page Length",(double) page_length);
    if (json_status != JSONSuccess) return(json_status);

    return(json_status);
}

int
PrintLogParameterHeader(scsi_device_t *sdp, log_page_header_t *hdr, log_parameter_header_t *phdr, int offset)
{
    uint16_t log_parameter_code = (uint16_t)StoH(phdr->log_parameter_code);
    uint8_t log_parameter_length = phdr->log_parameter_length + sizeof(*phdr);

    if (sdp->report_format != REPORT_FULL) {
	return(offset + sizeof(*phdr));
    }
    offset = PrintHexDebug(sdp, offset, (uint8_t *)phdr, sizeof(*phdr));
    /* Note: Don't like this here, but want the extra details! */
    if (hdr->log_page_code == LOG_PROTOCOL_SPEC_PAGE) {
	PrintHex(sdp, parameter_code_str, log_parameter_code, DNL);
	if (log_parameter_code == PROTOCOL_PRIMARY_PORT) {
	    Print(sdp, " (Primary Port)\n");
	} else if (log_parameter_code == PROTOCOL_SECONDARY_PORT) {
	    Print(sdp, " (Secondary Port)\n");
	} else {
	    Print(sdp, "\n");
	}
    } else {
	PrintHex(sdp, parameter_code_str, log_parameter_code, PNL);
    }
    if (hdr->log_page_code == LOG_OVER_UNDER_PAGE) {
	struct overrun_underrun_params *oup;
	oup = (overrun_underrun_params_t *)&phdr->log_parameter_code[1];
      /*
       * Only print count basis & cause fields if defined.
       */
      if (oup->oup_basis) {
	PrintNumeric(sdp, "Count Basis Definition", oup->oup_basis, DNL);
	if (oup->oup_basis < num_count_basis) {
	    Print(sdp, " (%s)\n", count_basis_table[oup->oup_basis]);
	} else {
	    Print(sdp, "\n");
	}
      }
      if (oup->oup_cause) {
	PrintNumeric(sdp, "Cause Field Definition", oup->oup_cause, DNL);
	if (oup->oup_cause < num_cause_field) {
	    Print(sdp, " (%s)\n", cause_field_table[oup->oup_cause]);
	} else {
	    Print(sdp, "\n");
	}
      }
      /* Type field purposely omitted */
    }

    PrintNumeric(sdp, "Format and Linking", phdr->log_format_linking, DNL);
    Print(sdp, " (%s)\n", format_linking_table[phdr->log_format_linking]);

    if (phdr->obsolete_byte2_b2_4 || sdp->DebugFlag) {
	PrintHex(sdp, "Obsolete (byte 2, bits 2:4)", phdr->obsolete_byte2_b2_4, PNL);
    }
    PrintNumeric(sdp, "Target Save Disable (TSD)", phdr->log_tsd, PNL);
    if (phdr->obsolete_byte2_b6 || sdp->DebugFlag) {
	PrintHex(sdp, "Obsolete (byte 2, bit 6)", phdr->obsolete_byte2_b6, PNL);
    }
    if ( ((phdr->log_format_linking == BOUNDED_DATA_COUNTER) ||
	  (phdr->log_format_linking == BOUNDED_UNBOUNDED_DATA_COUNTER)) &&
	 ((sdp->page_control == LOG_PCF_CURRENT_CUMULATIVE) ||
	  (sdp->page_control == LOG_PCF_DEFAULT_CUMULATIVE)) ) {
	PrintBoolean(sdp, False, "Disable Update (DU)", phdr->log_du, DNL);
	Print(sdp, " (%s)\n",
	      (phdr->log_du) ? "Counter reached maximum value" : "Data counting is enabled");
    }
    PrintDecHex(sdp, "Parameter Length", phdr->log_parameter_length, PNL);
    return(offset);
}

JSON_Status
PrintLogParameterHeaderJson(scsi_device_t *sdp, JSON_Object *object,
			    log_page_header_t *hdr, log_parameter_header_t *phdr)
{
    uint16_t log_parameter_code = (uint16_t)StoH(phdr->log_parameter_code);
    uint8_t log_parameter_length = phdr->log_parameter_length + sizeof(*phdr);
    char text[SMALL_BUFFER_SIZE];
    JSON_Status json_status = JSONSuccess;

    if (sdp->report_format != REPORT_FULL) {
	return(json_status);
    }
    /* Note: Hex makes more sense to match SCSI specs. */
    (void)sprintf(text, "0x%02x", log_parameter_code);
    json_status = json_object_set_string(object, "Parameter Code", text);
    if (json_status != JSONSuccess) return(json_status);
    /* Note: Don't like this here, but want the extra details! */
    if (hdr->log_page_code == LOG_PROTOCOL_SPEC_PAGE) {
	char *pcp;
	if (log_parameter_code == PROTOCOL_PRIMARY_PORT) {
	    pcp = "Primary Port";
	} else if (log_parameter_code == PROTOCOL_SECONDARY_PORT) {
	    pcp = "Secondary Port";
	} else {
	    pcp = "Unknown Port";
	}
	json_status = json_object_set_string(object, "Parameter Code Description", pcp);
	if (json_status != JSONSuccess) return(json_status);
    }

    if (hdr->log_page_code == LOG_OVER_UNDER_PAGE) {
	struct overrun_underrun_params *oup;
	oup = (overrun_underrun_params_t *)&phdr->log_parameter_code[1];
      /*
       * Only print count basis & cause fields if defined.
       */
      if (oup->oup_basis) {
	json_status = json_object_set_number(object, "Count Basis Definition", (double)oup->oup_basis);
	if (json_status != JSONSuccess) return(json_status);
	if (oup->oup_basis < num_count_basis) {
	    json_status = json_object_set_string(object, "Count Basis Description", count_basis_table[oup->oup_basis]);
	    if (json_status != JSONSuccess) return(json_status);
	}
      }
      if (oup->oup_cause) {
	json_status = json_object_set_number(object, "Cause Field Definition", (double)oup->oup_cause);
	if (json_status != JSONSuccess) return(json_status);
	if (oup->oup_cause < num_cause_field) {
	    json_status = json_object_set_string(object, "Count Field Description", cause_field_table[oup->oup_cause]);
	    if (json_status != JSONSuccess) return(json_status);
	}
      }
      /* Type field purposely omitted */
    }

    json_status = json_object_set_number(object, "Format and Linking", (double)phdr->log_format_linking);
    if (json_status != JSONSuccess) return(json_status);
    json_status = json_object_set_string(object, "Format and Linking Description", format_linking_table[phdr->log_format_linking]);
    if (json_status != JSONSuccess) return(json_status);

    json_status = json_object_set_number(object, "Obsolete (byte 2, bits 2:4)", (double)phdr->obsolete_byte2_b2_4);
    if (json_status != JSONSuccess) return(json_status);
    json_status = json_object_set_number(object, "Target Save Disable", (double)phdr->log_tsd);
    if (json_status != JSONSuccess) return(json_status);
    if (phdr->obsolete_byte2_b6 || sdp->DebugFlag) {
	json_status = json_object_set_number(object, "Obsolete (byte 2, bit 6)", (double)phdr->obsolete_byte2_b6);
	if (json_status != JSONSuccess) return(json_status);
    }
    if ( (phdr->log_format_linking == BOUNDED_DATA_COUNTER) &&
	 ((sdp->page_control == LOG_PCF_CURRENT_CUMULATIVE) ||
	  (sdp->page_control == LOG_PCF_DEFAULT_CUMULATIVE)) ) {
	json_object_set_boolean(object, "Disable Update", phdr->log_du);
	if (json_status != JSONSuccess) return(json_status);
	json_status = json_object_set_string(object, "Disable Update Description",
					     (phdr->log_du) ? "Counter reached maximum value" : "Data counting is enabled");
	if (json_status != JSONSuccess) return(json_status);
    }
    json_status = json_object_set_number(object, "Parameter Length", (double)phdr->log_parameter_length);
    return(json_status);
}

/* ============================================================================================== */

/*
 * Supported Log Pages (page 0x00).
 */
int
log_sense_supported_decode(scsi_device_t *sdp, io_params_t *iop,
			   scsi_generic_t *sgp, log_page_t *log_page)
{
    log_page_header_t *hdr = &log_page->log_hdr;
    uint8_t *pages, page_code = hdr->log_page_code;
    uint16_t page_length = (uint16_t)StoH(hdr->log_page_length);
    uint8_t device_type = iop->sip->si_inquiry->inq_dtype;
    char *page_name = NULL;
    int status = SUCCESS;

    if (sdp->output_format == JSON_FMT) {
	char *json_string = NULL;
	json_string = log_sense_supported_to_json(sdp, iop, hdr, "Supported Log Pages");
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }
    /* Format: <page header><page code>...*/
    pages = (uint8_t *)hdr + sizeof(*hdr);

    PrintHeader(sdp, "Log Pages Supported");

    if (sdp->DebugFlag) {
	uint8_t *ucp = (uint8_t *)hdr;
	int length = page_length;
	int offset = 0;
	offset = PrintHexData(sdp, offset, ucp, length);
    }

    for ( ; page_length-- ; pages++) {
	page_code = *pages;
	page_name = get_log_page_name(device_type, page_code, iop->vendor_id);
	Printf(sdp, "%34.34s Page (Code = 0x%02x)\n", page_name, page_code);
    }
    Printf(sdp, "\n");
    return(status);
}

/*
 * Supported Log Pages (Page 0x00) in JSON Format:
 */
char *
log_sense_supported_to_json(scsi_device_t *sdp, io_params_t *iop,
			    log_page_header_t *hdr, char *page_name)
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
    int	page_length = (int)StoH(hdr->log_page_length);
    uint8_t *pages, page_code = LOG_ALL_PAGES;
    char *log_page_name;
    uint8_t device_type = iop->sip->si_inquiry->inq_dtype;
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

    ucp = (uint8_t *)hdr;
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
    pages = (uint8_t *)hdr + sizeof(*hdr);

    for ( ; page_length-- ; pages++) {
	page_code = *pages;
	log_page_name = get_log_page_name(device_type, page_code, iop->vendor_id);
	(void)sprintf(text, "Page 0x%02x", page_code);
	json_status = json_object_set_string(object, text, log_page_name);
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
 * Temperature Log Page 0x0D
 */
int
log_sense_temperature_decode(scsi_device_t *sdp, io_params_t *iop,
			     scsi_generic_t *sgp, log_page_t *log_page)
{
    log_page_header_t *hdr = &log_page->log_hdr;
    log_parameter_header_t *phdr = &log_page->log_phdr;
    uint8_t page_code = hdr->log_page_code;
    int page_length = (int)StoH(hdr->log_page_length);
    int param_length = 0;
    uint16_t log_parameter_code = 0;
    uint16_t log_param_length = 0;
    uint8_t device_type = iop->sip->si_inquiry->inq_dtype;
    void *log_param_data = NULL;
    char *page_name = get_log_page_name(device_type, page_code, iop->vendor_id);
    char *tp = NULL, *str = NULL;
    uint8_t value8 = 0;
    int offset = 0;
    int status = SUCCESS;

    if (sdp->output_format == JSON_FMT) {
	char *json_string = NULL;
	json_string = log_sense_temperature_to_json(sdp, iop, log_page, page_name);
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }
    /*
     * Data Layout: 
     *  Log Page Header
     *  Log Parameter Header
     *  Temperature Parameters
     */
    offset = PrintLogPageHeader(sdp, hdr, page_name, offset);

    while (page_length > 0) {

	if (sdp->report_format == REPORT_FULL) Printf(sdp, "\n");
	offset = PrintLogParameterHeader(sdp, hdr, phdr, offset);

	log_parameter_code = (uint16_t)StoH(phdr->log_parameter_code);
	log_param_length = phdr->log_parameter_length;
	log_param_data = (uint8_t *)phdr + sizeof(*phdr);

	param_length = log_param_length;
	offset = PrintHexDebug(sdp, offset, (uint8_t *)log_param_data, param_length);

	switch (log_parameter_code) {

	    case TLP_TEMP_PARAM: {
		temp_log_param_t *tlp = log_param_data;
		str = "Current Temperature";
		value8 = tlp->tlp_temperature;
		break;
	    }
	    case TLP_REF_TEMP_PARAM: {
		ref_temp_log_param_t *rtp = log_param_data;
		str = "Reference Temperature";
		value8 = rtp->rtp_ref_temperature;
		break;
	    }
	    default:
		return (FAILURE);
		/*NOTREACHED*/
		break;
	}
	PrintDecimal(sdp, str, (uint64_t)value8, DNL);
	switch (value8) {
	    case TLP_TEMP_LESS_ZERO:
		str = "(Less than zero)";
		break;
	    case TLP_TEMP_NOT_AVAIL:
		str = "(Not available)";
		break;
	    default:
		str = "Celsius";
		break;
	}
	Print(sdp, " %s\n", str);
	param_length += sizeof(*phdr);
	page_length -= param_length;
	phdr = (log_parameter_header_t *)((uint8_t *)phdr + param_length);
    }
    Printf(sdp, "\n");
    return(status);
}

/*
 * Temperature Log Page 0x0D in JSON.
 */
char *
log_sense_temperature_to_json(scsi_device_t *sdp, io_params_t *iop,
			      log_page_t *log_page, char *page_name)
{
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    /* Log Parameter List */
    JSON_Value  *pvalue = NULL;
    JSON_Object *pobject = NULL;
    JSON_Array	*pdesc_array;
    JSON_Value	*pdesc_value = NULL;
    JSON_Status json_status;
    char *json_string = NULL;
    log_page_header_t *hdr = &log_page->log_hdr;
    log_parameter_header_t *phdr = &log_page->log_phdr;
    uint8_t page_code = hdr->log_page_code;
    uint16_t page_length = (uint16_t)StoH(hdr->log_page_length);
    int param_length = 0;
    uint16_t log_parameter_code = 0;
    uint16_t log_param_length = 0;
    void *log_param_data = NULL;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL, *str = NULL;
    uint8_t *ucp = NULL;
    uint8_t value8 = 0;
    int length = 0;
    int offset = 0;
    int toffset = 0;
    int addl_len = 0;
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

    ucp = (uint8_t *)hdr;
    length = sizeof(*hdr);
    if (sdp->report_format == REPORT_FULL) {
	json_status = json_object_set_number(object, "Length", (double)length);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(object, "Offset", (double)offset);
	if (json_status != JSONSuccess) goto finish;
	offset = FormatHexBytes(text, offset, ucp, length);
	json_status = json_object_set_string(object, "Bytes", text);
	if (json_status != JSONSuccess) goto finish;
    }
    json_status = PrintLogPageHeaderJson(sdp, object, hdr);
    if (json_status != JSONSuccess) goto finish;

    while (page_length > 0) {
	JSON_Object *mobject = object;
	if (sdp->report_format == REPORT_FULL) {
	    if (pvalue == NULL) {
		pvalue  = json_value_init_object();
		pobject = json_value_get_object(pvalue);
	    }
	    if (pdesc_value == NULL) {
		pdesc_value = json_value_init_array();
		pdesc_array = json_value_get_array(pdesc_value);
	    }
	    mobject = pobject;
	}
	log_parameter_code = (uint16_t)StoH(phdr->log_parameter_code);
	log_param_length = phdr->log_parameter_length;
	log_param_data = (uint8_t *)phdr + sizeof(*phdr);

	ucp = (uint8_t *)phdr;
	param_length = sizeof(*phdr) + log_param_length;
	length = param_length;
	if (sdp->report_format == REPORT_FULL) {
	    json_status = json_object_set_number(mobject, "Length", (double)length);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(mobject, "Offset", (double)offset);
	    if (json_status != JSONSuccess) break;
	    offset = FormatHexBytes(text, offset, ucp, length);
	    json_status = json_object_set_string(mobject, "Bytes", text);
	    if (json_status != JSONSuccess) break;
	}
	json_status = PrintLogParameterHeaderJson(sdp, mobject, hdr, phdr);
	if (json_status != JSONSuccess) goto finish;

	switch (log_parameter_code) {

	    case TLP_TEMP_PARAM: {
		temp_log_param_t *tlp = log_param_data;
		str = "Current Temperature";
		value8 = tlp->tlp_temperature;
		break;
	    }
	    case TLP_REF_TEMP_PARAM: {
		ref_temp_log_param_t *rtp = log_param_data;
		str = "Reference Temperature";
		value8 = rtp->rtp_ref_temperature;
		break;
	    }
	    default:
		goto finish;
		/*NOTREACHED*/
		break;
	}
	switch (value8) {
	    case TLP_TEMP_LESS_ZERO:
		tp = "(Less than zero)";
		break;
	    case TLP_TEMP_NOT_AVAIL:
		tp = "(Not available)";
		break;
	    default:
		tp = "Celsius";
		break;
	}
	(void)sprintf(text, "%d %s", value8, tp);
	json_status = json_object_set_string(mobject, str, text);
	if (json_status != JSONSuccess) goto finish;

	page_length -= param_length;
	phdr = (log_parameter_header_t *)((uint8_t *)phdr + param_length);

	if (sdp->report_format == REPORT_FULL) {
	    json_status = json_array_append_value(pdesc_array, pvalue);
	    pvalue = NULL;
	    if (json_status != JSONSuccess) break;
	}
    }
    /* Add the Log Parameter List. */
    if (pdesc_value) {
	json_status = json_object_dotset_value(object, "Log Parameter List", pdesc_value);
	pdesc_value = NULL;
	if (json_status != JSONSuccess) goto finish;
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
 * Protocol Specific Port Log Page 0x18
 */
int
log_sense_protocol_specific_decode(scsi_device_t *sdp, io_params_t *iop,
				   scsi_generic_t *sgp, log_page_t *log_page)
{
    log_page_header_t *hdr = &log_page->log_hdr;
    log_protocol_specific_t *psp = (log_protocol_specific_t *)&log_page->log_phdr;
    log_parameter_header_t *phdr = &psp->phdr;
    sas_phy_log_descriptor_t *spld = NULL;
    phy_event_descriptor_t *ped = NULL;
    uint8_t page_code = hdr->log_page_code;
    uint16_t page_length = (uint16_t)StoH(hdr->log_page_length);
    uint16_t log_parameter_code = (uint16_t)StoH(phdr->log_parameter_code);
    uint8_t device_type = iop->sip->si_inquiry->inq_dtype;
    char *page_name = get_log_page_name(device_type, page_code, iop->vendor_id);
    char *tp = NULL;
    int offset = 0;
    int phy_desc = 0, phy_event = 0;
    int status = SUCCESS;

    if (sdp->output_format == JSON_FMT) {
	char *json_string = NULL;
	json_string = log_sense_protocol_specific_to_json(sdp, iop, log_page, page_name);
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }
    /*
     * Data Layout: 
     *	Log Page Header
     *  Protocol-Specific Port Parameters
     *  SAS Phy Log Descriptor List
     *  Phy Event Descriptor List
     */
    offset = PrintLogPageHeader(sdp, hdr, page_name, offset);
    if (sdp->report_format == REPORT_FULL) Printf(sdp, "\n");
    offset = PrintLogParameterHeader(sdp, hdr, phdr, offset);

    Printf(sdp, "\n");
    offset = PrintHexDebug(sdp, offset, (uint8_t *)psp - sizeof(*phdr), (sizeof(*psp) - sizeof(*phdr)));
    PrintHex(sdp, "Protocol Identifier", psp->protocol_identifier, DNL);
    Print(sdp, " (%s)\n", find_protocol_identifier(psp->protocol_identifier));
    if (psp->reserved_byte4_b4_4 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 4, bits 4:4)", psp->reserved_byte4_b4_4, PNL);
    }
    if (psp->reserved_byte5 || sdp->DebugFlag) {
	PrintHex(sdp, "Reserved (byte 5)", psp->reserved_byte5, PNL);
    }
    PrintDecimal(sdp, "Generation Code", psp->generation_code, PNL);
    PrintDecimal(sdp, "Number of PHYs", psp->number_of_phys, PNL);

    /* Sanity check, we only support SAS right now! */
    if (psp->protocol_identifier != PROTOCOL_ID_SAS_Serial_SCSI_Protocol) {
	Eprintf(sdp, "Only the SAS Serial SCSI Protocol is implemented!");
	return(FAILURE);
    }
 
    spld = (sas_phy_log_descriptor_t *)((uint8_t *)psp + sizeof(*psp));
    /*
     * Loop through th SAS Phy Log Descriptor List.
     */
    for (phy_desc = 0; (phy_desc < psp->number_of_phys); phy_desc++) {

	Printf(sdp, "\n");
	offset = PrintHexDebug(sdp, offset, (uint8_t *)spld, sizeof(*spld));
	PrintDecimal(sdp, "SAS Phy Log Descriptor", phy_desc, PNL);
	if (spld->reserved_byte0 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 0)", spld->reserved_byte0, PNL);
	}
	PrintDecimal(sdp, "Phy Identifier", spld->phy_identifier, PNL);
	if (spld->reserved_byte2 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 2)", spld->reserved_byte2, PNL);
	}
	PrintDecHex(sdp, "SAS Phy Log Descriptor Length", spld->sas_phy_log_descriptor_length, PNL);

	PrintDecimal(sdp, "Attached Reason", spld->attached_reason, DNL);
	Print(sdp, " (%s)\n", find_identify_reason(spld->attached_reason));
	PrintDecimal(sdp, "Attached Device Type", spld->attached_device_type, DNL);
	Print(sdp, " (%s)\n", find_sas_device_type(spld->attached_device_type));
	if (spld->reserved_byte4_b7 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 4, bit 7)", spld->reserved_byte4_b7, PNL);
	}

	PrintHex(sdp, "Negotiated Physical Link Rate", spld->negotiated_physical_link_rate, DNL);
	Print(sdp, " (%s)\n", find_link_rate(spld->negotiated_physical_link_rate));
	PrintHex(sdp, "Last Link Reset Reason", spld->reason, DNL);
	Print(sdp, " (%s)\n", find_identify_reason(spld->reason));

	if (spld->reserved_byte6_b0 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 6, bit 0)", spld->reserved_byte6_b0, PNL);
	}
	PrintBoolean(sdp, False, "SMP Initiator Port", spld->smp_initiator_port, PNL);
	PrintBoolean(sdp, False, "STP Initiator Port", spld->stp_initiator_port, PNL);
	PrintBoolean(sdp, False, "SSP Initiator Port", spld->ssp_initiator_port, PNL);
	if (spld->reserved_byte6_b4_4 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 6, bits 4:4)", spld->reserved_byte6_b4_4, PNL);
	}

	if (spld->reserved_byte7_b0 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 7, bit 0)", spld->reserved_byte7_b0, PNL);
	}
	PrintBoolean(sdp, False, "SMP Target Port", spld->smp_target_port, PNL);
	PrintBoolean(sdp, False, "STP Target Port", spld->stp_target_port, PNL);
	PrintBoolean(sdp, False, "SSP Target Port", spld->ssp_target_port, PNL);
	if (spld->reserved_byte7_b4_7 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 7, bits 4:7)", spld->reserved_byte7_b4_7, PNL);
	}

	PrintLongHexP(sdp, "The SAS Address", StoH(spld->sas_address), PNL);
	PrintLongHexP(sdp, "Attached SAS Address", StoH(spld->attached_sas_address), PNL);
	PrintDecimal(sdp, "Attached Phy Identifier", spld->attached_phy_identifier, PNL);

	if (spld->reserved_byte25 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 25)", spld->reserved_byte25, PNL);
	}
	if (spld->reserved_byte26 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 26)", spld->reserved_byte26, PNL);
	}
	if (spld->reserved_byte27 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 27)", spld->reserved_byte27, PNL);
	}
	if (spld->reserved_byte28 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 28)", spld->reserved_byte28, PNL);
	}
	if (spld->reserved_byte29 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 29)", spld->reserved_byte29, PNL);
	}
	if (spld->reserved_byte30 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 30)", spld->reserved_byte30, PNL);
	}
	if (spld->reserved_byte31 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 31)", spld->reserved_byte31, PNL);
	}

	PrintDecimal(sdp, "Invalid Dword Count", (uint32_t)StoH(spld->invalid_dword_count), PNL);
	PrintDecimal(sdp, "Running Disparity Error Count", (uint32_t)StoH(spld->running_disparity_error_count), PNL);
	PrintDecimal(sdp, "Loss of Dword Synchronization", (uint32_t)StoH(spld->loss_of_dword_synchronization), PNL);
	PrintDecimal(sdp, "Phy Reset Problem", (uint32_t)StoH(spld->phy_reset_problem), PNL);

	if (spld->reserved_byte48 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 48)", spld->reserved_byte48, PNL);
	}
	if (spld->reserved_byte49 || sdp->DebugFlag) {
	    PrintHex(sdp, "Reserved (byte 49)", spld->reserved_byte49, PNL);
	}

	PrintDecHex(sdp, "Phy Event Descriptor Length", spld->phy_event_descriptor_length, PNL);
	PrintDecimal(sdp, "Number of Event Descriptors", spld->number_of_event_descriptors, PNL); 

	ped = (phy_event_descriptor_t *)((uint8_t *)spld + sizeof(*spld));
	/*
	 * Loop through the Phy event descriptor list.
	 */
	for (phy_event = 0; (phy_event < spld->number_of_event_descriptors); phy_event++, ped++) {
	    char *phy_event_source_string = NULL;

	    Printf(sdp, "\n");
	    offset = PrintHexDebug(sdp, offset, (uint8_t *)ped, sizeof(*ped));
	    PrintDecimal(sdp, "Phy Event Descriptor", phy_event, PNL);
	    if (ped->reserved_byte0 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 0)", ped->reserved_byte0, PNL);
	    }
	    if (ped->reserved_byte1 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 1)", ped->reserved_byte1, PNL);
	    }
	    if (ped->reserved_byte2 || sdp->DebugFlag) {
		PrintHex(sdp, "Reserved (byte 2)", ped->reserved_byte2, PNL);
	    }
	    PrintHex(sdp, "Phy Event Source", ped->phy_event_source, PNL);
	    phy_event_source_string = find_phy_event_source(ped->phy_event_source);
	    PrintAscii(sdp, "Phy Event Source Description", phy_event_source_string, PNL);
	    PrintDecimal(sdp, phy_event_source_string, (uint32_t)StoH(ped->phy_event), PNL);
	    PrintDecimal(sdp, "Peak Value Detector Threshold", (uint32_t)StoH(ped->peak_value_detector_threshold), PNL);
	}

	/* Point to the next SAS Phy Log Descriptor (if any). */
	spld = (sas_phy_log_descriptor_t *)((uint8_t *)spld + sizeof(*spld) + spld->sas_phy_log_descriptor_length);
    }
    Printf(sdp, "\n");
    return(status);
}

/*
 * Protocol Specific Port Log Page 0x18 in JSON.
 */
char *
log_sense_protocol_specific_to_json(scsi_device_t *sdp, io_params_t *iop,
				    log_page_t *log_page, char *page_name)
{
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    /* Phy Log Descriptor List */
    JSON_Value  *pvalue = NULL;
    JSON_Object *pobject = NULL;
    JSON_Array	*pdesc_array;
    JSON_Value	*pdesc_value = NULL;
    /* Phy Event Descriptor List. */
    JSON_Value  *evalue = NULL;
    JSON_Object *eobject = NULL;
    JSON_Array	*edesc_array;
    JSON_Value	*edesc_value = NULL;
    JSON_Status json_status;
    char *json_string = NULL;
    log_page_header_t *hdr = &log_page->log_hdr;
    log_protocol_specific_t *psp = (log_protocol_specific_t *)&log_page->log_phdr;
    log_parameter_header_t *phdr = &psp->phdr;
    sas_phy_log_descriptor_t *spld = NULL;
    phy_event_descriptor_t *ped = NULL;
    uint8_t page_code = hdr->log_page_code;
    uint16_t page_length = (uint16_t)StoH(hdr->log_page_length);
    uint16_t log_parameter_code = (uint16_t)StoH(phdr->log_parameter_code);
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    uint8_t *ucp = NULL;
    int length = 0;
    int offset = 0;
    int toffset = 0;
    int addl_len = 0;
    int phy_desc = 0, phy_event = 0;
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

    ucp = (uint8_t *)hdr;
    length = sizeof(*hdr) + sizeof(*psp);
    json_status = json_object_set_number(object, "Length", (double)length);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Offset", (double)offset);
    if (json_status != JSONSuccess) goto finish;
    offset = FormatHexBytes(text, offset, ucp, length);
    json_status = json_object_set_string(object, "Bytes", text);
    if (json_status != JSONSuccess) goto finish;

    json_status = PrintLogPageHeaderJson(sdp, object, hdr);
    if (json_status != JSONSuccess) goto finish;

    json_status = PrintLogParameterHeaderJson(sdp, object, hdr, phdr);
    if (json_status != JSONSuccess) goto finish;

    json_status = json_object_set_number(object, "Protocol Identifier", (double)psp->protocol_identifier);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_string(object, "Protocol Identifier Description",
					 find_protocol_identifier(psp->protocol_identifier));
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Reserved (byte 4, bits 4:4)", psp->reserved_byte4_b4_4);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Reserved (byte 5)", (double)psp->reserved_byte5);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Generation Code", (double)psp->generation_code);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Number of PHYs", (double)psp->number_of_phys);
    if (json_status != JSONSuccess) goto finish;

    /* Sanity check, we only support SAS right now! */
    if (psp->protocol_identifier != PROTOCOL_ID_SAS_Serial_SCSI_Protocol) {
	json_status = json_object_set_number(object, "Unsupported Protocol Identifier", (double)psp->protocol_identifier);
	goto finish;
    }

    spld = (sas_phy_log_descriptor_t *)((uint8_t *)psp + sizeof(*psp));
    /*
     * Loop through th SAS Phy Log Descriptor List.
     */
    for (phy_desc = 0; (phy_desc < psp->number_of_phys); phy_desc++) {

	if (pvalue == NULL) {
	    pvalue  = json_value_init_object();
	    pobject = json_value_get_object(pvalue);
	}
	if (pdesc_value == NULL) {
	    pdesc_value = json_value_init_array();
	    pdesc_array = json_value_get_array(pdesc_value);
	}

	ucp = (uint8_t *)spld;
	length = sizeof(*spld);
	json_status = json_object_set_number(pobject, "Length", (double)length);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Offset", (double)offset);
	if (json_status != JSONSuccess) break;
	offset = FormatHexBytes(text, offset, ucp, length);
	json_status = json_object_set_string(pobject, "Bytes", text);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(pobject, "SAS Phy Log Descriptor", (double)phy_desc);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(pobject, "Reserved (byte 0)", (double)spld->reserved_byte0);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Phy Identifier", (double)spld->phy_identifier);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Reserved (byte 2)", (double)spld->reserved_byte2);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "SAS Phy Log Descriptor Length", (double)spld->sas_phy_log_descriptor_length);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(pobject, "Attached Reason", (double)spld->attached_reason);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_string(pobject, "Attached Reason Description", find_identify_reason(spld->attached_reason));
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Attached Device Type", (double)spld->attached_device_type);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_string(pobject, "Attached Device Type Description", find_sas_device_type(spld->attached_device_type));
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Reserved (byte 4, bit 7)", (double)spld->reserved_byte4_b7);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(pobject, "Negotiated Physical Link Rate", (double)spld->negotiated_physical_link_rate);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_string(pobject, "Negotiated Physical Link Description", find_link_rate(spld->negotiated_physical_link_rate));
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Last Link Reset Reason", spld->reason);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_string(pobject, "Last Link Reset Reason Description", find_identify_reason(spld->reason));
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(pobject, "Reserved (byte 6, bit 0)", (double)spld->reserved_byte6_b0);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_boolean(pobject, "SMP Initiator Port", spld->smp_initiator_port);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_boolean(pobject, "STP Initiator Port", spld->stp_initiator_port);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_boolean(pobject, "SSP Initiator Port", spld->ssp_initiator_port);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Reserved (byte 6, bits 4:4)", (double)spld->reserved_byte6_b4_4);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(pobject, "Reserved (byte 7, bit 0)", (double)spld->reserved_byte7_b0);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_boolean(pobject, "SMP Target Port", spld->smp_target_port);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_boolean(pobject, "STP Target Port", spld->stp_target_port);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_boolean(pobject, "SSP Target Port", spld->ssp_target_port);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Reserved (byte 7, bits 4:7)", (double)spld->reserved_byte7_b4_7);
	if (json_status != JSONSuccess) break;

	(void)sprintf(text, LLFXFMT, StoH(spld->sas_address));
	json_status = json_object_set_string(pobject, "SAS Address", text);
	if (json_status != JSONSuccess) break;

	(void)sprintf(text, LLFXFMT, StoH(spld->attached_sas_address));
	json_status = json_object_set_string(pobject, "Attached SAS Address", text);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(pobject, "Attached Phy Identifier", (double)spld->attached_phy_identifier);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(pobject, "Reserved (byte 25)", spld->reserved_byte25);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Reserved (byte 26)", (double)spld->reserved_byte26);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Reserved (byte 27)", (double)spld->reserved_byte27);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Reserved (byte 28)", (double)spld->reserved_byte28);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Reserved (byte 29)", (double)spld->reserved_byte29);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Reserved (byte 30)", (double)spld->reserved_byte30);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Reserved (byte 31)", (double)spld->reserved_byte31);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(pobject, "Invalid Dword Count", (double)StoH(spld->invalid_dword_count));
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Running Disparity Error Count", (double)StoH(spld->running_disparity_error_count));
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Loss of Dword Synchronization", (double)StoH(spld->loss_of_dword_synchronization));
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Phy Reset Problem", (double)StoH(spld->phy_reset_problem));
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(pobject, "Reserved (byte 48)", (double)spld->reserved_byte48);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Reserved (byte 49)", (double)spld->reserved_byte49);
	if (json_status != JSONSuccess) break;

	json_status = json_object_set_number(pobject, "Phy Event Descriptor Length", (double)spld->phy_event_descriptor_length);
	if (json_status != JSONSuccess) break;
	json_status = json_object_set_number(pobject, "Number of Event Descriptors", (double)spld->number_of_event_descriptors); 
	if (json_status != JSONSuccess) break;

	ped = (phy_event_descriptor_t *)((uint8_t *)spld + sizeof(*spld));
	/*
	 * Loop through the Phy event descriptor list.
	 */
	for (phy_event = 0; (phy_event < spld->number_of_event_descriptors); phy_event++, ped++) {
	    char *phy_event_source_string = NULL;

	    if (evalue == NULL) {
		evalue  = json_value_init_object();
		eobject = json_value_get_object(evalue);
	    }
	    if (edesc_value == NULL) {
		edesc_value = json_value_init_array();
		edesc_array = json_value_get_array(edesc_value);
	    }

	    ucp = (uint8_t *)ped;
	    length = sizeof(*ped);
	    json_status = json_object_set_number(eobject, "Length", (double)length);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(eobject, "Offset", (double)offset);
	    if (json_status != JSONSuccess) break;
	    offset = FormatHexBytes(text, offset, ucp, length);
	    json_status = json_object_set_string(eobject, "Bytes", text);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_number(eobject, "Phy Event Descriptor", (double)phy_event);
	    if (json_status != JSONSuccess) break;

	    json_status = json_object_set_number(eobject, "Reserved (byte 0)", (double)ped->reserved_byte0);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(eobject, "Reserved (byte 1)", (double)ped->reserved_byte1);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(eobject, "Reserved (byte 2)", (double)ped->reserved_byte2);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(eobject, "Phy Event Source", (double)ped->phy_event_source);
	    if (json_status != JSONSuccess) break;
	    phy_event_source_string = find_phy_event_source(ped->phy_event_source);
	    json_status = json_object_set_string(eobject, "Phy Event Source Description", phy_event_source_string);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(eobject, phy_event_source_string, (double)StoH(ped->phy_event));
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(eobject, "Peak Value Detector Threshold", (double)StoH(ped->peak_value_detector_threshold));
	    if (json_status != JSONSuccess) break;

	    json_status = json_array_append_value(edesc_array, evalue);
	    evalue = NULL;
	    if (json_status != JSONSuccess) break;
	}

	/* Point to the next SAS Phy Log Descriptor (if any). */
	spld = (sas_phy_log_descriptor_t *)((uint8_t *)spld + sizeof(*spld) + spld->sas_phy_log_descriptor_length);

	/* Add the Phy Event Descriptor List. */
	if (edesc_value) {
	    json_object_set_value(pobject, "Phy Event Descriptor List", edesc_value);
	    edesc_value = NULL;
	    if (json_status != JSONSuccess) break;
	}
	json_status = json_array_append_value(pdesc_array, pvalue);
	pvalue = NULL;
	if (json_status != JSONSuccess) break;
    }
    /* Add the Phy Log Descriptor List. */
    if (pdesc_value) {
	json_status = json_object_dotset_value(object, "Phy Log Descriptor List", pdesc_value);
	pdesc_value = NULL;
	if (json_status != JSONSuccess) goto finish;
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
 * Log Page Counters and Other (unknown) Pages:
 */
int
log_page_decode(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp, log_page_t *log_page)
{
    log_page_header_t *hdr = &log_page->log_hdr;
    log_parameter_header_t *phdr = &log_page->log_phdr;
    uint8_t page_code = hdr->log_page_code;
    int page_length = (int)StoH(hdr->log_page_length);
    uint16_t log_parameter_code = 0;
    uint16_t log_param_length = 0;
    uint8_t device_type = iop->sip->si_inquiry->inq_dtype;
    void *log_param_data = NULL;
    char *page_name = get_log_page_name(device_type, page_code, iop->vendor_id);
    int offset = 0;
    int status = SUCCESS;

    if (sdp->output_format == JSON_FMT) {
	char *json_string = NULL;
	json_string = log_page_decode_to_json(sdp, iop, log_page, page_name);
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }
    /*
     * Data Layout: 
     *  Log Page Header
     *  Log Parameter Header
     *  Log Parameter Data
     */
    offset = PrintLogPageHeader(sdp, hdr, page_name, offset);

    /*
     * Loop through variable length log page parameters.
     */
    while (page_length > 0) {
	int param_entry_length = 0;
	char *param_str = NULL;

	if (CmdInterruptedFlag == True)	break;
	if (sdp->report_format == REPORT_FULL) Printf(sdp, "\n");
	offset = PrintLogParameterHeader(sdp, hdr, phdr, offset);

	log_parameter_code = (uint16_t)StoH(phdr->log_parameter_code);
	log_param_length = phdr->log_parameter_length;
	log_param_data = (uint8_t *)phdr + sizeof(*phdr);

	offset = PrintHexDebug(sdp, offset, (uint8_t *)log_param_data, log_param_length);

	if ( (isErrorCounterPage(hdr->log_page_code)) &&
	     (log_parameter_code < num_error_types) ) {
	    param_str = error_counter_types[log_parameter_code];
	}

	PrintLogParameter(sdp, phdr, log_parameter_code, log_param_data,
			  log_param_length, param_str);

	param_entry_length = (sizeof(*phdr) + log_param_length);
	page_length -= param_entry_length;
	phdr = (log_parameter_header_t *)((uint8_t *)phdr + param_entry_length);
    }
    Printf(sdp, "\n");
    return(status);
}

/*
 * Log Page Counters and Other (unknown) Pages in JSON:
 */
char *
log_page_decode_to_json(scsi_device_t *sdp, io_params_t *iop, log_page_t *log_page, char *page_name)
{
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    /* Log Parameter List */
    JSON_Value  *pvalue = NULL;
    JSON_Object *pobject = NULL;
    JSON_Array	*pdesc_array;
    JSON_Value	*pdesc_value = NULL;
    JSON_Status json_status;
    char *json_string = NULL;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    uint8_t *ucp = NULL;
    int length = 0;
    /* Normal Declarations */
    log_page_header_t *hdr = &log_page->log_hdr;
    log_parameter_header_t *phdr = &log_page->log_phdr;
    uint8_t page_code = hdr->log_page_code;
    int page_length = (int)StoH(hdr->log_page_length);
    int param_length = 0;
    uint16_t log_parameter_code = 0;
    uint16_t log_param_length = 0;
    uint8_t device_type = iop->sip->si_inquiry->inq_dtype;
    void *log_param_data = NULL;
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

    ucp = (uint8_t *)hdr;
    length = sizeof(*hdr);
    if (sdp->report_format == REPORT_FULL) {
	json_status = json_object_set_number(object, "Length", (double)length);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_number(object, "Offset", (double)offset);
	if (json_status != JSONSuccess) goto finish;
	offset = FormatHexBytes(text, offset, ucp, length);
	json_status = json_object_set_string(object, "Bytes", text);
	if (json_status != JSONSuccess) goto finish;
    }
    json_status = PrintLogPageHeaderJson(sdp, object, hdr);
    if (json_status != JSONSuccess) goto finish;

    while (page_length > 0) {
	int param_entry_length = 0;
	char *param_str = NULL;
	JSON_Object *mobject = object;

	if (sdp->report_format == REPORT_FULL) {
	    if (pvalue == NULL) {
		pvalue  = json_value_init_object();
		pobject = json_value_get_object(pvalue);
	    }
	    if (pdesc_value == NULL) {
		pdesc_value = json_value_init_array();
		pdesc_array = json_value_get_array(pdesc_value);
	    }
	    mobject = pobject;
	}
	log_parameter_code = (uint16_t)StoH(phdr->log_parameter_code);
	log_param_length = phdr->log_parameter_length;
	log_param_data = (uint8_t *)phdr + sizeof(*phdr);

	ucp = (uint8_t *)phdr;
	param_length = sizeof(*phdr) + log_param_length;
	length = param_length;
	if (sdp->report_format == REPORT_FULL) {
	    json_status = json_object_set_number(mobject, "Length", (double)length);
	    if (json_status != JSONSuccess) break;
	    json_status = json_object_set_number(mobject, "Offset", (double)offset);
	    if (json_status != JSONSuccess) break;
	    offset = FormatHexBytes(text, offset, ucp, length);
	    json_status = json_object_set_string(mobject, "Bytes", text);
	    if (json_status != JSONSuccess) break;
	}
	json_status = PrintLogParameterHeaderJson(sdp, mobject, hdr, phdr);
	if (json_status != JSONSuccess) goto finish;


	if ( (isErrorCounterPage(hdr->log_page_code)) &&
	     (log_parameter_code < num_error_types) ) {
	    param_str = error_counter_types[log_parameter_code];
	}

	json_status = PrintLogParameterJson(sdp, mobject, phdr, log_parameter_code,
					    log_param_data, log_param_length, param_str);
	if (json_status != JSONSuccess) goto finish;

	page_length -= param_length;
	phdr = (log_parameter_header_t *)((uint8_t *)phdr + param_length);

	if (sdp->report_format == REPORT_FULL) {
	    json_status = json_array_append_value(pdesc_array, pvalue);
	    pvalue = NULL;
	    if (json_status != JSONSuccess) break;
	}
    }
    /* Add the Log Parameter List. */
    if (pdesc_value) {
	json_status = json_object_dotset_value(object, "Log Parameter List", pdesc_value);
	pdesc_value = NULL;
	if (json_status != JSONSuccess) goto finish;
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

hbool_t
isErrorCounterPage(uint8_t page_code)
{
    hbool_t counter_page = False;

    switch (page_code) {
	case LOG_WRITE_ERROR_PAGE:
	case LOG_READ_ERROR_PAGE:
	case LOG_READREV_ERROR_PAGE:
	case LOG_VERIFY_ERROR_PAGE:
	    counter_page = True;
	    break;
    }
    return(counter_page);
}

/*
 * General purpose display of parameter data for full/brief formats.
 */
void
PrintLogParameter(scsi_device_t *sdp, log_parameter_header_t *phdr, uint16_t param_code,
		  void *param_data, uint32_t param_length, char *param_str)
{
    char display[STRING_BUFFER_SIZE];
    uint8_t *bp = (u_char *)param_data;
    hbool_t unknown_size = FALSE;
    uint64_t counter = 0;

    if ( ((phdr->log_format_linking == BOUNDED_DATA_COUNTER) ||
	  (phdr->log_format_linking == BOUNDED_UNBOUNDED_DATA_COUNTER)) ) {
	if (param_length > sizeof(uint64_t)) {
	    unknown_size = True;
	} else if (param_length) {
	    counter = stoh(bp, param_length);
	}
    }

    if ( unknown_size || (phdr->log_format_linking == BINARY_FORMAT_LIST) ) {
	/*
	 *  --> Binary Format List Parameters <--
	 */
	if (param_length) {
	    if (param_str) {
		PrintAscii(sdp, param_str, "", DNL);
		PrintFields(sdp, bp, param_length);		/* Just hex data. */
	    } else {
		if (sdp->report_format == REPORT_FULL) {
		    PrintAscii(sdp, paramater_data_str, "", DNL);
		} else {
		    sprintf(display, "%s 0x%x, %s (%u)",
			    parameter_str, param_code,
			    paramater_data_str, param_length);
		    PrintAscii(sdp, display, "", DNL);
		}
		PrintHAFields(sdp, bp, param_length);		/* Hex and Ascii */
	    }
	}
    } else if (phdr->log_format_linking == ASCII_FORMAT_LIST) {
	/*
	 *  --> ASCII Format List <--
	 */
	memcpy(display, param_data, param_length);
	display[param_length] = '\0';
	PrintAscii(sdp, paramater_data_str, display, PNL);
    } else {
	/*
	 *  --> Bounded or Unbounded Data Counter <--
	 */
	if (param_str) {
	    PrintLongLong(sdp, param_str, counter, PNL);
	} else {
	    if (sdp->report_format == REPORT_FULL) {
		sprintf(display, "%s", counter_value_str);
	    } else {
		sprintf(display, "%s 0x%x, %s (%u)",
			parameter_str, param_code,
			counter_value_str, param_length);
	    }
	    PrintLongDecHex(sdp, display, counter, PNL);
	}
    }
    return;
}

JSON_Status
PrintLogParameterJson(scsi_device_t *sdp, JSON_Object *object,
		      log_parameter_header_t *phdr, uint16_t param_code,
		      void *param_data, uint32_t param_length, char *param_str)
{
    char text[STRING_BUFFER_SIZE];
    uint8_t *bp = (u_char *)param_data;
    hbool_t unknown_size = FALSE;
    uint64_t counter = 0;
    JSON_Status json_status = JSONSuccess;

    if ( ((phdr->log_format_linking == BOUNDED_DATA_COUNTER) ||
	  (phdr->log_format_linking == BOUNDED_UNBOUNDED_DATA_COUNTER)) ) {
	if (param_length > sizeof(uint64_t)) {
	    unknown_size = True;
	} else if (param_length) {
	    counter = stoh(bp, param_length);
	}
    }

    if ( unknown_size || (phdr->log_format_linking == BINARY_FORMAT_LIST) ) {
	/*
	 *  --> Binary Format List Parameters <--
	 */
	if (param_length) {
	    if (param_str) {
		int offset = 0;
		(void)FormatHexBytes(text, offset, bp, param_length);
		json_status = json_object_set_string(object, param_str, text);
	    } else {
		int offset = 0;
		(void)FormatHexBytes(text, offset, bp, param_length);
		json_status = json_object_set_string(object, paramater_data_str, text);
	    }
	}
    } else if (phdr->log_format_linking == ASCII_FORMAT_LIST) {
	/*
	 *  --> ASCII Format List <--
	 */
	memcpy(text, param_data, param_length);
	text[param_length] = '\0';
	json_status = json_object_set_string(object, paramater_data_str, text);
    } else {
	/*
	 *  --> Bounded or Unbounded Data Counter <--
	 */
	if (param_str) {
	    json_status = json_object_set_number(object, param_str, (double)counter);
	} else {
	    if (sdp->report_format == REPORT_FULL) {
		sprintf(text, "%s", counter_value_str);
	    } else {
		sprintf(text, "%s 0x%x, %s (%u)",
			parameter_str, param_code,
			counter_value_str, param_length);
	    }
	    json_status = json_object_set_number(object, text, (double)counter);
	}
    }
    return(json_status);
}

/* ============================================================================================== */

/*
 * Protocol Identifier Lookup Table/Function:
 */
typedef struct protocol_identifier_entry {
    uint8_t	protocol_identifier_code;
    char	*protocol_identifier_name;
} protocol_identifier_entry_t;

protocol_identifier_entry_t protocol_identifier_table[] = {
    { PROTOCOL_ID_Fibre_Channel_Protocol,	"Fibre_Channel_Protocol"	},
    { PROTOCOL_ID_Obsolete,			"Obsolete"			},
    { PROTOCOL_ID_Serial_Storage_Architecture,	"Serial Storage Architecture"	},
    { PROTOCOL_ID_Serial_Bus_Protocol,		"Serial Bus Protocol"		},
    { PROTOCOL_ID_SCSI_RDMA_Protocol,		"SCSI RDMA Protocol"		},
    { PROTOCOL_ID_Internet_SCSI_iSCSI,		"Internet SCSI (iSCSI)"		},
    { PROTOCOL_ID_SAS_Serial_SCSI_Protocol,	"SAS Serial SCSI Protocol"	},
    { PROTOCOL_ID_Automation_Drive_Interface,	"Automation/Drive Interface"	},
    { PROTOCOL_ID_AT_Attachment_Interface,	"AT Attachment Interface"	},
    { PROTOCOL_ID_USB_Attached_SCSI,		"USB Attached SCSI"		},
    { PROTOCOL_ID_SCSI_over_PCI_Express,	"SCSI over PCI Express"		},
    { PROTOCOL_ID_PCI_Express_Protocols,	"PCI Express Protocols"		},
    { PROTOCOL_ID_RESERVED_0x0C,		"Reserved 0x0C"			},
    { PROTOCOL_ID_RESERVED_0x0D,		"Reserved 0x0D"			},
    { PROTOCOL_ID_RESERVED_0x0E,		"Reserved 0x0E"			},
    { PROTOCOL_ID_No_Specific_Protocol,		"No Specific Protocol"		}
};
int num_protocol_identifier_entries = ( sizeof(protocol_identifier_table) / sizeof(protocol_identifier_entry_t) );

char *
find_protocol_identifier(uint8_t protocol_identifier)
{
    if (protocol_identifier < num_protocol_identifier_entries) {
	return( protocol_identifier_table[protocol_identifier].protocol_identifier_name );
    } else {
	return( "<reserved>" );
    }
}

/*
 * Identify Reason Lookup Table/Function:
 */
typedef struct identify_reason_entry {
    uint8_t	identify_reason_code;
    char	*identify_reason_name;
} identify_reason_entry_t;

identify_reason_entry_t identify_reason_table[] = {
    { REASON_Power_On,				"Power On"			},
    { REASON_Open_Connection_Request,		"Open Connection Request"	},
    { REASON_Hard_Reset,			"Hard Reset"			},
    { REASON_SMP_PHY_CONTROL_Function,		"SMP PHY CONTROL function"	},
    { REASON_Loss_of_Dword_Synchronization,	"Loss of Dword Synchronization"	},
    { REASON_Multiplexing_Sequence_Mixup,	"Multiplexing Sequence Mixup"	},
    { REASON_I_T_Nexus_Loss_Timer_Expired,	"I_T Nexus Loss Timer Expired"	},
    { REASON_Break_Timeout_Timer_Expired,	"Break Timeout Timer Expired"	},
    { REASON_Phy_Test_Function_Stopped,		"Phy Test Function Stopped"	},
    { REASON_Expander_Reduced_Functionality,	"Expander Reduced Functionality"}
};
int num_identify_reason_entries = ( sizeof(identify_reason_table) / sizeof(identify_reason_entry_t) );

char *
find_identify_reason(uint8_t identify_reason)
{
    if (identify_reason < num_identify_reason_entries) {
	return( identify_reason_table[identify_reason].identify_reason_name );
    } else {
	return( "<reserved>" );
    }
}

/*
 * Negotiated Link Rate Lookup Table/Function:
 */
typedef struct link_rate_entry {
    uint8_t	link_rate_code;
    char	*link_rate_name;
} link_rate_entry_t;

link_rate_entry_t link_rate_table[] = {
    { LINK_RATE_UNKNOWN,			"Phy enabled, Unknown Link Rate"	},
    { LINK_RATE_PHY_DISABLED,			"Phy Disabled"				},
    { LINK_RATE_SPEED_NEGOTIATION_FAILED,	"Phy Enabled, Speed Negotiation Failed"	},
    { LINK_RATE_SATA_SPINUP_HOLD_STATE,		"Phy Enabled, SATA Spinup Hold State"	},
    { LINK_RATE_PORT_SELECTOR,			"Phy Enabled, Port Selector"		},
    { LINK_RATE_RESET_IN_PROGRESS,		"Phy Enabled, Reset In Progress"	},
    { LINK_RATE_UNSUPPORTED_PHY_ATTACHED,	"Phy Enabled, Unsupported Phy Attached"	},
    { LINK_RATE_RESERVED_0x07,			"Reserved 0x07"				},
    { LINK_RATE_SPEED_1_5Gbps,			"1.5 Gbps"				},
    { LINK_RATE_SPEED_3Gbps,			"3 Gbps"				},
    { LINK_RATE_SPEED_6Gbps,			"6 Gbps"				},
    { LINK_RATE_SPEED_12Gbps,			"12 Gbps"				},
    { LINK_RATE_SPEED_22_5Gbps,			"22.5 Gbps"				},
    { LINK_RATE_RESERVED_0x0D,			"Reserved 0x0D"				},
    { LINK_RATE_RESERVED_0x0E,			"Reserved 0x0E"				},
    { LINK_RATE_RESERVED_0x0F,			"Reserved 0x0F"				}
};
int num_link_rate_entries = ( sizeof(link_rate_table) / sizeof(link_rate_entry_t) );

char *
find_link_rate(uint8_t link_rate)
{
    if (link_rate < num_link_rate_entries) {
	return( link_rate_table[link_rate].link_rate_name );
    } else {
	return( "<reserved>" );
    }
}

char *
find_phy_event_source(uint8_t phy_event_source)
{
    if (phy_event_source == PHY_EVENT_INVALID_DWORD_COUNT) {
	return("Invalid Dword Count");
    } else if (phy_event_source == PHY_EVENT_RUNNING_DISPARITY_ERROR_COUNT) {
	return("Running Disparity Error Count");
    } else if (phy_event_source == PHY_EVENT_LOSS_OF_DWORD_SYNC) {
	return("Loss of Dword Synchronization");
    } else if (phy_event_source == PHY_EVENT_PHY_RESET_PROBLEM) {
	return("Phy Reset Problem");
    } else {
	return("<unknown>");
    }
}

/*
 * Log Lookup Table/Functions:
 */
typedef struct log_page_entry {
    uint8_t		page_code;	/* The page code. */
    uint16_t		device_type;	/* THe device type (from Inquiry) */
    vendor_id_t		vendor_id;	/* The vendor ID (internal) */
    char		*page_name;	/* The page name. */
    char		*parse_name;	/* The parse name. */
} log_page_entry_t;

log_page_entry_t log_page_table[] = {
    { LOG_ALL_PAGES, 		ALL_DEVICE_TYPES,	VID_ALL,	"Supported",		"supported"		},
    { LOG_OVER_UNDER_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Buffer Overrun/Underrun Counter", "overrun_underrun" },
    { LOG_WRITE_ERROR_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Write Error Counter",	"write_error"		},
    { LOG_READ_ERROR_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Read Error Counter",	"read_error"		},
    { LOG_READREV_ERROR_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Read Reverse Error Counter", "read_reverse"	},
    { LOG_VERIFY_ERROR_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Verify Error Counter",	"verify_error"		},
    { LOG_NONMED_ERROR_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Non-medium error counter", "non-medium"	},
    { LOG_LASTN_EVENTS_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Last n error events",	"last_error_events"	},
    { LOG_FORMAT_STATUS_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Format Status",	"format_status"		},
    { LOG_LASTN_DEFFERED_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Last n Deferred or Async Events", "last_deferred_events" },
    { LOG_SEQUENTIAL_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Sequential-Access Device", "sequential_access"	},
    { LOG_BLOCK_PROVISION_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Logical Block Provisioning", "logical_block_provisioning" },
    { LOG_TEMPERATURE_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Temperature",		"temperature"		},
    { LOG_START_STOP_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Start-stop Cycle Counter", "start_stop_cycle"	},
    { LOG_APP_CLIENT_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Application Client",	"application_client"	},
    { LOG_SELF_TEST_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Self-Test Results",	"self_test_results"	},
    { LOG_SOLID_STATE_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Solid State Media",	"solid_state_media"	},
    { LOG_BACK_SCAN_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Background-Scan Results", "background_scan"	},
    { LOG_NONVOL_CACHE_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Non-Volatile Cache",	"non_volatile_cache"	},
    { LOG_PROTOCOL_SPEC_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Protocol Specific Port", "protocol_specific_port" },
    { LOG_STATS_PERF_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Statistics and Performance", "statistics_performance" },
    { LOG_INFO_EXCEPT_PAGE,	ALL_DEVICE_TYPES,	VID_ALL,	"Informational Exceptions", "informational"	}
};
int num_log_page_entries = ( sizeof(log_page_table) / sizeof(log_page_entry_t) );

uint8_t
find_log_page_code(scsi_device_t *sdp, char *page_name, int *status)
{
    log_page_entry_t *lpe = log_page_table;
    size_t length = strlen(page_name);
    int i;

    if (length == 0) {
	Printf(sdp, "\n");
	Printf(sdp, "Log Page Codes/Names:\n");
	for (i = 0; i < num_log_page_entries; i++, lpe++) {
	    Printf(sdp, "    0x%02x - %s (%s)\n",
		   lpe->page_code, lpe->page_name, lpe->parse_name);
	}
	Printf(sdp, "\n");
	*status = WARNING;
	return(LOG_PAGE_UNKNOWN);
    }

    /*
     * Note: Need to add device type and vendor ID checks, when implemented.
     */
    for (i = 0; i < num_log_page_entries; i++, lpe++) {
	/* Allow a matching a portion (start of string). */
	if (strncasecmp(lpe->parse_name, page_name, length) == 0) {
	    *status = SUCCESS;
	    return( lpe->page_code );
	}
    }
    *status = FAILURE;
    return(LOG_PAGE_UNKNOWN);
}

char *
get_log_page_name(uint8_t device_type, uint8_t page_code, uint8_t vendor_id)
{
    log_page_entry_t *lpe = log_page_table;
    char *page_name = NULL;
    int i;

    for (i = 0; i < num_log_page_entries; i++, lpe++) {
	if ( ((lpe->device_type == ALL_DEVICE_TYPES) || (lpe->device_type == device_type)) &&
	     (lpe->page_code == page_code) &&
	     ((lpe->vendor_id == VID_ALL) || (lpe->vendor_id == vendor_id)) ) {
	    return( lpe->page_name );
	}
    }
    if (page_code == LOG_LAST_RESERVED) {
	page_name = "Reserved";
    } else if ( (page_code >= LOG_VENDOR_START) &&
		(page_code <= LOG_VENDOR_END) ) {
	page_name = "Vendor Specific";
    } else {
	page_name = "Unknown";
    }
    return(page_name);
}
