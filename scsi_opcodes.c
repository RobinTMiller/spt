/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2016			    *
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
 * File:	scsi_opcodes.c
 * Author:	Robin T. Miller
 * Date:	February 7th, 2012
 *
 * Descriptions:
 *	Functions and tables to decode SCSI opcode data.
 *
 * Modification History:
 * 
 * October 3rd, 2014 by Robin T. Miller
 * 	Modified SCSI generic opaque pointer to tool_specific information,
 * so we can pass more tool specific information (e.g. dt versus spt).
 * 
 * June 15th, 2014 by Robin T. Miller
 * 	When setting up initial I/O parameters, if data verification has been
 * requested, ensure the pattern buffer has been allocated and initialized,
 * otherwise data validation is skipped! The mainline only allocated this
 * pattern buffer if the direction is read and we have a data length, but
 * now that assumption is no longer true, since folks wanted the direction
 * and length to be setup automatically by encode functions. (sigh)
 * 
 * November 2nd, 2013 by Robin T. Miller
 * 	Removed bypass flag check in initialize_io_parameters() to ensure
 * the data direction and length get setup for normal read/write commands.
 * Previously, bypass allowed improper setting to allow negative testing.
 * 
 * February 7th, 2012 by Robin T. Miller
 * 	Move SCSI operation code table here from scsi_data.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spt.h"
#include "libscsi.h"
#include "inquiry.h"

#include "scsi_cdbs.h"

#include "spt_mtrand64.h"

/*
 * Forward References:
 */ 
uint64_t find_max_capacity(scsi_device_t *sdp);
int initialize_devices(scsi_device_t *sdp);
int initialize_slices(scsi_device_t *sdp);
void initialize_slice(scsi_device_t *sdp, scsi_device_t *tsdp);
int do_sanity_check_src_dst_devices(scsi_device_t *sdp, io_params_t *siop, io_params_t *iop);
#if _BIG_ENDIAN_
void init_swapped(	scsi_device_t	*sdp,
			void		*buffer,
			size_t		count,
			uint32_t	pattern );
#endif /* _BIG_ENDIAN_ */
int GetCapacity(scsi_device_t *sdp, io_params_t *iop);
char *get_data_direction(enum scsi_data_dir data_dir);
void initialize_io_limits(scsi_device_t *sdp, io_params_t *iop, uint64_t data_blocks);
int initialize_io_parameters(scsi_device_t *sdp, io_params_t *iop, uint64_t max_lba, uint64_t max_blocks);
int process_end_of_data(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp);

/*
 * Encode/Decode Functions:
 */ 
int random_rw6_encode(void *arg);
int random_rw10_encode(void *arg);
int random_rw16_encode(void *arg);
int random_caw16_encode(void *arg);
int extended_copy_encode(void *arg);
int receive_rod_token_decode(void *arg);
int write_using_token_encode(void *arg);
int write_using_token_encode_compat(void *arg);
int random_unmap_encode(void *arg);
int read_capacity16_encode(void *arg);
int read_capacity16_decode(void *arg);
int get_lba_status_encode(void *arg);
int get_lba_status_decode(void *arg);
int report_luns_encode(void *arg);
int report_luns_decode(void *arg);
int rtpgs_encode(void *arg);
int rtpgs_decode(void *arg);

/* Support Functions: */
int wut_extended_copy_verify_data(scsi_device_t *sdp);
void check_thin_provisioning(scsi_device_t *sdp, scsi_generic_t *sgp, io_params_t *iop);
int get_block_provisioning(scsi_device_t *sdp, scsi_generic_t *sgp, io_params_t *iop);
int get_unmap_block_limits(scsi_device_t *sdp, scsi_generic_t *sgp, io_params_t *iop, uint64_t max_blocks);
int random_rw_complete_io(scsi_device_t *sdp, uint64_t max_lba, uint64_t max_blocks);
int random_rw_process_cdb(scsi_device_t *sdp, io_params_t *iop, uint64_t max_lba, uint64_t max_blocks);
int random_rw_process_data(scsi_device_t *sdp);
int random_rw_ReadVerifyData(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
			     uint64_t lba, uint32_t bytes);
void restore_saved_parameters(scsi_device_t *sdp);
int scsiReadData(io_params_t *iop, scsi_io_type_t read_type, scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes);
int scsiWriteData(io_params_t *iop, scsi_io_type_t write_type, scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes);

/* See utilities.c */
extern char *expected_str;
extern char *received_str;
int extended_copy_verify_buffers(scsi_device_t	*sdp,
				 scsi_generic_t	*sgp,
				 scsi_generic_t	*ssgp,
				 uint32_t	blocks,
				 uint64_t	src_starting_lba,
				 uint64_t	dst_starting_lba,
				 unsigned char	*dbuffer,
				 unsigned char	*vbuffer,
				 size_t		count);

/*
 * I/O Support Functions:
 */ 
uint64_t
find_max_capacity(scsi_device_t *sdp)
{
    uint64_t max_device_capacity = 0;
    int device_index;

    /*
     * For now, setup capacity, later maybe do Inquiry too.
     */ 
    for (device_index = 0; (device_index < sdp->io_devices); device_index++) {
	io_params_t *iop = &sdp->io_params[device_index];

	if (iop->device_capacity < max_device_capacity) {
	    max_device_capacity = iop->device_capacity;
	}
    }
    return (max_device_capacity);
}

int
initialize_devices(scsi_device_t *sdp)
{
    int device_index;
    int status = SUCCESS;

    /*
     * For now, setup capacity, later maybe do Inquiry too.
     */ 
    for (device_index = 0; (device_index < sdp->io_devices); device_index++) {
	io_params_t *iop = &sdp->io_params[device_index];

	if ( !iop->device_size || !iop->device_capacity) {
	    status = GetCapacity(sdp, iop);
	    if (status != SUCCESS) break;
	}
    }
    return (status);
}

int
initialize_multiple_devices(scsi_device_t *sdp)
{
    io_params_t *iop = &sdp->io_params[IO_INDEX_DSF];
    scsi_generic_t *sgp = &iop->sg;
    io_params_t *miop = &sdp->io_params[IO_INDEX_DSF1];
    scsi_generic_t *msgp = &miop->sg;
    int status = SUCCESS;

    /* First time setup. */
    if (msgp->data_length == 0) {
	switch (sdp->iomode) {
	    case IOMODE_COPY:
		msgp->cdb[0] = (uint8_t)sdp->scsi_write_type;
		msgp->data_dir = scsi_data_write;
		break;
	    case IOMODE_MIRROR:
	    case IOMODE_VERIFY:
		msgp->cdb[0] = (uint8_t)sdp->scsi_read_type;
		msgp->data_dir = scsi_data_read;
		break;
	    default:
		ReportDeviceInformation(sdp, msgp);
		Fprintf(sdp, "Invalid I/O mode detected, mode %d!\n", sdp->iomode);
		return(FAILURE);
	}
	msgp->cdb_size = GetCdbLength(msgp->cdb[0]);
	miop->sop = ScsiOpcodeEntry(msgp->cdb, miop->device_type);
	if (miop->sop == NULL) {
	    ReportDeviceInformation(sdp, msgp);
	    Fprintf(sdp, "SCSI opcode lookup failed, opcode = 0x%02x\n", msgp->cdb[0]);
	    return(FAILURE);
	}
	msgp->data_length = sgp->data_length;
	/* Invoked during main() processihng, buffer allocated there! */
	//msgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
	//if (msgp->data_buffer == NULL) return(FAILURE);
	
	status = initialize_devices(sdp);
	if (status != SUCCESS) return(status);
	if ( (sdp->bypass == False) && (sdp->iomode == IOMODE_MIRROR) ) {
	    /* We expect the source and mirror devices to be exactly the same! */
	    if ( (iop->device_size == miop->device_size) &&
		 (iop->device_capacity != miop->device_capacity) ) {
		ReportErrorInformation(sdp);
		Fprintf(sdp, "The device capacity is different between the selected devices!\n");
		Fprintf(sdp, "  Base Device: %s, Capacity: " LUF " blocks\n", sgp->dsf, iop->device_capacity);
		Fprintf(sdp, "Mirror Device: %s, Capacity: " LUF " blocks\n", msgp->dsf, miop->device_capacity);
		return(FAILURE);
	    }
	} else if (iop->device_capacity != miop->device_capacity) {
	    /* Common processing for copy/verify operations. */
	    status = do_sanity_check_src_dst_devices(sdp, iop, miop);
	}
    }
    return (status);
}

int
do_sanity_check_src_dst_devices(scsi_device_t *sdp, io_params_t *siop, io_params_t *iop)
{
    scsi_generic_t *sgp = &iop->sg;		/* destination device */
    scsi_generic_t *ssgp = &siop->sg;		/* source device */
    int status = SUCCESS;

    if (sdp->bypass == True) return(status);
    if ( ANY_DATA_LIMITS(siop) || ANY_DATA_LIMITS(iop) ) return(status);
    if ( (sdp->image_copy == True) &&
	 ((siop->device_size == iop->device_size) &&
	  (siop->device_capacity > iop->device_capacity)) ) {
	ReportErrorInformation(sdp);
	Fprintf(sdp, "The source device capacity is larger than the destination device!\n");
	Fprintf(sdp, "     Source Device: %s, Capacity: " LUF " blocks\n", ssgp->dsf, siop->device_capacity);
	Fprintf(sdp, "Destination Device: %s, Capacity: " LUF " blocks\n", sgp->dsf, iop->device_capacity);
	return(FAILURE);
    }
    if ( (siop->device_size == iop->device_size) &&
	 (iop->device_capacity != siop->device_capacity) ) {
	Printf(sdp, "WARNING: The device capacity is different between the selected devices!\n");
	Printf(sdp, "     Source Device: %s, Capacity: " LUF " blocks\n", ssgp->dsf, siop->device_capacity);
	Printf(sdp, "Destination Device: %s, Capacity: " LUF " blocks\n", sgp->dsf, iop->device_capacity);
	if (sdp->slices) {
	    Printf(sdp, "Setting both devices to the smallest capacity to ensure the same block ranges!\n");
	    iop->device_capacity = min(iop->device_capacity, siop->device_capacity);
	    siop->device_capacity = min(iop->device_capacity, siop->device_capacity);
	}
    }
    return (status);
}
    
int
sanity_check_src_dst_devices(scsi_device_t *sdp)
{
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    io_params_t *siop = &sdp->io_params[IO_INDEX_SRC];
    int status;

    status = initialize_devices(sdp);
    if (status != SUCCESS) return(status);
    status = do_sanity_check_src_dst_devices(sdp, siop, iop);
    return (status);
}

int
initialize_slices(scsi_device_t *sdp)
{
    io_params_t *iop;
    scsi_generic_t *sgp;
    int device_index;
    int status;

    status = initialize_devices(sdp);
    if (status != SUCCESS) return(status);

    for (device_index = 0; (device_index < sdp->io_devices); device_index++) {
	iop = &sdp->io_params[device_index];
	sgp = &iop->sg;
	
	/* Verify CDB's do *not* transfer any data! */
	if ( (sgp->data_dir != scsi_data_none) && (sgp->data_length == 0) ) {
	    ReportErrorInformation(sdp);
	    Fprintf(sdp, "Please specify a data length for this CDB!\n");
	    return (FAILURE);
	}

	initialize_io_limits(sdp, iop, 0);
	if (iop->block_limit < sdp->slices) {
	    ReportErrorInformation(sdp);
	    Fprintf(sdp, "The block limit (" LUF ") is less than the number of slices (%u)!\n",
		    iop->block_limit, sdp->slices);
	    return (FAILURE);
	}
	iop->slice_lba = iop->starting_lba;
	sdp->slice_number = 0;
	iop->slice_length = (iop->block_limit / sdp->slices);
	iop->slice_resid = (iop->block_limit - (iop->slice_length * sdp->slices));
    }
    return (SUCCESS);
}

void
initialize_slice(scsi_device_t *sdp, scsi_device_t *tsdp)
{
    io_params_t *iop, *tiop;
    scsi_generic_t *tsgp;
    int device_index;

    sdp->slice_number++;
    
    for (device_index = 0; (device_index < sdp->io_devices); device_index++) {

	iop = &sdp->io_params[device_index];
	tiop = &tsdp->io_params[device_index];
        tsgp = &iop->sg;

	tiop->starting_lba = iop->slice_lba;
	tiop->ending_lba = (tiop->starting_lba + iop->slice_length);
	iop->slice_lba += iop->slice_length;
	tiop->data_limit = 0;
	if (sdp->slice_number == sdp->slices) {
	    tiop->ending_lba += iop->slice_resid;
	}
	tiop->block_limit = (tiop->ending_lba - tiop->starting_lba);
	/* Don't modify length for Write Same and Verify CDB's! */
	if ( !iop->cdb_blocks &&
	     (tiop->block_limit * tiop->device_size) < tsgp->data_length) {
	    tsgp->data_length = (uint32_t)(tiop->block_limit * tiop->device_size);
	}
    }
    return;
}

#if _BIG_ENDIAN_
/*
 * init_swapped() - Initialize Buffer with a Swapped Data Pattern.
 *
 * Inputs:
 * 	sdp = The SCSI device pointer.
 *	buffer = Pointer to buffer to init.
 *	count = Number of bytes to initialize.
 *	pattern = Data pattern to init buffer with.
 *
 * Return Value:
 *	Void
 */
void
init_swapped(	scsi_device_t	*sdp,
		void		*buffer,
		size_t		count,
		uint32_t	pattern )
{
    register uchar_t *bptr = buffer;
    union {
        uchar_t pat[sizeof(uint32_t)];
        uint32_t pattern;
    } p;
    register size_t i;

    /*
     * Initialize the buffer with a data pattern.
     */
    p.pattern = pattern;
    for (i = count; i ; ) {
        *bptr++ = p.pat[--i & (sizeof(uint32_t) - 1)];
    }
    return;
}
#endif /* _BIG_ENDIAN_ */

int
GetCapacity(scsi_device_t *sdp, io_params_t *iop)
{
    scsi_generic_t *sgp = &iop->sg;
    ReadCapacity16_data_t ReadCapacity16_data;
    ReadCapacity16_data_t *rcdp = &ReadCapacity16_data;
    int status;

    iop->lbpme_flag = False;
    iop->lbprz_flag = False;
    iop->lbpmgmt_valid = False;
    /*
     * 16byte CDB may fail on some disks, but 10-byte should succeed!
     */
    status = ReadCapacity16(sgp->fd, sgp->dsf, sgp->debug, False,
                            NULL, NULL, rcdp, sizeof(*rcdp), 0,
                            sgp->timeout, sgp->tsp);
    if (status == SUCCESS) {
        uint32_t device_size = (uint32_t)StoH(rcdp->block_length);
        uint64_t device_capacity = StoH(rcdp->last_block);
        if (device_size) {
            iop->device_size = device_size;
        }
        if (device_capacity) {
            iop->device_capacity = (device_capacity + 1);
        }
        iop->lbpmgmt_valid = True;
        if (rcdp->lbpme) iop->lbpme_flag = True;
        if (rcdp->lbprz) iop->lbprz_flag = True;
    } else {
        ReadCapacity10_data_t ReadCapacity10_data;
        ReadCapacity10_data_t *rcdp = &ReadCapacity10_data;
        status = ReadCapacity10(sgp->fd, sgp->dsf, sgp->debug, TRUE,
                                NULL, NULL, rcdp, sizeof(*rcdp), 0,
                                sgp->timeout, sgp->tsp);
        if (status == SUCCESS) {
            uint32_t device_size = (uint32_t)StoH(rcdp->block_length);
            uint32_t device_capacity = (uint32_t)StoH(rcdp->last_block);
            if (device_size) {
                iop->device_size = device_size;
            }
            if (device_capacity) {
                iop->device_capacity = (uint64_t)(device_capacity + 1);
            }
        }
    }
    if ( (status == SUCCESS) && sdp->DebugFlag) {
        Printf(sdp, "Device: %s, Device Size: %u bytes, Capacity: " LUF " blocks (thread %d)\n",
                sgp->dsf, iop->device_size, iop->device_capacity, sdp->thread_number);
    }
    return (status);
}

int
read_capacity16_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    ReadCapacity16_CDB_t *cdb;
    ReadCapacity16_data_t *rcdp;

    cdb = (ReadCapacity16_CDB_t *)sgp->cdb;
    sgp->data_dir = scsi_data_read;
    if ( (sgp->data_buffer == NULL) || (sgp->data_length < sizeof(*rcdp)) ) {
        if (sgp->data_buffer) free_palign(sdp, sgp->data_buffer);
        sgp->data_length = sizeof(*rcdp);
        sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
        if (sgp->data_buffer == NULL) return(FAILURE);
    }
    HtoS(cdb->allocation_length, sgp->data_length);
    return(SUCCESS);
}

int
read_capacity16_decode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    ReadCapacity16_data_t *rcdp;
    uint32_t block_length;
    uint64_t logical_blocks;
    uint16_t lowest_aligned;
    int status = SUCCESS;

    if ( (rcdp = (ReadCapacity16_data_t *)sgp->data_buffer) == NULL) {
        ReportDeviceInformation(sdp, sgp);
        Fprintf(sdp, "No capacity buffer provided!\n");
        return(FAILURE);
    }

    Printf(sdp, "\n");
    Printf(sdp, "Read Capacity(16) Data: (data length %u bytes)\n", sgp->data_length);
    Printf(sdp, "\n");

    logical_blocks = StoH(rcdp->last_block) + 1;
    block_length = (uint32_t)StoH(rcdp->block_length);

    PrintLongDec(sdp, "Maximum Capacity", logical_blocks, (block_length) ? DNL : PNL);
    if (block_length) {
        double bytes;
        bytes = ((double) logical_blocks * (double) block_length);
        Print(sdp, " (%.3f megabytes)\n", (bytes / (double) MBYTE_SIZE));
    }
    PrintDecimal(sdp, "Block Length", (uint32_t)block_length, PNL);
    PrintYesNo(sdp, False, "Protection Enabled", rcdp->prot_en, PNL);
    PrintDecimal(sdp, "Protection Type", rcdp->p_type, PNL);
    PrintDecimal(sdp, "Logical Blocks per Physical Exponent", rcdp->lbppbe, DNL);
    Print(sdp, " (%u blocks per physical)\n", (1 << rcdp->lbppbe));
    PrintDecimal(sdp, "Protection Information Exponent", rcdp->p_i_exponent, PNL);
    PrintYesNo(sdp, False, "Logical Block Provisioning Management", rcdp->lbpme, DNL);
    Print(sdp, " (%s Provisioned)\n", (rcdp->lbpme) ? "Thin" : "Full");
    PrintYesNo(sdp, False, "Logical Block Provisioning Read Zeroes", rcdp->lbprz, DNL);
    Print(sdp, "%s\n", (rcdp->lbprz) ? " (unmapped blocks read as zero)" : "");
    lowest_aligned = ((rcdp->lowest_aligned_msb << 8) | rcdp->lowest_aligned_lsb);
    PrintDecimal(sdp, "Lowest Aligned Logical Block Address", lowest_aligned, PNL);
    return(status);
}

char *
get_data_direction(enum scsi_data_dir data_dir)
{
    switch (data_dir) {
	case scsi_data_none:
	    return ("none");
	case scsi_data_read:
	    return ("read");
	case scsi_data_write:
	    return ("write");
	default:
	    return ("unknown");
    }
}

void
initialize_io_limits(scsi_device_t *sdp, io_params_t *iop, uint64_t data_blocks)
{
    scsi_generic_t *sgp = &iop->sg;

    if (data_blocks == 0) {
	/* Write Same and Verify CDB's don't set blocks by length. */
	if (iop->cdb_blocks) {
	    data_blocks = iop->cdb_blocks;
	} else if (iop->sop && iop->sop->default_blocks) {
	    iop->cdb_blocks = data_blocks = iop->sop->default_blocks;
	} else {
	    data_blocks = (sgp->data_length / iop->device_size);
	}
    }
    iop->current_lba = iop->starting_lba;
    if (iop->data_limit) {
	uint64_t blocks, total_blocks;
	total_blocks = (iop->device_capacity - iop->starting_lba);
	blocks = howmany(iop->data_limit, iop->device_size);
	if (blocks < total_blocks) {
	    total_blocks = blocks;
	}
	iop->ending_lba = (iop->starting_lba + total_blocks);
    } else if (iop->ending_lba == 0) {
	/* Note: Block limit is already setup for slices! */
	if (iop->block_limit) {
	    iop->ending_lba = (iop->starting_lba + iop->block_limit);
	} else {
	    iop->ending_lba = iop->device_capacity;
	}
    }
    iop->block_count = 0;
    iop->block_limit = (iop->ending_lba - iop->starting_lba);
    if (iop->block_limit < data_blocks) {
	/* Set to proper data length. */
	if (iop->cdb_blocks) {
	    iop->cdb_blocks = iop->block_limit;
	} else {
	    sgp->data_length = (uint32_t)(iop->block_limit * iop->device_size);
	}
    }
    if (sdp->DebugFlag) {
	Printf(sdp, "Device: %s, Starting lba=" LUF ", ending lba=" LUF ", block limit=" LUF " (thread %d)\n",
	       sgp->dsf, iop->starting_lba, (iop->ending_lba - 1), iop->block_limit, sdp->thread_number);
    }
    return;
}

int
process_end_of_data(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp)
{
    iop->cdb_blocks   = iop->saved_cdb_blocks;
    sgp->data_length  = iop->saved_data_length;
    iop->block_limit  = iop->saved_block_limit;
    iop->starting_lba = iop->saved_starting_lba;
    iop->ending_lba   = iop->saved_ending_lba;
    iop->list_identifier = iop->saved_list_identifier;
    iop->end_of_data  = True;
    return (END_OF_DATA);
}

int
initialize_io_parameters(
    scsi_device_t *sdp, io_params_t *iop, uint64_t max_lba, uint64_t max_blocks)
{
    scsi_generic_t *sgp = &iop->sg;
    uint64_t data_blocks;
    int status = SUCCESS;

    if ( !iop->device_size || !iop->device_capacity) {
	status = GetCapacity(sdp, iop);
	if (status != SUCCESS) return(status);
    }
    if (iop->first_time == True) {
	iop->first_time = False;
	iop->end_of_data = False;
	/* Sanity Checks */
	if (sgp->data_dir != iop->sop->data_dir) {
	    if (sdp->DebugFlag && sdp->verbose) {
		Printf(sdp, "%s: Wrong data direction specified: current=%s, correct=%s, fixing...\n",
		       sgp->dsf,
		       get_data_direction(sgp->data_dir),
		       get_data_direction(iop->sop->data_dir));
	    }
	    sgp->data_dir = iop->sop->data_dir;
	}
	/* Verify CDB's do *not* transfer any data! */
	if ( (sgp->data_dir != scsi_data_none) && (sgp->data_length == 0) ) {
	    uint32_t data_length = iop->device_size;
	    /* We'll make the data length as accurate as possible. */
	    /* Note: For non-read/write opcodes, this could be wrong! */
	    /* Also Note: We need to extend the opcode table w/callout! */
	    if ( (iop->sop->opcode != SOPC_WRITE_SAME) &&
		 (iop->sop->opcode != SOPC_WRITE_SAME_16) ) {
		if (iop->cdb_blocks) {
		    data_length = (uint32_t)(iop->cdb_blocks * iop->device_size);
		}
	    }
	    if (sdp->DebugFlag && sdp->verbose) {
		Printf(sdp, "%s: The data length was omitted (%u), so setting length to %u bytes!\n",
		       sgp->dsf, sgp->data_length, data_length);
	    }
	    iop->data_length = data_length;
	    sgp->data_length = data_length;
	    if (sgp->data_buffer) free_palign(sdp, sgp->data_buffer);
	    sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
	    if (sgp->data_buffer == NULL) return(FAILURE);
	}
	/* Write Same, Xcopy, and Verify CDB's don't set blocks by length. */
	if (iop->cdb_blocks) {
	    data_blocks = iop->cdb_blocks;
	} else if (iop->sop && iop->sop->default_blocks) {
	    iop->cdb_blocks = data_blocks = iop->sop->default_blocks;
	} else if (sgp->data_dir == scsi_data_none) {
	    iop->cdb_blocks = data_blocks = max_blocks;
	} else {
	    data_blocks = (sgp->data_length / iop->device_size);
	}
	/*
	 * Handle commands scaling max blocks by range or segments.
	 */
	if (iop->scale_count) {
	    if (iop->saved_cdb_blocks) {
		/* Restore saved. */
		iop->cdb_blocks = iop->saved_cdb_blocks;
		data_blocks = max_blocks = iop->cdb_blocks;
	    } else {
		/* Scale per segments. */
		iop->cdb_blocks *= iop->scale_count;
		data_blocks = iop->cdb_blocks;
		max_blocks *= iop->scale_count;
	    }
	}
	iop->saved_cdb_blocks = iop->cdb_blocks;
	iop->saved_data_length = sgp->data_length;
	/* Range Checks */
	if (sgp->data_dir != scsi_data_none) {
	    if ( (iop->disable_length_check == False) &&
		 (sgp->data_length % iop->device_size) ) {
		ReportDeviceInformation(sdp, sgp);
		Fprintf(sdp, "The data length (%u) is NOT modulo the device size (%u)!\n", 
		       sgp->data_length, iop->device_size);
		return (FAILURE);
	    } else if (iop->data_limit && (iop->data_limit % iop->device_size)) {
		ReportDeviceInformation(sdp, sgp);
		Fprintf(sdp, "The data limit (" LUF ") is NOT modulo the device size (%u)!\n", 
		       iop->data_limit, iop->device_size);
		return (FAILURE);
	    } else if (iop->step_value && (iop->step_value % iop->device_size)) {
		ReportDeviceInformation(sdp, sgp);
		Fprintf(sdp, "The step value (" LUF ") is NOT modulo the device size (%u)!\n", 
		       iop->step_value, iop->device_size);
		return (FAILURE);
	    }
	}
	if (iop->starting_lba > max_lba) {
	    ReportDeviceInformation(sdp, sgp);
	    Fprintf(sdp, "The starting lba (" LUF ") is greater than max (" LUF ") supported by this CDB!\n",
		    iop->starting_lba, max_lba);
	    return (FAILURE);
	} else if (iop->starting_lba > iop->device_capacity) {
	    ReportDeviceInformation(sdp, sgp);
	    Fprintf(sdp, "The starting lba (" LUF ") is greater than the device capacity (" LUF ")!\n",
		    iop->starting_lba, iop->device_capacity);
	    return (FAILURE);
	} else if (iop->ending_lba && (iop->ending_lba < iop->starting_lba)) {
	    ReportDeviceInformation(sdp, sgp);
	    Fprintf(sdp, "The ending lba (" LUF ") is less than the starting lba (" LUF ")!\n",
		    iop->ending_lba, iop->starting_lba);
	    return (FAILURE);
	} else if (iop->ending_lba && (iop->ending_lba > iop->device_capacity)) {
	    ReportDeviceInformation(sdp, sgp);
	    Fprintf(sdp, "The ending lba (" LUF ") is greater than the device capacity (" LUF ")!\n",
		    iop->ending_lba, iop->device_capacity);
	    return (FAILURE);
	} else if ( (sdp->bypass == False) && (data_blocks > max_blocks) ) {
	    ReportDeviceInformation(sdp, sgp);
	    Fprintf(sdp, "The number of blocks (" LUF ") is greater than max (" LUF ") supported by this CDB!\n",
		    data_blocks, max_blocks);
	    return (FAILURE);
	}
	initialize_io_limits(sdp, iop, data_blocks);
	/* These must be restored when looping. */
	iop->saved_block_limit = iop->block_limit;
	iop->saved_starting_lba = iop->starting_lba;
	iop->saved_ending_lba = iop->ending_lba;
	iop->saved_list_identifier = iop->list_identifier;
	/*
	 * Note: If data verification requested, and we have *not* allocated the
	 * pattern buffer, do so now. Mainline only sets up if direction and/or
	 * data length was specified by the user, but now that has changed!
	 * FYI: Without this pattern buffer, data verification does NOT happen!
	*/
	if ( (iop->sop->data_dir == scsi_data_read) &&
	     (sgp->data_length && (sdp->pattern_buffer == NULL)) &&
	     ((sdp->compare_data == True) || (sdp->user_pattern == True)) ) {
	    sdp->pattern_buffer = malloc_palign(sdp, sgp->data_length, 0);
	    if (sdp->iot_pattern == False) {
		InitBuffer(sdp->pattern_buffer, (size_t)sgp->data_length, sdp->pattern);
	    }
	}
    } else {
	uint64_t blocks_transferred, step_blocks = 0;
	/* Adjust for current operation just completed. */
	if (iop->cdb_blocks) {
	    blocks_transferred = iop->cdb_blocks;
	    data_blocks = blocks_transferred;
	    if (sdp->io_multiple_sources &&
		(data_blocks < iop->saved_cdb_blocks) ) {
		/* Restore the original number of blocks. */
		data_blocks = iop->cdb_blocks = iop->saved_cdb_blocks;
	    }
	} else {
	    blocks_transferred = howmany(sgp->data_transferred, iop->device_size);
	    data_blocks = (sgp->data_length / iop->device_size);
	}
	iop->block_count += blocks_transferred;
	if (iop->block_count == iop->block_limit) {
	    status = process_end_of_data(sdp, iop, sgp);
	    return (status);
	}
	/* Note: This should NOT happen, if we do things right! */
	if (iop->block_count > iop->block_limit) {
	    if (sdp->xDebugFlag) {
		Printf(sdp, "WARNING: Transferred too many blocks: block count = " LUF ", block limit = " LUF "\n",
		       iop->block_count, iop->block_limit);
	    }
	    status = process_end_of_data(sdp, iop, sgp);
	    return (status);
	}
	iop->current_lba += blocks_transferred;
	/* Prepare for next operation, limiting as necessary. */
	if ( (iop->block_count + data_blocks) > iop->block_limit) {
	    /* Set to proper data length. */
	    data_blocks = (iop->block_limit - iop->block_count);
	    if (iop->cdb_blocks) {
		iop->cdb_blocks = data_blocks;
	    } else {
		sgp->data_length = (uint32_t)(data_blocks * iop->device_size);
	    }
	}
	if (iop->step_value) {
	    step_blocks = (iop->step_value / iop->device_size);
	    iop->current_lba += step_blocks;
	    if ( (iop->current_lba + data_blocks) > iop->ending_lba) {
		status = process_end_of_data(sdp, iop, sgp);
		return (status);
	    }
	}
    }
    if (sdp->iot_pattern) {
	if ( (iop->sop->data_dir == scsi_data_read) && (sdp->compare_data == True) && sdp->pattern_buffer) {
	    (void)init_iotdata(sdp, iop, sdp->pattern_buffer, sgp->data_length, (uint32_t)iop->current_lba, sdp->iot_seed);
	} else if (iop->sop->data_dir == scsi_data_write) {
	    (void)init_iotdata(sdp, iop, sgp->data_buffer, sgp->data_length, (uint32_t)iop->current_lba, sdp->iot_seed_per_pass);
	}
    }
    return (status);
}

int
random_rw6_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    DirectRW6_CDB_t *cdb = (DirectRW6_CDB_t *)sgp->cdb;
    uint64_t max_lba = SCSI_MAX_LBA, max_blocks = SCSI_MAX_BLOCKS;
    int status;

    status = random_rw_process_cdb(sdp, iop, max_lba, max_blocks);
    if (status != SUCCESS) return (status);

    HtoS(cdb->lba, iop->current_lba);
    if (iop->cdb_blocks) {
        cdb->length = (uint8_t)iop->cdb_blocks;
    } else {
        cdb->length = (uint8_t)(sgp->data_length / iop->device_size);
    }
    return (SUCCESS);
}

int
random_rw10_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_DSF];
    scsi_generic_t *sgp = &iop->sg;
    DirectRW10_CDB_t *cdb = (DirectRW10_CDB_t *)sgp->cdb;
    uint64_t max_lba = SCSI_MAX_LBA10, max_blocks = SCSI_MAX_BLOCKS10;
    int status;

    status = random_rw_process_cdb(sdp, iop, max_lba, max_blocks);
    if (status != SUCCESS) return (status);

    HtoS(cdb->lba, iop->current_lba);
    if (iop->cdb_blocks) {
        HtoS(cdb->length, iop->cdb_blocks);
    } else {
        HtoS(cdb->length, (sgp->data_length / iop->device_size));
    }
    return (SUCCESS);
}

int
random_rw16_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    DirectRW16_CDB_t *cdb = (DirectRW16_CDB_t *)sgp->cdb;
    uint64_t max_lba = SCSI_MAX_LBA16, max_blocks = SCSI_MAX_BLOCKS16;
    int status;

    status = random_rw_process_cdb(sdp, iop, max_lba, max_blocks);
    if (status != SUCCESS) return (status);

    HtoS(cdb->lba, iop->current_lba);

    if (iop->cdb_blocks) {
        HtoS(cdb->length, iop->cdb_blocks);
    } else {
        HtoS(cdb->length, (sgp->data_length / iop->device_size));
    }
    return (SUCCESS);
}

int
random_rw_complete_io(scsi_device_t *sdp, uint64_t max_lba, uint64_t max_blocks)
{
    io_params_t *iop;
    int device_index;
    int status = SUCCESS;

    for (device_index = 0; (device_index < sdp->io_devices); device_index++) {
	int device_status;
	iop = &sdp->io_params[device_index];
	device_status = initialize_io_parameters(sdp, iop, max_lba, max_blocks);
	if (device_status != SUCCESS) {
	    status = device_status;	/* End of data or failure, go no further! */
	    break;
	}
    }
    return (status);
}

int
random_rw_process_cdb(scsi_device_t *sdp, io_params_t *iop, uint64_t max_lba, uint64_t max_blocks)
{
    scsi_generic_t *sgp = &iop->sg;
    int status = SUCCESS;

    /*
     * If this is a write request, test mode, a single device, and read
     * after write is enabled, then read and verify the last data written.
     * Note: All read and write requests come through this code flow!
     */
    if ( (iop->first_time == False) && (sdp->iomode == IOMODE_TEST) &&
	 (sgp->data_dir == scsi_data_write) && (sdp->io_devices == 1) &&
	 (sdp->status == SUCCESS) && (sdp->read_after_write == True) ) {
	status = random_rw_ReadVerifyData(sdp, iop, sgp, iop->current_lba, sgp->data_transferred);
	if (status != SUCCESS) return(status);
    }

    if (iop->first_time) {
	status = initialize_io_parameters(sdp, iop, max_lba, max_blocks);
	if (status != SUCCESS) return (status);
	if ( (sdp->iomode != IOMODE_TEST) && (sdp->io_devices > 1) ) {
	    io_params_t *miop = &sdp->io_params[IO_INDEX_DSF1];
	    //status = initialize_multiple_devices(sdp);
	    //if (status != SUCCESS) return(status);
	    status = initialize_io_parameters(sdp, miop, max_lba, max_blocks);
	}
    } else {
	if ( (sdp->iomode != IOMODE_TEST) && (sdp->io_devices > 1) ) {
	    status = random_rw_process_data(sdp);
	    if (status != SUCCESS) return(status);
	    status = random_rw_complete_io(sdp, max_lba, max_blocks);
	} else {
	    status = initialize_io_parameters(sdp, iop, max_lba, max_blocks);
	}
    }
    if (status == END_OF_DATA) {
	restore_saved_parameters(sdp);
	iop->end_of_data = True;
    }
    return(status);
}
    
int
random_rw_ReadVerifyData(
    scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
    uint64_t lba, uint32_t bytes)
{
    uint8_t *verify_buffer = (uint8_t *)sgp->data_buffer;
    scsi_generic_t *rsgp = Malloc(sdp, sizeof(*sgp));
    uint32_t blocks = howmany(bytes, iop->device_size);
    int status = SUCCESS;

    if (rsgp == NULL) return(FAILURE);
    /*
     * Duplicate the SCSI generic, to keep sane (CDB, SCSI name, etc).
     */ 
    *rsgp = *sgp;
    rsgp->data_buffer = malloc_palign(sdp, bytes, 0);
    if (rsgp->data_buffer == NULL) {
	free(rsgp);
	return(FAILURE);
    }
    status = scsiReadData(iop, sdp->scsi_read_type, rsgp, lba, blocks, bytes);
    if ( (status == SUCCESS) && (sdp->compare_data == True) ) {
	status = VerifyBuffers(sdp, rsgp->data_buffer,
			       verify_buffer, rsgp->data_transferred);
	if (status == FAILURE) {
	    if (sdp->iot_pattern) {
		process_iot_data(sdp, iop, verify_buffer,
				 rsgp->data_buffer, rsgp->data_transferred);
	    }
	}
    }
    free_palign(sdp, rsgp->data_buffer);
    free(rsgp);
    return (status);
}

int
random_rw_process_data(scsi_device_t *sdp)
{
    io_params_t *iop = &sdp->io_params[IO_INDEX_DSF];
    scsi_generic_t *sgp = &iop->sg;
    io_params_t *miop = &sdp->io_params[IO_INDEX_DSF1];
    scsi_generic_t *msgp = &miop->sg;
    uint32_t blocks;
    size_t bytes;
    uint64_t src_starting_lba, dst_starting_lba;
    int status;
    
    bytes = sgp->data_transferred;
    blocks = howmany(sgp->data_transferred, iop->device_size);
    if ( (miop->block_count + blocks) > miop->block_limit ) {
	blocks = (uint32_t)(miop->block_limit - miop->block_count);
	bytes = (blocks * miop->device_size);
    }
    src_starting_lba = iop->current_lba;
    dst_starting_lba = miop->current_lba;
    if (sdp->xDebugFlag) {
	Printf(sdp, "Starting %s:\n", (sdp->iomode == IOMODE_COPY) ? "Copy" : "Verify");
	PrintDecHex(sdp, "Number of Blocks", blocks, PNL);
	PrintLongDecHex(sdp, "Source Block Device LBA", src_starting_lba, DNL);
	Print(sdp, " (lba's " LUF " - " LUF ")\n", src_starting_lba, (src_starting_lba + blocks - 1));
	PrintLongDecHex(sdp, "Destination Block Device LBA", dst_starting_lba, DNL);
	Print(sdp, " (lba's " LUF " - " LUF ")\n", dst_starting_lba, (dst_starting_lba + blocks - 1));
	Printf(sdp, "\n");
    }

    if (sdp->iomode == IOMODE_COPY) {
	void *saved_buffer = msgp->data_buffer;
	msgp->data_buffer = sgp->data_buffer;
	status = scsiWriteData(miop, sdp->scsi_write_type, msgp, dst_starting_lba, blocks, (uint32_t)bytes);
	msgp->data_buffer = saved_buffer;
	if ( (status == SUCCESS) && (sdp->compare_data) ) {
	    msgp->data_dir = scsi_data_read;
	    status = scsiReadData(miop, sdp->scsi_read_type, msgp, dst_starting_lba, blocks, (uint32_t)bytes);
	    if (status != SUCCESS) return(status);
	    status = extended_copy_verify_buffers(sdp, msgp, sgp,
						  blocks, src_starting_lba, dst_starting_lba,
						  msgp->data_buffer, sgp->data_buffer, (uint32_t)bytes);
	    msgp->data_dir = scsi_data_write;
	}
    } else { /* Mirror or Verify modes. */
	status = scsiReadData(miop, sdp->scsi_read_type, msgp, dst_starting_lba, blocks, (uint32_t)bytes);
	if (status != SUCCESS) return(status);
	status = extended_copy_verify_buffers(sdp, msgp, sgp,
					      blocks, src_starting_lba, dst_starting_lba,
					      msgp->data_buffer, sgp->data_buffer, bytes);
    }
    return(status);
}

int
scsiReadData(io_params_t *iop, scsi_io_type_t read_type, scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes)
{
    int status;
    
    status = ReadData(read_type, sgp, lba, blocks, bytes);
    if (status == SUCCESS) {
	iop->total_blocks += blocks;
	iop->total_transferred += bytes;
    }
    return(status);
}

int
scsiWriteData(io_params_t *iop, scsi_io_type_t write_type, scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes)
{
    int status;
	
    status = WriteData(write_type, sgp, lba, blocks, bytes);
    if (status == SUCCESS) {
	iop->total_blocks += blocks;
	iop->total_transferred += bytes;
    }
    return(status);
}

/*
 * Restore IO parameters for the next iteration (if any).
 */ 
void
restore_saved_parameters(scsi_device_t *sdp)
{
    io_params_t *iop;
    scsi_generic_t *sgp;
    int device_index;

    for (device_index = 0; (device_index < sdp->io_devices); device_index++) {
        iop = &sdp->io_params[device_index];
	sgp = &iop->sg;
	iop->first_time   = True;
	iop->cdb_blocks   = iop->saved_cdb_blocks;
	sgp->data_length  = iop->saved_data_length;
	iop->block_limit  = iop->saved_block_limit;
	iop->starting_lba = iop->saved_starting_lba;
	iop->ending_lba   = iop->saved_ending_lba;
	iop->list_identifier = iop->saved_list_identifier;
    }
    return;
}

int
extended_copy_verify_buffers(scsi_device_t  *sdp,
			     scsi_generic_t *sgp,
			     scsi_generic_t *ssgp,
			     uint32_t	   blocks,
			     uint64_t	   src_starting_lba,
			     uint64_t	   dst_starting_lba,
			     unsigned char  *dbuffer,
			     unsigned char  *vbuffer,
			     size_t	   count)
{
    register unsigned char *dptr = dbuffer;
    register unsigned char *vptr = vbuffer;
    register uint32_t i;

    if (memcmp(dptr, vptr, count) == 0) {
        return (SUCCESS);
    }
    for (i = 0; (i < count); i++, dptr++, vptr++) {
	if (*dptr != *vptr) {
            time_t error_time = time((time_t *) 0);
	    size_t dump_size = min(sdp->dump_limit, count);

	    ReportErrorMessage(sdp, sgp, "Data Compare Error");
	    PrintHeader(sdp, "Data Verification Failure Information");
	    PrintAscii(sdp, "Source Device", ssgp->dsf, PNL);
	    PrintAscii(sdp, "Destination Device", sgp->dsf, PNL);
	    PrintDecHex(sdp, "Number of Blocks", blocks, PNL);
	    PrintLongDecHex(sdp, "Source Block Device LBA", src_starting_lba, DNL);
	    Print(sdp, " (lba's " LUF " - " LUF ")\n", src_starting_lba, (src_starting_lba + blocks - 1));
	    PrintLongDecHex(sdp, "Destination Block Device LBA", dst_starting_lba, DNL);
	    Print(sdp, " (lba's " LUF " - " LUF ")\n", dst_starting_lba, (dst_starting_lba + blocks - 1));
	    Printf(sdp, "\n");

	    /* expected */
	    dump_buffer(sdp, expected_str, vbuffer, vptr, dump_size, count, True);
	    /* received */
	    dump_buffer(sdp, received_str, dbuffer, dptr, dump_size, count, False);
	    return (FAILURE);
	}
    }
    return (SUCCESS);
}

/* ======================================================================== */

/*
 * SCSI Operation Code Table.
 */
static scsi_opcode_t scsiOpcodeTable[] = {
    /*
     * SCSI Operation Codes for All Devices.
     */
    {   SOPC_CHANGE_DEFINITION,	    0x00, ALL_DEVICE_TYPES, "Change Definition"		},
    {	SOPC_COMPARE,		    0x00, ALL_DEVICE_TYPES, "Compare"			},
    {	SOPC_COPY,		    0x00, ALL_DEVICE_TYPES, "Copy"			},
    {	SOPC_COPY_VERIFY,	    0x00, ALL_DEVICE_TYPES, "Copy and Verify"		},
    {	SOPC_GET_CONFIGURATION,	    0x00, ALL_DEVICE_TYPES, "Get Configuration"		},
    {	SOPC_INQUIRY,		    0xFF, ALL_DEVICE_TYPES, "Inquiry",
	scsi_data_read, inquiry_encode, inquiry_decode					},
    {	SOPC_INQUIRY,		    0x00, ALL_DEVICE_TYPES, "Inquiry - Supported Pages"	},
    {	SOPC_INQUIRY,		    0x80, ALL_DEVICE_TYPES, "Inquiry - Serial Number"	 	},
    {	SOPC_INQUIRY,		    0x83, ALL_DEVICE_TYPES, "Inquiry - Device Identification"	},
    {	SOPC_INQUIRY,		    0x85, ALL_DEVICE_TYPES, "Inquiry - Management Network Addresses" },
    {	SOPC_INQUIRY,		    0x86, ALL_DEVICE_TYPES, "Inquiry - Extended Inquiry Data"	},
    {	SOPC_INQUIRY,		    0x87, ALL_DEVICE_TYPES, "Inquiry - Mode Page Policy"	},
    {	SOPC_INQUIRY,		    0x8F, ALL_DEVICE_TYPES, "Inquiry - Third-party Copy"	},
    {	SOPC_INQUIRY,		    0xB0, ALL_RANDOM_DEVICES, "Inquiry - Block Limits"		},
    {	SOPC_INQUIRY,		    0xB2, ALL_RANDOM_DEVICES, "Inquiry - Logical Block Provisioning" },
    {	SOPC_INQUIRY,		    0xC0, BITMASK(DTYPE_DIRECT), "Inquiry - Filer IP Address"	},
    {	SOPC_INQUIRY,		    0xC1, BITMASK(DTYPE_DIRECT), "Inquiry - Proxy Information"	},
    {	SOPC_INQUIRY,		    0xC2, BITMASK(DTYPE_DIRECT), "Inquiry - Target Port Information"},
    {	SOPC_LOG_SELECT,	    0x00, ALL_DEVICE_TYPES, "Log Select"		},
    {	SOPC_LOG_SENSE,		    0x00, ALL_DEVICE_TYPES, "Log Sense"			},
    {	SOPC_MODE_SELECT_6,	    0x00, ALL_DEVICE_TYPES, "Mode Select(6)"		},
    {	SOPC_MODE_SELECT_10,	    0x00, ALL_DEVICE_TYPES, "Mode Select(10)"		},
    {	SOPC_MODE_SENSE_6,	    0x00, ALL_DEVICE_TYPES, "Mode Sense(6)"		},
    {	SOPC_MODE_SENSE_10,	    0x00, ALL_DEVICE_TYPES, "Mode Sense(10)"		},
    {	SOPC_READ_BUFFER,	    0x00, ALL_DEVICE_TYPES, "Read Buffer"		},
    {	SOPC_RECEIVE_DIAGNOSTIC,    0x00, ALL_DEVICE_TYPES, "Receive Diagnostic"	},
    {	SOPC_REQUEST_SENSE,	    0x00, ALL_DEVICE_TYPES, "Request Sense"		},
    {	SOPC_SEND_DIAGNOSTIC,	    0x00, ALL_DEVICE_TYPES, "Send Diagnostic"		},
    {	SOPC_TEST_UNIT_READY,	    0x00, ALL_DEVICE_TYPES, "Test Unit Ready"		},
    {	SOPC_WRITE_BUFFER,	    0x00, ALL_DEVICE_TYPES, "Write Buffer"		},
    {	SOPC_PERSISTENT_RESERVE_IN, 0xFF, ALL_DEVICE_TYPES, "Persistent Reserve In"	},
    {	SOPC_PERSISTENT_RESERVE_IN, 0x00, ALL_DEVICE_TYPES, "Persistent Reserve In - Read Keys"	},
    {	SOPC_PERSISTENT_RESERVE_IN, 0x01, ALL_DEVICE_TYPES, "Persistent Reserve In - Read Reservations"	},
    {	SOPC_PERSISTENT_RESERVE_IN, 0x02, ALL_DEVICE_TYPES, "Persistent Reserve In - Report Capabilities" },
    {	SOPC_PERSISTENT_RESERVE_IN, 0x03, ALL_DEVICE_TYPES, "Persistent Reserve In - Read Full Status" },
    {	SOPC_PERSISTENT_RESERVE_OUT,0xFF, ALL_DEVICE_TYPES, "Persistent Reserve Out"	},
    {	SOPC_PERSISTENT_RESERVE_OUT,0x00, ALL_DEVICE_TYPES, "Persistent Reserve Out - Register" },
    {	SOPC_PERSISTENT_RESERVE_OUT,0x01, ALL_DEVICE_TYPES, "Persistent Reserve Out - Reserve" },
    {	SOPC_PERSISTENT_RESERVE_OUT,0x02, ALL_DEVICE_TYPES, "Persistent Reserve Out - Release" },
    {	SOPC_PERSISTENT_RESERVE_OUT,0x03, ALL_DEVICE_TYPES, "Persistent Reserve Out - Clear" },
    {	SOPC_PERSISTENT_RESERVE_OUT,0x04, ALL_DEVICE_TYPES, "Persistent Reserve Out - Preempt" },
    {	SOPC_PERSISTENT_RESERVE_OUT,0x05, ALL_DEVICE_TYPES, "Persistent Reserve Out - Preempt and Clear" },
    {	SOPC_PERSISTENT_RESERVE_OUT,0x06, ALL_DEVICE_TYPES, "Persistent Reserve Out - Register and Ignore" },
    {	SOPC_PERSISTENT_RESERVE_OUT,0x07, ALL_DEVICE_TYPES, "Persistent Reserve Out - Register and Move" },
    {   SOPC_REPORT_LUNS,           0x00, ALL_DEVICE_TYPES, "Report Luns"               },
    {	SOPC_MAINTENANCE_IN,	    0x00, ALL_DEVICE_TYPES, "Maintenance In"		},
    {	SOPC_MAINTENANCE_IN,	    0x05, ALL_DEVICE_TYPES, "Maintenance In - Report Device Identifier" },
    {	SOPC_MAINTENANCE_IN,	    0x06, ALL_DEVICE_TYPES, "Maintenance In - Report States" },
    {	SOPC_MAINTENANCE_IN,	    0x08, ALL_DEVICE_TYPES, "Maintenance In - Report Supported Configuration Method" },
    {	SOPC_MAINTENANCE_IN,	    0x09, ALL_DEVICE_TYPES, "Maintenance In - Report Unconfigured Capacity" },
    {   SOPC_MAINTENANCE_IN,        0x0A, ALL_DEVICE_TYPES, "Maintenance In - Report Target Port Groups" },
    {	SOPC_MAINTENANCE_IN,	    0x0C, ALL_DEVICE_TYPES, "Maintenance In - Report Supported Operation Codes" },
    {	SOPC_MAINTENANCE_IN,	    0x0D, ALL_DEVICE_TYPES, "Maintenance In - Report Supported Task Mgmt Functions" },
    /*
     * SCSI Operation Codes for Direct-Access Devices.
     */
    {	SOPC_FORMAT_UNIT,	    0x00, ALL_RANDOM_DEVICES, "Format Unit"		},
    {	SOPC_LOCK_UNLOCK_CACHE,	    0x00, ALL_RANDOM_DEVICES, "Lock/Unlock Cache"	},
    {	SOPC_PREFETCH,		    0x00, ALL_RANDOM_DEVICES, "Prefetch"		},
    {	SOPC_PREVENT_ALLOW_REMOVAL, 0x00, ALL_RANDOM_DEVICES, "Prevent/Allow Removal"	},
    {	SOPC_READ_6,		    0x00, ALL_RANDOM_DEVICES, "Read(6)",
	scsi_data_read,	random_rw6_encode, NULL						},
    {	SOPC_READ_10,		    0x00, ALL_RANDOM_DEVICES, "Read(10)",
	scsi_data_read, random_rw10_encode, NULL					},
    {	SOPC_READ_CAPACITY,	    0x00, ALL_RANDOM_DEVICES, "Read Capacity"		},
    {	SOPC_READ_DEFECT_DATA,	    0x00, ALL_RANDOM_DEVICES, "Read Defect Data"	},
    {	SOPC_READ_LONG,		    0x00, ALL_RANDOM_DEVICES, "Read Long"		},
    {	SOPC_REASSIGN_BLOCKS,	    0x00, ALL_RANDOM_DEVICES, "Reassign Blocks"		},
    {	SOPC_RELEASE,		    0x00, ALL_RANDOM_DEVICES, "Release"			},
    {	SOPC_RESERVE,		    0x00, ALL_RANDOM_DEVICES, "Reserve"			},
    {	SOPC_REZERO_UNIT,	    0x00, ALL_RANDOM_DEVICES, "Rezero Unit"		},
    {	SOPC_SEARCH_DATA_EQUAL,	    0x00, ALL_RANDOM_DEVICES, "Search Data Equal"	},
    {	SOPC_SEARCH_DATA_HIGH,	    0x00, ALL_RANDOM_DEVICES, "Search Data High"	},
    {	SOPC_SEARCH_DATA_LOW,	    0x00, ALL_RANDOM_DEVICES, "Search Data Low"		},
    {	SOPC_SEEK_6,		    0x00, ALL_RANDOM_DEVICES, "Seek(6)"			},
    {	SOPC_SEEK_10,		    0x00, ALL_RANDOM_DEVICES, "Seek(10)"		},
    {	SOPC_SET_LIMITS,	    0x00, ALL_RANDOM_DEVICES, "Set Limits"		},
    {	SOPC_START_STOP_UNIT,	    0x00, ALL_RANDOM_DEVICES, "Start/Stop Unit"		},
    {	SOPC_SYNCHRONIZE_CACHE,	    0x00, ALL_RANDOM_DEVICES, "Synchronize Cache"	},
    {   SOPC_UNMAP,                 0x00, ALL_RANDOM_DEVICES, "Unmap"                   },
    {	SOPC_VERIFY,		    0x00, ALL_RANDOM_DEVICES, "Verify(10)",
	scsi_data_none, random_rw10_encode, NULL, SCSI_MAX_BLOCKS10			},
    {	SOPC_WRITE_6,		    0x00, ALL_RANDOM_DEVICES, "Write(6)",
	scsi_data_write, random_rw6_encode, NULL					},
    {	SOPC_WRITE_10,		    0x00, ALL_RANDOM_DEVICES, "Write(10)",
	scsi_data_write, random_rw10_encode, NULL					},
    {	SOPC_WRITE_VERIFY,	    0x00, ALL_RANDOM_DEVICES, "Write and Verify",
	scsi_data_write, random_rw10_encode, NULL					},
    {	SOPC_WRITE_LONG,	    0x00, ALL_RANDOM_DEVICES, "Write Long"		},
    {	SOPC_WRITE_SAME,	    0x00, ALL_RANDOM_DEVICES, "Write Same",
	scsi_data_write, random_rw10_encode, NULL, SCSI_MAX_BLOCKS10			},
    /*
     * 16-byte Opcodes:
     */
    {   SOPC_EXTENDED_COPY,         0x00, ALL_RANDOM_DEVICES, "Extended Copy"           },
    {   SOPC_RECEIVE_COPY_RESULTS,  0x00, ALL_RANDOM_DEVICES, "Receive Copy Results"    },
    {   SOPC_RECEIVE_ROD_TOKEN_INFO,0x00, ALL_RANDOM_DEVICES, "Receive ROD Token Information" },
    {	SOPC_READ_16,		    0x00, ALL_RANDOM_DEVICES, "Read(16)",
	scsi_data_read, random_rw16_encode, NULL					},
    {	SOPC_WRITE_16,		    0x00, ALL_RANDOM_DEVICES, "Write(16)",
	scsi_data_write, random_rw16_encode, NULL					},
    {	SOPC_WRITE_AND_VERIFY_16,   0x00, ALL_RANDOM_DEVICES, "Write and Verify(16)",
	scsi_data_write, random_rw16_encode, NULL					},
    {	SOPC_VERIFY_16,		    0x00, ALL_RANDOM_DEVICES, "Verify(16)",
	scsi_data_none, random_rw16_encode, NULL, SCSI_MAX_BLOCKS16			},
    {	SOPC_SYNCHRONIZE_CACHE_16,  0x00, ALL_RANDOM_DEVICES, "Synchronize Cache(16)"	},
    {	SOPC_WRITE_SAME_16,	    0x00, ALL_RANDOM_DEVICES, "Write Same(16)",
	scsi_data_write, random_rw16_encode, NULL, WRITE_SAME_MAX_BLOCKS16		},
    {	SOPC_SERVICE_ACTION_IN_16,  0x00, ALL_RANDOM_DEVICES, "Service Action In(16)",
	scsi_data_read									},
    {	SOPC_SERVICE_ACTION_IN_16,  0x10, ALL_RANDOM_DEVICES, "Read Capacity(16)",
	scsi_data_read, read_capacity16_encode, read_capacity16_decode			},
    {   SOPC_SERVICE_ACTION_IN_16,  0x12, ALL_RANDOM_DEVICES, "Get LBA Status(16)"      },
    {   SOPC_COMPARE_AND_WRITE,     0x00, ALL_RANDOM_DEVICES, "Compare and Write(16)"   }
#if 0
    /*
     * SCSI Operation Codes for Sequential-Access Devices.
     */
    {	SOPC_ERASE				0x19
    {	SOPC_ERASE_16				0x93
    {	SOPC_LOAD_UNLOAD			0x1B
    {	SOPC_LOCATE				0x2B
    {	SOPC_LOCATE_16				0x92
    {	SOPC_PREVENT_ALLOW_REMOVAL		0x1E
    {	SOPC_READ				0x08
    {	SOPC_READ_BLOCK_LIMITS			0x05
    {	SOPC_READ_POSITION			0x34
    {	SOPC_READ_REVERSE			0x0F
    {	SOPC_RECOVER_BUFFERED_DATA		0x14
    {	SOPC_RELEASE_UNIT			0x17
    {	SOPC_RESERVE_UNIT			0x16
    {	SOPC_REWIND				0x01
    {	SOPC_SPACE				0x11
    {	SOPC_VERIFY_TAPE			0x13
    {	SOPC_WRITE				0x0A
    {	SOPC_WRITE_FILEMARKS			0x10

    /*
     * SCSI Operation Codes for Printer Devices.
     */
    {	SOPC_FORMAT				0x04
    {	SOPC_PRINT				0x0A
    {	SOPC_RECOVER_BUFFERED_DATA		0x14
    {	SOPC_RELEASE_UNIT			0x17
    {	SOPC_RESERVE_UNIT			0x16
    {	SOPC_SLEW_PRINT				0x0B
    {	SOPC_STOP_PRINT				0x1B
    {	SOPC_SYNCHRONIZE_BUFFER			0x10
    
    /*
     * SCSI Operation Codes for Processor Devices.
     */
    {	SOPC_RECEIVE				0x08
    {	SOPC_SEND				0x0A

    /*
     * SCSI Operation Codes for Write-Once Devices.
     */
    {	SOPC_LOCK_UNLOCK_CACHE			0x36
    {	SOPC_MEDIUM_SCAN			0x38
    {	SOPC_PREFETCH				0x34
    {	SOPC_PREVENT_ALLOW_REMOVAL		0x1E
    {	SOPC_READ_6				0x08
    {	SOPC_READ_10				0x28
    {	SOPC_READ_12				0xA8
    {	SOPC_READ_CAPACITY			0x25
    {	SOPC_READ_LONG				0x3E
    {	SOPC_REASSIGN_BLOCKS			0x07
    {	SOPC_RELEASE				0x17
    {	SOPC_RESERVE				0x16
    {	SOPC_REZERO_UNIT			0x01

    /*
     * SCSI Operation Codes for CD-ROM Devices.
     */
    {	SOPC_LOCK_UNLOCK_CACHE			0x36
    {	SOPC_PAUSE_RESUME			0x4B
    {	SOPC_PLAY_AUDIO_10			0x45
    {	SOPC_PLAY_AUDIO_12			0xA5
    {	SOPC_PLAY_AUDIO_MSF			0x47
    {	SOPC_PLAY_AUDIO_TRACK_INDEX		0x48
    {	SOPC_PLAY_TRACK_RELATIVE_10		0x49
    {	SOPC_PLAY_TRACK_RELATIVE_12		0xA9
    {	SOPC_PREFETCH				0x34
    {	SOPC_PREVENT_ALLOW_REMOVAL		0x1E
    {	SOPC_READ_6				0x08
    {	SOPC_READ_10				0x28
    {	SOPC_READ_12				0xA8
    {	SOPC_READ_FORMAT_CAPACITIES		0x23
    {	SOPC_READ_CAPACITY			0x25
    {	SOPC_READ_HEADER			0x44
    {	SOPC_READ_LONG				0x3E
    {	SOPC_READ_SUBCHANNEL			0x42
    {	SOPC_READ_TOC				0x43
    {	SOPC_RELEASE				0x17
    {	SOPC_RESERVE				0x16
    {	SOPC_REZERO_UNIT			0x01
    {	SOPC_SEARCH_DATA_EQUAL_10		0x31
    {	SOPC_SEARCH_DATA_EQUAL_12		0xB1
    {	SOPC_SEARCH_DATA_HIGH_10		0x30
    {	SOPC_SEARCH_DATA_HIGH_12		0xB0
    {	SOPC_SEARCH_DATA_LOW_10			0x32
    {	SOPC_SEARCH_DATA_LOW_12			0xB2
    {	SOPC_SEEK_6				0x0B
    {	SOPC_SEEK_10				0x2B
    {	SOPC_SET_LIMITS_10			0x33
    {	SOPC_SET_LIMITS_12			0xB3
    {	SOPC_START_STOP_UNIT			0x1B
    {	SOPC_SYNCHRONIZE_CACHE			0x35
    {	SOPC_VERIFY_10				0x2F
    {	SOPC_VERIFY_12				0xAF

    /*
     * SCSI Operation Codes for Scanner Devices.
     */
    {	SOPC_GET_DATA_BUFFER_STATUS		0x34
    {	SOPC_GET_WINDOW				0x25
    {	SOPC_OBJECT_POSITION			0x31
    {	SOPC_READ_SCANNER			0x28
    {	SOPC_RELEASE_UNIT			0x17
    {	SOPC_RESERVE_UNIT			0x16
    {	SOPC_SCAN				0x1B
    {	SOPC_SET_WINDOW				0x24
    {	SOPC_SEND_SCANNER			0x2A
    
    /*
     * SCSI Operation Codes for Optical Memory Devices.
     */
    {	SOPC_ERASE_10				0x2C
    {	SOPC_ERASE_12				0xAC
    {	SOPC_FORMAT_UNIT			0x04
    {	SOPC_LOCK_UNLOCK_CACHE			0x36
    {	SOPC_MEDIUM_SCAN			0x38
    {	SOPC_PREFETCH				0x34
    {	SOPC_PREVENT_ALLOW_REMOVAL		0x1E
    {	SOPC_READ_6				0x08
    {	SOPC_READ_10				0x28
    {	SOPC_READ_12				0xA8
    {	SOPC_READ_CAPACITY			0x25
    {	SOPC_READ_DEFECT_DATA_10		0x37
    {	SOPC_READ_DEFECT_DATA_12		0xB7
    {	SOPC_READ_GENERATION			0x29
    {	SOPC_READ_LONG				0x3E
    {	SOPC_READ_UPDATED_BLOCK			0x2D
    {	SOPC_REASSIGN_BLOCKS			0x07
    {	SOPC_RELEASE				0x17
    {	SOPC_RESERVE				0x16
    {	SOPC_REZERO_UNIT			0x01
    {	SOPC_SEARCH_DATA_EQUAL_10		0x31
    {	SOPC_SEARCH_DATA_EQUAL_12		0xB1
    {	SOPC_SEARCH_DATA_HIGH_10		0x30
    {	SOPC_SEARCH_DATA_HIGH_12		0xB0
    {	SOPC_SEARCH_DATA_LOW_10			0x32
    {	SOPC_SEARCH_DATA_LOW_12			0xB2
    {	SOPC_SEEK_6				0x0B
    {	SOPC_SEEK_10				0x2B
    {	SOPC_SET_LIMITS_10			0x33
    {	SOPC_SET_LIMITS_12			0xB3
    {	SOPC_START_STOP_UNIT			0x1B
    {	SOPC_SYNCHRONIZE_CACHE			0x35
    {	SOPC_UPDATE_BLOCK			0x3D
    {	SOPC_VERIFY_10				0x2F
    {	SOPC_VERIFY_12				0xAF
    {	SOPC_WRITE_6				0x0A
    {	SOPC_WRITE_10				0x2A
    {	SOPC_WRITE_12				0xAA
    {	SOPC_WRITE_VERIFY_10			0x2E
    {	SOPC_WRITE_VERIFY_12			0xAE
    {	SOPC_WRITE_LONG				0x3F

    /*
     * SCSI Operation Codes for Medium-Changer Devices.
     */
    {	SOPC_EXCHANGE_MEDIUM			0xA6
    {	SOPC_INITIALIZE_ELEMENT_STATUS		0x07
    {	SOPC_MOVE_MEDIUM			0xA5
    {	SOPC_POSITION_TO_ELEMENT		0x2B
    {	SOPC_PREVENT_ALLOW_REMOVAL		0x1E
    {	SOPC_READ_ELEMENT_STATUS		0xB8
    {	SOPC_RELEASE				0x17
    {	SOPC_REQUEST_VOLUME_ELEMENT_ADDRESS	0xB5
    {	SOPC_RESERVE				0x16
    {	SOPC_REZERO_UNIT			0x01
    {	SOPC_SEND_VOLUME_TAG			0xB6
    
    /*
     * SCSI Operation Codes for Communication Devices.
     */
    {	SOPC_GET_MESSAGE_6			0x08
    {	SOPC_GET_MESSAGE_10			0x28
    {	SOPC_GET_MESSAGE_12			0xA8
    {	SOPC_SEND_MESSAGE_6			0x0A
    {	SOPC_SEND_MESSAGE_10			0x2A
    {	SOPC_SEND_MESSAGE_12			0xAA
#endif
};
static int scsi_opcodeEntries = ( sizeof(scsiOpcodeTable) / sizeof(scsi_opcode_t) );

scsi_opcode_t *
ScsiOpcodeEntry(unsigned char *cdb, unsigned short device_type)
{
    scsi_opcode_t *sop = scsiOpcodeTable;
    unsigned char opcode = cdb[0];
    unsigned char subcode = 0;
    hbool_t check_subcode = False;
    int entrys;

    /*
     * For opcodes w/subcodes, detect/setup the subcode here.
     */
    switch (opcode) {
	case SOPC_INQUIRY:
	    if (cdb[1] && INQ_EVPD) {
		subcode = cdb[2]; /* page code */
		check_subcode = True;
	    }
	    break;

#if 0 /* TBA */
	case SOPC_MODE_SENSE_6:
	case SOPC_MODE_SENSE_10:
    	    subcode = cdb[2] & 0x3f;	/* Low 6 bits = page code */
	    check_subcode = True;
	    break;
#endif

        case SOPC_EXTENDED_COPY:
	case SOPC_MAINTENANCE_IN:
	case SOPC_PERSISTENT_RESERVE_IN:
        case SOPC_PERSISTENT_RESERVE_OUT:
        case SOPC_RECEIVE_ROD_TOKEN_INFO:
    	    subcode = cdb[1] & 0x1f;	/* Low 5 bits = action */
	    check_subcode = True;
	    break;

	case SOPC_SERVICE_ACTION_IN_16:
	    subcode = cdb[1];
	    check_subcode = True;
	    break;

	default:
	    break;
    }

    if (check_subcode) {
        for (entrys = 0; entrys < scsi_opcodeEntries; sop++, entrys++) {
	    if ( ISSET(sop->device_mask, device_type) &&
		 (sop->opcode == opcode) && (sop->subcode == subcode) ) {
		return(sop);
	    }
	}
    }
    for (entrys = 0, sop = scsiOpcodeTable; entrys < scsi_opcodeEntries; sop++, entrys++) {
	if ( ISSET(sop->device_mask, device_type) &&
	     (sop->opcode == opcode) ) {
	    return(sop);
	}
    }
    return((scsi_opcode_t *) 0);
}

void
ShowScsiOpcodes(scsi_device_t *sdp)
{
    scsi_opcode_t *sop = scsiOpcodeTable;
    int entrys;

    Print(sdp, "  Opcode  Subcode  Direction  Encode  Decode   Default   Opcode Name\n");
    Print(sdp, "  ------  -------  ---------  ------  ------  ---------  -----------\n");

    for (entrys = 0, sop = scsiOpcodeTable; entrys < scsi_opcodeEntries; sop++, entrys++) {
	char *data_dir;
	if (sop->data_dir == scsi_data_none) {
	    data_dir = "none";
	} else if (sop->data_dir == scsi_data_read) {
	    data_dir = "read";
	} else if (sop->data_dir == scsi_data_write) {
	    data_dir = "write";
	} else {
	    data_dir = "???";
	}

	/*
	Printf(sdp, "  Opcode  Subcode  Direction  Encode  Decode   Default   Opcode Name\n");
	                0xhh    0xhh      write     xxx     xxx    429496729  string...
	 */
	Print(sdp, "   0x%02x    0x%02x      %-5.5s     %-3.3s     %-3.3s   %10u  %s\n",
	      sop->opcode, sop->subcode, data_dir,
	      (sop->encode) ? "yes" : "no",
	      (sop->decode) ? "yes" : "no",
	      sop->default_blocks, sop->opname);
    }
    return;
}
