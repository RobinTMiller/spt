#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spt.h"
//#include "libscsi.h"
#include "inquiry.h"
#include "scsi_cdbs.h"

/*
 * Forward References:
 */ 
char *GetDeviceName(uint8_t device_type);
char *GetPeripheralQualifier(inquiry_t *inquiry, hbool_t fullname);

/*
 * Short names are preferred, but backwards compatability must be kept.
 */
static struct {
	char *sname;
	char *fname;
} pqual_table[] = {
  { "Device Connected",     "Peripheral Device Connected"      }, /* 0x0 */
  { "Device NOT Connected", "Peripheral Device NOT Connected"  }, /* 0x1 */
  { "Reserved",		    "Reserved"			       }, /* 0x2 */
  { "No Physical Device Support", "No Physical Device Support" }  /* 0x3 */
};

static struct {
	char *sname;
	char *fname;
} ansi_table[] = {
  { "!ANSI", "May or maynot comply to ANSI-approved standard"},	/* 0x0 */
  { "SCSI-1", "Complies to ANSI X3.131-1986, SCSI-1"	},	/* 0x1 */
  { "SCSI-2", "Complies to ANSI X3.131-1994, SCSI-2"	},	/* 0x2 */
  { "SCSI-3", "Complies to ANSI X3.301-1997, SCSI-3"	},	/* 0x3 */
  { "SPC-2",  "Complies to ANSI INCITS 351-2001, SPC-2"	},	/* 0x4 */
  { "SPC-3",  "Complies to ANSI INCITS 408-2005, SPC-3"	},	/* 0x5 */
  { "SPC-4",  "Complies to ANSI Working Draft, SPC-4"	}	/* 0x6 */
};
static int ansi_entries = (sizeof(ansi_table) / sizeof(ansi_table[0]));

static char *rdf_table[] = {
	"SCSI-1",						/* 0x00 */
	"CCS",							/* 0x01 */
	"SCSI-2",						/* 0x02 */
	"SCSI-3"						/* 0x03 */
							 /* 0x04 - 0x1F */
};
static int rdf_entries = (sizeof(rdf_table) / sizeof(char *));

/*
 * Operating Definition Parameter Table.
 */
static char *opdef_table[] = {
	"Use Current",						/* 0x00 */
	"SCSI-1",						/* 0x01 */
	"CCS",							/* 0x02 */
	"SCSI-2",						/* 0x03 */
	"SCSI-3",						/* 0x04 */
	"SPC-3"							/* 0x05 */
};
static int opdef_entries = (sizeof(opdef_table) / sizeof(char *));

char *reserved_str = "Reserved";
char *vendor_specific_str = "Vendor Specific";

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
    char vid[sizeof(inquiry->inq_vid)+1];
    char pid[sizeof(inquiry->inq_pid)+1];
    char revlevel[sizeof(inquiry->inq_revlevel)+1];
    int	addl_len = (int) inquiry->inq_addlen;
    int status = SUCCESS;

    if (inquiry == NULL) {
	Fprintf(sdp, "No data buffer, so no data to decode!\n");
	return(FAILURE);
    }

    PrintHeader(sdp, "Inquiry Information");

    PrintHex(sdp, "Peripheral Device Type", inquiry->inq_dtype, DNL);
    Print(sdp, " (%s)\n", GetDeviceName(inquiry->inq_dtype));

    PrintHex(sdp, "Peripheral Qualifier", inquiry->inq_pqual, DNL);
    Print(sdp, " (%s)\n", GetPeripheralQualifier(inquiry, True));

    PrintHex(sdp, "Device Type Modifier", inquiry->inq_dtypmod, PNL);

    PrintYesNo(sdp, False, "Removable Media", inquiry->inq_rmb, PNL);

    PrintDecimal(sdp, "ANSI Version", inquiry->inq_ansi, DNL);
    Print(sdp, " (%s)\n", (inquiry->inq_ansi < ansi_entries)
	    ? ansi_table[inquiry->inq_ansi].fname
	    : reserved_str);

    PrintDecimal(sdp, "ECMA Version", inquiry->inq_ecma, PNL);

    PrintDecimal(sdp, "ISO Version", inquiry->inq_iso, PNL);

    PrintDecimal(sdp, "Response Data Format", inquiry->inq_rdf, DNL);
    Print(sdp, " (%s)\n", (inquiry->inq_rdf < rdf_entries)
	    ? rdf_table[inquiry->inq_rdf]
	    : reserved_str);

    if (inquiry->inq_rdf == RDF_SCSI2) {
	if (inquiry->inq_ansi >= ANSI_SCSI3) {
	    PrintYesNo(sdp, False, "Hierarchical Support", inquiry->inq_hisup, PNL);
	    PrintYesNo(sdp, False, "Normal ACA Support", inquiry->inq_normaca, PNL);
	}
	if ( (inquiry->inq_ansi < ANSI_SPC2) &&
	     (inquiry->inq_dtype == DTYPE_PROCESSOR) ) {
	    PrintYesNo(sdp, False, "Async Event Notification", inquiry->inq_aenc, PNL);
	}
    }

    PrintDecimal(sdp, "Additional Length", inquiry->inq_addlen, PNL);

    if (addl_len == 0) {
	if (inquiry->inq_reserved_6 || inquiry->inq_flags ||
	    isprint(*inquiry->inq_vid) ||
	    isprint(*inquiry->inq_pid) ||
	    isprint(*inquiry->inq_revlevel)) {
	    addl_len = STD_ADDL_LEN;
	} else {
	    return(status);
	}
    }

    if (inquiry->inq_ansi >= ANSI_SCSI3) {
	PrintYesNo(sdp, False, "Supports Protection Information", inquiry->inq_protect, PNL);
	if (inquiry->inq_res_5_b1_b2) {
	    PrintHex(sdp, "Reserved (byte 5, bits 1:2)", inquiry->inq_res_5_b1_b2, PNL);
	}
	PrintYesNo(sdp, False, "Third Party Copy Support", inquiry->inq_3pc, PNL);
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
	PrintYesNo(sdp, False, "Access Controls Coordinator", inquiry->inq_acc, PNL);
	PrintYesNo(sdp, False, "Storage Controller Components", inquiry->inq_sccs, PNL);
    }
    if (--addl_len == 0) return(status);

    /*
     * Inquiry byte 6 has new bits defined in SCSI-3.  Since devices
     * identify themselves as SCSI-2, only display bits that are set.
     */
    if ( (inquiry->inq_rdf == RDF_SCSI2) &&
	 (inquiry->inq_ansi >= ANSI_SCSI2) ) {
	if (inquiry->inq_mchngr || (inquiry->inq_ansi >= ANSI_SCSI3)) {
	    PrintYesNo(sdp, False, "Medium Changer Support", inquiry->inq_mchngr, PNL);
	}
	if (inquiry->inq_multip || (inquiry->inq_ansi >= ANSI_SCSI3)) {
	    PrintYesNo(sdp, False, "Multi-port Device Support", inquiry->inq_multip, PNL);
	}
	if (inquiry->inq_vs_6_b5) {
	    PrintYesNo(sdp, False, "Vendor Specific (byte 6, bit 5)", inquiry->inq_vs_6_b5, PNL);
	}
	if (inquiry->inq_encserv || (inquiry->inq_ansi >= ANSI_SCSI3)) {
	    PrintYesNo(sdp, False, "Embedded Enclosure Services", inquiry->inq_encserv, PNL);
	}
	if (inquiry->inq_bque || (inquiry->inq_ansi >= ANSI_SCSI3)) {
	    PrintYesNo(sdp, False, "Basic Command Queuing", inquiry->inq_bque, PNL);
	}
    } else {
	if (inquiry->inq_reserved_6) {
	    PrintHex(sdp, "Reserved (byte 6)", inquiry->inq_reserved_6, PNL);
	}
    }
    if (--addl_len == 0) return(status);

    /*
     * Although SCSI-1 declared the flags field as reserved, many
     * vendors set these bits, so we'll display them if non-zero.
     */
    if ( (inquiry->inq_rdf == RDF_SCSI2) || (inquiry->inq_flags) ) {
	if (inquiry->inq_vs_7_b0) {
	    PrintYesNo(sdp, False, "Vendor Specific (byte 7, bit 0)", inquiry->inq_vs_7_b0, PNL);
	}
	PrintYesNo(sdp, False, "Command Queuing Support", inquiry->inq_cmdque, PNL);
	PrintYesNo(sdp, False, "Linked Command Support", inquiry->inq_linked, PNL);
	PrintYesNo(sdp, False, "Synchronous Data Transfers", inquiry->inq_sync, PNL);
	PrintYesNo(sdp, False, "Support for 16 Bit Transfers", inquiry->inq_wbus16, PNL);
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

    PrintAscii(sdp, "Vendor Specific Data", "", DNL);
    PrintHAFields(sdp, inquiry->inq_vendor_unique, addl_len);
    Printf(sdp, "\n");
    return(status);
}

char *
GetDeviceName(uint8_t device_type)
{
    static struct {
	char *fname;                /* Full device type name.       */
	uint8_t dtype;              /* The Inquiry device type.     */
    } dnames[] = {
	{ "Direct Access",          DTYPE_DIRECT        },
	{ "Sequential Access",      DTYPE_SEQUENTIAL    },
	{ "Printer",                DTYPE_PRINTER       },
	{ "Processor",              DTYPE_PROCESSOR     },
	{ "Write-Once/Read-Many",   DTYPE_WORM          },
	{ "Read-Only Direct Access",DTYPE_RODIRECT      },
	{ "Scanner",                DTYPE_SCANNER       },
	{ "Optical Memory",         DTYPE_OPTICAL       },
	{ "Medium Changer",         DTYPE_CHANGER       },
	{ "Communications",         DTYPE_COMM          },
	{ "Graphics Pre-press",     DTYPE_PREPRESS_0    },
	{ "Graphics Pre-press",     DTYPE_PREPRESS_1    },
	{ "Array Controller",       DTYPE_RAID          },
	{ "Enclosure Service",      DTYPE_ENCLOSURE     },
	{ "Utility Device",         DTYPE_UTILITY       },
	{ "not present",            DTYPE_NOTPRESENT    }
    };
    static int dtype_entries = sizeof(dnames) / sizeof(dnames[0]);
    int i;

    for (i = 0; i < dtype_entries; i++) {
	if (dnames[i].dtype == device_type) {
	    return (dnames[i].fname);
	}
    }
    return ("Unknown");
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
