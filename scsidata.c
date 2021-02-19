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
 * File:	scsi_data.c
 * Author:	Robin T. Miller
 * Date:	June 5th, 2007
 *
 * Descriptions:
 *	Functions and tables to decode SCSI data.
 *
 * Modification History:
 * 
 * December 12th, 2020 by Robin T. Miller
 *      Added functions to decode segment and target descriptor types.
 * 
 * November 28th, 2020 by Robin T. Miller
 *      Update XCOPY (LID1) descriptor to display List ID Usage.
 * 
 * November 16th, 2020 by Robin T. Miller
 *      When dumping PopulateToken (PT) and Write Using Token (WUT) data,
 * report the list identifier. We need the list ID to correlate errors.
 * 
 * June 27th, 2018 by Robin T. Miller
 *      Added decoding of ATA Status Return descriptor.
 * 
 * June 13th, 2018 by Robin T. Miller
 *      Added decoding of format in progress sense specific data.
 * 
 * October 19th, 2017 by Robin T. Miller
 *      Add additional asc/ascq entries for asc 0x21 (0x4 thru 0x7 are new).
 * 
 * April 28th, 2014 by Robin T. Miller
 * 	Fix recursive call to DumpSenseData(), which requires the SCSI generic
 * pointer for new logging, to avoid dereferencing a NULL pointer and dying.
 * 
 * April 11th, 2012 by Robin T. Miller
 * 	Added token operation asc/asq (0x23, 0xNN) codes and error messages.
 *	Also added asc/ascq 0x55/0x0C and 0x55/0x0D for ROD Token errors.
 *
 * February 6th, 2012 by Robin T. Miller
 * 	When displaying additional sense data, dynamically allocate the memory
 * required, since the previously allocated 32 bytes could be exceeded!
 * 
 * May 12th, 2010 by Robin T. Miller
 * 	Added function to return SCSI operation code name.
 * 
 * April 23rd, 2010 by Robin T. Miller
 *	Update with new asc/ascq pairs from SPC4.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spt.h"
//#include "libscsi.h"
#include "inquiry.h"
#include "scsi_opcodes.h"

/*
 * Forward References:
 */
hbool_t	isReadWriteRequest(scsi_generic_t *sgp);
	
/*
 * parse_show_scsi_args() - Parse the show SCSI keywords.
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
int
parse_show_scsi_args(scsi_device_t *sdp, char **argv, int argc, int *arg_index)
{
    char *string;
    int status = SUCCESS;

    for ( ; *arg_index < argc; (*arg_index)++) {
	string = argv[*arg_index];

	if ( match(&string, "ascq=") ) {
	    uint16_t ascq;
	    uint8_t asc, asq;
	    char *ascq_msg;
	    ascq = (uint16_t)number(sdp, string, HEX_RADIX, &status, False);
	    asc = (uint8_t)(ascq >> 8);
	    asq = (uint8_t)ascq;
	    ascq_msg = ScsiAscqMsg(asc, asq);
	    if (ascq_msg) {
		Print(sdp, "Sense Code/Qualifier = (%#x, %#x) = %s\n",
		       asc, asq, ascq_msg);
	    } else {
		Print(sdp, "Sense Code/Qualifier = (%#x, %#x)\n",
		       asc, asq);
	    }
	    continue;
	}
	if ( match(&string, "key=") ) {
	    char *skey_msg;
	    uint8_t sense_key;
	    sense_key = (uint8_t)number(sdp, string, HEX_RADIX, &status, False);
	    skey_msg = SenseKeyMsg(sense_key);
	    Print(sdp, "Sense Key = %#x = %s\n", sense_key, skey_msg);
	    continue;
	}
	if ( match(&string, "status=") ) {
	    char *scsi_status_msg;
	    uint8_t scsi_status;
	    scsi_status = (uint32_t)number(sdp, string, HEX_RADIX, &status, False);
	    scsi_status_msg = ScsiStatus(scsi_status);
	    Print(sdp, "SCSI Status = %#x = %s\n", scsi_status, scsi_status_msg);
	    continue;
	}
	Eprintf(sdp, "Valid show scsi keyword: %s\n", string);
	Printf(sdp, "Valid show scsi keywords are: ascq|key|status|uec\n");
	status = FAILURE;
	/* Unknown keyword, so simply break and continue parsing other args. */
	//( (*arg_index)-- );
	break;
    }
    return(status);
}

void
print_scsi_status(scsi_generic_t *sgp, uint8_t scsi_status, uint8_t sense_key, uint8_t asc, uint8_t ascq)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    Fprintf(opaque, "    SCSI Status: %02Xh = %s\n", scsi_status, ScsiStatus(scsi_status));
    Fprintf(opaque, "      Sense Key: %02Xh = %s\n", sense_key, SenseKeyMsg(sense_key));
    Fprintf(opaque, "       asc/ascq: %02Xh/%02Xh = %s\n", asc, ascq, ScsiAscqMsg(asc, ascq));
    fflush(stderr);
}

/*
 * SCSI Status Code Table.
 */
static struct SCSI_StatusTable {
    unsigned char scsi_status;
    char          *status_msg;
    char	  *status_name;
} scsi_StatusTable[] = {
    { SCSI_GOOD,                 "SCSI_GOOD",		"good"	},	/* 0x00 */
    { SCSI_CHECK_CONDITION,      "SCSI_CHECK_CONDITION","cc"	},	/* 0x02 */
    { SCSI_CONDITION_MET,        "SCSI_CONDITION_MET",	"cmet"	},	/* 0x04 */
    { SCSI_BUSY,                 "SCSI_BUSY",		"busy"	},	/* 0x08 */
    { SCSI_INTERMEDIATE,         "SCSI_INTERMEDIATE",	"inter"	},	/* 0x10 */
    { SCSI_INTER_COND_MET,       "SCSI_INTER_COND_MET",	"icmet"	}, 	/* 0x14 */
    { SCSI_RESERVATION_CONFLICT, "SCSI_RESERVATION_CONFLICT", "rescon"},/* 0x18 */
    { SCSI_COMMAND_TERMINATED,   "SCSI_COMMAND_TERMINATED", "term" },	/* 0x22 */ /* obsolete */
    { SCSI_QUEUE_FULL,           "SCSI_QUEUE_FULL",	"qfull"	},	/* 0x28 */
    { SCSI_ACA_ACTIVE,           "SCSI_ACA_ACTIVE",	"aca_active"},	/* 0x30 */
    { SCSI_TASK_ABORTED,         "SCSI_TASK_ABORTED",	"aborted"}	/* 0x40 */
};
static int scsi_StatusEntrys = sizeof(scsi_StatusTable) / sizeof(scsi_StatusTable[0]);

/*
 * ScsiStatusMsg() - Translate SCSI Status to Message Text.
 *
 * Inputs:
 *  scsi_status = The SCSI status value.
 *
 * Return Value:
 *  Returns the SCSI status text string.
 */
char *
ScsiStatus(unsigned char scsi_status)
{
    struct SCSI_StatusTable *cst = scsi_StatusTable;
    int entrys;

    for (entrys = 0; entrys < scsi_StatusEntrys; cst++, entrys++) {
	if (cst->scsi_status == scsi_status) {
	    return (cst->status_msg);
	}
    }
    return ("???");
}

int
LookupScsiStatus(char *status_name)
{
    struct SCSI_StatusTable *cst = scsi_StatusTable;
    int entrys;

    for (entrys = 0; entrys < scsi_StatusEntrys; cst++, entrys++) {
	if (strcmp(cst->status_name, status_name) == 0) {
	    return (cst->scsi_status);
	}
    }
    return (-1);
}

/* ======================================================================== */

static struct SCSI_SenseKeyTable {
    unsigned char sense_key;
    char          *sense_msg;
    char	  *sense_name;
} scsi_SenseKeyTable[] = {
    { SKV_NOSENSE,		"NO SENSE",		"none"		},	/* 0x00 */
    { SKV_RECOVERED,		"RECOVERED ERROR",	"recovered"	},	/* 0x01 */
    { SKV_NOT_READY,		"NOT READY",		"notready"	},	/* 0x02 */
    { SKV_MEDIUM_ERROR,		"MEDIUM ERROR",		"medium"	},	/* 0x03 */
    { SKV_HARDWARE_ERROR,	"HARDWARE ERROR",	"hardware"	},	/* 0x04 */
    { SKV_ILLEGAL_REQUEST,	"ILLEGAL REQUEST",	"illegal"	}, 	/* 0x05 */
    { SKV_UNIT_ATTENTION,	"UNIT ATTENTION",	"ua"		},	/* 0x06 */
    { SKV_DATA_PROTECT,		"DATA PROTECT",		"dataprot"	},	/* 0x07 */
    { SKV_BLANK_CHECK,		"BLANK CHECK",		"blank"		},	/* 0x08 */
    { SKV_VENDOR_SPECIFIC,	"VENDOR SPECIFIC",	"vendor"	},	/* 0x09 */
    { SKV_COPY_ABORTED,		"COPY ABORTED",		"copyaborted"	},	/* 0x0a */
    { SKV_ABORTED_CMD,		"ABORTED COMMAND",	"aborted"	},	/* 0x0b*/
    { SKV_VOLUME_OVERFLOW,	"VOLUME OVERFLOW",	"overflow"	},	/* 0x0d */
    { SKV_MISCOMPARE,		"MISCOMPARE",		"miscompare"	}	/* 0x0e */
};
static int scsi_SenseKeyEntrys = sizeof(scsi_SenseKeyTable) / sizeof(scsi_SenseKeyTable[0]);

/*
 * ScsiStatusMsg() - Translate SCSI Status to Message Text.
 *
 * Inputs:
 *  scsi_status = The SCSI status value.
 *
 * Return Value:
 *  Returns the SCSI status text string.
 */
char *
SenseKeyMsg(unsigned char sense_key)
{
    struct SCSI_SenseKeyTable *skp = scsi_SenseKeyTable;
    int entrys;

    for (entrys = 0; entrys < scsi_SenseKeyEntrys; skp++, entrys++) {
	if (skp->sense_key == sense_key) {
	    return (skp->sense_msg);
	}
    }
    return ("???");
}

int
LookupSenseKey(char *sense_key_name)
{
    struct SCSI_SenseKeyTable *skp = scsi_SenseKeyTable;
    int entrys;

    for (entrys = 0; entrys < scsi_SenseKeyEntrys; skp++, entrys++) {
	if (strcmp(skp->sense_name, sense_key_name) == 0) {
	    return(skp->sense_key);
	}
    }
    return (-1);
}

/* ======================================================================== */

/*
 * Sense Code/Qualifier Table:
 *
 *                       D - DIRECT ACCESS BLOCK DEVICE (SBC-2)  Device Column key:
 *                       . T - SEQUENTIAL ACCESS DEVICE (SSC-2)  blank = code not used
 *                       .   L - PRINTER DEVICE (SSC)            not blank = code used
 *                       .     P - PROCESSOR DEVICE (SPC-2) 
 *                       .     . W - WRITE ONCE BLOCK DEVICE (SBC) 
 *                       .     .   R - CD/DVD DEVICE (MMC-4) 
 *                       .     .     O - OPTICAL MEMORY BLOCK DEVICE (SBC) 
 *                       .     .     . M - MEDIA CHANGER DEVICE (SMC-2) 
 *                       .     .     .   A - STORAGE ARRAY DEVICE (SCC-2) 
 *                       .     .     .     E - ENCLOSURE SERVICES DEVICE (SES) 
 *                       .     .     .     . B - SIMPLIFIED DIRECT-ACCESS DEVICE (RBC) 
 *                       .     .     .     .   K - OPTICAL CARD READER/WRITER DEVICE (OCRW) 
 *                       .     .     .     .     V - AUTOMATION/DRIVE INTERFACE (ADC) 
 *                       .     .     .     .     . F - OBJECT-BASED STORAGE (OSD) 
 *                       .     .     .     .     . 
 *         ASC  ASCQ     D T L P W R O M A E B K V F    Description 
 */
struct sense_entry SenseCodeTable[] = {
	{ 0x00, 0x00, /* D T L P W R O M A E B K V F */ "No additional sense information"			},
	{ 0x00, 0x01, /* T                           */ "Filemark detected"					},
	{ 0x00, 0x02, /* T                           */ "End-of-partition/medium detected"			},
	{ 0x00, 0x03, /* T                           */ "Setmark detected"					},
	{ 0x00, 0x04, /* T                           */ "Beginning-of-partition/medium detected"		},
	{ 0x00, 0x05, /* T   L                       */ "End-of-data detected"					},
	{ 0x00, 0x06, /* D T L P W R O M A E B K V F */ "I/O process terminated"				},
	{ 0x00, 0x07, /*   T                         */ "Programmable early warning detected"			},
	{ 0x00, 0x11, /*           R                 */ "Audio play operation in progress"			},
	{ 0x00, 0x12, /*           R                 */ "Audio play operation paused"				},
	{ 0x00, 0x13, /*           R                 */ "Audio play operation successfully completed"		},
	{ 0x00, 0x14, /*           R                 */ "Audio play operation stopped due to error"		},
	{ 0x00, 0x15, /*           R                 */ "No current audio status to return"			},
	{ 0x00, 0x16, /* D T L P W R O M A E B K V F */ "Operation in progress"					},
	{ 0x00, 0x17, /* D T L   W R O M A E B K V F */ "Cleaning requested"					},
	{ 0x00, 0x18, /* T                           */ "Erase operation in progress"				},
	{ 0x00, 0x19, /* T                           */ "Locate operation in progress"				},
	{ 0x00, 0x1A, /* T                           */ "Rewind operation in progress"				},
	{ 0x00, 0x1B, /* T                           */ "Set capacity operation in progress"			},
	{ 0x00, 0x1C, /* T                           */ "Verify operation in progress"				},
	{ 0x00, 0x1D, /* D T                 B       */ "ATA pass through information available"		},
	{ 0x00, 0x1E, /* D T       R M A E B   K V   */ "Conflicting SA creation request"			},
	{ 0x01, 0x00, /* D       W   O       B K     */ "No index/sector signal"				},
	{ 0x02, 0x00, /* D       W R O M     B K     */ "No seek complete"					},
	{ 0x03, 0x00, /* D T L   W   O       B K     */ "Peripheral device write fault"				},
	{ 0x03, 0x01, /* T                           */ "No write current"					},
	{ 0x03, 0x02, /* T                           */ "Excessive write errors"				},
	{ 0x04, 0x00, /* D T L P W R O M A E B K V F */ "Logical unit not ready, cause not reportable"		},
	{ 0x04, 0x01, /* D T L P W R O M A E B K V F */ "Logical unit is in process of becoming ready"		},
	{ 0x04, 0x02, /* D T L P W R O M A E B K V F */ "Logical unit not ready, initializing command required" },
	{ 0x04, 0x03, /* D T L P W R O M A E B K V F */ "Logical unit not ready, manual intervention required"  },
	{ 0x04, 0x04, /* D T L     R O       B       */ "Logical unit not ready, format in progress"		},
	{ 0x04, 0x05, /* D T     W   O M A   B K     */ "Logical unit not ready, rebuild in progress"		},
	{ 0x04, 0x06, /* D T     W   O M A   B K     */ "Logical unit not ready, recalculation in progress"	},
	{ 0x04, 0x07, /* D T L P W R O M A E B K V F */ "Logical unit not ready, operation in progress"		},
	{ 0x04, 0x08, /*           R                 */ "Logical unit not ready, long write in progress"	},
	{ 0x04, 0x09, /* D T L P W R O M A E B K V F */ "Logical unit not ready, self-test in progress"		},
	{ 0x04, 0x0A, /* D T L P W R O M A E B K V F */ "Logical unit not accessible, asymmetric access state transition" },
	{ 0x04, 0x0B, /* D T L P W R O M A E B K V F */ "Logical unit not accessible, target port in standby state" },
	{ 0x04, 0x0C, /* D T L P W R O M A E B K V F */ "Logical unit not accessible, target port in unavailable state" },
	{ 0x04, 0x0D, /*                           F */ "Logical unit not ready, structure check required"	},
	{ 0x04, 0x10, /* D T     W R O M     B       */ "Logical unit not ready, auxiliary memory not accessible" },
	{ 0x04, 0x11, /* D T     W R O M A E B   V F */ "Logical unit not ready, notify (enable spinup) required" },
	{ 0x04, 0x12, /*                         V   */ "Logical unit not ready, offline"			},
	{ 0x04, 0x13, /* D T       R   M A E B K V   */ "Logical unit not ready, sa creation in progress"	},
	{ 0x04, 0x14, /* D                   B       */ "Logical unit not ready, space allocation in progress"	},
	{ 0x04, 0x15, /*               M             */ "Logical unit not ready, robotics disabled"		},
	{ 0x04, 0x16, /*               M             */ "Logical unit not ready, configuration required"	},
	{ 0x04, 0x17, /*               M             */ "Logical unit not ready, calibration required"		},
	{ 0x04, 0x18, /*               M             */ "Logical unit not ready, a door is open"		},
	{ 0x04, 0x19, /*               M             */ "Logical unit not ready, operating in sequential mode"	},
	{ 0x04, 0x1B, /* D T L     R O       B       */ "Host Interface Not Ready, sanitize in progress"	},
	{ 0x04, 0x1C, /* D T L     R O       B       */ "Logical Unit Not Ready, waiting for power grant"	},
	{ 0x05, 0x00, /* D T L   W R O M A E B K V F */ "Logical unit does not respond to selection"		},
	{ 0x06, 0x00, /* D       W R O M     B K     */ "No reference position found"				},
	{ 0x07, 0x00, /* D T L   W R O M     B K     */ "Multiple peripheral devices selected"			},
	{ 0x08, 0x00, /* D T L   W R O M A E B K V F */ "Logical unit communication failure"			},
	{ 0x08, 0x01, /* D T L   W R O M A E B K V F */ "Logical unit communication time-out"			},
	{ 0x08, 0x02, /* D T L   W R O M A E B K V F */ "Logical unit communication parity error"		},
	{ 0x08, 0x03, /* D T       R O M     B K     */ "Logical unit communication CRC error (ULTRA-DMA/32)"	},
	{ 0x08, 0x04, /* D T L P W R O         K     */ "Unreachable copy target"				},
	{ 0x09, 0x00, /* D T     W R O       B       */ "Track following error"					},
	{ 0x09, 0x01, /*         W R O         K     */ "Tracking servo failure"				},
	{ 0x09, 0x02, /*         W R O         K     */ "Focus servo failure"					},
	{ 0x09, 0x03, /*         W R O               */ "Spindle servo failure"					},
	{ 0x09, 0x04, /* D T     W R O       B       */ "Head select fault"					},
	{ 0x0A, 0x00, /* D T L P W R O M A E B K V F */ "Error log overflow"					},
	{ 0x0B, 0x00, /* D T L P W R O M A E B K V F */ "Warning"						},
	{ 0x0B, 0x01, /* D T L P W R O M A E B K V F */ "Warning - specified temperature exceeded"		},
	{ 0x0B, 0x02, /* D T L P W R O M A E B K V F */ "Warning - enclosure degraded"				},
	{ 0x0B, 0x03, /* D T L P W R O M A E B K V F */ "Warning - background self-test failed"			},
	{ 0x0B, 0x04, /* D T L P W R O   A E B K V F */ "Warning - background pre-scan detected medium error"	},
	{ 0x0B, 0x05, /* D T L P W R O   A E B K V F */ "Warning - background medium scan detected medium error" },
	{ 0x0B, 0x06, /* D T L P W R O M A E B K V F */ "Warning - non-volatile cache now volatile"		},
	{ 0x0B, 0x07, /* D T L P W R O M A E B K V F */ "Warning - degraded power to non-volatile cache"	},
	{ 0x0B, 0x08, /* D T L P W R O M A E B K V F */ "Warning - power loss expected"				},
	{ 0x0C, 0x00, /*   T       R                 */ "Write error"						},
	{ 0x0C, 0x01, /*                       K     */ "Write error - recovered with auto reallocation"	},
	{ 0x0C, 0x02, /* D       W   O       B K     */ "Write error - auto reallocation failed"		},
	{ 0x0C, 0x03, /* D       W   O       B K     */ "Write error - recommend reassignment"			},
	{ 0x0C, 0x04, /* D T     W   O       B       */ "Compression check miscompare error"			},
	{ 0x0C, 0x05, /* D T     W   O       B       */ "Data expansion occurred during compression"		},
	{ 0x0C, 0x06, /* D T     W   O       B       */ "Block not compressible"				},
	{ 0x0C, 0x07, /*           R                 */ "Write error - recovery needed"				},
	{ 0x0C, 0x08, /*           R                 */ "Write error - recovery failed"				},
	{ 0x0C, 0x09, /*           R                 */ "Write error - loss of streaming"			},
	{ 0x0C, 0x0A, /*           R                 */ "Write error - padding blocks added"			},
	{ 0x0C, 0x0B, /* D T     W R O M     B       */ "Auxiliary memory write error"				},
	{ 0x0C, 0x0C, /* D T L P W R O M A E B K V F */ "Write error - unexpected unsolicited data"		},
	{ 0x0C, 0x0D, /* D T L P W R O M A E B K V F */ "Write error - not enough unsolicited data"		},
	{ 0x0C, 0x0F, /*           R                 */ "Defects in error window"				},
	{ 0x0D, 0x00, /* D T L P W R O   A     K     */ "Error detected by third party temporary initiator"	},
	{ 0x0D, 0x01, /* D T L P W R O   A     K     */ "Third party device failure"				},
	{ 0x0D, 0x02, /* D T L P W R O   A     K     */ "Copy target device not reachable"			},
	{ 0x0D, 0x03, /* D T L P W R O   A     K     */ "Incorrect copy target device type"			},
	{ 0x0D, 0x04, /* D T L P W R O   A     K     */ "Copy target device data underrun"			},
	{ 0x0D, 0x05, /* D T L P W R O   A     K     */ "Copy target device data overrun"			},
	{ 0x0E, 0x00, /* D T   P W R O M A E B K   F */ "Invalid information unit"				},
	{ 0x0E, 0x01, /* D T   P W R O M A E B K   F */ "Information unit too short"				},
	{ 0x0E, 0x02, /* D T   P W R O M A E B K   F */ "Information unit too long"				},
	{ 0x0E, 0x03, /* D T   P   R   M A E B K   F */ "Invalid field in command information unit"		},
	{ 0x10, 0x00, /* D       W   O       B K     */ "ID CRC or ECC error"					},
	{ 0x10, 0x01, /* D T     W   O               */ "Data block guard check failed"				},
	{ 0x10, 0x02, /* D T     W   O               */ "Data block application tag check failed"		},
	{ 0x10, 0x03, /* D T     W   O               */ "Data block reference tag check failed"			},
	{ 0x11, 0x00, /* D T     W R O       B K     */ "Unrecovered read error"				},
	{ 0x11, 0x01, /* D T     W R O       B K     */ "Read retries exhausted"				},
	{ 0x11, 0x02, /* D T     W R O       B K     */ "Error too long to correct"				},
	{ 0x11, 0x03, /* D T     W   O       B K     */ "Multiple read errors"					},
	{ 0x11, 0x04, /* D       W   O       B K     */ "Unrecovered read error - auto reallocate failed"	},
	{ 0x11, 0x05, /*         W R O       B       */ "L-EC uncorrectable error"				},
	{ 0x11, 0x06, /*         W R O       B       */ "CIRC unrecovered error"				},
	{ 0x11, 0x07, /*         W O         B       */ "Data re-synchronization error"				},
	{ 0x11, 0x08, /*   T                         */ "Incomplete block read"					},
	{ 0x11, 0x09, /*   T                         */ "No gap found"						},
	{ 0x11, 0x0A, /* D T         O       B K     */ "Miscorrected"						},
	{ 0x11, 0x0B, /* D       W   O       B K     */ "Unrecovered read error - recommend reassignment"	},
	{ 0x11, 0x0C, /* D       W   O       B K     */ "Unrecovered read error - recommend rewrite the data"	},
	{ 0x11, 0x0D, /* D T     W R O       B       */ "De-compression crc error"				},
	{ 0x11, 0x0E, /* D T     W R O       B       */ "Cannot decompress using declared algorithm"		},
	{ 0x11, 0x0F, /*           R                 */ "Error reading UPC/EAN number"				},
	{ 0x11, 0x10, /*           R                 */ "Error reading ISRC number"				},
	{ 0x11, 0x11, /*           R                 */ "Read error - loss of streaming"			},
	{ 0x11, 0x12, /* D T     W R O M     B       */ "Auxiliary memory read error"				},
	{ 0x11, 0x13, /* D T L P W R O M A E B K V F */ "Read error - failed retransmission request"		},
	{ 0x11, 0x14, /* D                           */ "Read error - LBA marked bad by application client"	},
	{ 0x12, 0x00, /* D       W   O       B K     */ "Address mark not found for id field"			},
	{ 0x13, 0x00, /* D       W   O       B K     */ "Address mark not found for data field"			},
	{ 0x14, 0x00, /* D T L   W R O       B K     */ "Recorded entity not found"				},
	{ 0x14, 0x01, /* D T     W R O       B K     */ "Record not found"					},
	{ 0x14, 0x02, /*   T                         */ "Filemark or setmark not found"				},
	{ 0x14, 0x03, /*   T                         */ "End-of-data not found"					},
	{ 0x14, 0x04, /*   T                         */ "Block sequence error"					},
	{ 0x14, 0x05, /* D T     W   O       B K     */ "Record not found - recommend reassignment"		},
	{ 0x14, 0x06, /* D T     W   O       B K     */ "Record not found - data auto-reallocated"		},
	{ 0x14, 0x07, /*   T                         */ "Locate operation failure"				},
	{ 0x15, 0x00, /* D T L   W R O M     B K     */ "Random positioning error"				},
	{ 0x15, 0x01, /* D T L   W R O M     B K     */ "Mechanical positioning error"				},
	{ 0x15, 0x02, /* D T     W R O       B K     */ "Positioning error detected by read of medium"		},
	{ 0x16, 0x00, /* D       W   O       B K     */ "Data synchronization mark error"			},
	{ 0x16, 0x01, /* D       W   O       B K     */ "Data sync error - data rewritten"			},
	{ 0x16, 0x02, /* D       W   O       B K     */ "Data sync error - recommend rewrite"			},
	{ 0x16, 0x03, /* D       W   O       B K     */ "Data sync error - data auto-reallocated"		},
	{ 0x16, 0x04, /* D       W   O       B K     */ "Data sync error - recommend reassignment"		},
	{ 0x17, 0x00, /* D T     W R O       B K     */ "Recovered data with no error correction applied"	},
	{ 0x17, 0x01, /* D T     W R O       B K     */ "Recovered data with retries"				},
	{ 0x17, 0x02, /* D T     W R O       B K     */ "Recovered data with positive head offset"		},
	{ 0x17, 0x03, /* D T     W R O       B K     */ "Recovered data with negative head offset"		},
	{ 0x17, 0x04, /*         W R O       B       */ "Recovered data with retries and/or circ applied"	},
	{ 0x17, 0x05, /* D       W R O       B K     */ "Recovered data using previous sector id"		},
	{ 0x17, 0x06, /* D       W   O       B K     */ "Recovered data without ECC - data auto-reallocated"   	},
	{ 0x17, 0x07, /* D       W R O       B K     */ "Recovered data without ECC - recommend reassignment"	},
	{ 0x17, 0x08, /* D       W R O       B K     */ "Recovered data without ECC - recommend rewrite"	},
	{ 0x17, 0x09, /* D       W R O       B K     */ "Recovered data without ECC - data rewritten"		},
	{ 0x18, 0x00, /* D T     W R O       B K     */ "Recovered data with error correction applied"		},
	{ 0x18, 0x01, /* D       W R O       B K     */ "Recovered data with error corr. & retries applied"	},
	{ 0x18, 0x02, /* D       W R O       B K     */ "Recovered data - data auto-reallocated"		},
	{ 0x18, 0x03, /*           R                 */ "Recovered data with CIRC"				},
	{ 0x18, 0x04, /*           R                 */ "Recovered data with L-EC"				},
	{ 0x18, 0x05, /* D       W R O       B K     */ "Recovered data - recommend reassignment"		},
	{ 0x18, 0x06, /* D       W R O       B K     */ "Recovered data - recommend rewrite"			},
	{ 0x18, 0x07, /* D       W   O       B K     */ "Recovered data with ecc - data rewritten"		},
	{ 0x18, 0x08, /*           R                 */ "Recovered data with linking"				},
	{ 0x19, 0x00, /* D           O         K     */ "Defect list error"					},
	{ 0x19, 0x01, /* D           O         K     */ "Defect list not available"				},
	{ 0x19, 0x02, /* D           O         K     */ "Defect list error in primary list"			},
	{ 0x19, 0x03, /* D           O         K     */ "Defect list error in grown list"			},
	{ 0x1A, 0x00, /* D T L P W R O M A E B K V F */ "Parameter list length error"				},
	{ 0x1B, 0x00, /* D T L P W R O M A E B K V F */ "Synchronous data transfer error"			},
	{ 0x1C, 0x00, /* D           O       B K     */ "Defect list not found"					},
	{ 0x1C, 0x01, /* D           O       B K     */ "Primary defect list not found"				},
	{ 0x1C, 0x02, /* D           O       B K     */ "Grown defect list not found"				},
	{ 0x1D, 0x00, /* D T     W R O       B K     */ "Miscompare during verify operation"			},
	{ 0x1D, 0x01, /* D                   B       */ "Miscompare verify of unmapped lba"			},
	{ 0x1E, 0x00, /* D       W   O       B K     */ "Recovered id with ECC correction"			},
	{ 0x1F, 0x00, /* D           O         K     */ "Partial defect list transfer"				},
	{ 0x20, 0x00, /* D T L P W R O M A E B K V F */ "Invalid command operation code"			},
	{ 0x20, 0x01, /* D T   P W R O M A E B K     */ "Access denied - initiator pending-enrolled"		},
	{ 0x20, 0x02, /* D T   P W R O M A E B K     */ "Access denied - no access rights"			},
	{ 0x20, 0x03, /* D T   P W R O M A E B K     */ "Access denied - invalid mgmt id key"			},
	{ 0x20, 0x04, /*   T                         */ "Illegal command while in write capable state"		},
	{ 0x20, 0x05, /*   T                         */ "Obsolete"						},
	{ 0x20, 0x06, /*   T                         */ "Illegal command while in explicit address mode"	},
	{ 0x20, 0x07, /*   T                         */ "Illegal command while in implicit address mode"	},
	{ 0x20, 0x08, /* D T P   W R O M A E B K     */ "Access denied - enrollment conflict"			},
	{ 0x20, 0x09, /* D T P   W R O M A E B K     */ "Access denied - invalid lu identifier"			},
	{ 0x20, 0x0A, /* D T P   W R O M A E B K     */ "Access denied - invalid proxy token"			},
	{ 0x20, 0x0B, /* D T P   W R O M A E B K     */ "Access denied - ACL LUN conflict"			},
	{ 0x20, 0x0C, /*   T                         */ "Illegal command when not in append-only mode"		},
	{ 0x21, 0x00, /* D T     W R O M     B K     */ "Logical block address out of range"			},
	{ 0x21, 0x01, /* D T     W R O M     B K     */ "Invalid element address"				},
	{ 0x21, 0x02, /*           R                 */ "Invalid address for write"				},
	{ 0x21, 0x03, /*           R                 */ "Invalid write crossing layer jump"			},
	{ 0x21, 0x04, /* D                           */ "Unaligned write command"				},
	{ 0x21, 0x05, /* D                           */ "Write boundary violation"				},
	{ 0x21, 0x06, /* D                           */ "Attempt to read invalid data"				},
	{ 0x21, 0x07, /* D                           */ "Read boundary violation"				},
	{ 0x22, 0x00, /* D                           */ "Illegal function (use 20 00, 24 00, or 26 00)"		},
	{ 0x23, 0x00, /* D T   P             B       */ "Invalid token operation, cause not reportable"		},
	{ 0x23, 0x01, /* D T   P             B       */ "Invalid token operation, unsupported token type"	},
	{ 0x23, 0x02, /* D T   P             B       */ "Invalid token operation, remote token usage not supported" },
	{ 0x23, 0x03, /* D T   P             B       */ "Invalid token operation, remote rod token creation not supported" },
	{ 0x23, 0x04, /* D T   P             B       */ "Invalid token operation, token unknown"		},
	{ 0x23, 0x05, /* D T   P             B       */ "Invalid token operation, token corrupt"		},
	{ 0x23, 0x06, /* D T   P             B       */ "Invalid token operation, token revoked"		},
	{ 0x23, 0x07, /* D T   P             B       */ "Invalid token operation, token expired"		},
	{ 0x23, 0x08, /* D T   P             B       */ "Invalid token operation, token cancelled"		},
	{ 0x23, 0x09, /* D T   P             B       */ "Invalid token operation, token deleted"		},
	{ 0x23, 0x0A, /* D T   P             B       */ "Invalid token operation, invalid token length"		},
	{ 0x24, 0x00, /* D T L P W R O M A E B K V F */ "Invalid field in CDB"					},
	{ 0x24, 0x01, /* D T L P W R O M A E B K V F */ "CDB decryption error"					},
	{ 0x24, 0x02, /*   T                         */ "Obsolete"						},
	{ 0x24, 0x03, /*   T                         */ "Obsolete"						},
	{ 0x24, 0x04, /*                           F */ "Security audit value frozen"				},
	{ 0x24, 0x05, /*                           F */ "Security working key frozen"				},
	{ 0x24, 0x06, /*                           F */ "Nonce not unique"					},
	{ 0x24, 0x07, /*                           F */ "Nonce timestamp out of range"				},
	{ 0x24, 0x08, /* D T       R   M A E B K V   */ "Invalid XCDB"						},
	{ 0x25, 0x00, /* D T L P W R O M A E B K V F */ "Logical unit not supported"				},
	{ 0x26, 0x00, /* D T L P W R O M A E B K V F */ "Invalid field in parameter list"			},
	{ 0x26, 0x01, /* D T L P W R O M A E B K V F */ "Parameter not supported"				},
	{ 0x26, 0x02, /* D T L P W R O M A E B K V F */ "Parameter value invalid"				},
	{ 0x26, 0x03, /* D T L P W R O M A E   K     */ "Threshold parameters not supported"			},
	{ 0x26, 0x04, /* D T L P W R O M A E B K V F */ "Invalid release of persistent reservation"		},
	{ 0x26, 0x05, /* D T L P W R O M A   B K     */ "Data decryption error"					},
	{ 0x26, 0x06, /* D T L P W R O         K     */ "Too many target descriptors"				},
	{ 0x26, 0x07, /* D T L P W R O         K     */ "Unsupported target descriptor type code"		},
	{ 0x26, 0x08, /* D T L P W R O         K     */ "Too many segment descriptors"				},
	{ 0x26, 0x09, /* D T L P W R O         K     */ "Unsupported segment descriptor type code"		},
	{ 0x26, 0x0A, /* D T L P W R O         K     */ "Unexpected inexact segment"				},
	{ 0x26, 0x0B, /* D T L P W R O         K     */ "Inline data length exceeded"				},
	{ 0x26, 0x0C, /* D T L P W R O         K     */ "Invalid operation for copy source or destination"	},
	{ 0x26, 0x0D, /* D T L P W R O         K     */ "Copy segment granularity violation"			},
	{ 0x26, 0x0E, /* D T   P W R O M A E B K     */ "Invalid parameter while port is enabled"		},
	{ 0x26, 0x0F, /*                           F */ "Invalid data-out buffer integrity check value"		},
	{ 0x26, 0x10, /*   T                         */ "Data decryption key fail limit reached"		},
	{ 0x26, 0x11, /*   T                         */ "Incomplete key-associated data set"			},
	{ 0x26, 0x12, /*   T                         */ "Vendor specific key reference not found"		},
	{ 0x27, 0x00, /* D T     W R O       B K     */ "Write protected"					},
	{ 0x27, 0x01, /* D T     W R O       B K     */ "Hardware write protected"				},
	{ 0x27, 0x02, /* D T     W R O       B K     */ "Logical unit software write protected"			},
	{ 0x27, 0x03, /*   T       R                 */ "Associated write protect"		       		},
	{ 0x27, 0x04, /*   T       R                 */ "Persistent write protect"				},
	{ 0x27, 0x05, /*   T       R                 */ "Permanent write protect"				},
	{ 0x27, 0x06, /*           R                 */ "Conditional write protect"				},
	{ 0x27, 0x07, /* D                   B       */ "Space allocation failed write protect"			},
	{ 0x27, 0x08, /* D                   B       */ "Zone is read only"					},
	{ 0x28, 0x00, /* D T L P W R O M A E B K V F */ "Not ready to ready change, medium may have changed"	},
	{ 0x28, 0x01, /* D T     W R O M     B       */ "Import or export element accessed"			},
	{ 0x28, 0x02, /*           R                 */ "Format-layer may have changed"				},
	{ 0x28, 0x03, /*               M             */ "Import/export element accessed, medium changed"	},
	{ 0x29, 0x00, /* D T L P W R O M A E B K V F */ "Power on, reset, or bus device reset occurred"		},
	{ 0x29, 0x01, /* D T L P W R O M A E B K V F */ "Power on occurred"					},
	{ 0x29, 0x02, /* D T L P W R O M A E B K V F */ "SCSI bus reset occurred"				},
	{ 0x29, 0x03, /* D T L P W R O M A E B K V F */ "Bus device reset function occurred"			},
	{ 0x29, 0x04, /* D T L P W R O M A E B K V F */ "Device internal reset"					},
	{ 0x29, 0x05, /* D T L P W R O M A E B K V F */ "Transceiver mode changed to single-ended"		},
	{ 0x29, 0x06, /* D T L P W R O M A E B K V F */ "Transceiver mode changed to LVD"			},
	{ 0x29, 0x07, /* D T L P W R O M A E B K V F */ "I_T nexus loss occurred"				},
	{ 0x2A, 0x00, /* D T L W R O M   A E B K V F */ "Parameters changed"					},
	{ 0x2A, 0x01, /* D T L W R O M   A E B K V F */ "Mode parameters changed"				},
	{ 0x2A, 0x02, /* D T L W R O M   A E   K     */ "Log parameters changed"				},
	{ 0x2A, 0x03, /* D T L P W R O M A E   K     */ "Reservations preempted"				},
	{ 0x2A, 0x04, /* D T L P W R O M A E         */ "Reservations released"					},
	{ 0x2A, 0x05, /* D T L P W R O M A E         */ "Registrations preempted"				},
	{ 0x2A, 0x06, /* D T L P W R O M A E B K V F */ "Asymmetric access state changed"			},
	{ 0x2A, 0x07, /* D T L P W R O M A E B K V F */ "Implicit asymmetric access state transition failed"	},
	{ 0x2A, 0x08, /* D T     W R O M A E B K V F */ "Priority changed"					},
	{ 0x2A, 0x09, /* D                           */ "Capacity data has changed"				},
	{ 0x2A, 0x0A, /* D T                         */ "Error history I_T nexus cleared"			},
	{ 0x2A, 0x0B, /* D T                         */ "Error history snapshot released"			},
	{ 0x2A, 0x0C, /*                           F */ "Error recovery attributes have changed"		},
	{ 0x2A, 0x0D, /*   T                         */ "Data encryption capabilities changed"			},
	{ 0x2A, 0x10, /* D T           M   E     V   */ "Timestamp changed"					},
	{ 0x2A, 0x11, /*   T                         */ "Data encryption parameters changed by another I_T nexus" },
	{ 0x2A, 0x12, /*   T                         */ "Data encryption parameters changed by vendor specific event" },
	{ 0x2A, 0x13, /*   T                         */ "Data encryption key instance counter has changed"	},
	{ 0x2A, 0x14, /* D T       R   M A E B K V   */ "SA creation capabilities data has changed"		},
	{ 0x2B, 0x00, /* D T L P W R O         K     */ "Copy cannot execute since host cannot disconnect"	},
	{ 0x2C, 0x00, /* D T L P W R O M A E B K V F */ "Command sequence error"				},
	{ 0x2C, 0x01, /*                             */ "Too many windows specified"				},
	{ 0x2C, 0x02, /*                             */ "Invalid combination of windows specified"		},
	{ 0x2C, 0x03, /*           R                 */ "Current program area is not empty"			},
	{ 0x2C, 0x04, /*           R                 */ "Current program area is empty"				},
	{ 0x2C, 0x05, /*                    B        */ "Illegal power condition request"			},
	{ 0x2C, 0x06, /*           R                 */ "Persistent prevent conflict"				},
	{ 0x2C, 0x07, /* D T L P W R O M A E B K V F */ "Previous busy status"					},
	{ 0x2C, 0x08, /* D T L P W R O M A E B K V F */ "Previous task set full status"				},
	{ 0x2C, 0x09, /* D T L P W R O M   E B K V F */ "Previous reservation conflict status"			},
	{ 0x2C, 0x0A, /*                           F */ "Partition or collection contains user objects"		},
	{ 0x2C, 0x0B, /*   T                         */ "Not reserved"						},
	{ 0x2D, 0x00, /*   T                         */ "Overwrite error on update in place"			},
	{ 0x2E, 0x00, /*           R                 */ "Insufficient time for operation"			},
	{ 0x2F, 0x00, /* D T L P W R O M A E B K V F */ "Commands cleared by another initiator"			},
	{ 0x2F, 0x01, /* D                           */ "Commands cleared by power loss notification"		},
	{ 0x2F, 0x02, /* D T L P W R O M A E B K V F */ "Commands cleared by device server"			},
	{ 0x30, 0x00, /* D T     W R O M     B K     */ "Incompatible medium installed"				},
	{ 0x30, 0x01, /* D T     W R O       B K     */ "Cannot read medium - unknown format"			},
	{ 0x30, 0x02, /* D T     W R O       B K     */ "Cannot read medium - incompatible format"		},
	{ 0x30, 0x03, /* D T       R           K     */ "Cleaning cartridge installed"				},
	{ 0x30, 0x04, /* D T     W R O       B K     */ "Cannot write medium - unknown format"			},
	{ 0x30, 0x05, /* D T     W R O       B K     */ "Cannot write medium - incompatible format"		},
	{ 0x30, 0x06, /* D T     W R O       B       */ "Cannot format medium - incompatible medium"		},
	{ 0x30, 0x07, /* D T L   W R O M A E B K V F */ "Cleaning failure"					},
	{ 0x30, 0x08, /*           R                 */ "Cannot write - application code mismatch"		},
	{ 0x30, 0x09, /*           R                 */ "Current session not fixated for append"		},
	{ 0x30, 0x0A, /* D T     W R O M A E B K     */ "Cleaning request rejected"				},
	{ 0x30, 0x0C, /*   T                         */ "Worm medium - overwrite attempted"			},
	{ 0x30, 0x0D, /*   T                         */ "Worm medium - integrity check"				},
	{ 0x30, 0x10, /*           R                 */ "Medium not formatted"					},
	{ 0x30, 0x11, /*               M             */ "Incompatible volume type"				},
	{ 0x30, 0x12, /*               M             */ "Incompatible volume qualifier"				},
	{ 0x30, 0x13, /*               M             */ "Cleaning volume expired"				},
	{ 0x31, 0x00, /* D T     W R O       B K     */ "Medium format corrupted"				},
	{ 0x31, 0x01, /* D L       R O       B       */ "Format command failed"					},
	{ 0x31, 0x02, /*           R                 */ "Zoned formatting failed due to spare linking"		},
	{ 0x32, 0x00, /* D       W   O       B K     */ "No defect spare location available"			},
	{ 0x32, 0x01, /* D       W   O       B K     */ "Defect list update failure"				},
	{ 0x33, 0x00, /*   T                         */ "Tape length error"					},
	{ 0x34, 0x00, /* D T L P W R O M A E B K V F */ "Enclosure failure"					},
	{ 0x35, 0x00, /* D T L P W R O M A E B K V F */ "Enclosure services failure"				},
	{ 0x35, 0x01, /* D T L P W R O M A E B K V F */ "Unsupported enclosure function"			},
	{ 0x35, 0x02, /* D T L P W R O M A E B K V F */ "Enclosure services unavailable"			},
	{ 0x35, 0x03, /* D T L P W R O M A E B K V F */ "Enclosure services transfer failure"			},
	{ 0x35, 0x04, /* D T L P W R O M A E B K V F */ "Enclosure services transfer refused"			},
	{ 0x35, 0x05, /* D T L   W R O M A E B K V F */ "Enclosure services checksum error"			},
	{ 0x36, 0x00, /*     L                       */ "Ribbon, ink, or toner failure"				},
	{ 0x37, 0x00, /* D T L   W R O M A E B K V F */ "Rounded parameter"					},
	{ 0x38, 0x00, /*                     B       */ "Event status notification"				},
	{ 0x38, 0x02, /*                     B       */ "ESN - power management class event"			},
	{ 0x38, 0x04, /*                     B       */ "ESN - media class event"				},
	{ 0x38, 0x06, /*                     B       */ "ESN - device busy class event"				},
	{ 0x38, 0x07, /* D                           */ "Thin provisioning soft threshold reached"		},
	{ 0x39, 0x00, /* D T L   W R O M A E   K     */ "Saving parameters not supported"			},
	{ 0x3A, 0x00, /* D T L   W R O M     B K     */ "Medium not present"					},
	{ 0x3A, 0x01, /* D T     W R O M     B K     */ "Medium not present - tray closed"			},
	{ 0x3A, 0x02, /* D T     W R O M     B K     */ "Medium not present - tray open"			},
	{ 0x3A, 0x03, /* D T     W R O M     B       */ "Medium not present - loadable"				},
	{ 0x3A, 0x04, /* D T     W R O M     B       */ "Medium not present - medium auxiliary memory accessible" },
	{ 0x3B, 0x00, /*   T L                       */ "Sequential positioning error"				},
	{ 0x3B, 0x01, /*   T                         */ "Tape position error at beginning-of-medium"		},
	{ 0x3B, 0x02, /*   T                         */ "Tape position error at end-of-medium"			},
	{ 0x3B, 0x03, /*     L                       */ "Tape or electronic vertical forms unit not ready"	},
	{ 0x3B, 0x04, /*     L                       */ "Slew failure"						},
	{ 0x3B, 0x05, /*     L                       */ "Paper jam"						},
	{ 0x3B, 0x06, /*     L                       */ "Failed to sense top-of-form"				},
	{ 0x3B, 0x07, /*     L                       */ "Failed to sense bottom-of-form"			},
	{ 0x3B, 0x08, /*   T                         */ "Reposition error"					},
	{ 0x3B, 0x09, /*                             */ "Read past end of medium"				},
	{ 0x3B, 0x0A, /*                             */ "Read past beginning of medium"				},
	{ 0x3B, 0x0B, /*                             */ "Position past end of medium"				},
	{ 0x3B, 0x0C, /*   T                         */ "Position past beginning of medium"			},
	{ 0x3B, 0x0D, /* D T     W R O M     B K     */ "Medium destination element full"			},
	{ 0x3B, 0x0E, /* D T     W R O M     B K     */ "Medium source element empty"				},
	{ 0x3B, 0x0F, /*       R                     */ "End of medium reached"					},
	{ 0x3B, 0x11, /* D T     W R O M     B K     */ "Medium magazine not accessible"			},
	{ 0x3B, 0x12, /* D T     W R O M     B K     */ "Medium magazine removed"				},
	{ 0x3B, 0x13, /* D T     W R O M     B K     */ "Medium magazine inserted"				},
	{ 0x3B, 0x14, /* D T     W R O M     B K     */ "Medium magazine locked"				},
	{ 0x3B, 0x15, /* D T     W R O M     B K     */ "Medium magazine unlocked"				},
	{ 0x3B, 0x16, /*       R                     */ "Mechanical positioning or changer error"		},
	{ 0x3B, 0x17, /*                           F */ "Read past end of user object"				},
	{ 0x3B, 0x18, /*               M             */ "Element disabled"					},
	{ 0x3B, 0x19, /*               M             */ "Element enabled"					},
	{ 0x3B, 0x1A, /*               M             */ "Data transfer device removed"				},
	{ 0x3B, 0x1B, /*               M             */ "Data transfer device inserted"				},
	{ 0x3D, 0x00, /* D T L P W R O M A E   K     */ "Invalid bits in identify message"			},
	{ 0x3E, 0x00, /* D T L P W R O M A E B K V F */ "Logical unit has not self-configured yet"		},
	{ 0x3E, 0x01, /* D T L P W R O M A E B K V F */ "Logical unit failure"					},
	{ 0x3E, 0x02, /* D T L P W R O M A E B K V F */ "Timeout on logical unit"				},
	{ 0x3E, 0x03, /* D T L P W R O M A E B K V F */ "Logical unit failed self-test"				},
	{ 0x3E, 0x04, /* D T L P W R O M A E B K V F */ "Logical unit unable to update self-test log"		},
	{ 0x3F, 0x00, /* D T L P W R O M A E B K V F */ "Target operating conditions have changed"		},
	{ 0x3F, 0x01, /* D T L P W R O M A E B K V F */ "Microcode has been changed"				},
	{ 0x3F, 0x02, /* D T L P W R O M     B K     */ "Changed operating definition"				},
	{ 0x3F, 0x03, /* D T L P W R O M A E B K V F */ "Inquiry data has changed"				},
	{ 0x3F, 0x04, /* D T     W R O M A E B K     */ "Component device attached"				},
	{ 0x3F, 0x05, /* D T     W R O M A E B K     */ "Device identifier changed"				},
	{ 0x3F, 0x06, /* D T     W R O M A E B       */ "Redundancy group created or modified"			},
	{ 0x3F, 0x07, /* D T     W R O M A E B       */ "Redundancy group deleted"				},
	{ 0x3F, 0x08, /* D T     W R O M A E B       */ "Spare created or modified"				},
	{ 0x3F, 0x09, /* D T     W R O M A E B       */ "Spare deleted"						},
	{ 0x3F, 0x0A, /* D T     W R O M A E B K     */ "Volume set created or modified"			},
	{ 0x3F, 0x0B, /* D T     W R O M A E B K     */ "Volume set deleted"					},
	{ 0x3F, 0x0C, /* D T     W R O M A E B K     */ "Volume set deassigned"					},
	{ 0x3F, 0x0D, /* D T     W R O M A E B K     */ "Volume set reassigned"					},
	{ 0x3F, 0x0E, /* D T L P W R O M A E         */ "Reported LUNs data has changed"			},
	{ 0x3F, 0x0F, /* D T L P W R O M A E B K V F */ "Echo buffer overwritten"				},
	{ 0x3F, 0x10, /* D T     W R O M     B       */ "Medium loadable"					},
	{ 0x3F, 0x11, /* D T     W R O M     B       */ "Medium auxiliary memory accessible"			},
	{ 0x3F, 0x12, /* D T L P W R   M A E B K   F */ "iSCSI IP address added"				},
	{ 0x3F, 0x13, /* D T L P W R   M A E B K   F */ "iSCSI IP address removed"				},
	{ 0x3F, 0x14, /* D T L P W R   M A E B K   F */ "iSCSI IP address changed"				},
	{ 0x40, 0x00, /* D                           */ "RAM failure (should use 40 NN)"			},
	{ 0x40,  '*', /* D T L P W R O M A E B K V F */ "Diagnostic failure on component NN (80H-FFH)"		},
	{ 0x41, 0x00, /* D                           */ "Data path failure (should use 40 NN)"			},
	{ 0x42, 0x00, /* D                           */ "Power-on or self-test failure (should use 40 NN)"	},
	{ 0x43, 0x00, /* D T L P W R O M A E B K V F */ "Message error"						},
	{ 0x44, 0x00, /* D T L P W R O M A E B K V F */ "Internal target failure"				}, 
	{ 0x44, 0x71, /* D T                 B       */ "ATA device failed set features"			},
	{ 0x45, 0x00, /* D T L P W R O M A E B K V F */ "Select or reselect failure"				}, 
	{ 0x46, 0x00, /* D T L P W R O M     B K     */ "Unsuccessful soft reset"				},
	{ 0x47, 0x00, /* D T L P W R O M A E B K V F */ "SCSI parity error"					},
	{ 0x47, 0x01, /* D T L P W R O M A E B K V F */ "Data phase CRC error detected"				},
	{ 0x47, 0x02, /* D T L P W R O M A E B K V F */ "SCSI parity error detected during ST data phase"	},
	{ 0x47, 0x03, /* D T L P W R O M A E B K V F */ "Information unit iuCRC error detected"			},
	{ 0x47, 0x04, /* D T L P W R O M A E B K V F */ "Asynchronous information protection error detected"	},
	{ 0x47, 0x05, /* D T L P W R O M A E B K V F */ "Protocol service CRC error"				},
	{ 0x47, 0x06, /* D T           M A E B K V F */ "Phy test function in progress"				},
	{ 0x47, 0x7F, /* D T   P W R O M A E B K     */ "Some commands cleared by ISCSI protocol event"		},
	{ 0x48, 0x00, /* D T L P W R O M A E B K V F */ "Initiator detected error message received"		},
	{ 0x49, 0x00, /* D T L P W R O M A E B K V F */ "Invalid message error"					},
	{ 0x4A, 0x00, /* D T L P W R O M A E B K V F */ "Command phase error"					},
	{ 0x4B, 0x00, /* D T L P W R O M A E B K V F */ "Data phase error"					},
	{ 0x4B, 0x01, /* D T   P W R O M A E B K     */ "Invalid target port transfer tag received"		},
	{ 0x4B, 0x02, /* D T   P W R O M A E B K     */ "Too much write data"					},
	{ 0x4B, 0x03, /* D T   P W R O M A E B K     */ "ACK/NAK timeout"					},
	{ 0x4B, 0x04, /* D T   P W R O M A E B K     */ "NAK received"						},
	{ 0x4B, 0x05, /* D T   P W R O M A E B K     */ "Data offset error"					},
	{ 0x4B, 0x06, /* D T   P W R O M A E B K     */ "Initiator response timeout"				},
	{ 0x4B, 0x07, /* D T   P W R O M A E B K   F */ "Connection lost"					},
	{ 0x4C, 0x00, /* D T L P W R O M A E B K V F */ "Logical unit failed self-configuration"		},
	{ 0x4D,  '*', /* D T L P W R O M A E B K V F */ "Tagged overlapped commands (NN = task tag)"		},
	{ 0x4E, 0x00, /* D T L P W R O M A E B K V F */ "Overlapped commands attempted"				},
	{ 0x50, 0x00, /*   T                         */ "Write append error"					},
	{ 0x50, 0x01, /*   T                         */ "Write append position error"				},
	{ 0x50, 0x02, /*   T                         */ "Position error related to timing"			},
	{ 0x51, 0x00, /*   T       R O               */ "Erase failure"						},
	{ 0x51, 0x01, /*           R                 */ "Erase failure - incomplete erase operation detected"	},
	{ 0x52, 0x00, /*   T                         */ "Cartridge fault"					},
	{ 0x53, 0x00, /* D T L   W R O M     B K     */ "Media load or eject failed"				},
	{ 0x53, 0x01, /*   T                         */ "Unload tape failure"					},
	{ 0x53, 0x02, /* D T     W R O M     B K     */ "Medium removal prevented"				},
	{ 0x53, 0x03, /*               M             */ "Medium removal prevented by data transfer element"	},
	{ 0x53, 0x04, /*   T                         */ "Medium thread or unthread failure"			},
	{ 0x54, 0x00, /*       P                     */ "SCSI to host system interface failure"			},
	{ 0x55, 0x00, /*       P                     */ "System resource failure"				},
	{ 0x55, 0x01, /* D           O       B K     */ "System buffer full"					},
	{ 0x55, 0x02, /* D T L P W R O M A E   K     */ "Insufficient reservation resources"			},
	{ 0x55, 0x03, /* D T L P W R O M A E   K     */ "Insufficient resources"				},
	{ 0x55, 0x04, /* D T L P W R O M A E   K     */ "Insufficient registration resources"			},
	{ 0x55, 0x05, /* D T   P W R O M A E B K     */ "Insufficient access control resources"			},
	{ 0x55, 0x06, /* D T     W R O M     B       */ "Auxiliary memory out of space"				},
	{ 0x55, 0x07, /*                           F */ "Quota error"						},
	{ 0x55, 0x08, /*   T                         */ "Maximum number of supplemental decryption keys exceeded" },
	{ 0x55, 0x09, /*               M             */ "Medium auxiliary memory not accessible"		},
	{ 0x55, 0x0A, /*               M             */ "Data currently unavailable"				},
	{ 0x55, 0x0B, /* D T L P W R O M A E B K V F */ "Insufficient power for operation"			},
	{ 0x55, 0x0C, /* D T   P             B       */ "Insufficient resources to create rod"			},
	{ 0x55, 0x0D, /* D T   P             B       */ "Insufficient resources to create rod token"		},
	{ 0x57, 0x00, /*           R                 */ "Unable to recover table-of-contents"			},
	{ 0x58, 0x00, /*             O               */ "Generation does not exist"				},
	{ 0x59, 0x00, /*             O               */ "Updated block read"					},
	{ 0x5A, 0x00, /* D T L P W R O M     B K     */ "Operator request or state change input"		},
	{ 0x5A, 0x01, /* D T     W R O M     B K     */ "Operator medium removal request"			},
	{ 0x5A, 0x02, /* D T     W R O   A   B K     */ "Operator selected write protect"			},
	{ 0x5A, 0x03, /* D T     W R O   A   B K     */ "Operator selected write permit"			},
	{ 0x5B, 0x00, /* D T L P W R O M       K     */ "Log exception"						},
	{ 0x5B, 0x01, /* D T L P W R O M       K     */ "Threshold condition met"				},
	{ 0x5B, 0x02, /* D T L P W R O M       K     */ "Log counter at maximum"				},
	{ 0x5B, 0x03, /* D T L P W R O M       K     */ "Log list codes exhausted"				},
	{ 0x5C, 0x00, /* D           O               */ "Rpl status change"					},
	{ 0x5C, 0x01, /* D           O               */ "Spindles synchronized"					},
	{ 0x5C, 0x02, /* D           O               */ "Spindles not synchronized"				},
	{ 0x5D, 0x00, /* D T L P W R O M A E B K V F */ "Failure prediction threshold exceeded"			},
	{ 0x5D, 0x01, /*           R         B       */ "Media failure prediction threshold exceeded"		},
	{ 0x5D, 0x02, /*           R                 */ "Logical unit failure prediction threshold exceeded"	},
	{ 0x5D, 0x03, /*           R                 */ "Spare area exhaustion prediction threshold exceeded"	},
	{ 0x5D, 0x10, /* D                   B       */ "Hardware impending failure general hard drive failure"	},
	{ 0x5D, 0x11, /* D                   B       */ "Hardware impending failure drive error rate too high"	},
	{ 0x5D, 0x12, /* D                   B       */ "Hardware impending failure data error rate too high"	},
	{ 0x5D, 0x13, /* D                   B       */ "Hardware impending failure seek error rate too high"	},
	{ 0x5D, 0x14, /* D                   B       */ "Hardware impending failure too many block reassigns"	},
	{ 0x5D, 0x15, /* D                   B       */ "Hardware impending failure access times too high"	},
	{ 0x5D, 0x16, /* D                   B       */ "Hardware impending failure start unit times too high"	},
	{ 0x5D, 0x17, /* D                   B       */ "Hardware impending failure channel parametrics"	},
	{ 0x5D, 0x18, /* D                   B       */ "Hardware impending failure controller detected"	},
	{ 0x5D, 0x19, /* D                   B       */ "Hardware impending failure throughput performance"	},
	{ 0x5D, 0x1A, /* D                   B       */ "Hardware impending failure seek time performance"	},
	{ 0x5D, 0x1B, /* D                   B       */ "Hardware impending failure spin-up retry count"	},
	{ 0x5D, 0x1C, /* D                   B       */ "Hardware impending failure drive calibration retry count" },
	{ 0x5D, 0x20, /* D                   B       */ "Controller impending failure general hard drive failure"  },
	{ 0x5D, 0x21, /* D                   B       */ "Controller impending failure drive error rate too high"},
	{ 0x5D, 0x22, /* D                   B       */ "Controller impending failure data error rate too high"	},
	{ 0x5D, 0x23, /* D                   B       */ "Controller impending failure seek error rate too high"	},
	{ 0x5D, 0x24, /* D                   B       */ "Controller impending failure too many block reassigns"	},
	{ 0x5D, 0x25, /* D                   B       */ "Controller impending failure access times too high"	},
	{ 0x5D, 0x26, /* D                   B       */ "Controller impending failure start unit times too high"},
	{ 0x5D, 0x27, /* D                   B       */ "Controller impending failure channel parametrics"	},
	{ 0x5D, 0x28, /* D                   B       */ "Controller impending failure controller detected"	},
	{ 0x5D, 0x29, /* D                   B       */ "Controller impending failure throughput performance"	},
	{ 0x5D, 0x2A, /* D                   B       */ "Controller impending failure seek time performance"	},
	{ 0x5D, 0x2B, /* D                   B       */ "Controller impending failure spin-up retry count"	},
	{ 0x5D, 0x2C, /* D                   B       */ "Controller impending failure drive calibration retry count" },
	{ 0x5D, 0x30, /* D                   B       */ "Data channel impending failure general hard drive failure"  },
	{ 0x5D, 0x31, /* D                   B       */ "Data channel impending failure drive error rate too high"   },
	{ 0x5D, 0x32, /* D                   B       */ "Data channel impending failure data error rate too high"    },
	{ 0x5D, 0x33, /* D                   B       */ "Data channel impending failure seek error rate too high"    },
	{ 0x5D, 0x34, /* D                   B       */ "Data channel impending failure too many block reassigns"    },
	{ 0x5D, 0x35, /* D                   B       */ "Data channel impending failure access times too high"	     },
	{ 0x5D, 0x36, /* D                   B       */ "Data channel impending failure start unit times too high"   },
	{ 0x5D, 0x37, /* D                   B       */ "Data channel impending failure channel parametrics"	},
	{ 0x5D, 0x38, /* D                   B       */ "Data channel impending failure controller detected"	},
	{ 0x5D, 0x39, /* D                   B       */ "Data channel impending failure throughput performance"	},
	{ 0x5D, 0x3A, /* D                   B       */ "Data channel impending failure seek time performance"	},
	{ 0x5D, 0x3B, /* D                   B       */ "Data channel impending failure spin-up retry count"	},
	{ 0x5D, 0x3C, /* D                   B       */ "Data channel impending failure drive calibration retry count" },
	{ 0x5D, 0x40, /* D                   B       */ "Servo impending failure general hard drive failure"	},
	{ 0x5D, 0x41, /* D                   B       */ "Servo impending failure drive error rate too high"	},
	{ 0x5D, 0x42, /* D                   B       */ "Servo impending failure data error rate too high"	},
	{ 0x5D, 0x43, /* D                   B       */ "Servo impending failure seek error rate too high"	},
	{ 0x5D, 0x44, /* D                   B       */ "Servo impending failure too many block reassigns"	},
	{ 0x5D, 0x45, /* D                   B       */ "Servo impending failure access times too high"		},
	{ 0x5D, 0x46, /* D                   B       */ "Servo impending failure start unit times too high"	},
	{ 0x5D, 0x47, /* D                   B       */ "Servo impending failure channel parametrics"		},
	{ 0x5D, 0x48, /* D                   B       */ "Servo impending failure controller detected"		},
	{ 0x5D, 0x49, /* D                   B       */ "Servo impending failure throughput performance"	},
	{ 0x5D, 0x4A, /* D                   B       */ "Servo impending failure seek time performance"		},
	{ 0x5D, 0x4B, /* D                   B       */ "Servo impending failure spin-up retry count"		},
	{ 0x5D, 0x4C, /* D                   B       */ "Servo impending failure drive calibration retry count"	},
	{ 0x5D, 0x50, /* D                   B       */ "Spindle impending failure general hard drive failure"	},
	{ 0x5D, 0x51, /* D                   B       */ "Spindle impending failure drive error rate too high"	},
	{ 0x5D, 0x52, /* D                   B       */ "Spindle impending failure data error rate too high"	},
	{ 0x5D, 0x53, /* D                   B       */ "Spindle impending failure seek error rate too high"	},
	{ 0x5D, 0x54, /* D                   B       */ "Spindle impending failure too many block reassigns"	},
	{ 0x5D, 0x55, /* D                   B       */ "Spindle impending failure access times too high"	},
	{ 0x5D, 0x56, /* D                   B       */ "Spindle impending failure start unit times too high"	},
	{ 0x5D, 0x57, /* D                   B       */ "Spindle impending failure channel parametrics"		},
	{ 0x5D, 0x58, /* D                   B       */ "Spindle impending failure controller detected"		},
	{ 0x5D, 0x59, /* D                   B       */ "Spindle impending failure throughput performance"	},
	{ 0x5D, 0x5A, /* D                   B       */ "Spindle impending failure seek time performance"	},
	{ 0x5D, 0x5B, /* D                   B       */ "Spindle impending failure spin-up retry count"		},
	{ 0x5D, 0x5C, /* D                   B       */ "Spindle impending failure drive calibration retry count" },
	{ 0x5D, 0x60, /* D                   B       */ "Firmware impending failure general hard drive failure"	},
	{ 0x5D, 0x61, /* D                   B       */ "Firmware impending failure drive error rate too high"	},
	{ 0x5D, 0x62, /* D                   B       */ "Firmware impending failure data error rate too high"	},
	{ 0x5D, 0x63, /* D                   B       */ "Firmware impending failure seek error rate too high"	},
	{ 0x5D, 0x64, /* D                   B       */ "Firmware impending failure too many block reassigns"	},
	{ 0x5D, 0x65, /* D                   B       */ "Firmware impending failure access times too high"	},
	{ 0x5D, 0x66, /* D                   B       */ "Firmware impending failure start unit times too high"	},
	{ 0x5D, 0x67, /* D                   B       */ "Firmware impending failure channel parametrics"	},
	{ 0x5D, 0x68, /* D                   B       */ "Firmware impending failure controller detected"	},
	{ 0x5D, 0x69, /* D                   B       */ "Firmware impending failure throughput performance"	},
	{ 0x5D, 0x6A, /* D                   B       */ "Firmware impending failure seek time performance"	},
	{ 0x5D, 0x6B, /* D                   B       */ "Firmware impending failure spin-up retry count"	},
	{ 0x5D, 0x6C, /* D                   B       */ "Firmware impending failure drive calibration retry count" },
	{ 0x5D, 0xFF, /* D T L P W R O M A E B K V F */ "Failure prediction threshold exceeded (false)"		},
	{ 0x5E, 0x00, /* D T L P W R O   A     K     */ "Low power condition on"				},
	{ 0x5E, 0x01, /* D T L P W R O   A     K     */ "Idle condition activated by timer"			},
	{ 0x5E, 0x02, /* D T L P W R O   A     K     */ "Standby condition activated by timer"			},
	{ 0x5E, 0x03, /* D T L P W R O   A     K     */ "Idle condition activated by command"			},
	{ 0x5E, 0x04, /* D T L P W R O   A     K     */ "Standby condition activated by command"		},
	{ 0x5E, 0x05, /* D T L P W R O   A     K     */ "Idle_B condition activated by timer"			},
	{ 0x5E, 0x06, /* D T L P W R O   A     K     */ "Idle_B condition activated by command"			},
	{ 0x5E, 0x07, /* D T L P W R O   A     K     */ "Idle_C condition activated by timer"			},
	{ 0x5E, 0x08, /* D T L P W R O   A     K     */ "Idle_C condition activated by command"			},
	{ 0x5E, 0x09, /* D T L P W R O   A     K     */ "Standby_Y condition activated by timer"		},
	{ 0x5E, 0x0A, /* D T L P W R O   A     K     */ "Standby_Y condition activated by command"		},
	{ 0x5E, 0x41, /*                     B       */ "Power state change to active"				},
	{ 0x5E, 0x42, /*                     B       */ "Power state change to idle"				},
	{ 0x5E, 0x43, /*                     B       */ "Power state change to standby"				},
	{ 0x5E, 0x45, /*                     B       */ "Power state change to sleep"				},
	{ 0x5E, 0x47, /*                     B K     */ "Power state change to device control"			},
	{ 0x60, 0x00, /*                             */ "Lamp failure"						},
	{ 0x61, 0x00, /*                             */ "Video acquisition error"				},
	{ 0x61, 0x01, /*                             */ "Unable to acquire video"				},
	{ 0x61, 0x02, /*                             */ "Out of focus"						},
	{ 0x62, 0x00, /*                             */ "Scan head positioning error"				},
	{ 0x63, 0x00, /*           R                 */ "End of user area encountered on this track"		},
	{ 0x63, 0x01, /*           R                 */ "Packet does not fit in available space"		},
	{ 0x64, 0x00, /*           R                 */ "Illegal mode for this track"				},
	{ 0x64, 0x01, /*           R                 */ "Invalid packet size"					},
	{ 0x65, 0x00, /* D T L P W R O M A E B K V F */ "Voltage fault"						},
	{ 0x66, 0x00, /*                             */ "Automatic document feeder cover up"			},
	{ 0x66, 0x01, /*                             */ "Automatic document feeder lift up"			},
	{ 0x66, 0x02, /*                             */ "Document jam in automatic document feeder"		},
	{ 0x66, 0x03, /*                             */ "Document miss feed automatic in document feeder"	},
	{ 0x67, 0x00, /*                 A           */ "Configuration failure"					},
	{ 0x67, 0x01, /*                 A           */ "Configuration of incapable logical units failed"	},
	{ 0x67, 0x02, /*                 A           */ "Add logical unit failed"				},
	{ 0x67, 0x03, /*                 A           */ "Modification of logical unit failed"			},
	{ 0x67, 0x04, /*                 A           */ "Exchange of logical unit failed"			},
	{ 0x67, 0x05, /*                 A           */ "Remove of logical unit failed"				},
	{ 0x67, 0x06, /*                 A           */ "Attachment of logical unit failed"			},
	{ 0x67, 0x07, /*                 A           */ "Creation of logical unit failed"			},
	{ 0x67, 0x08, /*                 A           */ "Assign failure occurred"				},
	{ 0x67, 0x09, /*                 A           */ "Multiply assigned logical unit"			},
	{ 0x67, 0x0A, /* D T L P W R O M A E B K V F */ "Set target port groups command failed"			},
	{ 0x67, 0x0B, /* D T                 B       */ "ATA device feature not enabled"			},
	{ 0x68, 0x00, /*                 A           */ "Logical unit not configured"				},
	{ 0x69, 0x00, /*                 A           */ "Data loss on logical unit"				},
	{ 0x69, 0x01, /*                 A           */ "Multiple logical unit failures"			},
	{ 0x69, 0x02, /*                 A           */ "Parity/data mismatch"					},
	{ 0x6A, 0x00, /*                 A           */ "Informational, refer to log"				},
	{ 0x6B, 0x00, /*                 A           */ "State change has occurred"				},
	{ 0x6B, 0x01, /*                 A           */ "Redundancy level got better"				},
	{ 0x6B, 0x02, /*                 A           */ "Redundancy level got worse"				},
	{ 0x6C, 0x00, /*                 A           */ "Rebuild failure occurred"				},
	{ 0x6D, 0x00, /*                 A           */ "Recalculate failure occurred"				},
	{ 0x6E, 0x00, /*                 A           */ "Command to logical unit failed"			},
	{ 0x6F, 0x00, /*           R                 */ "Copy protection key exchange failure - authentication failure" },
	{ 0x6F, 0x01, /*           R                 */ "Copy protection key exchange failure - key not present" },
	{ 0x6F, 0x02, /*           R                 */ "Copy protection key exchange failure - key not established" },
	{ 0x6F, 0x03, /*           R                 */ "Read of scrambled sector without authentication"	},
	{ 0x6F, 0x04, /*           R                 */ "Media region code is mismatched to logical unit region" },
	{ 0x6F, 0x05, /*           R                 */ "Drive region must be permanent/region reset count error" },
	{ 0x6F, 0x06, /*           R                 */ "Insufficient block count for binding nonce recording"	},
	{ 0x6F, 0x07, /*           R                 */ "Conflict in binding nonce recording"			},
	{ 0x70,  '*', /*   T                         */ "Decompression exception short algorithm id of NN"	},
	{ 0x71, 0x00, /*   T                         */ "Decompression exception long algorithm id"		},
	{ 0x72, 0x00, /*           R                 */ "Session fixation error"				},
	{ 0x72, 0x01, /*           R                 */ "Session fixation error writing lead-in"		},
	{ 0x72, 0x02, /*           R                 */ "Session fixation error writing lead-out"		},
	{ 0x72, 0x03, /*           R                 */ "Session fixation error - incomplete track in session"	},
	{ 0x72, 0x04, /*           R                 */ "Empty or partially written reserved track"		},
	{ 0x72, 0x05, /*           R                 */ "No more track reservations allowed"			},
	{ 0x72, 0x06, /*           R                 */ "RMZ extension is not allowed"				},
	{ 0x72, 0x07, /*           R                 */ "No more test zone extensions are allowed"		},
	{ 0x73, 0x00, /*           R                 */ "CD control error"					},
	{ 0x73, 0x01, /*           R                 */ "Power calibration area almost full"			},
	{ 0x73, 0x02, /*           R                 */ "Power calibration area is full"			},
	{ 0x73, 0x03, /*           R                 */ "Power calibration area error"				},
	{ 0x73, 0x04, /*           R                 */ "Program memory area update failure"			},
	{ 0x73, 0x05, /*           R                 */ "Program memory area is full"				},
	{ 0x73, 0x06, /*           R                 */ "RMA/PMA is almost full"				},
	{ 0x73, 0x10, /*           R                 */ "Current power calibration area almost full"		},
	{ 0x73, 0x11, /*           R                 */ "Current power calibration area is full"		},
	{ 0x73, 0x17, /*           R                 */ "RDZ is full"						},
	{ 0x74, 0x00, /*   T                         */ "Security error"					},
	{ 0x74, 0x01, /*   T                         */ "Unable to decrypt data"				},
	{ 0x74, 0x02, /*   T                         */ "Unencrypted data encountered while decrypting"		},
	{ 0x74, 0x03, /*   T                         */ "Incorrect data encryption key"				},
	{ 0x74, 0x04, /*   T                         */ "Cryptographic integrity validation failed"		},
	{ 0x74, 0x05, /*   T                         */ "Error decrypting data"					},
	{ 0x74, 0x06, /*   T                         */ "Unknown signature verification key"			},
	{ 0x74, 0x07, /*   T                         */ "Encryption parameters not useable"			},
	{ 0x74, 0x08, /* D T       R   M    E    V F */ "Digital signature validation failure"			},
	{ 0x74, 0x09, /*   T                         */ "Encryption mode mismatch on read"			},
	{ 0x74, 0x0A, /*   T                         */ "Encrypted block not raw read enabled"			},
	{ 0x74, 0x0B, /*   T                         */ "Incorrect encryption parameters"			},
	{ 0x74, 0x0C, /* D T       R   M A E B K V   */ "Unable to decrypt parameter list"			},
	{ 0x74, 0x0D, /*   T                         */ "Encryption algorithm disabled"				},
	{ 0x74, 0x10, /* D T       R   M A E B K V   */ "SA creation parameter value invalid"			},
	{ 0x74, 0x11, /* D T       R   M A E B K V   */ "SA creation parameter value rejected"			},
	{ 0x74, 0x12, /* D T       R   M A E B K V   */ "Invalid SA usage"					},
	{ 0x74, 0x21, /*   T                         */ "Data encryption configuration prevented"		},
	{ 0x74, 0x30, /* D T       R   M A E B K V   */ "SA creation parameter not supported"			},
	{ 0x74, 0x40, /* D T       R   M A E B K V   */ "Authentication failed"					},
	{ 0x74, 0x61, /*                         V   */ "External data encryption key manager access error"	},
	{ 0x74, 0x62, /*                         V   */ "External data encryption key manager error"		},
	{ 0x74, 0x63, /*                         V   */ "External data encryption key not found"		},
	{ 0x74, 0x64, /*                         V   */ "External data encryption request not authorized"	},
	{ 0x74, 0x6E, /*   T                         */ "External data encryption control timeout"		},
	{ 0x74, 0x6F, /*   T                         */ "External data encryption control error"		},
	{ 0x74, 0x71, /* D T       R   M   E     V   */ "Logical unit access not authorized"			},
	{ 0x74, 0x79, /* D                           */ "Security conflict in translated device"		}
};
int SenseCodeEntrys = sizeof(SenseCodeTable) / sizeof(struct sense_entry);

/*
 * Function to Find Additional Sense Code/Qualifier Message.
 */
char *
ScsiAscqMsg(unsigned char asc, unsigned char asq)
{
    struct sense_entry *se;
    int entrys = 0;

    for (se = SenseCodeTable; entrys < SenseCodeEntrys; se++, entrys++) {
	/*
	 * A sense qualifier of '*' (0x2A) is used for wildcarding.
	 */
	if ( (se->sense_code == asc) &&
	     ((se->sense_qualifier == asq) ||
	      (se->sense_qualifier == '*')) ) {
	    return(se->sense_message);
	}
    }
    return((char *) 0);
}

/* ======================================================================== */

char *
SenseCodeMsg(uint8_t error_code)
{
    char *msgp;
    if ( (error_code == ECV_CURRENT_FIXED) ||
	 (error_code == ECV_CURRENT_DESCRIPTOR) ) {
	msgp = "Current Error";
    } else if ( (error_code == ECV_DEFERRED_FIXED) ||
		(error_code == ECV_DEFERRED_DESCRIPTOR) ) {
	msgp = "Deferred Error";
    } else if (error_code == ECV_VENDOR_SPECIFIC) {
	msgp = "Vendor Specific";
    } else {
	msgp = "NO CODE";
    }
    return (msgp);
}

void
GetSenseErrors(scsi_sense_t *ssp, unsigned char *sense_key,
	       unsigned char *asc, unsigned char *asq)
{
    if ( (ssp->error_code == ECV_CURRENT_FIXED) ||
	 (ssp->error_code == ECV_DEFERRED_FIXED) ) {
	*sense_key = ssp->sense_key;
	*asc = ssp->asc;
	*asq = ssp->asq;
    } else if ( (ssp->error_code == ECV_CURRENT_DESCRIPTOR) ||
		(ssp->error_code == ECV_DEFERRED_DESCRIPTOR) ) {
	scsi_sense_desc_t *ssdp = (scsi_sense_desc_t *)ssp;
	*sense_key = ssdp->sense_key;
	*asc = ssdp->asc;
	*asq = ssdp->asq;
    } else {
	*sense_key = 0;
	*asc = 0;
	*asq = 0;
    }
    return;
}

void *
GetSenseDescriptor(scsi_sense_desc_t *ssdp, uint8_t desc_type)
{
    unsigned char *bp = (unsigned char *)ssdp + 8;
    int sense_length = (int)ssdp->addl_sense_len + 8;
    void *sense_desc = NULL;

    while (sense_length > 0) {
	sense_data_desc_header_t *sdhp = (sense_data_desc_header_t *)bp;
	int descriptor_length = sdhp->additional_length + sizeof(*sdhp);

	if (sdhp->descriptor_type == desc_type) {
	    sense_desc = bp;
	    break;
	}
	sense_length -= descriptor_length;
	bp += descriptor_length;
    }
    return(sense_desc);
}

void
GetSenseInformation(scsi_sense_t *ssp, uint8_t *info_valid, uint64_t *info_value)
{
    *info_valid = 0;
    *info_value = 0;
    if ( (ssp->error_code == ECV_CURRENT_FIXED) ||
	 (ssp->error_code == ECV_DEFERRED_FIXED) ) {
	*info_valid = ssp->info_valid;
	*info_value = (uint64_t)StoH(ssp->info_bytes);
    } else if ( (ssp->error_code == ECV_CURRENT_DESCRIPTOR) ||
		(ssp->error_code == ECV_DEFERRED_DESCRIPTOR) ) {
	scsi_sense_desc_t *ssdp = (scsi_sense_desc_t *)ssp;
	information_desc_type_t *idtp;
	idtp = GetSenseDescriptor(ssdp, INFORMATION_DESC_TYPE);
	if (idtp) {
	    *info_valid = idtp->info_valid;
	    *info_value = (uint64_t)StoH(idtp->information);
	}
    }
    return;
}

void
GetSenseCmdSpecific(scsi_sense_t *ssp, uint64_t *cmd_spec_value)
{
    *cmd_spec_value = 0;
    if ( (ssp->error_code == ECV_CURRENT_FIXED) ||
	 (ssp->error_code == ECV_DEFERRED_FIXED) ) {
	*cmd_spec_value = (uint64_t)StoH(ssp->cmd_spec_info);
    } else if ( (ssp->error_code == ECV_CURRENT_DESCRIPTOR) ||
		(ssp->error_code == ECV_DEFERRED_DESCRIPTOR) ) {
	scsi_sense_desc_t *ssdp = (scsi_sense_desc_t *)ssp;
	command_specific_desc_type_t *csp;
	csp = GetSenseDescriptor(ssdp, COMMAND_SPECIFIC_DESC_TYPE);
	if (csp) {
	    *cmd_spec_value = (uint64_t)StoH(csp->information);
	}
    }
    return;
}

void
GetSenseFruCode(scsi_sense_t *ssp, uint8_t *fru_value)
{
    *fru_value = 0;
    if ( (ssp->error_code == ECV_CURRENT_FIXED) ||
	 (ssp->error_code == ECV_DEFERRED_FIXED) ) {
	*fru_value = ssp->fru_code;
    } else if ( (ssp->error_code == ECV_CURRENT_DESCRIPTOR) ||
		(ssp->error_code == ECV_DEFERRED_DESCRIPTOR) ) {
	scsi_sense_desc_t *ssdp = (scsi_sense_desc_t *)ssp;
	fru_desc_type_t *frup;
	frup = GetSenseDescriptor(ssdp, FIELD_REPLACEABLE_UNIT_DESC_TYPE);
	if (frup) {
	    *fru_value = frup->fru_code;
	}
    }
    return;
}

/*
 * Command Specific XCOPY Byte Definitions:
 */ 
#define CMD_SRC_DEVICE		0
#define CMD_DST_DEVICE		1
#define CMD_SEGMENT_LOW		2
#define CMD_SEGMENT_HIGH	3

/*
 * Function to Dump Sense Data.
 */
void
DumpSenseData(scsi_generic_t *sgp, hbool_t recursive, scsi_sense_t *ssp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int sense_length = (int) ssp->addl_sense_len + 8;
    unsigned int cmd_spec_value, info_value;

    if (recursive == False) {
	Printf(opaque, "\n");
	if ( (ssp->error_code == ECV_CURRENT_DESCRIPTOR) ||
	     (ssp->error_code == ECV_DEFERRED_DESCRIPTOR) ) {
	    DumpSenseDataDescriptor(sgp, (scsi_sense_desc_t *)ssp);
	    return;
	}
	Printf(opaque, "Request Sense Data: (sense length %d bytes)\n", sense_length);
	Printf(opaque, "\n");
    }
    PrintHex(opaque, "Error Code", ssp->error_code, DNL);
    Print(opaque, " = %s\n", SenseCodeMsg(ssp->error_code));
    PrintAscii(opaque, "Information Field Valid", (ssp->info_valid) ? "Yes" : "No", PNL);
    if (ssp->obsolete) {
	PrintHex(opaque, "Obsolete", ssp->obsolete, PNL);
    }
    PrintHex(opaque, "Sense Key", ssp->sense_key, DNL);
    Print(opaque, " = %s\n", SenseKeyMsg(ssp->sense_key));
    info_value = (unsigned int)StoH(ssp->info_bytes);
    PrintDecHex(opaque, "Information Field", info_value, PNL);
    PrintDecHex(opaque, "Additional Sense Length", ssp->addl_sense_len, PNL);
    if ( (sense_length -= 8) > 0) {
	cmd_spec_value = (unsigned int)StoH(ssp->cmd_spec_info);
	PrintDecHex(opaque, "Command Specific Information", cmd_spec_value, PNL);
	sense_length -= 4;
    }
    if (sense_length > 0) {
	char *ascq_msg = ScsiAscqMsg(ssp->asc, ssp->asq);
	PrintAscii(opaque, "Additional Sense Code/Qualifier", "", DNL);
	Print(opaque, "(%#x, %#x)", ssp->asc, ssp->asq);
	if (ascq_msg) {
	    Print(opaque, " - %s\n", ascq_msg);
	} else {
	    Print(opaque, "\n");
	}
	sense_length -=2;
    }
    if (sense_length > 0) {
	PrintHex(opaque, "Field Replaceable Unit Code",	ssp->fru_code, PNL);
	sense_length--;
    }
    if (sense_length > 0) {
	int i;
	PrintAscii(opaque, "Sense Key Specific Bytes", "", DNL);
	for (i = 0; i < (int)sizeof(ssp->sense_key_specific); i++) {
	    Print(opaque, "%02x ", ssp->sense_key_specific[i]);
	    if (--sense_length == 0) break;
	}
	Print(opaque, "\n");
	if (ssp->sense_key == SKV_COPY_ABORTED) {
	    unsigned short field_ptr;
	    scsi_sense_copy_aborted_t *sksp;

	    sksp = (scsi_sense_copy_aborted_t *)ssp->sense_key_specific;
	    field_ptr = ((sksp->field_ptr1 << 8) + sksp->field_ptr0);
	    PrintHex (opaque, "Bit Pointer to Field in Error", sksp->bit_pointer,
				    (sksp->bpv) ? DNL : PNL);
	    if (sksp->bpv) {
		Print(opaque, " (valid, bit %u)\n", (sksp->bit_pointer + 1));
	    }
	    PrintAscii(opaque, "Bit Pointer Valid", (sksp->bpv) ? "Yes" : "No", PNL);
	    PrintDec(opaque, "Segment Descriptor", sksp->sd, DNL);
	    Print(opaque, " (%s)\n", (sksp->sd) ? "error is in segment descriptor"
						: "error is in parameter list");
	    PrintHex(opaque, "Byte Pointer to Field in Error", field_ptr,
					    (field_ptr) ? DNL : PNL);
	    if (field_ptr) {
		Print(opaque, " (byte %u)\n", (field_ptr + 1)); /* zero-based */
	    }
	} else if (ssp->sense_key == SKV_ILLEGAL_REQUEST) {
	    unsigned short field_ptr;
	    scsi_sense_illegal_request_t *sksp;
	    
	    sksp = (scsi_sense_illegal_request_t *)ssp->sense_key_specific;
	    field_ptr = ((sksp->field_ptr1 << 8) + sksp->field_ptr0);
	    PrintHex (opaque, "Bit Pointer to Field in Error", sksp->bit_pointer,
				    (sksp->bpv) ? DNL : PNL);
	    if (sksp->bpv) {
		Print(opaque, " (valid, bit %u)\n", (sksp->bit_pointer + 1));
	    }
	    PrintAscii(opaque, "Bit Pointer Valid", (sksp->bpv) ? "Yes" : "No", PNL);
	    PrintHex (opaque, "Error Field Command/Data (C/D)", sksp->c_or_d, DNL);
	    Print(opaque, " (%s)\n", (sksp->c_or_d) ? "Illegal parameter in CDB bytes"
						    : "Illegal parameter in Data sent");
	    PrintHex (opaque, "Byte Pointer to Field in Error", field_ptr,
					    (field_ptr) ? DNL : PNL);
	    if (field_ptr) {
		Print(opaque, " (byte %u)\n", (field_ptr + 1)); /* zero-based */
	    }
	} else if (ssp->sense_key == SKV_NOT_READY) {
	    scsi_sense_progress_indication_t *sksp;

	    sksp = (scsi_sense_progress_indication_t *)ssp->sense_key_specific;
	    if (sksp->sksv) {
		/* Progress indication is valid. */
		DumpProgressIndication(sgp, sksp);
	    }
	}
    }
    /*
     * Additional sense bytes (if any);
     */
    if (sense_length > 0) {
	unsigned char *asbp = ssp->addl_sense;
	char *buf = Malloc(opaque, (sense_length * 3) + 1);
	char *bp = buf;
	if (buf == NULL) return;
	do {
	    bp += sprintf(bp, "%02x ", *asbp++);
	} while (--sense_length);
	PrintAscii(opaque, "Additional Sense Bytes", buf, PNL);
	free(buf);
    }
    
    /*
     * Special Handling for XCOPY Sense Data:
     */ 
    if ( (recursive == False) &&
	 ((sgp->cdb[0] == SOPC_EXTENDED_COPY) && (sgp->cdb[1] == 0)) ) {
	scsi_sense_t *xssp;
	uint16_t segment_number = ( (ssp->cmd_spec_info[CMD_SEGMENT_HIGH] << 8) +
				    ssp->cmd_spec_info[CMD_SEGMENT_LOW] );

	if (ssp->cmd_spec_info[CMD_SRC_DEVICE]) {
	    char *msg;
	    int sense_length;
	    uint8_t *bp = (unsigned char *)ssp + ssp->cmd_spec_info[CMD_SRC_DEVICE];
	    uint8_t scsi_status = bp[0];
	    xssp = (scsi_sense_t *)&bp[1];
	    sense_length = (int)(xssp->addl_sense_len + 8);
	    Printf(opaque, "\n");
	    Printf(opaque, "Copy Source Device Sense Data: (sense length %d bytes)\n", sense_length);
	    Printf(opaque, "\n");
	    PrintDec(opaque, "Segment in Error", segment_number, PNL);
	    PrintHex(opaque, "SCSI Status", scsi_status, DNL);
	    msg = ScsiStatus(scsi_status);
	    Print(opaque, " (%s)\n", msg);
	    DumpSenseData(sgp, True, xssp);
	}
	if (ssp->cmd_spec_info[CMD_DST_DEVICE]) {
	    char *msg;
	    int sense_length;
	    uint8_t *bp = (unsigned char *)ssp + ssp->cmd_spec_info[CMD_DST_DEVICE];
	    uint8_t scsi_status = bp[0];
	    xssp = (scsi_sense_t *)&bp[1];
	    sense_length = (int)(xssp->addl_sense_len + 8);
	    Printf(opaque, "\n");
	    Printf(opaque, "Copy Destination Device Sense Data: (sense length %d bytes)\n", sense_length);
	    Printf(opaque, "\n");
	    PrintDec(opaque, "Segment in Error", segment_number, PNL);
	    PrintHex(opaque, "SCSI Status", scsi_status, DNL);
	    msg = ScsiStatus(scsi_status);
	    Print(opaque, " (%s)\n", msg);
	    if (scsi_status == SCSI_CHECK_CONDITION) {
		DumpSenseData(sgp, True, xssp);
	    }
	}
	Printf(opaque, "\n");
	DumpXcopyData(sgp);
    } else if ( (recursive == False) &&
		((sgp->cdb[0] == SOPC_EXTENDED_COPY && (sgp->cdb[1] == SCSI_XCOPY_POPULATE_TOKEN))) ) {
	Printf(opaque, "\n");
	DumpPTData(sgp);
    } else if ( (recursive == False) &&
		((sgp->cdb[0] == SOPC_EXTENDED_COPY && (sgp->cdb[1] == SCSI_XCOPY_WRITE_USING_TOKEN))) ) {
	Printf(opaque, "\n");
	DumpWUTData(sgp);
    } else if ( (recursive == False) &&
		((sgp->cdb[0] == SOPC_RECEIVE_ROD_TOKEN_INFO && (sgp->cdb[1] == RECEIVE_ROD_TOKEN_INFORMATION))) ) {
	Printf(opaque, "\n");
	DumpRRTIData(sgp);
    } else {
	DumpCdbData(sgp);
    }
    return;
}

/*
 * Function to Dump Sense Data (descriptor format).
 */
void
DumpSenseDataDescriptor(scsi_generic_t *sgp, scsi_sense_desc_t *ssdp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    int sense_length = (int) ssdp->addl_sense_len + 8;
    char *ascq_msg = ScsiAscqMsg(ssdp->asc, ssdp->asq);

    Printf(opaque, "\n");
    Printf(opaque, "Request Sense Data: (sense length %d bytes)\n", sense_length);
    Printf(opaque, "\n");
    PrintHex(opaque, "Error Code", ssdp->error_code, DNL);
    Print(opaque, " = %s\n", SenseCodeMsg(ssdp->error_code));
    PrintHex(opaque, "Sense Key", ssdp->sense_key, DNL);
    Print(opaque, " = %s\n", SenseKeyMsg(ssdp->sense_key));
    PrintAscii(opaque, "Additional Sense Code/Qualifier", "", DNL);
    Print(opaque, "(%#x, %#x)", ssdp->asc, ssdp->asq);
    if (ascq_msg) {
	Print(opaque, " - %s\n", ascq_msg);
    } else {
	Print(opaque, "\n");
    }
    PrintDecHex(opaque, "Additional Sense Length", ssdp->addl_sense_len, PNL);
    if ( (sense_length -= 8) > 0) {
	DumpSenseDescriptors(sgp, ssdp, sense_length);
    }
    DumpCdbData(sgp);
    Printf(opaque, "\n");
    return;
}

void
DumpSenseDescriptors(scsi_generic_t *sgp, scsi_sense_desc_t *ssdp, int sense_length)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    unsigned char *bp = (unsigned char *)ssdp + 8;

    while (sense_length > 0) {
	sense_data_desc_header_t *sdhp = (sense_data_desc_header_t *)bp;
	int descriptor_length = sdhp->additional_length + sizeof(*sdhp);

	switch (sdhp->descriptor_type) {

	    case INFORMATION_DESC_TYPE:
		DumpInformationSense(sgp, (information_desc_type_t *)bp);
		break;

	    case COMMAND_SPECIFIC_DESC_TYPE:
		DumpCommandSpecificSense(sgp, (command_specific_desc_type_t *)bp);
		break;

	    case SENSE_KEY_SPECIFIC_DESC_TYPE:
		DumpSenseKeySpecificSense(sgp, (sense_key_specific_desc_type_t *)bp);
		break;

	    case FIELD_REPLACEABLE_UNIT_DESC_TYPE:
		DumpFieldReplaceableUnitSense(sgp, (fru_desc_type_t *)bp);
		break;

	    case BLOCK_COMMAND_DESC_TYPE:
		DumpBlockCommandSense(sgp, (block_command_desc_type_t *)bp);
		break;

	    case ATA_STATUS_RETURN_DESC_TYPE:
		DumpAtaStatusReturnSense(sgp, (ata_status_return_desc_type_t *)bp);
		break;

	    default:
		Wprintf(opaque, "Unknown descriptor type %#x\n", sdhp->descriptor_type);
		break;
	}
	/* Adjust the sense length and point to next descriptor. */
	sense_length -= descriptor_length;
	bp += descriptor_length;
    }
    return;
}

void
DumpInformationSense(scsi_generic_t *sgp, information_desc_type_t *idtp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    uint64_t info_value;
    if (idtp->info_valid) {
	info_value = (uint64_t)StoH(idtp->information);
	PrintLongDecHex(opaque, "Information Field", info_value, PNL);
    }
    return;
}

void
DumpCommandSpecificSense(scsi_generic_t *sgp, command_specific_desc_type_t *csp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    uint64_t cmd_spec_value;
    cmd_spec_value = (uint64_t)StoH(csp->information);
    PrintLongDecHex(opaque, "Command Specific Information", cmd_spec_value, PNL);
    return;
}

void
DumpSenseKeySpecificSense(scsi_generic_t *sgp, sense_key_specific_desc_type_t *sksp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    scsi_sense_desc_t *ssdp = (scsi_sense_desc_t *)sgp->sense_data;
    /* Avoid issue of taking address of a bit field! (ugly) */
    unsigned char *bp = (unsigned char *)&sksp->reserved_byte3;
    int i;

    bp++; /* Point to sense specific data. */
    PrintHex(opaque, "Sense Key Valid", sksp->sksv, PNL);
    PrintDecHex(opaque, "Sense Key Specific Bits", sksp->sense_key_bits, PNL); 
    PrintAscii(opaque, "Sense Key Bytes", "", DNL); 
    for (i = 0; i < (int)sizeof(sksp->sense_key_bytes); i++) {
	Print(opaque, "%02x ", sksp->sense_key_bytes[i]);
    }
    Print(opaque, "\n");
    if (ssdp->sense_key == SKV_ILLEGAL_REQUEST) {
	DumpIllegalRequestSense(sgp, (scsi_sense_illegal_request_t *)bp);
    } else if ( (ssdp->sense_key == SKV_RECOVERED) ||
		(ssdp->sense_key == SKV_MEDIUM_ERROR) ||
		(ssdp->sense_key == SKV_HARDWARE_ERROR) ) {
	DumpMediaErrorSense(sgp, (scsi_media_error_sense_t *)bp);
    } else if (ssdp->sense_key == SKV_NOT_READY) {
	scsi_sense_progress_indication_t *sksp;

	sksp = (scsi_sense_progress_indication_t *)bp;
	if (sksp->sksv) {
	    /* Progress indication is valid. */
	    DumpProgressIndication(sgp, sksp);
	}
    }
    return;
}

void
DumpIllegalRequestSense(scsi_generic_t *sgp, scsi_sense_illegal_request_t *sirp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    unsigned short field_ptr;

    field_ptr = ((sirp->field_ptr1 << 8) + sirp->field_ptr0);
    PrintHex(opaque, "Bit Pointer to Field in Error", sirp->bit_pointer,
				    (sirp->bit_pointer) ? DNL : PNL);
    if (sirp->bpv) {
	Print(opaque, " (valid, bit %u)\n", (sirp->bit_pointer + 1));
    }
    PrintAscii(opaque, "Bit Pointer Valid", (sirp->bpv) ? "Yes" : "No", PNL);
    PrintHex(opaque, "Error Field Command/Data (C/D)", sirp->c_or_d, DNL);
    Print(opaque, " (%s)\n", (sirp->c_or_d) ? "Illegal parameter in CDB bytes"
	                                           : "Illegal parameter in Data sent");
    PrintHex(opaque, "Byte Pointer to Field in Error", field_ptr,
					    (field_ptr) ? DNL : PNL);
    if (field_ptr) {
	Print(opaque, " (byte %u)\n", (field_ptr + 1)); /* zero-based */
    }
    return;
}

void
DumpProgressIndication(scsi_generic_t *sgp, scsi_sense_progress_indication_t *skp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    uint16_t progress;
    float progress_percentage;
    char progress_display[10];

    progress = (uint16_t)StoH(skp->progress_indication);
    progress_percentage = ((float)progress / (float)65536) * 100;
    (void)sprintf(progress_display, "%.2f%%", progress_percentage);
    PrintAscii(opaque, "Progress Indication", progress_display, PNL);
}

char *error_recovery_types[] = {
    "Read",				/* 0x00 */
    "Verify",				/* 0x01 */
    "Write",				/* 0x02 */
    "Seek",				/* 0x03 */
    "Read Sync Byte branch",		/* 0x04 */
    "Read, Thermal Asperity branch",	/* 0x05 */
    "Read, Minus Mod branch",		/* 0x06 */
    "Verify, Sync Byte branch",		/* 0x07 */
    "Verify, Thermal Asperity branch",	/* 0x08 */
    "Verify, Minus Mod branch"		/* 0x09 */
};
static int num_error_recovery_types = sizeof(error_recovery_types) / sizeof(char *);

void
DumpMediaErrorSense(scsi_generic_t *sgp, scsi_media_error_sense_t *mep)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    PrintHex(opaque, "Error Recovery Type", mep->erp_type, DNL);
    if (mep->erp_type < num_error_recovery_types) {
	Print(opaque, " = %s\n", error_recovery_types[mep->erp_type]);
    } else {
	Print(opaque, "\n");
    }
    PrintDecimal(opaque, "Secondary Recovery Step", mep->secondary_step, PNL);
    PrintDecimal(opaque, "Actual Retry Count", mep->actual_retry_count, PNL);
    return;
}

void
DumpFieldReplaceableUnitSense(scsi_generic_t *sgp, fru_desc_type_t *frup)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    PrintHex(opaque, "Field Replaceable Unit Code", frup->fru_code, PNL);
    return;
}

void
DumpBlockCommandSense(scsi_generic_t *sgp, block_command_desc_type_t *bcp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    PrintHex(opaque, "ili bit", bcp->ili, PNL);
    return;
}

void
DumpAtaStatusReturnSense(scsi_generic_t *sgp, ata_status_return_desc_type_t *asp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    PrintYesNo(opaque, True, "Extend", asp->extend, PNL);
    if (asp->reserved_byte2_bits_1_7) {
	PrintHex(opaque, "Reserved byte 2, bits 1:7", asp->reserved_byte2_bits_1_7, PNL);
    }
    PrintDecHex(opaque, "ATA Error", asp->error, PNL);
    PrintDecHex(opaque, "ATA Sector Count", (uint32_t)StoH(asp->count), PNL);
    PrintLongDecHex(opaque, "Logical Block Address", (uint64_t)StoH(asp->lba), PNL);
    PrintDecHex(opaque, "Device", asp->device, PNL);
    PrintDecHex(opaque, "ATA Status", asp->status, PNL);
    return;
}

void
DumpCdbData(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    unsigned int dump_length;
    
    if (!sgp || !sgp->data_buffer || !sgp->data_length || !sgp->data_dump_limit) {
	return;
    }
    /*
     * Note: Don't dump data read, if nothing received (Illegal Request, etc).
     */
    if ( (sgp->data_dir == scsi_data_read) && (sgp->data_transferred == 0) ) {
	return;
    }
    /* This will be valid whenever commands succeed! */
    dump_length = min(sgp->data_transferred, sgp->data_dump_limit);
    if (dump_length == 0) {
	dump_length = min(sgp->data_length, sgp->data_dump_limit);
    }
    Printf(opaque, "\n");
    if (sgp->cdb[0] != SOPC_REQUEST_SENSE) {
	Printf(opaque, "CDB Data %s: (%u bytes)\n",
	       (sgp->data_dir == scsi_data_read) ? "Received" : "Sent",
	       dump_length);
	Printf(opaque, "\n");
	DumpFieldsOffset(opaque, (uint8_t *)sgp->data_buffer, dump_length);
    }
    return;
}

/* ================================================================================== */

void
DumpXcopyData(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    xcopy_lid1_parameter_list_t *paramp;
    //xcopy_id_cscd_desc_t *tgtdp;
    xcopy_id_cscd_ident_desc_t *tgtdp;
    xcopy_b2b_seg_desc_t *segdp;
    xcopy_cdb_t *cdb = (xcopy_cdb_t *)sgp->cdb;
    unsigned int target_list_length, segment_list_length;
    uint32_t xcopy_length;
    int num_targets, num_segments;
    unsigned char *bp = (unsigned char *)sgp->data_buffer;
    int target, segment;
    
    xcopy_length = (uint32_t)StoH(cdb->parameter_list_length);
    if (!xcopy_length) return;
    if (!sgp->data_buffer || !sgp->data_length) return;
    
    Printf(opaque, "Extended Copy Parameter Data: (destination device %s)\n", sgp->dsf);
    Printf(opaque, "\n");
    DumpFieldsOffset(opaque, (uint8_t *)sgp->data_buffer, xcopy_length);

    paramp = (xcopy_lid1_parameter_list_t *)bp;
    DumpParameterListDescriptor(sgp, paramp, (unsigned int)((unsigned char *)paramp - bp));

    target_list_length = (uint32_t)StoH(paramp->cscd_desc_list_length);
    if (!target_list_length) return;
    num_targets = (target_list_length / sizeof(*tgtdp));
    tgtdp = (xcopy_id_cscd_ident_desc_t *)(paramp + 1);
    /* TODO: Support multiple descriptor types! */
    //tgtdp = (xcopy_id_cscd_desc_t *)(paramp + 1);
    for (target = 0; target < num_targets; target++, tgtdp++) {
	DumpTargetDescriptor(sgp, tgtdp, target, (unsigned int)((unsigned char *)tgtdp - bp));
    }
    
    segment_list_length = (uint32_t)StoH(paramp->seg_desc_list_length);
    num_segments = (segment_list_length / sizeof(*segdp));
    if (!num_segments) return;
    segdp = (xcopy_b2b_seg_desc_t *)tgtdp;
    for (segment = 0; segment < num_segments; segment++, segdp++) {
	DumpSegmentDescriptor(sgp, segdp, segment, (unsigned int)((unsigned char *)segdp - bp));
    }
    return;
}
    
void
DumpParameterListDescriptor(scsi_generic_t *sgp, xcopy_lid1_parameter_list_t *paramp, unsigned offset)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    Printf(opaque, "\n");
    Printf(opaque, "Parameter List Descriptor: (offset: %u, length: %d)\n", offset, sizeof(*paramp));
    Printf(opaque, "\n");
    PrintHex(opaque, "List Identifier", paramp->list_identifier, PNL);
    PrintNumeric(opaque, "Priority", paramp->priority, PNL);
    PrintNumeric(opaque, "List ID Usage", paramp->listid_usage, PNL);
    PrintNumeric(opaque, "Sequential Striped (str)", paramp->str,  PNL);
    PrintNumeric(opaque, "Reserved (bits 6:7)", paramp->reserved_6_7,  PNL);
    PrintDecHex(opaque, "Target Descriptor List Length", (uint32_t)StoH(paramp->cscd_desc_list_length), PNL);
    //PrintHex(opaque, "Reserved (bytes 4 thru 7)", (uint32_t)StoH(paramp->reserved_4_7), PNL);
    PrintAscii(opaque, "Reserved (bytes 4 thru 7)", "", DNL);
    PrintFields(opaque, (uint8_t *)paramp->reserved_4_7, sizeof(paramp->reserved_4_7));
    PrintDecHex(opaque, "Segment Descriptor List Length", (uint32_t)StoH(paramp->seg_desc_list_length), PNL);
    PrintDecHex(opaque, "Inline Data Length", (uint32_t)StoH(paramp->inline_data_length), PNL);
    return;
}

void
DumpTargetDescriptor(scsi_generic_t *sgp, xcopy_id_cscd_ident_desc_t *tgtdp, int target_number, unsigned offset)
//DumpTargetDescriptor(scsi_generic_t *sgp, xcopy_id_cscd_desc_t *tgtdp, int target_number, unsigned offset)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    char *buffer, *bp;
    int i;
    Printf(opaque, "\n");
    Printf(opaque, "Target Descriptor %u: (offset: %u, length: %d)\n", target_number, offset, sizeof(*tgtdp));
    Printf(opaque, "\n");
    PrintHex(opaque, "Descriptor Type Code", tgtdp->desc_type_code, PNL);
    PrintHex(opaque, "Device Type", tgtdp->device_type,  PNL);
    PrintHex(opaque, "Relative Initiator Port ID", (uint32_t)StoH(tgtdp->relative_init_port_id), PNL);
    PrintHex(opaque, "Code Set", tgtdp->codeset, PNL);
    PrintHex(opaque, "Designator Type", tgtdp->designator_type, PNL);
    PrintHex(opaque, "Reserved (byte 6)", tgtdp->reserved_byte6, PNL);
    PrintDecHex(opaque, "Designator Length", tgtdp->designator_length, PNL);
    buffer = bp = Malloc(opaque, sizeof(tgtdp->designator) * 4);
    for (i = 0; i < (int)tgtdp->designator_length; i++) {
	bp += sprintf(bp, "%02x ", tgtdp->designator[i]);
    }
    PrintAscii(opaque, "Designator", buffer, PNL);
    free(buffer);
    //PrintAscii(opaque, "Reserved (bytes 24 thru 27)", "", DNL);
    //PrintFields(opaque, (uint8_t *)tgtdp->reserved_24_27, sizeof(tgtdp->reserved_24_27));
    PrintDec(opaque, "Device Type Specific Length", sizeof(tgtdp->type_spec_params), PNL);
    PrintBoolean(opaque, False, "PAD", tgtdp->type_spec_params.pad, PNL);
    PrintDecHex(opaque, "Disk Block Length", (uint32_t)StoH(tgtdp->type_spec_params.disk_block_length), PNL);
    return;
}

void
DumpSegmentDescriptor(scsi_generic_t *sgp, xcopy_b2b_seg_desc_t *segdp, int segment_number, unsigned offset)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    uint16_t blocks;
    uint64_t starting_lba;
    int src_index, dst_index;
    char *src_dsf = NULL, *dst_dsf = NULL;
    scsi_device_t *sdp = opaque;
    
    src_index = (int)StoH(segdp->src_cscd_desc_idx);
    dst_index = (int)StoH(segdp->dst_cscd_desc_idx);
    if (sdp && (src_index < sdp->io_devices) ) {
       src_dsf = sdp->io_params[src_index].sg.dsf;
    }
    if (sdp && (dst_index < sdp->io_devices) ) {
       dst_dsf = sdp->io_params[dst_index].sg.dsf;
    }
    
    Printf(opaque, "\n");
    Printf(opaque, "Segment Descriptor %u: (offset: %u, length: %d)\n", segment_number, offset, sizeof(*segdp));
    Printf(opaque, "\n");
    PrintHex(opaque, "Descriptor Type Code", segdp->desc_type_code, PNL);
    PrintBoolean(opaque, False, "CAT",  segdp->cat, PNL);
    PrintBoolean(opaque, False, "Destination Count (DC)",  segdp->dc, PNL);
    PrintDecHex(opaque, "Descriptor Length",  (uint32_t)StoH(segdp->desc_length), PNL);
    PrintDecimal(opaque, "Source Descriptor Index", src_index, DNL);
    if (src_dsf) {
	Print(opaque, " (%s)\n", src_dsf);
    } else {
	Print(opaque, "\n");
    }
    PrintDecimal(opaque, "Destination Descriptor Index", dst_index, DNL);
    if (dst_dsf) {
	Print(opaque, " (%s)\n", dst_dsf);
    } else {
	Print(opaque, "\n");
    }
    PrintAscii(opaque, "Reserved (bytes 8 thru 9)", "", DNL);
    PrintFields(opaque, (uint8_t *)segdp->reserved_bytes_8_9, sizeof(segdp->reserved_bytes_8_9));
    blocks = (uint16_t)StoH(segdp->block_device_num_of_blocks);
    PrintDecHex(opaque, "Number of Blocks", blocks, PNL);
    starting_lba = StoH(segdp->src_block_device_lba);
    PrintLongDecHex(opaque, "Source Block Device LBA", starting_lba, DNL);
    Print(opaque, " (lba's " LUF " - " LUF ")\n", starting_lba, (starting_lba + blocks - 1));
    starting_lba = StoH(segdp->dst_block_device_lba);
    PrintLongDecHex(opaque, "Destination Block Device LBA", starting_lba, DNL);
    Print(opaque, " (lba's " LUF " - " LUF ")\n", starting_lba, (starting_lba + blocks - 1));
    return;
}

void
DumpRangeDescriptor(scsi_generic_t *sgp, range_descriptor_t *rdp, int descriptor_number, unsigned offset)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    uint32_t blocks;
    uint64_t starting_lba;

    Printf(opaque, "\n");
    Printf(opaque, "Block Range Descriptor %u: (offset: %u, length: %d)\n", descriptor_number, offset, sizeof(*rdp));
    Printf(opaque, "\n");
    blocks = (uint32_t)StoH(rdp->length);
    PrintDecHex(opaque, "Number of Blocks", blocks, PNL);
    starting_lba = StoH(rdp->lba);
    PrintLongDecHex(opaque, "Source Block Device LBA", starting_lba, DNL);
    Print(opaque, " (lba's " LUF " - " LUF ")\n", starting_lba, (starting_lba + blocks - 1));
    PrintAscii(opaque, "Reserved (bytes 12 thru 15)", "", DNL);
    PrintFields(opaque, (uint8_t *)rdp->reserved_byte_12_15, sizeof(rdp->reserved_byte_12_15));
    return;
}

void
DumpPTData(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    populate_token_cdb_t *cdb = (populate_token_cdb_t *)sgp->cdb;
    scsi_device_t *sdp = opaque;
    populate_token_parameter_list_t *ptp;
    range_descriptor_t *rdp;
    unsigned char *bp = (unsigned char *)sgp->data_buffer;
    uint16_t range_descriptor_length;
    int descriptor, num_descriptors;
    unsigned int listid = (uint32_t)StoH(cdb->list_identifier);

    if (bp == NULL) return;

    ptp = (populate_token_parameter_list_t *)bp;
    if (sgp->data_length < sizeof(populate_token_parameter_list_t)) return;

    Printf(opaque, "\n");
    Printf(opaque, "Populate Token (PT) Parameter Data:\n");
    Printf(opaque, "\n");
    PrintDecHex(opaque, "List Identifier", listid, PNL);
    PrintDecimal(opaque, "Data Length", (uint16_t)StoH(ptp->data_length), PNL);
    PrintDecimal(opaque, "Immediate (bit 0)", ptp->immed, PNL);
    PrintDecimal(opaque, "ROD Type Valid (bit 1)", ptp->rtv, PNL);
    PrintHex(opaque, "Reserved (byte 2, bits 2:7)", ptp->reserved_byte2_b2_7, PNL);
    PrintHex(opaque, "Reserved (byte 3)", ptp->reserved_byte3, PNL);
    PrintHex(opaque, "Inactivity Timeout", (uint32_t)StoH(ptp->inactivity_timeout), PNL);
    PrintHex(opaque, "ROD Type", (uint32_t)StoH(ptp->rod_type), PNL);
    PrintHex(opaque, "Reserved (bytes 12 thru 13)", (uint32_t)StoH(ptp->reserved_byte_12_13), PNL);
    range_descriptor_length = (uint16_t)StoH(ptp->range_descriptor_list_length);
    PrintDecimal(opaque, "Range Descriptor List Length", range_descriptor_length, PNL);

    /* The block device range descriptors follows the parameter list. */

    if (sgp->data_length < (sizeof(populate_token_parameter_list_t) + range_descriptor_length)) return;
    num_descriptors = (range_descriptor_length / sizeof(range_descriptor_t));
    rdp = (range_descriptor_t *)(bp + sizeof(populate_token_parameter_list_t));
    
    for (descriptor = 0; descriptor < num_descriptors; descriptor++, rdp++) {
	DumpRangeDescriptor(sgp, rdp, descriptor, (unsigned int)((unsigned char *)rdp - bp));
    }
    return;
}

static struct RRTI_CopyStatusTable {
    unsigned char copy_status;
    char          *copy_status_msg;
} rrti_CopyStatusTable[] = {
    { COPY_STATUS_UNINIT,		"STATUS_UNINIT"		},	/* 0x00 */
    { COPY_STATUS_SUCCESS,		"STATUS_SUCCESS"	},	/* 0x01 */
    { COPY_STATUS_FAIL,			"STATUS_FAIL"		},	/* 0x02	*/
    { COPY_STATUS_SUCCESS_RESID,	"STATUS_SUCCESS_RESID"	},	/* 0x03 */
    { COPY_STATUS_FOREGROUND,		"STATUS_FOREGROUND"	},	/* 0x11 */
    { COPY_STATUS_BACKGROUND,		"STATUS_BACKGROUND"	},	/* 0x12 */
    { COPY_STATUS_TERMINATED,		"STATUS_TERMINATED"	}	/* 0xE0 */
};
static int rrti_CopyStatusEntrys = sizeof(rrti_CopyStatusTable) / sizeof(rrti_CopyStatusTable[0]);

char *
RRTICopyStatus(unsigned char copy_status)
{
    struct RRTI_CopyStatusTable *cst = rrti_CopyStatusTable;
    int entrys;

    for (entrys = 0; entrys < rrti_CopyStatusEntrys; cst++, entrys++) {
	if (cst->copy_status == copy_status) {
	    return (cst->copy_status_msg);
	}
    }
    return ("???");
}

void
DumpRRTIData(scsi_generic_t *sgp)
{
    receive_copy_results_cdb_t *cdb = (receive_copy_results_cdb_t *)sgp->cdb;
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    scsi_device_t *sdp = opaque;
    rrti_parameter_data_t *rrtip;
    unsigned char *bp = (unsigned char *)sgp->data_buffer;
    char *copy_status_msg;
    unsigned int listid = (uint32_t)StoH(cdb->list_identifier);

    if (bp == NULL) return;
    if (sgp->data_length < sizeof(rrti_parameter_data_t)) return;

    rrtip = (rrti_parameter_data_t *)bp;
    
    Printf(opaque, "\n");
    Printf(opaque, "Receive ROD Token Information (RRTI) Data:\n");
    Printf(opaque, "\n");
    PrintDecHex(opaque, "List Identifier", listid, PNL);
    PrintDecimal(opaque, "Available Data", (uint32_t)StoH(rrtip->available_data), PNL);
    PrintHex(opaque, "Response to Service Action", rrtip->response_to_service_action, DNL);
    switch (rrtip->response_to_service_action) {
	case SCSI_RRTI_PT:
	    Print(opaque, " (Populate Token)\n");
	    break;
	case SCSI_RRTI_WUT:
	    Print(opaque, " (Write Using Token)\n");
	    break;
	default:
	    Print(opaque, "\n");
	    break;
    }
    PrintHex(opaque, "Copy Operation Status", rrtip->copy_operation_status, DNL);
    copy_status_msg = RRTICopyStatus(rrtip->copy_operation_status);
    Print(opaque, " (%s)\n", copy_status_msg);
    PrintDecimal(opaque, "Operation Counter", (uint16_t)StoH(rrtip->operation_counter), PNL);
    PrintDecimal(opaque, "Estimated Status Update Delay", (uint32_t)StoH(rrtip->estimated_status_update_delay), PNL);
    PrintHex(opaque, "Extended Copy Completion Status", rrtip->extended_copy_completion_status, PNL);
    PrintDecimal(opaque, "Sense Data Field Length", rrtip->sense_data_field_length, PNL);
    PrintDecimal(opaque, "Sense Data Length", rrtip->sense_data_length, PNL);
    PrintDecHex(opaque, "Transfer Count Units", rrtip->transfer_count_units,  PNL);
    PrintLongDecHex(opaque, "Transfer Count", StoH(rrtip->transfer_count),  PNL);
    PrintDecimal(opaque, "Segments Processed",  (uint16_t)StoH(rrtip->segments_processed), PNL);
    //PrintLongHex(opaque, "Reserved (bytes 26 thru 31)", StoH(rrtip->reserved), PNL);
    PrintAscii(opaque, "Reserved (bytes 26 thru 31)", "", DNL);
    PrintFields(opaque, (uint8_t *)rrtip->reserved_byte_26_31, sizeof(rrtip->reserved_byte_26_31));
    /*
     * Display the sense data (if any).
     */ 
    if (rrtip->sense_data_length) {
	scsi_sense_t *ssp;
	PrintAscii(opaque, "Sense Data", "", DNL);
	bp += sizeof(rrti_parameter_data_t);
	PrintFields(opaque, (uint8_t *)bp, rrtip->sense_data_length);
	ssp = (scsi_sense_t *)bp;
	Printf(opaque, "\n");
	Printf(opaque, "Copy Sense Data: (sense length %u bytes)\n", rrtip->sense_data_length);
	Printf(opaque, "\n");
	DumpSenseData(sgp, True, ssp);
    }
    return;
}

void
DumpWUTData(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    write_using_token_cdb_t *cdb = (write_using_token_cdb_t *)sgp->cdb;
    scsi_device_t *sdp = opaque;
    wut_parameter_list_t *wutp;
    range_descriptor_t *rdp;
    wut_parameter_list_runt_t *runtp;
    unsigned char *bp = (unsigned char *)sgp->data_buffer;
    uint16_t range_descriptor_length;
    int descriptor, num_descriptors, range_descriptors_offset;
    unsigned int listid = (uint32_t)StoH(cdb->list_identifier);

    if (bp == NULL) return;
    wutp = (wut_parameter_list_t *)bp;
    if (sgp->data_length < sizeof(wut_parameter_list_t)) return;
    
    Printf(opaque, "\n");
    Printf(opaque, "Write Using Token (WUT) Parameter Data:\n");
    Printf(opaque, "\n");
    PrintDecHex(opaque, "List Identifier", listid, PNL);
    PrintDecimal(opaque, "Data Length", (uint16_t)StoH(wutp->data_length), PNL);
    PrintDecimal(opaque, "Immediate (bit 0)", wutp->immed, PNL);
    PrintDecimal(opaque, "Delete Token (bit 1)", wutp->del_tkn, PNL);
    PrintHex(opaque, "Reserved (byte 2, bits 2:7)", wutp->reserved_byte2_b2_7, PNL);
    PrintAscii(opaque, "Reserved (bytes 3 thru 7)", "", DNL);
    PrintFields(opaque, (uint8_t *)wutp->reserved_byte3_7, sizeof(wutp->reserved_byte3_7));
    PrintLongDec(opaque, "Offset Into ROD", StoH(wutp->offset_into_rod), PNL);
    
    /* Next is the ROD token data (512 bytes). */
    
    if (sgp->data_length < (sizeof(wut_parameter_list_t) + ROD_TOKEN_LENGTH)) return;
    PrintAscii(opaque, "ROD Token Data", "", DNL);
    PrintHAFields(opaque, (bp + ROD_TOKEN_OFFSET), ROD_TOKEN_LENGTH);
    
    /* Next is the range descriptor list descriotor. */
    runtp = (wut_parameter_list_runt_t *)(bp + ROD_TOKEN_OFFSET + ROD_TOKEN_LENGTH);
    PrintAscii(opaque, "Reserved (bytes 528 thru 533)", "", DNL);
    PrintFields(opaque, (uint8_t *)runtp->reserved, sizeof(runtp->reserved));
    range_descriptor_length = (uint16_t)StoH(runtp->range_descriptor_list_length);
    PrintDecimal(opaque, "Range Descriptor List Length", range_descriptor_length, PNL);

    /* The block device range descriptors follows the parameter list. */

    range_descriptors_offset = (ROD_TOKEN_OFFSET + ROD_TOKEN_LENGTH + sizeof(*runtp));
    if (sgp->data_length < (unsigned)(range_descriptors_offset +  range_descriptor_length)) return;
    num_descriptors = (range_descriptor_length / sizeof(range_descriptor_t));
    rdp = (range_descriptor_t *)(bp + range_descriptors_offset);
    
    for (descriptor = 0; descriptor < num_descriptors; descriptor++, rdp++) {
	DumpRangeDescriptor(sgp, rdp, descriptor, (unsigned int)((unsigned char *)rdp - bp));
    }
    return;
}

/* ================================================================================== */

hbool_t
isReadWriteRequest(scsi_generic_t *sgp)
{
    switch (sgp->cdb[0]) {
	case SOPC_READ_BUFFER:
	case SOPC_WRITE_BUFFER:
	case SOPC_READ_6:
	case SOPC_READ_10:
	case SOPC_READ_16:
	case SOPC_READ_LONG:
	case SOPC_WRITE_6:
	case SOPC_WRITE_10:
	case SOPC_WRITE_16:
	case SOPC_WRITE_VERIFY:
	case SOPC_WRITE_AND_VERIFY_16:
	case SOPC_WRITE_LONG:
	case SOPC_WRITE_SAME:
	case SOPC_WRITE_SAME_16:
	case SOPC_COMPARE_AND_WRITE:
	    return(True);
	    
	default:
	    return(False);
    }
}

void
GenerateSptCmd(scsi_generic_t *sgp)
{
    void *opaque = (sgp->tsp) ? sgp->tsp->opaque : NULL;
    scsi_device_t *sdp = opaque;
    char buffer[LOG_BUFSIZE];
    char *bp = buffer;
    unsigned int i;
    
    bp += sprintf(bp, "cdb=\"");
    for (i = 0; (i < sgp->cdb_size); i++) {
	bp += sprintf(bp, "%02x ", sgp->cdb[i]);
    }
    bp--; *bp = '\0';
    bp += sprintf(bp, "\" ");
    if (sgp->data_dir == scsi_data_none) {
	bp += sprintf(bp, "dir=none ");
    } else if (sgp->data_dir == scsi_data_read) {
	bp += sprintf(bp, "dir=read ");
    } else if (sgp->data_dir == scsi_data_write) {
	bp += sprintf(bp, "dir=write ");
    }
    if (sgp->data_length) {
	bp += sprintf(bp, "length=%u ", sgp->data_length);
    }
    if (sgp->cdb_name) {
	bp += sprintf(bp, "sname=\"%s\" ", sgp->cdb_name);
    }
    if (sdp && (sdp->user_pattern == True)) {
	if (sdp->iot_pattern == True) {
	    bp += sprintf(bp, "pattern=iot ");
	} else {
	    bp += sprintf(bp, "pattern=%08x ", sdp->pattern);
	}
    }
    if ( sgp->data_buffer && sgp->data_length && (sgp->data_dir == scsi_data_write) ) {
	uint8_t *dp = sgp->data_buffer;
	unsigned int data_length = sgp->data_length;
	/* This will be valid whenever commands succeed! */
	//data_length = min(sgp->data_transferred, sgp->data_dump_limit);
#if 0
	if (data_length == 0) {
	    data_length = min(sgp->data_length, sgp->data_dump_limit);
	}
#endif
	if ( isReadWriteRequest(sgp) == False) {
	    bp += sprintf(bp, "pout=\"");
	    for (i = 0; (i < data_length); i++) {
		bp += sprintf(bp, "%02x ", dp[i]);
	    }
	    if (data_length < sgp->data_length) {
		bp += sprintf(bp, "...");
	    } else {
		bp--; *bp = '\0';
	    }
	    bp += sprintf(bp, "\" ");
	}
    }
    bp--; *bp = '\0';
    bp += sprintf(bp, "\n");
    Printf(opaque, "# dsf=%s\n", (sgp->adsf) ? sgp->adsf : sgp->dsf);
    Printf(opaque, "%s", buffer);
    return;
}

/* ================================================================================== */
/*
 * Functions to Decode Segment and Target Descriptor Types:
 */

typedef struct segment_type_entry {
    int	segment_descriptor_type;
    char *segment_description;
} segment_type_entry_t;

segment_type_entry_t segment_type_table[] = {
    { SEGMENT_DESC_TYPE_COPY_BLOCK_TO_STREAM,
	"Copy from block device to stream device"	},
    { SEGMENT_DESC_TYPE_COPY_STREAM_TO_BLOCK,
	"Copy from stream device to block device"	},
    { SEGMENT_DESC_TYPE_COPY_BLOCK_TO_BLOCK,
	"Copy from block device to block device"	},
    { SEGMENT_DESC_TYPE_COPY_STREAM_TO_STREAM,
	"Copy from stream device to stream device"	},
    { SEGMENT_DESC_TYPE_COPY_INLINE_DATA_TO_STREAM,
	"Copy inline data to stream device"		},
    { SEGMENT_DESC_TYPE_COPY_EMBEDDED_TO_STREAM,
	"Copy embedded data to stream device"		},
    { SEGMENT_DESC_TYPE_READ_STREAM_DISCARD,
	"Read from stream device and discard"		},
    { SEGMENT_DESC_TYPE_VERIFY_CSCD,
	"Verify CSCD"					},
    { SEGMENT_DESC_TYPE_COPY_BLOCK_OFFSET_TO_STREAM,
	"Copy block device with offset to stream device" },
    { SEGMENT_DESC_TYPE_COPY_STREAM_TO_BLOCK_OFFSET,
	"Copy stream device to block device with offset" },
    { SEGMENT_DESC_TYPE_COPY_BLOCK_OFFSET_TO_BLOCK_OFFSET,
	"Copy block device with offset to block device with offset" },
    { SEGMENT_DESC_TYPE_COPY_BLOCK_TO_STREAM_HOLD_COPY,
	"Copy from block device to stream device and hold a copy of processed data for the application client" },
    { SEGMENT_DESC_TYPE_COPY_STREAM_TO_BLOCK_HOLD_COPY,
	"Copy from stream device to block device and hold a copy of processed data for the application client" },
    { SEGMENT_DESC_TYPE_COPY_BLOCK_TO_BLOCK_HOLD_COPY,
	"Copy from block device to block device and hold a copy of processed data for the application client" },
    { SEGMENT_DESC_TYPE_COPY_STREAM_TO_STREAM_HOLD_COPY,
	"Copy from stream device to stream device and hold a copy of processed data for the application client" },
    { SEGMENT_DESC_TYPE_READ_STREAM_HOLD_COPY,
	"Read from stream device and hold a copy of processed data for the application client." },
    { SEGMENT_DESC_TYPE_WRITE_FM_TO_SEQUENTIAL,
	"Write filemarks to sequential-access device"	},
    { SEGMENT_DESC_TYPE_SPACE_RECORDS_ON_SEQUENTIAL,
	"Space records or filemarks on sequential-access" },
    { SEGMENT_DESC_TYPE_LOCATE_ON_SEQUENTIAL,
	"Locate on sequential-access device"		},
    { SEGMENT_DESC_TYPE_TAPE_IMAGE_COPY,
	"Tape device image copy"			},
    { SEGMENT_DESC_TYPE_REGISTER_PERSISTEMT_RESERVATION_KEY,
	"Register persistent reservation key"		},
    { SEGMENT_DESC_TYPE_THIRD_PARTY_PR_SOURCE_I_T_NEXUS,
	"Third party persistent reservations source I_T nexus" },
    { SEGMENT_DESC_TYPE_BLOCK_IMAGE_COPY,
	"Block device image copy"			},
    { SEGMENT_DESC_TYPE_POPULATE_ROD_FROM_BLOCK_RANGES,
	"Populate ROD from one or more block ranges ROD" },
    { SEGMENT_DESC_TYPE_POPULATE_ROD_FROM_ONE_BLOCK_RANGE,
	"Populate ROD from one block range ROD"		}
};
static int segment_type_tableEntrys = sizeof(segment_type_table) / sizeof(segment_type_table[0]);

char *
FindSegmentTypeMsg(uint8_t segment_descriptor_type)
{
    segment_type_entry_t *stp = segment_type_table;
    int entrys;

    for (entrys = 0; entrys < segment_type_tableEntrys; stp++, entrys++) {
	if (stp->segment_descriptor_type == segment_descriptor_type) {
	    return (stp->segment_description);
	}
    }
    if ( (segment_descriptor_type >= SEGMENT_DESC_TYPE_RESERVED_START) ||
	 (segment_descriptor_type <= SEGMENT_DESC_TYPE_RESERVED_END) ) {
	return("<reserved>");
    } else {
	return("<unknown>");
    }
}

typedef struct target_type_entry {
    int	target_descriptor_type;
    char *target_description;
} target_type_entry_t;

target_type_entry_t target_type_table[] = {
    { TARGET_CSCD_TYPE_CODE_FC_N_PORT_NAME,	"Fibre Channel N_Port_Name"	},
    { TARGET_CSCD_TYPE_CODE_FC_N_PORT_ID,	"Fibre Channel N_Port_ID"	},
    { TARGET_CSCD_TYPE_CODE_FC_N_PORT_ID_NAME,	"Fibre Channel N_Port_ID w/N_Port_Name checking" },
    { TARGET_CSCD_TYPE_CODE_PARALLEL_INT_T_L,	"Parallel Interface T_L"	},
    { TARGET_CSCD_TYPE_CODE_IDENTIFICATION,	"Identification Descriptor"	},
    { TARGET_CSCD_TYPE_CODE_IPV4,		"IPv4"				},
    { TARGET_CSCD_TYPE_CODE_ALIAS,		"Alias"				},
    { TARGET_CSCD_TYPE_CODE_RDMA,		"RDMA"				},
    { TARGET_CSCD_TYPE_CODE_IEEE_EUI_64,	"IEEE 1394 EUI-64"		},
    { TARGET_CSCD_TYPE_CODE_SAS_SERIAL_SCSI,	"SAS Serial SCSI Protocol"	},
    { TARGET_CSCD_TYPE_CODE_IPV6,		"IPv6 CSCD descriptor"		},
    { TARGET_CSCD_TYPE_CODE_COPY_SERVICE,	"IP Copy Service"		},
    { TARGET_CSCD_TYPE_CODE_ROD,		"ROD"				}
};
static int target_type_tableEntrys = sizeof(target_type_table) / sizeof(target_type_table[0]);

char *
FindTargetTypeMsg(uint8_t target_descriptor_type)
{
    target_type_entry_t *ttp = target_type_table;
    int entrys;

    for (entrys = 0; entrys < target_type_tableEntrys; ttp++, entrys++) {
	if (ttp->target_descriptor_type == target_descriptor_type) {
	    return (ttp->target_description);
	}
    }
    if ( (target_descriptor_type >= TARGET_CSCD_TYPE_CODE_RESERVED_START) ||
	 (target_descriptor_type <= TARGET_CSCD_TYPE_CODE_RESERVED_END) ) {
	return("<reserved>");
    } else {
	return("<unknown>");
    }
}

char *
GetDescriptorTypeMsg(char **descriptor_type, uint8_t descriptor_type_code)
{
    if (descriptor_type_code < SEGMENT_DESC_TYPE_LAST_ENTRY) {
	*descriptor_type = "Segment Descriptor";
	return( FindSegmentTypeMsg(descriptor_type_code) );
    } else {
	*descriptor_type = "Target Descriptor";
	return( FindTargetTypeMsg(descriptor_type_code) );
    }
}
