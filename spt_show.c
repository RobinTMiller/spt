/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2021			    *
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
 * Module:	spt_show.c
 * Author:	Robin T. Miller
 * Date:	February 9th, 2018
 *
 * Description:
 *	This module provides functions to show various information.
 * 
 * Modification History:
 * 
 * April 3rd, 2021 by Robin T. Miller
 *      Add support for Solaris.
 * 
 * September 8th, 2020 by Robin T. Miller
 *      Add support for AIX.
 * 
 * May 27th, 2019 by Robin T. Miller
 *      Handle case where we don't have a device name or SCSI name to match.
 * 
 * April 26th, 2019 by Robin T. Miller
 *      Switch from strtok() to reentrant version strtok_r() to avoid issues
 * when multiple functions invoke this API and to be thread safe (libspt.so).
 * 
 * March 17th, 2018 by Robin T. Miller
 *      Update device path display to account for SCSI generic device (sg)
 * being tracked with the Linux device information. This mapping makes it
 * easier to parse the JSON device entries, since "sg" to "sd" mapping is
 * already done. Also for JSON, report the per device path target port.
 * 
 * March 1st, 2018 by Robin T. Miller
 *	For brief display, strip leading/trailing serial number spaces.
 *	Add displaying vendor ID in brief output (oversight on my part).
 *      Update device type parsing to allow comma separated list, since many
 * people desire just disks and enclosures, so "dtype=enclosure,direct".
 * 
 * February 20th, 2018 by Robin T. Miller
 *      Allow customizable show devices brief fields.
 *      Change "Device Identification" to "World Wide Name" (more common).
 * 
 * February 16th, 2018 by Robin T. Miller
 *      Move device name filtering to each show function, since we cannot do
 * this filtering at the OS layer, otherwise we do *not* get all device paths.
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
#include "spt_devices.h"

#include "parson.h"

#if defined(_WIN32)
#  include "spt_win.h"
#else /* !defined(_WIN32) */
#  include "spt_unix.h"
#endif /* defined(_WIN32) */

/*
 * Forward Declarations:
 */
void show_devices_format(scsi_device_t *sdp, scsi_device_entry_t *sdeh);
int FmtShowDevices(scsi_device_t *sdp, scsi_device_entry_t *sdep, char *format, char *buffer);

hbool_t match_user_filters(scsi_device_entry_t *sdep, scsi_filters_t *sfp);
void show_devices_brief(scsi_device_t *sdp, scsi_device_entry_t *sdeh);
void show_devices_full(scsi_device_t *sdp, scsi_device_entry_t *sdeh);
char *show_devices_to_json(scsi_device_t *sdp);
void FreeScsiDeviceTable(scsi_generic_t *sgp);
void FreeScsiFilters(scsi_device_t *sdp, scsi_filters_t *sfp);

static char *na_str = "N/A";
static char *not_available_str = "<not available>";

/*
 * parse_show_devices_args() - Parse the show devices keywords.
 *
 * Inputs:
 *      sdp = The SCSI device information.
 *      argv = the argument array.
 *      argc = The argument count.
 *      arg_index = Pointer to current arg index.
 *
 * Outputs:
 *	string = Updated input argument pointer.
 *
 * Return Value:
 *	Returns SUCCESS / FAILURE
 */ 
#define MAX_DTYPES 10

int
parse_show_devices_args(scsi_device_t *sdp, char **argv, int argc, int *arg_index)
{
    scsi_filters_t *sfp = &sdp->scsi_filters;
    char *string;
    int status = SUCCESS;

    for ( ; *arg_index < argc; (*arg_index)++) {
	string = argv[*arg_index];

	if ( match(&string, "device=") || match(&string, "devices=") ) {
	    if (sfp->device_paths) free(sfp->device_paths);
	    sfp->device_paths = strdup(string);
	    continue;
	}
	if ( match(&string, "device_type=") || match(&string, "dtype=") ||
	     match(&string, "device_types=") || match(&string, "dtypes=") ) {
	    char *token, *sep, *str, *saveptr;
	    uint8_t device_type, device_types[MAX_DTYPES];
	    int num_dtypes = 0;
	    /* Allow comma separated list of device types. */
	    str = strdup(string);
	    sep = ",";
	    token = strtok_r(str, sep, &saveptr);
	    while (token) {
		/* Note: Overloading dtype={hex|string} */
		if ((*token == '\0') || (isHexString(token) == False)) {
		    int status;
		    device_type = GetDeviceTypeCode(sdp, token, &status);
		    if (status == WARNING) {
			return( HandleExit(sdp, status) );
		    } else if (device_type == DTYPE_UNKNOWN) {
			Eprintf(sdp, "Did not find device type '%s'!\n", token);
			return( HandleExit(sdp, FAILURE) );
		    }
		} else { /* assume hex */
		    device_type = (uint8_t)number(sdp, token, HEX_RADIX, &status, False);
		}
		device_types[num_dtypes++] = device_type;
		if (num_dtypes == MAX_DTYPES) {
		    break;
		}
		token = strtok_r(NULL, sep, &saveptr);
	    }
	    if (num_dtypes) {
		sfp->device_types = Malloc(sdp, sizeof(uint8_t) * (num_dtypes + 1));
		memcpy(sfp->device_types, device_types, num_dtypes);
		sfp->device_types[num_dtypes] = DTYPE_UNKNOWN;
	    }
            continue;
	}
	if ( match(&string, "exclude=") ) {
	    if (sfp->exclude_paths) free(sfp->exclude_paths);
	    sfp->exclude_paths = strdup(string);
	    continue;
	}
	if ( match(&string, "product=") || match(&string, "pid=") ) {
	    if (sfp->product) free(sfp->product);
	    sfp->product = strdup(string);
	    continue;
	}
	if ( match(&string, "vendor=") || match(&string, "vid=") ) {
	    if (sfp->vendor) free(sfp->vendor);
	    sfp->vendor = strdup(string);
	    continue;
	}
	if ( match(&string, "revision=") || match(&string, "rev=") ) {
	    if (sfp->revision) free(sfp->revision);
	    sfp->revision = strdup(string);
	    continue;
	}
	if ( match(&string, "fw_version=") || match(&string, "fwver=") ) {
	    if (sfp->fw_version) free(sfp->fw_version);
	    sfp->fw_version = strdup(string);
	    continue;
	}
	if ( match(&string, "device_id=") || match(&string, "did=") || match(&string, "wwn=") ) {
	    if (sfp->device_id) free(sfp->device_id);
	    sfp->device_id = strdup(string);
	    continue;
	}
	if ( match(&string, "serial=") ) {
	    if (sfp->serial) free(sfp->serial);
	    sfp->serial = strdup(string);
	    continue;
	}
	if ( match(&string, "target_port=") || match(&string, "tport=") || match(&string, "sas_address=") ) {
	    if (sfp->target_port) free(sfp->target_port);
	    sfp->target_port = strdup(string);
	    continue;
	}
	if ( match(&string, "show-fields=") || match(&string, "sflds=") || match(&string, "fields=")) {
	    if (sdp->show_fields) free(sdp->show_fields);
	    sdp->show_fields = strdup(string);
	    continue;
	}
	if ( match(&string, "show-format=") || match(&string, "sfmt=") || match(&string, "format=") ) {
	    if (sdp->show_format) free(sdp->show_format);
	    sdp->show_format = strdup(string);
	    continue;
	}
	if ( match(&string, "show-path=") || match(&string, "spath=") || match(&string, "path=") ||
	     match(&string, "show-paths=") || match(&string, "spaths=") || match(&string, "paths=") ) {
	    if (sdp->show_paths) free(sdp->show_paths);
	    /* On Linux, we don't show all DM-MP paths, so allow simple way to enable all paths. */
	    if ( match(&string, "all") || match(&string, "*") ) {
		sfp->all_device_paths = True;
	    } else {
		sdp->show_paths = strdup(string);
	    }
	    continue;
	}
	/* Unknown keyword, so simply break and continue parsing other args. */
	( (*arg_index)-- );
	break;
    }
    return(status);
}

/*
 * Setup the head of the SCSI Device Table.
 */
scsi_device_entry_t scsiDeviceTable = { .sde_flink = &scsiDeviceTable, .sde_blink = &scsiDeviceTable };

int
show_devices(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp)
{
    scsi_device_entry_t *sdeh = &scsiDeviceTable;
    int status = SUCCESS;

    /* TODO: Implement this for other operating systems, esp. Windows! */
#if defined(_AIX) || defined(__linux__) || defined(SOLARIS) || defined(_WIN32)
    if (sdp->show_caching_flag == False) {
	/* Empty the list to avoid cached information. */
	FreeScsiDeviceTable(sgp);
    }
    if (sdeh->sde_flink == sdeh) {
	status = os_find_scsi_devices(sgp, &sdp->scsi_filters, sdp->show_paths);
    }
#endif /* defined(_AIX) || defined(__linux__) || defined(SOLARIS) || defined(_WIN32) */
    if (sdeh->sde_flink == sdeh) {
	FreeScsiFilters(sdp, &sdp->scsi_filters);
	return(WARNING); /* Empty list! */
    }
    if (sdp->show_format) {
	show_devices_format(sdp, sdeh);
    } else if (sdp->output_format == JSON_FMT) {
	char *json_string = NULL;
	json_string = show_devices_to_json(sdp);
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
    } else if (sdp->report_format == REPORT_BRIEF) {
	show_devices_brief(sdp, sdeh);
    } else {
	show_devices_full(sdp, sdeh);
    }
    FreeScsiFilters(sdp, &sdp->scsi_filters);
    return(status);
}

/*
 * Lookup a device path in a comma separated list of paths.
 */
hbool_t
match_device_paths(char *device_path, char *paths)
{
    char *sep = ",";
    char *devs = strdup(paths);
    char *saveptr;
    char *path = strtok_r(devs, sep, &saveptr);
    hbool_t device_found = False;

    while (path != NULL) {
	if (strcmp(path, device_path) == 0) {
	    device_found = True;
	    break;
	}
	path = strtok_r(NULL, sep, &saveptr);
    }
    free(devs);
    return(device_found);
}

hbool_t
match_user_filters(scsi_device_entry_t *sdep, scsi_filters_t *sfp)
{
    scsi_device_name_t *sdnh = &sdep->sde_names;
    scsi_device_name_t *sdnp;

    if ( (sfp->device_paths == NULL) && (sfp->serial == NULL) &&
	 (sfp->device_id == NULL) && (sfp->target_port == NULL) ) {
	return(True);
    }

    for (sdnp = sdnh->sdn_flink; (sdnp != sdnh); sdnp = sdnp->sdn_flink) {
	if (sfp->device_paths) {
	    if ( sdnp->sdn_device_path &&
		 match_device_paths(sdnp->sdn_device_path, sfp->device_paths) ) {
		return(True);
	    }
	    if ( sdnp->sdn_scsi_path &&
		 match_device_paths(sdnp->sdn_scsi_path, sfp->device_paths) ) {
		return(True);
	    }
	}
	if (sfp->device_id && sdep->sde_device_id) {
	    if (strcmp(sfp->device_id, sdep->sde_device_id) == 0) {
		return(True);
	    }
	}
	if (sfp->serial && sdep->sde_serial) {
	    /* Use substring search due to leading spaces in serial number! */
	    if (strstr(sdep->sde_serial, sfp->serial)) {
		return(True);
	    }
	}
	if (sfp->target_port && sdep->sde_target_port) {
	    if (strcmp(sfp->target_port, sdep->sde_target_port) == 0) {
		return(True);
	    }
	}
    }
    return(False);
}

#define FNAME_DEVICE_TYPE	"device_type"
#define FNAME_VENDOR		"vendor"
#define FNAME_PRODUCT		"product"
#define FNAME_REVISION		"revision"
#define FNAME_FWVERSION		"fw_version"
#define FNAME_DEVICE_ID		"device_id"
#define FNAME_SERIAL		"serial"
#define FNAME_TARGET_PORT	"target_port"
#define FNAME_PATHS		"paths"

typedef struct show_brief_table {
    char	*fname;			/* Full field name.		*/
    char	*sname;			/* Short field name.		*/
    char	*header;		/* The field header.		*/
    char	*fmt;			/* The format control string.	*/
} show_brief_table_t ;

static show_brief_table_t show_brief_table[] = {
    { .fname = FNAME_DEVICE_TYPE, .sname = "dtype",	.header = "Device Type",	.fmt = "%-11.11s"	},
    { .fname = FNAME_VENDOR,	  .sname = "vid",	.header = " Vendor ",		.fmt = "%-8.8s"		},
    { .fname = FNAME_PRODUCT,	  .sname = "pid",	.header = "     Product    ", 	.fmt = "%-16.16s"	},
    { .fname = FNAME_REVISION,	  .sname = "rev",	.header = "Revision",	 	.fmt = "  %-4.4s  "	},
    { .fname = FNAME_FWVERSION,	  .sname = "fwver",	.header = "FW Version",	 	.fmt = " %-8.8s "	},
    { .fname = FNAME_DEVICE_ID,	  .sname = "wwn",	.header = " World Wide Name  ",	.fmt = "%-18.18s"	},
    { .fname = FNAME_SERIAL,	  .sname = NULL,	.header = "  Serial Number   ",	.fmt = "%-18.18s"	},
    { .fname = FNAME_TARGET_PORT, .sname = "tport",	.header = "   Target Port    ",	.fmt = "%-18.18s"	},
    { .fname = FNAME_PATHS,	  .sname = NULL,	.header = "Device Paths",	.fmt = "%s"		}
};
static int num_show_entries = sizeof(show_brief_table) / sizeof(show_brief_table[0]);

/* Default Show Devices Format: */
/* Remember, the SPT_SHOW_FIELDS environment variable can override this! */
static char *show_brief_fields = "dtype,vid,pid,rev,serial,paths";

void
show_devices_brief(scsi_device_t *sdp, scsi_device_entry_t *sdeh)
{
    show_brief_table_t *stp;
    scsi_filters_t *sfp = &sdp->scsi_filters;
    scsi_device_entry_t *sdep;
    char line1[STRING_BUFFER_SIZE];
    char line2[STRING_BUFFER_SIZE];
    char paths[STRING_BUFFER_SIZE];
    char *bp = NULL, *pp, *lp1 = line1, *lp2 = line2;
    char *str, *sep, *token, *saveptr;
    char *show_fields = (sdp->show_fields) ? sdp->show_fields : show_brief_fields;
    hbool_t token_found = False;
    int entries;

    /* Copy string to avoid clobbering due to strtok_r() API! */
    str = strdup(show_fields);
    sep = ",";
    token = strtok_r(str, sep, &saveptr);
    while (token != NULL) {
	token_found = False;
	for (stp = show_brief_table, entries = 0; entries < num_show_entries; entries++, stp++) {
	    if ( (strcmp(token, stp->fname) == 0) ||
		 (stp->sname && (strcmp(token, stp->sname) == 0)) ) {
		/* Matched a table entry. */
		size_t i = 0, hdrlen = strlen(stp->header);
		lp1 += sprintf(lp1, "%s", stp->header);
		for (i = 0; i < hdrlen; i++) {
		    lp2 += sprintf(lp2, "%c", '-');
		}
		*lp1++ = ' '; *lp1 = '\0';
		*lp2++ = ' '; *lp2 = '\0';
		token_found = True;
	    }
	}
	if (token_found == False) {
	    Eprintf(sdp, "Invalid show devices field name: %s\n", token);
	    return;
	}
	token = strtok_r(NULL, sep, &saveptr);
    }
    if (sdp->show_header_flag) {
	Printf(sdp, "%s\n", line1);
	Printf(sdp, "%s\n", line2);
    }

    for (sdep = sdeh->sde_flink; (sdep != sdeh); sdep = sdep->sde_flink) {
	scsi_device_name_t *sdnh = &sdep->sde_names;
	scsi_device_name_t *sdnp;
	char *device_type = GetDeviceType(sdep->sde_device_type, False);

	if (match_user_filters(sdep, sfp) == False) {
	    continue;
	}
	/* Note: This could be optimized rather than parsing for each entry! */
	str = strcpy(str, show_fields);
	token = strtok_r(str, sep, &saveptr);
	bp = line1;
	while (token != NULL) {
	    for (stp = show_brief_table, entries = 0; entries < num_show_entries; entries++, stp++) {
		if ( (strcmp(token, stp->fname) == 0) ||
		     (stp->sname && (strcmp(token, stp->sname) == 0)) ) {
		    /* Matched a table entry, now process each field. */
		    if ( strcmp(stp->fname, FNAME_DEVICE_TYPE) == 0) {
			bp += sprintf(bp, stp->fmt, device_type);
			*bp++ = ' '; *bp = '\0';
			break;
		    }
		    if ( strcmp(stp->fname, FNAME_VENDOR) == 0) {
			bp += sprintf(bp, stp->fmt, sdep->sde_vendor);
			*bp++ = ' '; *bp = '\0';
			break;
		    }
		    if ( strcmp(stp->fname, FNAME_PRODUCT) == 0) {
			bp += sprintf(bp, stp->fmt, sdep->sde_product);
			*bp++ = ' '; *bp = '\0';
			break;
		    }
		    if ( strcmp(stp->fname, FNAME_REVISION) == 0) {
			bp += sprintf(bp, stp->fmt, sdep->sde_revision);
			*bp++ = ' '; *bp = '\0';
			break;
		    }
		    if ( strcmp(stp->fname, FNAME_FWVERSION) == 0) {
			if (sdep->sde_fw_version) {
			    bp += sprintf(bp, stp->fmt, sdep->sde_fw_version);
			} else {
			    bp += sprintf(bp, stp->fmt, sdep->sde_revision);
			}
			*bp++ = ' '; *bp = '\0';
			break;
		    }
		    if ( strcmp(stp->fname, FNAME_DEVICE_ID) == 0) {
			bp += sprintf(bp, stp->fmt, (sdep->sde_device_id) ? sdep->sde_device_id : not_available_str);
			*bp++ = ' '; *bp = '\0';
			break;
		    }
		    if ( strcmp(stp->fname, FNAME_SERIAL) == 0) {
			if (sdep->sde_serial == NULL) {
			    bp += sprintf(bp, stp->fmt, not_available_str);
			} else {
			    char serial[MEDIUM_BUFFER_SIZE];
			    char *sbp = serial;
			    char *snp = sdep->sde_serial;
			    memset(serial, '\0', sizeof(serial));
			    /* Copy the serial number without the goofy spaces! */
			    while (*snp) {
				if (*snp != ' ') {
				    *sbp = *snp;
				    sbp++;
				}
				snp++;
			    }
			    bp += sprintf(bp, stp->fmt, serial);
			}
			*bp++ = ' '; *bp = '\0';
			break;
		    }
		    if ( strcmp(stp->fname, FNAME_TARGET_PORT) == 0) {
			bp += sprintf(bp, stp->fmt, (sdep->sde_target_port) ? sdep->sde_target_port : not_available_str);
			*bp++ = ' '; *bp = '\0';
			break;
		    }
		    if ( strcmp(stp->fname, FNAME_PATHS) == 0) {
			pp = paths;
			for (sdnp = sdnh->sdn_flink; (sdnp != sdnh); sdnp = sdnp->sdn_flink) {
			    pp += sprintf(pp, "%s ", sdnp->sdn_device_path);
			    if (sdnp->sdn_scsi_path) {
				pp += sprintf(pp, "%s ", sdnp->sdn_scsi_path);
			    }
			}
			pp[-1] = '\0'; /* Remove the trailing space. */
			bp += sprintf(bp, stp->fmt, paths);
			*bp++ = ' '; *bp = '\0';
			break;
		    }
		}
	    }
	    token = strtok_r(NULL, sep, &saveptr);
	}
	if (bp[-1] == ' ') {
	    bp[-1] = '\0';
	}
	Printf(sdp, "%s\n", line1);
    }
    Free(sdp, str); str = NULL;
    return;
}

void
show_devices_full(scsi_device_t *sdp, scsi_device_entry_t *sdeh)
{
    scsi_filters_t *sfp = &sdp->scsi_filters;
    scsi_device_entry_t *sdep;
    char paths[STRING_BUFFER_SIZE];
    char *bp = NULL;

    PrintHeader(sdp, "SCSI Device Information");
    for (sdep = sdeh->sde_flink; (sdep != sdeh); sdep = sdep->sde_flink) {
	scsi_device_name_t *sdnh = &sdep->sde_names;
	scsi_device_name_t *sdnp;
	char *device_type = GetDeviceType(sdep->sde_device_type, True);

	if (match_user_filters(sdep, sfp) == False) {
	    continue;
	}
	bp = paths;
	for (sdnp = sdnh->sdn_flink; (sdnp != sdnh); sdnp = sdnp->sdn_flink) {
	    bp += sprintf(bp, "%s ", sdnp->sdn_device_path);
	    if (sdnp->sdn_scsi_path) {
		bp += sprintf(bp, "%s ", sdnp->sdn_scsi_path);
	    }
	}
	bp[-1] = '\0'; /* Remove the trailing space. */
	PrintAscii(sdp, "Device Paths", paths, PNL);

	PrintHex(sdp, "Peripheral Device Type", sdep->sde_device_type, DNL);
	Print(sdp, " (%s)\n", device_type);
	PrintAscii(sdp, "Vendor Identification", sdep->sde_vendor, PNL);
	PrintAscii(sdp, "Product Identification", sdep->sde_product, PNL);
	PrintAscii(sdp, "Firmware Revision Level", sdep->sde_revision, PNL);
	PrintAscii(sdp, "Full Firmware Version", (sdep->sde_fw_version) ? sdep->sde_fw_version : not_available_str, PNL);
	PrintAscii(sdp, "Product Serial Number", (sdep->sde_serial) ? sdep->sde_serial : not_available_str, PNL);
	PrintAscii(sdp, "Device World Wide Name", (sdep->sde_device_id) ? sdep->sde_device_id : not_available_str, PNL);
	PrintAscii(sdp, "Device Target Port", (sdep->sde_target_port) ? sdep->sde_target_port : not_available_str, PNL);
	Printf(sdp, "\n");
    }
    return;
}

/*
 * Show Devices in JSON Format.
 */
char *
show_devices_to_json(scsi_device_t *sdp)
{
    scsi_filters_t *sfp = &sdp->scsi_filters;
    scsi_device_entry_t *sdeh = &scsiDeviceTable;
    scsi_device_entry_t *sdep;
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    JSON_Value  *svalue = NULL;
    JSON_Object *sobject = NULL;
    JSON_Array	*device_array;
    JSON_Value	*device_value = NULL;
    /* For device type array. */
    JSON_Value  *dtvalue = NULL;
    JSON_Object *dtobject = NULL;
    JSON_Array	*dtype_array;
    JSON_Value	*dtype_value = NULL;
    JSON_Status json_status;
    char *json_string = NULL;
    char nexus[SMALL_BUFFER_SIZE];
    char paths[STRING_BUFFER_SIZE];
    char *bp = paths;
    int status = SUCCESS;

    root_value  = json_value_init_object();
    if (root_value == NULL) return(NULL);
    root_object = json_value_get_object(root_value);

    value = json_value_init_object();
    if (value == NULL) {
	json_value_free(root_value);
	return(NULL);
    }
    json_status = json_object_dotset_value(root_object, "SCSI Devices", value);
    if (json_status != JSONSuccess) goto finish;
    object = json_value_get_object(value);

    for (sdep = sdeh->sde_flink; (sdep != sdeh); sdep = sdep->sde_flink) {
	scsi_device_name_t *sdnh = &sdep->sde_names;
	scsi_device_name_t *sdnp;
	char *device_type = GetDeviceType(sdep->sde_device_type, True);

	if (match_user_filters(sdep, sfp) == False) {
	    continue;
	}
	if (device_value == NULL) {
	    device_value = json_value_init_array();
	    device_array = json_value_get_array(device_value);
	}
	if (svalue == NULL) {
	    svalue  = json_value_init_object();
	    sobject = json_value_get_object(svalue);
	}
	json_status = json_object_set_number(sobject, "Peripheral Device Type", (double)sdep->sde_device_type);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_string(sobject, "Peripheral Device Type Description", device_type);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_string(sobject, "Vendor Identification", sdep->sde_vendor);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_string(sobject, "Product Identification", sdep->sde_product);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_string(sobject, "Firmware Revision Level", sdep->sde_revision);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_string(sobject, "Full Firmware Version",
					     (sdep->sde_fw_version) ? sdep->sde_fw_version : not_available_str);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_string(sobject, "Product Serial Number",
					     (sdep->sde_serial) ? sdep->sde_serial : not_available_str);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_string(sobject, "Device World Wide Name", 
					     (sdep->sde_device_id) ? sdep->sde_device_id : not_available_str);
	if (json_status != JSONSuccess) goto finish;
	json_status = json_object_set_string(sobject, "Device Target Port", 
					     (sdep->sde_target_port) ? sdep->sde_target_port : not_available_str);
	if (json_status != JSONSuccess) goto finish;

	bp = paths;
	for (sdnp = sdnh->sdn_flink; (sdnp != sdnh); sdnp = sdnp->sdn_flink) {
	    char *device_path_type = NULL;
	    bp += sprintf(bp, "%s ", sdnp->sdn_device_path);
	    if (sdnp->sdn_scsi_path) {
		bp += sprintf(bp, "%s ", sdnp->sdn_scsi_path);
	    }
#if defined(__linux__)
	    device_path_type = os_get_device_path_type(sdnp);
#else /* !defined(__linux__) */
	    device_path_type = "Device Path";	/* This may vary by OS. */
#endif /* defined(__linux__) */
	    if (device_path_type) {
		if (dtvalue == NULL) {
		    dtvalue  = json_value_init_object();
		    dtobject = json_value_get_object(dtvalue);
		}
		if (dtype_value == NULL) {
		     dtype_value = json_value_init_array();
		     dtype_array = json_value_get_array(dtype_value);
		}
		json_status = json_object_set_string(dtobject, device_path_type, sdnp->sdn_device_path);
		if (json_status != JSONSuccess) goto finish;
		/* Report SCSI Nexus in 'lsscsi' format. */
		(void)sprintf(nexus, "[%d:%d:%d:%d]",
			      sdnp->sdn_bus, sdnp->sdn_channel, sdnp->sdn_target, sdnp->sdn_lun);
		json_status = json_object_set_string(dtobject, "SCSI Nexus", nexus);
		if (json_status != JSONSuccess) goto finish;

		if (sdnp->sdn_scsi_path) {
		    json_status = json_object_set_string(dtobject, "SCSI Device", sdnp->sdn_scsi_path);
		    if (json_status != JSONSuccess) goto finish;
		}
		if (sdnp->sdn_target_port) {
		    json_status = json_object_set_string(dtobject, "Device Target Port", sdnp->sdn_target_port);
		    if (json_status != JSONSuccess) goto finish;
		}
		json_array_append_value(dtype_array, dtvalue);
		dtvalue = NULL;
	    }
	}
	bp[-1] = '\0'; /* Remove the trailing space. */
	json_status = json_object_set_string(sobject, "Device Paths", paths);
	if (json_status != JSONSuccess) goto finish;

	if (dtype_value) {
	    json_object_set_value(sobject, "Path Types", dtype_value);
	    dtype_value = NULL;
	}

	json_array_append_value(device_array, svalue);
	svalue = NULL;
    }
    if (device_value) {
	json_object_set_value(object, "Device List", device_value);
	device_value = NULL;
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

void
show_devices_format(scsi_device_t *sdp, scsi_device_entry_t *sdeh)
{
    scsi_device_entry_t *sdep;
    char buffer[STRING_BUFFER_SIZE];
    char *bp = NULL;

    for (sdep = sdeh->sde_flink; (sdep != sdeh); sdep = sdep->sde_flink) {
	(void)FmtShowDevices(sdp, sdep, sdp->show_format, buffer);
	Printf(sdp, "%s\n", buffer);
    }
    return;
}

/*
 * FmtShowDevices() - Format Show Devices String. 
 *  
 * Show Format Control Strings: 
 *	%paths = The device paths.
 *	%device_type or %dtype = The device type.
 *	%product or %pid = The product identification.
 *	%vendor or %vid = The vendor identification.
 *      %revision or %rev = The firmware revision level.
 *      %fw_version or %fwrev = The full FW version.
 *	%serial = The device serial number.
 *	%device_id or %did = The device identification (WWID).
 *      %target_port or %tport = The device target port (SAS address).
 *
 * Outputs:
 *	Returns the formatted buffer length.
 */
int
FmtShowDevices(scsi_device_t *sdp, scsi_device_entry_t *sdep, char *format, char *buffer)
{
    char    *from = format;
    char    *to = buffer;
    int	    flen = 0;
    ssize_t length = strlen(format);

    while (length-- > 0) {
	int slen = 0;
	/*
	 * Parse format control keywords:
	 */
	if (*from == '%') {
	    char *key = (from + 1);
	    /*
	     * The approach taken is to allow upper or lower case.
	     */
	    if ( (strncasecmp(key, "dtype", flen=5) == 0) ||
		 (strncasecmp(key, "device_type", flen=11) == 0) ) {
		char *device_type = GetDeviceType(sdep->sde_device_type, False);
		slen = Sprintf(to, "%s", device_type);
		to += slen;
		from += (flen + 1);
		length -= flen;
		continue;
	    } else if (strncasecmp(key, "paths", flen=5) == 0) {
		scsi_device_name_t *sdnh = &sdep->sde_names;
		scsi_device_name_t *sdnp;
		char paths[STRING_BUFFER_SIZE];
		char *bp = paths;
		for (sdnp = sdnh->sdn_flink; (sdnp != sdnh); sdnp = sdnp->sdn_flink) {
		    bp += sprintf(bp, "%s ", sdnp->sdn_device_path);
		}
		bp[-1] = '\0';
		slen = Sprintf(to, "%s", paths);
		to += slen;
		from += (flen + 1);
		length -= flen;
		continue;
	    } else if ( (strncasecmp(key, "pid", flen=3) == 0) ||
			(strncasecmp(key, "product", flen=7) == 0) ) {
		slen = Sprintf(to, "%s", sdep->sde_product);
		to += slen;
		from += (flen + 1);
		length -= flen;
		continue;
	    } else if ( (strncasecmp(key, "vid", flen=3) == 0) ||
			(strncasecmp(key, "vendor", flen=6) == 0) ) {
		slen = Sprintf(to, "%s", sdep->sde_vendor);
		to += slen;
		from += (flen + 1);
		length -= flen;
		continue;
	    } else if ( (strncasecmp(key, "rev", flen=3) == 0) ||
			(strncasecmp(key, "revision", flen=8) == 0) ) {
		slen = Sprintf(to, "%s", sdep->sde_revision);
		to += slen;
		from += (flen + 1);
		length -= flen;
		continue;
	    } else if ( (strncasecmp(key, "fwver", flen=5) == 0) ||
			(strncasecmp(key, "fw_version", flen=10) == 0) ) {
		slen = Sprintf(to, "%s", (sdep->sde_fw_version) ? sdep->sde_fw_version : not_available_str);
		to += slen;
		from += (flen + 1);
		length -= flen;
		continue;
	    } else if (strncasecmp(key, "serial", flen=6) == 0) {
		slen = Sprintf(to, "%s", (sdep->sde_serial) ? sdep->sde_serial : not_available_str);
		to += slen;
		from += (flen + 1);
		length -= flen;
		continue;
	    } else if ( (strncasecmp(key, "did", flen=3) == 0) ||
			(strncasecmp(key, "wwn", flen=3) == 0) ||
			(strncasecmp(key, "device_id", flen=9) == 0) ) {
		slen = Sprintf(to, "%s", (sdep->sde_device_id) ? sdep->sde_device_id : not_available_str);
		to += slen;
		from += (flen + 1);
		length -= flen;
		continue;
	    } else if ( (strncasecmp(key, "tport", flen=5) == 0) ||
			(strncasecmp(key, "target_port", flen=11) == 0) ) {
		slen = Sprintf(to, "%s", (sdep->sde_target_port) ? sdep->sde_target_port : not_available_str);
		to += slen;
		from += (flen + 1);
		length -= flen;
		continue;
	    } else {
		*to++ = *from++;
		continue;
	    }
	} else if (*from == '\\') { /* not '%' */
	    switch (*(from + 1)) {
		case 'n': {
		  to += Sprintf(to, "\n");
		  break;
		}
		case 't': {
		  to += Sprintf(to, "\t");
		  break;
		}
		default: {
		  *to++ = *from;
		  *to++ = *(from + 1);
		  break;
		}
	    } /* end switch */
	    length--;
	    from += 2;
	    continue;
	} else { /* not '%' or '\\' */
	    *to++ = *from++;
	    continue;
	} /* end if '%' */
    } /* while (length-- > 0) */
    *to = '\0';	      /* NULL terminate! */
    return( (int)strlen(buffer) );
}

void
FreeScsiDeviceTable(scsi_generic_t *sgp)
{
    void *opaque = sgp->tsp->opaque;
    scsi_device_entry_t *sdeh = &scsiDeviceTable;
    scsi_device_entry_t *sdep;

    while ( (sdep = sdeh->sde_flink) != sdeh) {
	scsi_device_name_t *sdnh = &sdep->sde_names;
	scsi_device_name_t *sdnp;
	while ( (sdnp = sdnh->sdn_flink) != sdnh) {
	    sdnp->sdn_blink->sdn_flink = sdnp->sdn_flink;
	    sdnp->sdn_flink->sdn_blink = sdnp->sdn_blink;
	    Free(opaque, sdnp->sdn_device_path);
	    if (sdnp->sdn_scsi_path) {
		Free(opaque, sdnp->sdn_scsi_path);
	    }
	    if (sdnp->sdn_target_port) {
		Free(opaque, sdnp->sdn_target_port);
	    }
	    Free(opaque, sdnp);
	}
	if (sdep->sde_product) {
	    Free(opaque, sdep->sde_product);
	    sdep->sde_product = NULL;
	}
	if (sdep->sde_vendor) {
	    Free(opaque, sdep->sde_vendor);
	    sdep->sde_vendor = NULL;
	}
	if (sdep->sde_revision) {
	    Free(opaque, sdep->sde_revision);
	    sdep->sde_revision = NULL;
	}
	if (sdep->sde_fw_version) {
	    Free(opaque, sdep->sde_fw_version);
	    sdep->sde_fw_version = NULL;
	}
	if (sdep->sde_serial) {
	    Free(opaque, sdep->sde_serial);
	    sdep->sde_serial = NULL;
	}
	if (sdep->sde_device_id) {
	    Free(opaque, sdep->sde_device_id);
	    sdep->sde_device_id = NULL;
	}
	if (sdep->sde_target_port) {
	    Free(opaque, sdep->sde_target_port);
	    sdep->sde_target_port = NULL;
	}
	sdep->sde_blink->sde_flink = sdep->sde_flink;
	sdep->sde_flink->sde_blink = sdep->sde_blink;
	Free(opaque, sdep);
    }
    return;
}

void
FreeScsiFilters(scsi_device_t *sdp, scsi_filters_t *sfp)
{
    if (sfp->device_paths) {
	Free(sdp, sfp->device_paths);
	sfp->device_paths = NULL;
    }
    if (sfp->device_types) {
	Free(sdp, sfp->device_types);
	sfp->device_types = NULL;
    }
    if (sfp->exclude_paths) {
	Free(sdp, sfp->exclude_paths);
	sfp->exclude_paths = NULL;
    }
    if (sfp->product) {
	Free(sdp, sfp->product);
	sfp->product = NULL;
    }
    if (sfp->vendor) {
	Free(sdp, sfp->vendor);
	sfp->vendor = NULL;
    }
    if (sfp->revision) {
	Free(sdp, sfp->revision);
	sfp->revision = NULL;
    }
    if (sfp->fw_version) {
	Free(sdp, sfp->fw_version);
	sfp->fw_version = NULL;
    }
    if (sfp->device_id) {
	Free(sdp, sfp->device_id);
	sfp->device_id = NULL;
    }
    if (sfp->serial) {
	Free(sdp, sfp->serial);
	sfp->serial = NULL;
    }
    if (sfp->target_port) {
	Free(sdp, sfp->target_port);
	sfp->target_port = NULL;
    }
    if (sdp->show_fields) {
	free(sdp->show_fields);
	sdp->show_fields = NULL;
    }
    if (sdp->show_format) {
	free(sdp->show_format);
	sdp->show_format = NULL;
    }
    return;
}
