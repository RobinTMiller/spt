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
 * File:	scsi_opcodes.c
 * Author:	Robin T. Miller
 * Date:	February 7th, 2012
 *
 * Descriptions:
 *	Functions and tables to decode SCSI opcode data.
 *
 * Modification History:
 * 
 * April 23rd, 2019 by Robin T. Miller
 *      When allocating the initial data buffer, if writing and a pattern
 * is specified, initialize the data buffer with specified pattern. This
 * issue was found while trying to do Write Same sith non-zero data!
 * 
 * October 19th, 2017 by Robin T. Miller
 *      Added additional Inquiry VPD pages.
 *      Note: Not implemented, but useful for "showopcodes".
 * 
 * December 3rd, 2016 by Robin T. Miller
 *      Move SES functions to spt_ses.[ch] and scsi_ses.h
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spt.h"
#include "libscsi.h"
#include "inquiry.h"

#include "spt_ses.h"
#include "scsi_cdbs.h"
#include "scsi_diag.h"
#include "scsi_log.h"

#include "spt_mtrand64.h"
#include "parson.h"

/*
 * External Declarations: 
 */

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
int random_unmap_encode(void *arg);
int read_capacity10_encode(void *arg);
int read_capacity10_decode(void *arg);
int read_capacity16_encode(void *arg);
int read_capacity16_decode(void *arg);
int get_lba_status_encode(void *arg);
int get_lba_status_decode(void *arg);

int report_luns_encode(void *arg);
int report_luns_decode(void *arg);
int request_sense_encode(void *arg);
int request_sense_decode(void *arg);
int rtpgs_encode(void *arg);
int rtpgs_decode(void *arg);

/*
 * JSON Functions:
 */
char *read_capacity10_decode_json(scsi_device_t *sdp, io_params_t *iop, ReadCapacity10_data_t *rcdp, char *scsi_name);
char *read_capacity16_decode_json(scsi_device_t *sdp, io_params_t *iop, ReadCapacity16_data_t *rcdp, char *scsi_name);

/* Support Functions: */
int wut_extended_copy_verify_data(scsi_device_t *sdp);
void check_thin_provisioning(scsi_device_t *sdp, scsi_generic_t *sgp, io_params_t *iop);
int get_block_provisioning(scsi_device_t *sdp, scsi_generic_t *sgp, io_params_t *iop);
int get_unmap_block_limits(scsi_device_t *sdp, scsi_generic_t *sgp, io_params_t *iop, uint64_t max_blocks);
int cawWriteData(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
		 uint64_t lba, uint32_t blocks, uint32_t bytes);
int cawReadVerifyData(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
		      uint64_t lba, uint32_t blocks, uint32_t bytes);
int random_rw_complete_io(scsi_device_t *sdp, uint64_t max_lba, uint64_t max_blocks);
int random_rw_process_cdb(scsi_device_t *sdp, io_params_t *iop, uint64_t max_lba, uint64_t max_blocks);
int random_rw_process_data(scsi_device_t *sdp);
int random_rw_ReadVerifyData(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
			     uint64_t lba, uint32_t bytes);
void restore_saved_parameters(scsi_device_t *sdp);
int scsiReadData(io_params_t *iop, scsi_io_type_t read_type, scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes);
int scsiWriteData(io_params_t *iop, scsi_io_type_t write_type, scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes);

/* Token Based Extended Copy Functions: */
int populate_token_create(scsi_device_t *sdp);
int populate_token_init_devices(scsi_device_t *sdp);
int write_using_token_complete_io(scsi_device_t *sdp);
int rrti_verify_response(scsi_device_t *sdp, io_params_t *iop);
int rrti_process_response(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp);

/* XCOPY Support Functions: */
int extended_copy_init_devices(scsi_device_t *sdp);
void extended_copy_handle_same_lun(scsi_device_t *sdp);
void *extended_copy_setup_segments(scsi_device_t *sdp, void *bp);
void *extended_copy_setup_targets(scsi_device_t *sdp, void *bp);
uint64_t extended_copy_calculate_source_blocks(scsi_device_t *sdp, int *src_with_data);
void extended_copy_init_iop_segment_info(scsi_device_t *sdp);
void extended_copy_adjust_iop_segment_info(scsi_device_t *sdp);
int extended_copy_complete_io(scsi_device_t *sdp);
int extended_copy_verify_data(scsi_device_t *sdp);
int extended_copy_read_and_compare(scsi_device_t *sdp,
				   scsi_generic_t *dst_sgp, io_params_t *iop,
				   scsi_generic_t *src_sgp, io_params_t *siop,
				   uint16_t xcopy_blocks,
				   uint64_t src_starting_lba, uint64_t dst_starting_lba);

/* Report Target Port Group Functions: */
void print_rtpgs_header(scsi_device_t *sdp, uint32_t rtpgs_length);
int PrintTargetPortDescriptor(scsi_device_t *sdp, report_target_port_group_desc_t *tpgd);

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
    /* Note: Removing this, since it excludes sanity checks below, and has bit me twice! */
    //if ( ANY_DATA_LIMITS(siop) || ANY_DATA_LIMITS(iop) ) return(status);
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
        /* Perform a few sanity checks to enforce 'image' mode, and/or avoid misleading results. */
	if (sdp->image_copy || sdp->slices) {
	    Printf(sdp, "Setting both devices to the smallest capacity to ensure the same block ranges!\n");
	    iop->device_capacity = min(iop->device_capacity, siop->device_capacity);
	    siop->device_capacity = min(iop->device_capacity, siop->device_capacity);
            /* Note: User has control over these options per device, but avoid false failures! (for me) */
            /* FYI: We may need further checks, but these are a starting point. I don't like overrides! */
	    if (siop->data_limit) {
        	iop->data_limit = siop->data_limit;
	    }
	    if (siop->starting_lba) {
        	iop->starting_lba = siop->starting_lba;
	    }
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
get_unmap_block_limits(scsi_device_t *sdp, scsi_generic_t *sgp, io_params_t *iop, uint64_t max_blocks)
{
    inquiry_block_limits_t block_limits;
    inquiry_block_limits_t *blp = &block_limits;
    int status;

    status = GetBlockLimits(sgp->fd, sgp->dsf, sgp->debug, False, blp, sgp->tsp);
    if (status == SUCCESS) {
	iop->max_unmap_lba_count = blp->max_unmap_lba_count;
	iop->max_range_descriptors = blp->max_unmap_descriptor_count;
	iop->optimal_unmap_granularity = blp->optimal_unmap_granularity;
    } else {
	iop->max_unmap_lba_count = (uint32_t)max_blocks;
    }
    /* Override the default blocks, if user did not specify. */
    /* Note: The CDB blocks is already set from default w/slices! */
    if ( (iop->cdb_blocks == 0) ||
	 ( (sdp->bypass == False) && (iop->cdb_blocks > iop->max_unmap_lba_count) ) ) {
	iop->cdb_blocks = iop->max_unmap_lba_count;
    }
    if ( (status == SUCCESS) && sdp->DebugFlag && (sdp->thread_number == 1) ) {
	Printf(sdp, "Device: %s, Max Unmap LBA Count: %u blocks, Max Range Descriptors: %u, CDB Blocks: " LUF " blocks\n",
	       sgp->dsf, iop->max_unmap_lba_count, iop->max_range_descriptors, iop->cdb_blocks);
    }
    //check_thin_provisioning(sdp, sgp, iop);
    return(status);
}

/* ======================================================================== */

int
random_unmap_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    uint64_t max_lba = UNMAP_MAX_LBA, max_blocks = UNMAP_MAX_BLOCKS;
    unmap_cdb_t *cdb;
    unmap_parameter_list_header_t *uphp;
    unmap_block_descriptor_t *ubdp;
    int range_count;
    uint32_t unmap_length, min_size = 0, max_size, incr_size;
    uint32_t blocks, blocks_per_range, blocks_left, blocks_resid;
    uint64_t lba;
    int range, status;

    cdb = (unmap_cdb_t *)sgp->cdb;
    if (iop->first_time) {
	sgp->data_dir = scsi_data_write;
	if (!iop->data_length) iop->data_length = sgp->data_length;
	unmap_length = sizeof(*uphp) + (sdp->range_count * sizeof(*ubdp));
	if ( (sgp->data_buffer == NULL) || (iop->data_length < unmap_length) ) {
	    if (sgp->data_buffer) free_palign(sdp, sgp->data_buffer);
	    iop->data_length = unmap_length;
	    sgp->data_buffer = malloc_palign(sdp, iop->data_length, 0);
	    if (sgp->data_buffer == NULL) return(FAILURE);
	}
	sgp->data_length = iop->data_length;
	iop->disable_length_check = True;
	if (iop->max_unmap_lba_count == 0) {
	    status = get_unmap_block_limits(sdp, sgp, iop, max_blocks);
	}
    }
    if (iop->max_unmap_lba_count) {
	max_blocks = iop->max_unmap_lba_count;
    }
    status = initialize_io_parameters(sdp, iop, max_lba, max_blocks);
    if (status != SUCCESS) return (status);

    uphp = (unmap_parameter_list_header_t *)sgp->data_buffer;
    ubdp = (unmap_block_descriptor_t *)(uphp + 1);
    
    lba = iop->current_lba;
    range_count = sdp->range_count;
    blocks_per_range = (uint32_t)(iop->cdb_blocks / range_count);
    if (blocks_per_range == 0) {
	range_count = 1;
	blocks_per_range = (uint32_t)iop->cdb_blocks;
    }
    blocks_resid = (uint32_t)(iop->cdb_blocks - (blocks_per_range * range_count));

    if ( (iop->user_min == True) || (iop->user_max == True) || (iop->incr_variable == True) ) {
	min_size = (iop->min_size) ? iop->min_size : 1;
	max_size = (iop->max_size) ? iop->max_size : (uint32_t)(max_blocks / sdp->range_count);
	/* Note: This limit is no longer needed, but I'm fearful of breaking scripts! */
	//if (sdp->bypass == False) {
	//    max_size = min(blocks_per_range, max_size);
	//}
	incr_size = (iop->incr_size) ? iop->incr_size : 1;
    }
    
    blocks_left = (uint32_t)iop->cdb_blocks;
    for (range = 0; (range < range_count); range++, ubdp++) {
	if ( (iop->user_min == True) || (iop->user_max == True) || (iop->incr_variable == True) ) {
	    if (iop->incr_variable == True) {
		uint32_t randum = (uint32_t)genrand64_int64();
		blocks = ((randum % max_size) + min_size);
		if (iop->optimal_unmap_granularity) {
		    blocks = roundup(blocks,iop->optimal_unmap_granularity);
		}
	    } else if ( (iop->user_min == True) || (iop->user_max == True) ) {
		blocks = min_size;
		min_size += incr_size;
		if (min_size > max_size) min_size = iop->min_size;
	    }
	    blocks = min(blocks, blocks_left);
	    /* Last range gets remaining blocks. */
	    if ( ((range + 1) == range_count) ) {
		blocks = blocks_left;
	    }
	} else { /* non-min/max/incr handling */
	    blocks = min(blocks_per_range, blocks_left);
	    if ( ((range + 1) == range_count) && blocks_resid) {
		blocks += blocks_resid;
	    }
	}
	if (sdp->xDebugFlag) {
	    Printf(sdp, "\n");
	    PrintNumeric(sdp, "Range Descriptor", range, PNL);
	    PrintDecHex(sdp, "Number of Blocks",  blocks, PNL);
	    PrintLongDecHex(sdp, "Starting Logical Block Address", lba, DNL);
	    Print(sdp, " (lba's " LUF " - " LUF ")\n", lba, (lba + blocks - 1));
	}
	HtoS(ubdp->lba, lba);
	HtoS(ubdp->length, blocks);
	lba += blocks;
	/* Handle case where we run out of blocks before using all descriptors! */
	blocks_left -= min(blocks, blocks_left);
	if (blocks_left == 0) break;
    }
    if (sdp->xDebugFlag) {
	Printf(sdp, "\n");
    }
    /* Set the CDB length and parameter list based on range descriptors used. */
    if (range < range_count) {
	range_count = (range + 1);
    }
    unmap_length = sizeof(*uphp) + (range_count * sizeof(*ubdp));
    /* Note: With variable block ranges, this must always be set! */
    sgp->data_length = unmap_length;
    HtoS(cdb->parameter_list_length, unmap_length);
    HtoS(uphp->data_length,  (unmap_length - sizeof(uphp->data_length)));
    HtoS(uphp->block_descriptor_length, (unmap_length - sizeof(*uphp)));
    return(status);
}

/* ======================================================================== */

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

/* ======================================================================== */

int
setup_read_capacity10(scsi_device_t *sdp, scsi_generic_t *sgp)
{
    ReadCapacity10_CDB_t *cdb;
    uint32_t data_length = sizeof(ReadCapacity10_data_t);

    cdb = (ReadCapacity10_CDB_t *)sgp->cdb;
    memset(cdb, '\0', sizeof(*cdb));
    cdb->opcode = (uint8_t)SOPC_READ_CAPACITY;
    sgp->data_dir = scsi_data_read;
    sgp->data_length = data_length;
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
read_capacity10_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    ReadCapacity10_CDB_t *cdb;
    ReadCapacity10_data_t *rcdp;

    cdb = (ReadCapacity10_CDB_t *)sgp->cdb;
    sgp->data_dir = scsi_data_read;
    if ( (sgp->data_buffer == NULL) || (sgp->data_length < sizeof(*rcdp)) ) {
	if (sgp->data_buffer) free_palign(sdp, sgp->data_buffer);
	sgp->data_length = sizeof(*rcdp);
	sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
	if (sgp->data_buffer == NULL) return(FAILURE);
    }
    return(SUCCESS);
}

int
read_capacity10_decode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    ReadCapacity10_data_t *rcdp;
    uint32_t block_length;
    uint32_t logical_blocks;
    int status = SUCCESS;
    
    if ( (rcdp = (ReadCapacity10_data_t *)sgp->data_buffer) == NULL) {
	ReportDeviceInformation(sdp, sgp);
	Fprintf(sdp, "No capacity buffer provided!\n");
	return(FAILURE);
    }

    if (sdp->output_format == JSON_FMT) {
	char *json_string;
	json_string = read_capacity10_decode_json(sdp, iop, rcdp, "Read Capacity(10)");
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
    }

    Printf(sdp, "\n");
    Printf(sdp, "Read Capacity(10) Data: (data length %u bytes)\n", sgp->data_length);
    Printf(sdp, "\n");

    logical_blocks = (uint32_t)StoH(rcdp->last_block) + 1;
    block_length = (uint32_t)StoH(rcdp->block_length);

    PrintDecimal(sdp, "Maximum Capacity", logical_blocks, (block_length) ? DNL : PNL);
    if (block_length) {
	double bytes;
	bytes = ((double) logical_blocks * (double) block_length);
	Print(sdp, " (%.3f megabytes)\n", (bytes / (double) MBYTE_SIZE));
    }
    PrintDecimal(sdp, "Block Length", block_length, PNL);
    return(status);
}

/*
 * Read Capacity 10 in JSON Format:
 */
char *
read_capacity10_decode_json(scsi_device_t *sdp, io_params_t *iop, ReadCapacity10_data_t *rcdp, char *scsi_name)
{
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    JSON_Status json_status;
    char *json_string = NULL;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    uint8_t *ucp = NULL;
    int length = 0;
    int offset = 0;
    uint32_t block_length;
    uint32_t logical_blocks;
    int status = SUCCESS;

    root_value  = json_value_init_object();
    if (root_value == NULL) return(NULL);
    root_object = json_value_get_object(root_value);

    value  = json_value_init_object();
    if (value == NULL) {
	json_value_free(root_value);
	return(NULL);
    }
    json_status = json_object_dotset_value(root_object, scsi_name, value);
    object = json_value_get_object(value);

    ucp = (uint8_t *)rcdp;
    length = sizeof(*rcdp);
    json_status = json_object_set_number(object, "Length", (double)length);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Offset", (double)offset);
    if (json_status != JSONSuccess) goto finish;
    offset = FormatHexBytes(text, offset, ucp, length);
    json_status = json_object_set_string(object, "Bytes", text);
    if (json_status != JSONSuccess) goto finish;

    logical_blocks = (uint32_t)StoH(rcdp->last_block) + 1;
    block_length = (uint32_t)StoH(rcdp->block_length);

    json_status = json_object_set_number(object, "Maximum Capacity", (double)logical_blocks);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Block Length", (double)block_length);
    if (json_status != JSONSuccess) goto finish;

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

/* ======================================================================== */

int
setup_read_capacity16(scsi_device_t *sdp, scsi_generic_t *sgp)
{
    ReadCapacity16_CDB_t *cdb;
    uint32_t data_length = sizeof(ReadCapacity16_data_t);

    cdb = (ReadCapacity16_CDB_t *)sgp->cdb;
    memset(cdb, '\0', sizeof(*cdb));
    cdb->opcode = (uint8_t)SOPC_SERVICE_ACTION_IN_16; 
    cdb->service_action = SCSI_SERVICE_ACTION_READ_CAPACITY_16;
    sgp->data_dir = scsi_data_read;
    sgp->data_length = data_length;
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

    if (sdp->output_format == JSON_FMT) {
	char *json_string;
	json_string = read_capacity16_decode_json(sdp, iop, rcdp, "Read Capacity(16)");
	if (json_string) {
	    PrintLines(sdp, json_string);
	    Printnl(sdp);
	    json_free_serialized_string(json_string);
	}
	return(status);
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

/*
 * Read Capacity 16 in JSON Format:
 */
char *
read_capacity16_decode_json(scsi_device_t *sdp, io_params_t *iop, ReadCapacity16_data_t *rcdp, char *scsi_name)
{
    JSON_Value	*root_value;
    JSON_Object *root_object;
    JSON_Value  *value;
    JSON_Object *object;
    JSON_Status json_status;
    char *json_string = NULL;
    char text[STRING_BUFFER_SIZE];
    char *tp = NULL;
    uint8_t *ucp = NULL;
    int length = 0;
    int offset = 0;
    uint32_t block_length;
    uint64_t logical_blocks;
    uint16_t lowest_aligned;
    int status = SUCCESS;

    root_value  = json_value_init_object();
    if (root_value == NULL) return(NULL);
    root_object = json_value_get_object(root_value);

    value  = json_value_init_object();
    if (value == NULL) {
	json_value_free(root_value);
	return(NULL);
    }
    json_status = json_object_dotset_value(root_object, scsi_name, value);
    object = json_value_get_object(value);

    ucp = (uint8_t *)rcdp;
    length = sizeof(*rcdp);
    json_status = json_object_set_number(object, "Length", (double)length);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Offset", (double)offset);
    if (json_status != JSONSuccess) goto finish;
    offset = FormatHexBytes(text, offset, ucp, length);
    json_status = json_object_set_string(object, "Bytes", text);
    if (json_status != JSONSuccess) goto finish;

    logical_blocks = StoH(rcdp->last_block) + 1;
    block_length = (uint32_t)StoH(rcdp->block_length);

    json_status = json_object_set_number(object, "Maximum Capacity", (double)logical_blocks);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Block Length", (double)block_length);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(object, "Protection Enabled", rcdp->prot_en);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Protection Type", (double)rcdp->p_type);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Logical Blocks per Physical Exponent", (double)rcdp->lbppbe);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_number(object, "Protection Information Exponent", (double)rcdp->p_i_exponent);
    if (json_status != JSONSuccess) goto finish;
    json_status = json_object_set_boolean(object, "Logical Block Provisioning Management", rcdp->lbpme);
    json_status = json_object_set_boolean(object, "Logical Block Provisioning Read Zeroes", rcdp->lbprz);
    if (json_status != JSONSuccess) goto finish;
    lowest_aligned = ((rcdp->lowest_aligned_msb << 8) | rcdp->lowest_aligned_lsb);
    json_status = json_object_set_number(object, "Lowest Aligned Logical Block Address", (double)lowest_aligned);

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

/* ======================================================================== */

int
get_lba_status_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    get_lba_status_cdb_t *cdb;
    get_lba_status_param_data_t *paramp;
    lba_status_descriptor_t *lbasdp;
    uint32_t min_size, max_size;
    uint64_t max_lba = GLS_MAX_LBA, max_blocks = GLS_MAX_BLOCKS;
    int status;

    if (iop->first_time == True) {
	sdp->decode_flag = True;
	sgp->data_dir = scsi_data_read;
	iop->disable_length_check = True;
	iop->deallocated_blocks = 0;
	iop->mapped_blocks = 0;
	iop->total_lba_blocks = 0;
	iop->cdb_blocks = 1; /* Magic to initialize parameters below! */
    }
    cdb = (get_lba_status_cdb_t *)sgp->cdb;
    min_size = ( sizeof(*paramp) + sizeof(*lbasdp) );
    max_size = ( sizeof(*paramp) + (sizeof(*lbasdp) * MAX_LBA_STATUS_DESC) );
    /* Allocate/reallocate data buffers (as required). */
    if ( (sgp->data_buffer == NULL) || (sgp->data_length < min_size) ) {
	if (sgp->data_buffer) free_palign(sdp, sgp->data_buffer);
	sgp->data_length = max_size;
	sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
	if (sgp->data_buffer == NULL) return(FAILURE);
    }
    HtoS(cdb->allocation_length, sgp->data_length);

    status = initialize_io_parameters(sdp, iop, max_lba, max_blocks);
    if (status == END_OF_DATA) {
	PrintHeader(sdp, "Get LBA Status Information");
	PrintLongDec(sdp, "Mapped Blocks", iop->mapped_blocks,  PNL);
	PrintLongDec(sdp, "Deallocated Blocks", iop->deallocated_blocks,  PNL);
	PrintLongDec(sdp, "Total Blocks", (iop->deallocated_blocks + iop->mapped_blocks),  PNL);
	Printf(sdp, "\n");
    }
    if (status != SUCCESS) return(status);

    HtoS(cdb->start_lba, iop->current_lba);
    return(status);
}

int
get_lba_status_decode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    get_lba_status_param_data_t *paramp;
    lba_status_descriptor_t *lbasdp;
    uint32_t min_size;
    uint32_t descriptors, param_data_length, extent_length;
    uint64_t starting_lba, total_extents = 0;
    int status = SUCCESS;
    
    if ( (paramp = (get_lba_status_param_data_t *)sgp->data_buffer) == NULL) {
	ReportDeviceInformation(sdp, sgp);
	Fprintf(sdp, "No Get LBA Status Parameter buffer provided!\n");
	return(FAILURE);
    }
    if ( !iop->device_size || !iop->device_capacity) {
	status = GetCapacity(sdp, iop);
	if (status != SUCCESS) return(status);
    }
    param_data_length = (uint32_t)StoH(paramp->parameter_data_length);
    /* Header + 1 descriptor */
    min_size = ( (sizeof(*paramp) - sizeof(paramp->parameter_data_length)) + sizeof(*lbasdp) );
    if (param_data_length < min_size) {
	return(SUCCESS);
    }
    descriptors = (param_data_length / sizeof(*lbasdp));
    lbasdp = (lba_status_descriptor_t *)((unsigned char *)sgp->data_buffer + sizeof(*paramp));
    
    if (iop->first_time == True) {
	if (sdp->xDebugFlag) {
	    Printf(sdp, "\n");
	    Printf(sdp, "Get LBA Status(16) Data: (parameter data length %u bytes, descriptors %u)\n",
		   param_data_length, descriptors);
	    Printf(sdp, "\n");
	}
	iop->first_time = False;
    }
    param_data_length -= sizeof(paramp->parameter_data_length);

    while (descriptors--) {
	char *provisioning_status;
	starting_lba = StoH(lbasdp->start_lba);
	extent_length = (uint32_t)StoH(lbasdp->extent_length);
	provisioning_status = (lbasdp->provisioning_status & SCSI_PROV_STATUS_HOLE) ? "Deallocated" : "Mapped";
	if (sdp->xDebugFlag) {
	    PrintLongDec(sdp, "Starting LBA", starting_lba,  DNL);
	    Print(sdp, " (lba's " LUF " - " LUF ")\n", starting_lba, (starting_lba + extent_length - 1));
	    PrintDecimal(sdp, "Extent Length", extent_length,  PNL);
	    PrintAscii(sdp, "Provisioning Status", provisioning_status, PNL);
	    if (descriptors) Printf(sdp, "\n");
	}
	if ( (iop->total_lba_blocks + extent_length) > iop->block_limit ) {
	    extent_length = (uint32_t)(iop->block_limit - iop->total_lba_blocks);
	    if (extent_length == 0) break;
	}
	total_extents += extent_length;
	iop->total_lba_blocks += extent_length;

	if (lbasdp->provisioning_status & SCSI_PROV_STATUS_HOLE) {
	    iop->deallocated_blocks += extent_length;
	} else {
	    iop->mapped_blocks += extent_length;
	}
	if (iop->total_lba_blocks >= iop->block_limit) {
	    break;
	}
	lbasdp++;
    }
    if (sdp->xDebugFlag) {
	Printf(sdp, "\n");
	PrintLongDec(sdp, "Deallocated Blocks", iop->deallocated_blocks,  PNL);
	PrintLongDec(sdp, "Mapped Blocks", iop->mapped_blocks,  PNL);
	PrintLongDec(sdp, "Total Blocks", (iop->deallocated_blocks + iop->mapped_blocks),  PNL);
	Printf(sdp, "\n");
    }
    /* Cludge to adjust for CDB blocks we don't have! */
    if (iop->cdb_blocks == 1) {
	iop->total_blocks--;	/* For multiple iterations! */
	iop->total_blocks += total_extents;
    }
    iop->cdb_blocks = total_extents;
    return(SUCCESS);
}

/* ======================================================================== */

#define REPORT_LUNS_BUFSIZE	(sizeof(report_luns_header_t) + (1024 * sizeof(report_luns_entry_t)))

int
report_luns_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    report_luns_cdb_t *cdb;

    cdb = (report_luns_cdb_t *)sgp->cdb;
    sgp->data_dir = scsi_data_read;
    if ( (sgp->data_buffer == NULL) || (sgp->data_length < sizeof(report_luns_header_t)) ) {
	if (sgp->data_buffer) free_palign(sdp, sgp->data_buffer);
	sgp->data_length = REPORT_LUNS_BUFSIZE;
	sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
	if (sgp->data_buffer == NULL) return(FAILURE);
    }
    HtoS(cdb->length, sgp->data_length);
    return(SUCCESS);
}

int
report_luns_decode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    report_luns_header_t *rlhp;
    report_luns_entry_t *rlep;
    PeripheralDeviceAddressing_t *pdap;
    char tmp_buffer[MEDIUM_BUFFER_SIZE];
    char *tmp = tmp_buffer;
    uint32_t list_length;
    uint32_t lun_entries;
    uint64_t lun_value;
    uint32_t entries;
    
    if ( (rlhp = (report_luns_header_t *)sgp->data_buffer) == NULL) {
	ReportDeviceInformation(sdp, sgp);
	Fprintf(sdp, "No report luns buffer provided!\n");
	return(FAILURE);
    }
    list_length = (uint32_t)StoH(rlhp->list_len);
    lun_entries = (list_length / sizeof(*rlep));
    Printf(sdp, "\n");
    Printf(sdp, "Report LUNs Data: (list length %u bytes, %u entries)\n",
	   list_length, lun_entries);
    Printf(sdp, "\n");

    rlep = (report_luns_entry_t *)(rlhp + 1);
    for (entries = 0; (entries < lun_entries); rlep++) {
	if (CmdInterruptedFlag) break;
	lun_value = StoH(rlep->lun_entry);
	pdap = (PeripheralDeviceAddressing_t *)rlep;
	//(void)sprintf(tmp, "Logical Unit Entry #%u", ++entries);
	//PrintAscii(sdp, tmp, "", DNL);
	//PrintFields(sdp, (uint8_t *)rlep, sizeof(*rlep));
	//PrintLongHex(sdp, tmp, lun_value, DNL);
	switch (pdap->address_method) {

	    case SCSI_PeripheralDeviceAddressing:
		(void)sprintf(tmp, "Peripheral Device Addressing #%u", ++entries);
		PrintAscii(sdp, tmp, "", DNL);
		Print(sdp, "0x%016llx", lun_value);
		if (pdap->bus_identifier == SCSI_BusIdentifierLun) {
		    Print(sdp, " (lun %u)\n", pdap->target_or_lun[0]);
		} else {
		    Print(sdp, " (bus id: %u, target: %u)\n", pdap->bus_identifier, pdap->target_or_lun[0]);
		}
		break;

	    default:
		(void)sprintf(tmp, "Logical Unit Entry #%u", ++entries);
		PrintAscii(sdp, tmp, "", DNL);
		Print(sdp, "0x%016llx\n", lun_value);
		break;
	}
    }
    return(SUCCESS);
}

/* ======================================================================== */

/*
 * Default Report Target iPort Groups Buffer Size:
 */
#define MAX_REPORT_TPGS			16
#define MAX_TARGET_PORTS_PER_TPG	16
#define REPORT_TPG_BUF_SIZE     ( sizeof(rtpg_header_t) + \
				  ( \
				    sizeof(rtpg_desc_extended_header_t) + \
				    (sizeof(report_target_port_group_desc_t) * MAX_TARGET_PORTS_PER_TPG) \
                                  ) * MAX_REPORT_TPGS \
                                )

int
setup_rtpg(scsi_device_t *sdp, scsi_generic_t *sgp)
{
    uint32_t data_length = REPORT_TPG_BUF_SIZE;
    maintenance_in_cdb_t *cdb;

    cdb = (maintenance_in_cdb_t *)sgp->cdb;
    memset(cdb, '\0', sizeof(*cdb));
    cdb->opcode = SOPC_MAINTENANCE_IN;
    cdb->service_action = 0x0A;			/* TODO: Define this! */
    sgp->data_dir = scsi_data_read;
    sgp->data_length = data_length;
    sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
    if (sgp->data_buffer == NULL) return (FAILURE);
    /* Setup to execute a CDB operation. */
    sdp->op_type = SCSI_CDB_OP;
    sdp->encode_flag = True;
    sdp->decode_flag = True;
    sgp->cdb_size = GetCdbLength(cdb->opcode);
    return(SUCCESS);
}

int
rtpgs_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    maintenance_in_cdb_t *cdb;
    
    cdb = (maintenance_in_cdb_t *)sgp->cdb;
    sgp->data_dir = scsi_data_read;
    if ( (sgp->data_buffer == NULL) || (sgp->data_length < REPORT_TPG_BUF_SIZE) ) {
        if (sgp->data_buffer) free_palign(sdp, sgp->data_buffer);
        sgp->data_length = REPORT_TPG_BUF_SIZE;
        sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
        if (sgp->data_buffer == NULL) return(FAILURE);
    }
    HtoS(cdb->allocation_length, sgp->data_length);
    return(SUCCESS);
}

void
print_rtpgs_header(scsi_device_t *sdp, uint32_t rtpgs_length)
{
    Printf(sdp, "\n");
    Printf(sdp, "Report Target Port Groups Data: (data length %u bytes)\n", rtpgs_length);
    Printf(sdp, "\n");
    return;
}

int
PrintTargetPortDescriptor(scsi_device_t *sdp, report_target_port_group_desc_t *tpgd)
{
    uint32_t target_port_id;
    uint32_t target_port_count;
    uint32_t target_port_group;
    target_port_desc_t *tpd;
    int length;
    char *str;

    target_port_group = (uint32_t)StoH(tpgd->target_port_group);
    PrintHexDec(sdp, "Target Port Group Number", target_port_group, PNL);
    PrintHex(sdp, "Asymmetric Access State", tpgd->alua_state, DNL);
    switch (tpgd->alua_state) {
	case SCSI_TGT_ALUA_ACTIVE_OPTIMIZED: str = "Active/Optimized"; break;
	case SCSI_TGT_ALUA_ACTIVE_NON_OPTIMIZED: str = "Active/Non-Optimized"; break;
	case SCSI_TGT_ALUA_STANDBY: str = "Standby"; break;
	case SCSI_TGT_ALUA_UNAVAILABLE: str = "Unavailable"; break;
	case SCSI_TGT_ALUA_OFFLINE: str = "Offline"; break;
	case SCSI_TGT_ALUA_TRANSITIONING: str = "Transitioning"; break;
	default: str = "Reserved"; break;
    }
    Print(sdp, " = %s\n", str);

    PrintYesNo(sdp, FALSE, "Preferred Target Port Group", tpgd->pref, PNL);
    PrintYesNo(sdp, FALSE, "Active/Optimized State Supported", tpgd->ao_sup, PNL);
    PrintYesNo(sdp, FALSE, "Active/Non-Optimized State Supported", tpgd->an_sup, PNL);
    PrintYesNo(sdp, FALSE, "Standby State Supported", tpgd->s_sup, PNL);
    PrintYesNo(sdp, FALSE, "Unavailable State Supported", tpgd->u_sup, PNL);
    PrintYesNo(sdp, FALSE, "Transition State Supported", tpgd->t_sup, PNL);

    PrintHex(sdp, "ALUA Status Code", tpgd->status_code, DNL);
    switch (tpgd->status_code) {
	case 0: str = "No Status Available"; break; /* Always 0 for NetApp! */
	case 1: str = "ALUA State Altered by SET TPG cmd"; break;
	case 2: str = "ALUA State Altered by Implicit ALUA"; break;
	default: str = "Reserved"; break;
    }
    Print(sdp, " = %s\n", str);
    /* Note: This field has meaning in 7-mode! */
    PrintHex(sdp, "Vendor Specific Data", tpgd->vendor_specific, PNL);
    PrintDecimal(sdp, "Target Port Count", tpgd->target_port_count, PNL);
    if (target_port_count = tpgd->target_port_count) {
	tpd = (target_port_desc_t *)(tpgd + 1);
	PrintAscii(sdp, "Relative Target Port IDs", "", DNL);
	while (target_port_count--) {
	    target_port_id = (uint32_t)StoH(tpd->relative_target_port_id);
	    Print(sdp, "0x%04x", target_port_id);
	    if (target_port_count) Print(sdp, ", ");
	    tpd++;
	}
	Print(sdp, "\n");
    }
    length = ( sizeof(*tpgd) + ((sizeof(*tpd) * tpgd->target_port_count)) );
    return(length);
}

int
rtpgs_decode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    maintenance_in_cdb_t *cdb;
    rtpg_header_t *tpgh;
    rtpg_desc_extended_header_t  *tpgeh;
    report_target_port_group_desc_t *tpgd;
    uint32_t rtpgs_length;
    
    if (sgp->data_buffer == NULL) return(SUCCESS);

    cdb = (maintenance_in_cdb_t *)sgp->cdb;
    if (cdb->service_action == 0x0a) {
	tpgh = (rtpg_header_t *)sgp->data_buffer;
	rtpgs_length = (uint32_t)StoH(tpgh->length);
	print_rtpgs_header(sdp, rtpgs_length);
	rtpgs_length -= ( sizeof(*tpgh) - sizeof(tpgh->length) );
	tpgd = (report_target_port_group_desc_t *)(tpgh + 1);
    } else {
	tpgeh = (rtpg_desc_extended_header_t  *)sgp->data_buffer;
	rtpgs_length = (uint32_t)StoH(tpgeh->length);
	print_rtpgs_header(sdp, rtpgs_length);
	PrintDecimal(sdp, "Format Type", tpgeh->format_type, PNL);
	PrintDecimal(sdp, "Implicit Transition Time", tpgeh->implicit_transition_time, DNL);
	Print(sdp, " (upper bound of takeover/giveback window)\n");
	rtpgs_length -= ( sizeof(*tpgeh) - sizeof(tpgeh->length) );
	tpgd = (report_target_port_group_desc_t *)(tpgeh + 1);
	Printf(sdp, "\n");
    }

    while ((int)rtpgs_length > 0) {
	int length = PrintTargetPortDescriptor(sdp, tpgd);
	if (rtpgs_length -= length) Printf(sdp, "\n");
	tpgd = (report_target_port_group_desc_t *)((ptr_t)tpgd + length);
    }
    return(SUCCESS);
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
	    /* Honor the users' data pattern, if specified! (IOT handled below) */
	    if ( (sdp->user_data == False) && (sdp->user_pattern == True) &&
		 (iop->sop->data_dir == scsi_data_write) && (sdp->iot_pattern == False) ) {
		InitBuffer(sgp->data_buffer, (size_t)sgp->data_length, sdp->pattern);
	    }
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
random_caw16_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    CompareWrite16_CDB_t *cdb = (CompareWrite16_CDB_t *)sgp->cdb;
    uint64_t max_lba = SCSI_MAX_LBA16, max_blocks = SCSI_MAX_BLOCKS;
    uint32_t write_blocks = 1;
    int status;

    if ( (iop->first_time == True) && (sdp->iterations == 0) ) {
	if (sgp->data_length == 0) {
	    /* Note: The sop *should* always be setup already! */
	    if (iop->sop && iop->sop->default_blocks) {
		sgp->data_length = (iop->sop->default_blocks * iop->device_size);
	    } else {
		sgp->data_length = iop->device_size;
	    }
	}
	/* 
	 * Internally we have two buffers, so double the length. 
	 * First buffer: compare data, Second buffer: write data
	 */
	sgp->data_length *= 2;
	iop->data_length = sgp->data_length;
	if (sgp->data_buffer) free_palign(sdp, sgp->data_buffer);
	sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
	if (sgp->data_buffer == NULL) return(FAILURE);
    }

    /*
     * If requested, read and verify the last data written.
     * Note: The sdp status is from the last CAW operation.
     * This additional check is to handle onerr continue.
     */
    if ( (iop->first_time == False) &&
	 (sdp->status == SUCCESS) && (sdp->read_after_write == True) ) {
	status = cawReadVerifyData(sdp, iop, sgp,
				   iop->current_lba, write_blocks, (uint32_t)iop->device_size);
	if (status != SUCCESS) return(status);
    }

    status = initialize_io_parameters(sdp, iop, max_lba, max_blocks);
    if (status != SUCCESS) {
	if (status == END_OF_DATA) {
	    restore_saved_parameters(sdp);
	    iop->end_of_data = True;
	}
	return(status);
    }

    if (sdp->iot_pattern) {
	size_t data_length = (sgp->data_length / 2);
	uint8_t *compare_buffer = sgp->data_buffer;
	uint8_t *write_buffer = (uint8_t *)sgp->data_buffer + data_length;
	uint64_t iterations = (sdp->iterations + 1);
	uint32_t iot_seed = (uint32_t)(sdp->iot_seed * iterations);
	uint32_t next_iot_seed = (uint32_t)(sdp->iot_seed * (iterations + 1));

	if (sdp->iterations == 0) {
	    /* Note: For first iteration, IOT compare data is correct. */
	    (void)init_iotdata(sdp, iop, write_buffer, (uint32_t)data_length,
			       (uint32_t)iop->current_lba, next_iot_seed);
	    /* Allow IOT data to be pre-written (for performance). */
	    if (sdp->prewrite_flag == True) {
		status = cawWriteData(sdp, iop, sgp,
				      iop->current_lba, write_blocks, (uint32_t)iop->device_size);
		if (status != SUCCESS) return(status);
	    }
	} else {
	    /*
	     * Initialize the compare and write data using iterations to change seed.
	     */
	    (void)init_iotdata(sdp, iop, compare_buffer, (uint32_t)data_length,
			       (uint32_t)iop->current_lba, iot_seed);
	    (void)init_iotdata(sdp, iop, write_buffer, (uint32_t)data_length,
			       (uint32_t)iop->current_lba, next_iot_seed);
	}
    } else {
	size_t data_length = (sgp->data_length / 2);
	uint8_t *compare_buffer = sgp->data_buffer;
	uint8_t *write_buffer = (uint8_t *)sgp->data_buffer + data_length;

	/* Alternate the pattern with inverted pattern each iteration. */
	if ( (sdp->iterations & 1) == 0) {
	    InitBuffer(compare_buffer, data_length, sdp->pattern);
	    InitBuffer(write_buffer, data_length, ~sdp->pattern);
	} else {
	    InitBuffer(compare_buffer, data_length, ~sdp->pattern);
	    InitBuffer(write_buffer, data_length, sdp->pattern);
	}
	/* Allow pattern to be pre-written (1 block writes are slow). */
	if (sdp->prewrite_flag == True) {
	    status = cawWriteData(sdp, iop, sgp,
				  iop->current_lba, write_blocks, (uint32_t)iop->device_size);
	    if (status != SUCCESS) return(status);
	}
    }
    /* Now, initialize the LBA and block length. */
    HtoS(cdb->lba, iop->current_lba);
    if (iop->cdb_blocks) {
	cdb->blocks = (uint8_t)iop->cdb_blocks;
    } else {
	cdb->blocks = (uint8_t)(sgp->data_length / iop->device_size);
    }
    return (SUCCESS);
}

int
cawWriteData(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
	     uint64_t lba, uint32_t blocks, uint32_t bytes)
{
    scsi_generic_t *wsgp = Malloc(sdp, sizeof(*sgp));
    int status = SUCCESS;
    
    if (wsgp == NULL) return(FAILURE);
    /*
     * Duplicate the SCSI generic, to keep sane (CDB, SCSI name, etc).
     */ 
    *wsgp = *sgp;
    status = scsiWriteData(iop, sdp->scsi_write_type, wsgp, lba, blocks, bytes);
    free(wsgp);
    return (status);
}

int
cawReadVerifyData(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp,
		  uint64_t lba, uint32_t blocks, uint32_t bytes)
{
    /* Note: Compare buffer becomes the read buffer. */
    /*       The write data becomes our verify buffer. */
    uint8_t *read_buffer = sgp->data_buffer;
    uint8_t *verify_buffer = (uint8_t *)sgp->data_buffer + bytes;
    scsi_generic_t *rsgp = Malloc(sdp, sizeof(*sgp));
    int status = SUCCESS;
    
    if (rsgp == NULL) return(FAILURE);
    /*
     * Duplicate the SCSI generic, to keep sane (CDB, SCSI name, etc).
     */ 
    *rsgp = *sgp;
    status = scsiReadData(iop, sdp->scsi_read_type, rsgp, lba, blocks, bytes);
    if ( (status == SUCCESS) && (sdp->compare_data == True) ) {
	status = VerifyBuffers(sdp, read_buffer,
			       verify_buffer, rsgp->data_transferred);
	if (status == FAILURE) {
	    if (sdp->iot_pattern) {
		process_iot_data(sdp, iop, verify_buffer,
				 read_buffer, rsgp->data_transferred);
	    }
	}
    }
    free(rsgp);
    return (status);
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
 * XCOPY Support Functions:
 */ 
int
extended_copy_init_devices(scsi_device_t *sdp)
{
    io_params_t *iop;
    scsi_generic_t *sgp;
    uint64_t max_lba = SCSI_MAX_LBA16, max_blocks = XCOPY_MAX_BLOCKS_PER_SEGMENT;
    int device_index;
    int status = SUCCESS;

    if (sdp->io_devices > XCOPY_MIN_DEVS) {
	sdp->io_multiple_sources = True;
	sdp->segment_count = max((sdp->io_devices - 1), sdp->segment_count);
    }

    for (device_index = 0; (device_index < sdp->io_devices); device_index++) {

	iop = &sdp->io_params[device_index];
	sgp = &iop->sg;

	if (!iop->designator_length && (sgp->fd != INVALID_HANDLE_VALUE) ) {
	    status = GetXcopyDesignator(sgp->fd, sgp->dsf, sgp->debug, FALSE, NULL,
					&iop->designator_id, &iop->designator_length,
					&iop->designator_type, sgp->tsp);
	    if (status == FAILURE) {
		ReportDeviceInformation(sdp, sgp);
		Fprintf(sdp, "Unable to retrieve Inquiry Designator ID for device %s!\n",
			sgp->dsf);
		return (status);
	    }
	}
    }
 
    for (device_index = 0; (device_index < sdp->io_devices); device_index++) {

	iop = &sdp->io_params[device_index];
	sgp = &iop->sg;

	if ( !iop->device_size || !iop->device_capacity) {
	    status = GetCapacity(sdp, iop);
	    if (status != SUCCESS) return(status);
	}

	if (sgp->data_buffer && (sgp->data_length < iop->device_size) ) {
	    free_palign(sdp, sgp->data_buffer);
	    sgp->data_buffer = NULL;
	}
	sgp->data_dir = scsi_data_write;
	sgp->data_length = iop->device_size;
	iop->data_length = sgp->data_length;
	if (!sgp->data_buffer) {
	    sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
	    if (sgp->data_buffer == NULL) return(FAILURE);
	}
	sdp->iot_pattern = False;
	sdp->user_pattern = False;
	iop->scale_count = sdp->segment_count;

	status = initialize_io_parameters(sdp, iop, max_lba, max_blocks);
	if (status != SUCCESS) return (status);
    }
    return (status);
}

void
extended_copy_handle_same_lun(scsi_device_t *sdp)
{
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    io_params_t *siop = &sdp->io_params[IO_INDEX_SRC];
    scsi_generic_t *ssgp = &siop->sg;

    /*
     * Only one device, then clone the first!
     * 
     * Remember: We come through here for *each* iteration!
     */ 
    if ( sdp->io_same_lun || (sdp->io_devices < XCOPY_MIN_DEVS) ) {
	*siop = *iop;
	*ssgp = *sgp;
	sdp->io_devices = 2;
	siop->cloned_device = True;
	/* Leave file handle open for later verify. */
    } else if (iop->user_starting_lba || siop->user_starting_lba) {
	/* Avoid special same LUN setup if starting LBA's are specified. */
	return;
    }

    /*
     * Special Handling for same LUN xcopy, but only if source parameters are omitted.
     */ 
    if ( (sdp->io_devices == XCOPY_MIN_DEVS)		       &&
	 (siop->designator_length == iop->designator_length) &&
	 (memcmp(siop->designator_id, iop->designator_id, iop->designator_length) == 0) ) {

	sdp->io_same_lun = True;

	/* Source = 1st half, Destination = 2nd half */
	iop->block_limit /= 2;
	if (iop->block_limit == 0) {
	    iop->block_limit++;
	}
	iop->cdb_blocks = min(iop->cdb_blocks, iop->block_limit);

	/* Setup Source Parameters */
	siop->starting_lba = iop->starting_lba;
	siop->current_lba = siop->starting_lba;
	siop->block_limit = iop->block_limit;
	siop->ending_lba = (siop->starting_lba + siop->block_limit);
	siop->cdb_blocks = iop->cdb_blocks;

	/* Setup Destination Parameters */
	iop->starting_lba = siop->ending_lba;
	iop->current_lba = iop->starting_lba;
	iop->ending_lba = (iop->starting_lba + iop->block_limit);

	if (sdp->DebugFlag && (sdp->iterations == 0)) {
	    Printf(sdp, "Same LUN detected, setting up source and destination parameters:\n");
	    Printf(sdp, "     Source: %s, Starting lba=" LUF ", ending lba=" LUF ", block limit=" LUF " (thread %d)\n",
		   ssgp->dsf, siop->starting_lba, (siop->ending_lba - 1), siop->block_limit, sdp->thread_number);
	    Printf(sdp, "Destination: %s, Starting lba=" LUF ", ending lba=" LUF ", block limit=" LUF " (thread %d)\n",
		   sgp->dsf, iop->starting_lba, (iop->ending_lba - 1), iop->block_limit, sdp->thread_number);
	}
    }
    return;
}
	
void *
extended_copy_setup_targets(scsi_device_t *sdp, void *bp)
{
    io_params_t *iop;
    xcopy_id_cscd_ident_desc_t *tgtdp;
    int device_index;

    tgtdp = (xcopy_id_cscd_ident_desc_t *)bp;
    /*
     * Setup the Target Descriptores:
     */ 
    for (device_index = 0; (device_index < sdp->io_devices); device_index++) {

	iop = &sdp->io_params[device_index];

	tgtdp->desc_type_code = XCOPY_CSCD_TYPE_CODE_IDENTIFICATION;
	tgtdp->device_type = DTYPE_DIRECT;
	tgtdp->codeset = IID_CODE_SET_BINARY;
	tgtdp->designator_type =  iop->designator_type;
        tgtdp->association = IID_ASSOC_LOGICAL_UNIT;
	tgtdp->designator_length = iop->designator_length;
	memcpy(tgtdp->designator, iop->designator_id, iop->designator_length);
	HtoS(tgtdp->type_spec_params.disk_block_length, iop->device_size);

	tgtdp++;
    }
    return(tgtdp);
}

uint64_t
extended_copy_calculate_source_blocks(scsi_device_t *sdp, int *src_with_data)
{
    io_params_t *iop;
    int device_index, source_with_data = 0;
    uint64_t source_blocks = 0;

    for (device_index = IO_INDEX_SRC; (device_index < sdp->io_devices); device_index++) {
        iop = &sdp->io_params[device_index];
	if (iop->end_of_data == False) {
	    source_blocks += iop->cdb_blocks;
	    source_with_data++;
	}
    }
    if (src_with_data) {
	*src_with_data = source_with_data;
    }
    return (source_blocks);
}

void
extended_copy_init_iop_segment_info(scsi_device_t *sdp)
{
    io_params_t *iop;
    int device_index;

    for (device_index = IO_INDEX_BASE; (device_index < sdp->io_devices); device_index++) {
        iop = &sdp->io_params[device_index];
	if (iop->end_of_data == False) {
	    iop->segment_blocks = 0;
	    iop->segment_lba = iop->current_lba;
	}
    }
    return;
}

void
extended_copy_adjust_iop_segment_info(scsi_device_t *sdp)
{
    io_params_t *iop;
    int device_index;

    for (device_index = IO_INDEX_BASE; (device_index < sdp->io_devices); device_index++) {
        iop = &sdp->io_params[device_index];
	if (iop->end_of_data == False) {
	    iop->cdb_blocks = iop->segment_blocks;
	}
    }
    return;
}

void *
extended_copy_setup_segments(scsi_device_t *sdp, void *bp)
{
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    io_params_t *siop = &sdp->io_params[IO_INDEX_SRC];
    xcopy_b2b_seg_desc_t *segdp;
    int device_index, segment, segments, sources_with_data;
    uint16_t src_target_index = IO_INDEX_SRC;
    uint32_t blocks_per_segment, blocks_residual, src_segment_blocks;
    uint64_t dst_blocks, src_blocks, max_blocks, segment_blocks;

    /* 
     * Note: The blocks per CDB have already been scaled by segment count! 
     */
    dst_blocks = iop->cdb_blocks;
    src_blocks = extended_copy_calculate_source_blocks(sdp, &sources_with_data);
    max_blocks = min(dst_blocks, src_blocks);

    segments = sdp->segment_count;
    blocks_per_segment = (uint32_t)(max_blocks / segments);
    blocks_residual = (uint32_t)(max_blocks % segments);

    /*
     * Lastly, setup the segment descriptor(s).
     */
    segdp = (xcopy_b2b_seg_desc_t *)bp;
    segment_blocks = 0;
    
    extended_copy_init_iop_segment_info(sdp);

    for (segment = 0, device_index = IO_INDEX_SRC; (segment < segments); ) {
	int segments_this_loop = 0;

	for (device_index = IO_INDEX_SRC; (device_index < sdp->io_devices); device_index++) {
	    siop = &sdp->io_params[device_index];
    
	    src_target_index = device_index;
	    if ( (siop->end_of_data == True) ||
		 ((siop->cdb_blocks - siop->segment_blocks) == 0) ) {
		continue;
	    }
	    segments_this_loop++;
	    segdp->desc_type_code = XCOPY_DESC_TYPE_CODE_BLOCK_TO_BLOCK_SEG_DESC;
	    HtoS(segdp->desc_length, XCOPY_B2B_SEGMENT_LENGTH);
	    HtoS(segdp->src_cscd_desc_idx, src_target_index);
	    HtoS(segdp->dst_cscd_desc_idx, IO_INDEX_DST);
	    if ( (segment + 1) == segments) {
		blocks_per_segment += blocks_residual;
	    }
	    /* Setup the range of blocks. */
	    if (sdp->io_multiple_sources == True) {
		/* Note: Each source may have its' own block range. */
		src_segment_blocks = (uint32_t)min(blocks_per_segment, siop->cdb_blocks);
		src_segment_blocks = (uint32_t)min(src_segment_blocks, dst_blocks);
		HtoS(segdp->block_device_num_of_blocks, src_segment_blocks);
	    } else {
		HtoS(segdp->block_device_num_of_blocks, blocks_per_segment);
	    }
	    HtoS(segdp->src_block_device_lba, siop->segment_lba);
	    HtoS(segdp->dst_block_device_lba, iop->segment_lba);
    
	    if (sdp->io_multiple_sources == True) {
		iop->segment_lba += src_segment_blocks;
		siop->segment_lba += src_segment_blocks;
		iop->segment_blocks += src_segment_blocks;
		siop->segment_blocks += src_segment_blocks;
		dst_blocks -= src_segment_blocks;
		segment_blocks += src_segment_blocks;
	    } else {
		iop->segment_lba += blocks_per_segment;
		siop->segment_lba += blocks_per_segment;
		iop->segment_blocks += blocks_per_segment;
		siop->segment_blocks += blocks_per_segment;
		dst_blocks -= blocks_per_segment;
		segment_blocks += blocks_per_segment;
	    }
	    segdp++;
	    if (segment++ == segments) break;
	    if (dst_blocks == 0) break;
	}
	if (dst_blocks == 0) break;
	if (segments_this_loop == 0) break;
    }
#if 0
    if ( sdp->xDebugFlag && (segment_blocks != max_blocks) ) {
	Printf(sdp, "Partial segment transfer, segment blocks = " LUF ", max blocks = " LUF "\n", 
	       segment_blocks,  max_blocks);
    }
#endif
    extended_copy_adjust_iop_segment_info(sdp);
    return(segdp);
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
extended_copy_complete_io(scsi_device_t *sdp)
{
    uint64_t max_lba = SCSI_MAX_LBA16, max_blocks = XCOPY_MAX_BLOCKS_PER_SEGMENT;
    io_params_t *iop;
    int device_index;
    int status = SUCCESS;

    for (device_index = 0; (device_index < sdp->io_devices); device_index++) {
	iop = &sdp->io_params[device_index];
	if (iop->end_of_data == False) {
	    int device_status;
	    device_status = initialize_io_parameters(sdp, iop, max_lba, max_blocks);
	    if (device_status == END_OF_DATA) {
		if (device_index == IO_INDEX_DST) {
		    status = device_status;
		    break;	/* Don't need to go any further! */
		}
	    } else if (device_status != SUCCESS) {
		status = device_status;
	    }
	}
    }
    if (status == SUCCESS) {
	if (extended_copy_calculate_source_blocks(sdp, NULL) == 0) {
	    iop->end_of_data = True;
	    status = END_OF_DATA;
	}
    }
    return (status);
}

int
extended_copy_verify_data(scsi_device_t *sdp)
{
    io_params_t *iop = &sdp->io_params[IO_INDEX_DST];
    scsi_generic_t *sgp = &iop->sg;
    io_params_t *siop;
    scsi_generic_t *ssgp;
    xcopy_lid1_parameter_list_t *paramp;
    xcopy_id_cscd_ident_desc_t *tgtdp;
    //xcopy_id_cscd_desc_t *tgtdp;
    xcopy_b2b_seg_desc_t *segdp;
    xcopy_cdb_t *cdb = (xcopy_cdb_t *)sgp->cdb;
    unsigned int target_list_length, segment_list_length;
    uint16_t blocks;
    uint64_t src_starting_lba, dst_starting_lba;
    int src_index, dst_index;
    uint32_t xcopy_length;
    int num_targets, num_segments;
    unsigned char *bp = (unsigned char *)sgp->data_buffer;
    int status = SUCCESS;
    int segment;

    xcopy_length = (uint32_t)StoH(cdb->parameter_list_length);
    if (!xcopy_length) {
	ReportDeviceInformation(sdp, sgp);
	Fprintf(sdp, "The xcopy CDB parameter list length is zero, we are broken!\n");
	return (FAILURE);
    }
    if (!sgp->data_buffer || !sgp->data_length) {
	ReportDeviceInformation(sdp, sgp);
	Fprintf(sdp, "The xcopy data buffer or length is NULL/zero length, we are broken!\n");
	return (FAILURE);
    }

    paramp = (xcopy_lid1_parameter_list_t *)bp;
    target_list_length = (uint32_t)StoH(paramp->cscd_desc_list_length);
    if (!target_list_length) {
	ReportDeviceInformation(sdp, sgp);
	Fprintf(sdp, "The xcopy target descriptor is zero length, we are broken!\n");
	return (FAILURE);
    }
    num_targets = (target_list_length / sizeof(*tgtdp));
    tgtdp = (xcopy_id_cscd_ident_desc_t *)(paramp + 1);
    //tgtdp = (xcopy_id_cscd_desc_t *)(paramp + 1);

    segment_list_length = (uint32_t)StoH(paramp->seg_desc_list_length);
    num_segments = (segment_list_length / sizeof(*segdp));
    if (!num_segments) {
	ReportDeviceInformation(sdp, sgp);
	Fprintf(sdp, "The xcopy segment desciptor length is zero, we are broken!\n");
	return (FAILURE);
    }
    segdp = (xcopy_b2b_seg_desc_t *)(tgtdp + num_targets);
    for (segment = 0; segment < num_segments; segment++, segdp++) {
	if (CmdInterruptedFlag) break;
	src_index = (int)StoH(segdp->src_cscd_desc_idx);
	dst_index = (int)StoH(segdp->dst_cscd_desc_idx);
	blocks = (uint16_t)StoH(segdp->block_device_num_of_blocks);
	src_starting_lba = StoH(segdp->src_block_device_lba);
	dst_starting_lba = StoH(segdp->dst_block_device_lba);
	if (sdp->xDebugFlag) {
	    Printf(sdp, "Starting Data Verification (Segment %d):\n", segment);
	    PrintDecHex(sdp, "Number of Blocks", blocks, PNL);
	    PrintLongDecHex(sdp, "Source Block Device LBA", src_starting_lba, DNL);
	    Print(sdp, " (lba's " LUF " - " LUF ")\n", src_starting_lba, (src_starting_lba + blocks - 1));
	    PrintLongDecHex(sdp, "Destination Block Device LBA", dst_starting_lba, DNL);
	    Print(sdp, " (lba's " LUF " - " LUF ")\n", dst_starting_lba, (dst_starting_lba + blocks - 1));
	    Printf(sdp, "\n");
	}
	siop = &sdp->io_params[src_index];
	ssgp = &siop->sg;
	status = extended_copy_read_and_compare(sdp, sgp, iop, ssgp, siop, blocks,
						src_starting_lba, dst_starting_lba);
	if (status != SUCCESS) break;
    }
    return (status);
}

int
extended_copy_read_and_compare(scsi_device_t *sdp,
			       scsi_generic_t *dst_sgp, io_params_t *iop,
			       scsi_generic_t *src_sgp, io_params_t *siop,
			       uint16_t xcopy_blocks,
			       uint64_t src_starting_lba, uint64_t dst_starting_lba)
{
    scsi_generic_t *sgp = Malloc(sdp, sizeof(*sgp));
    scsi_generic_t *ssgp = Malloc(sdp, sizeof(*ssgp));
    uint16_t blocks, current_blocks, read_blocks;
    uint32_t bytes;
    int status = SUCCESS;
    
    if (!sgp || !ssgp) {
	if (sgp) free(sgp);
	if (ssgp) free(ssgp);
	return (FAILURE);
    }
    /*
     * Duplicate the source/destination SCSI generic, to keep sane.
     */ 
    *sgp = *dst_sgp;
    *ssgp = *src_sgp;
    sgp->errlog = True;
    ssgp->errlog = True;

    /*
     * Setup the default number of blocks to read and allocate buffers.
     */ 
    read_blocks = (sdp->scsi_read_length / iop->device_size);
    sgp->data_length = sdp->scsi_read_length;
    ssgp->data_length = sdp->scsi_read_length;
    sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
    ssgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
    if (!sgp->data_buffer || !ssgp->data_buffer) {
	ReportDeviceInformation(sdp, sgp);
	Fprintf(sdp, "Failed to allocate %u bytes for data buffer!\n", sgp->data_length);
	if (sgp->data_buffer) free_palign(sdp, sgp->data_buffer);
	if (ssgp->data_buffer) free_palign(sdp, ssgp->data_buffer);
	free(sgp); free(ssgp);
	return(FAILURE);
    }

    /*
     * Verify the source and destination blocks.
     */ 
    for (blocks = 0, current_blocks = 0; (current_blocks < xcopy_blocks); ) {
	if (CmdInterruptedFlag) break;
	blocks = min(read_blocks, (xcopy_blocks - current_blocks));
	bytes = (blocks * iop->device_size);
	/* Read the source blocks. */
	status = scsiReadData(siop, sdp->scsi_read_type, ssgp, src_starting_lba, blocks, (uint32_t)bytes);
	if (status != SUCCESS) break;
	/* Read the destination blocks. */
	status = scsiReadData(iop, sdp->scsi_read_type, sgp, dst_starting_lba, blocks, (uint32_t)bytes);
	if (status != SUCCESS) break;
	/* Now, verify the source and destination data. */
	status = extended_copy_verify_buffers(sdp, sgp,	ssgp,
					      blocks, src_starting_lba, dst_starting_lba,
					      sgp->data_buffer, ssgp->data_buffer, bytes);
	if (status != SUCCESS) break;
	current_blocks += blocks;
	dst_starting_lba += blocks;
	src_starting_lba += blocks;
    }
    free_palign(sdp, sgp->data_buffer);
    free_palign(sdp, ssgp->data_buffer);
    free(sgp);
    free(ssgp);
    return (status);
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

int
extended_copy_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_DST];
    scsi_generic_t *sgp = &iop->sg;
    xcopy_cdb_t *cdb = (xcopy_cdb_t *)sgp->cdb;
    xcopy_lid1_parameter_list_t *paramp;
    xcopy_id_cscd_ident_desc_t *tgtdp;
    //xcopy_id_cscd_desc_t *tgtdp;
    xcopy_b2b_seg_desc_t *segdp;
    int number_targets, number_segments;
    uint32_t xcopy_length;
    int status = SUCCESS;
    void *bp;

    /* Remember: The first_time flag is false when slices specified! */ 
    if (iop->first_time || !iop->designator_length) {

	status = extended_copy_init_devices(sdp);
	if (status != SUCCESS) return (status);

	/*
	 * Check for and setup single/same LUN information.
	 */ 
	extended_copy_handle_same_lun(sdp);

    } else {
	if (sdp->compare_data) {
	    status = extended_copy_verify_data(sdp);
	    if (status != SUCCESS) return (status);
	}
	status = extended_copy_complete_io(sdp);

	/* Source or destination finished? */
	if (status == END_OF_DATA) {
	    restore_saved_parameters(sdp);
	    iop->end_of_data = True;
	    return (status);
	}
    }

    /*
     * Setup the XCOPY length and CDB parameter list length.
     */ 
    xcopy_length = sizeof(*paramp) + (sizeof(*tgtdp) * sdp->io_devices) +
				     (sizeof(*segdp) * sdp->segment_count);

    if (xcopy_length > iop->data_length) {
	free_palign(sdp, sgp->data_buffer);
	sgp->data_buffer = malloc_palign(sdp, xcopy_length, 0);
	if (sgp->data_buffer == NULL) return(FAILURE);
    }
    sgp->data_length = xcopy_length;	/* Note: Overrides original length! */
    memset(sgp->data_buffer, '\0', sgp->data_length);
    bp = sgp->data_buffer;
    paramp = (xcopy_lid1_parameter_list_t *)bp;
    tgtdp = (xcopy_id_cscd_ident_desc_t *)(paramp + 1);
    //tgtdp = (xcopy_id_cscd_desc_t *)(paramp + 1);

    /*
     * Setup the Parameter List:
     */ 
    paramp->nlid = 1;			/* No list identifier. */
    paramp->nrcr = 1;			/* No report copy results. */

    /*
     * Setup the Target Descriptors:
     */ 
    bp = extended_copy_setup_targets(sdp, tgtdp);
    number_targets = (int)( ((unsigned char *)bp - (unsigned char *)tgtdp) / sizeof(*tgtdp) );

    /*
     * Setup the Segment Descriptors:
     */ 
    segdp = (xcopy_b2b_seg_desc_t *)bp;
    bp = extended_copy_setup_segments(sdp, segdp);
    number_segments = (int)( ((unsigned char *)bp - (unsigned char *)segdp) / sizeof(*segdp) );

    HtoS(paramp->cscd_desc_list_length, (sizeof(*tgtdp) * number_targets));
    HtoS(paramp->seg_desc_list_length, (sizeof(*segdp) * number_segments));
    
    /*
     * Calculate the updated parameter list length and copy to our xcopy CDB.
     */ 
    xcopy_length = (uint32_t)( (unsigned char *)bp - (unsigned char *)sgp->data_buffer );
    sgp->data_length = xcopy_length;
    HtoS(cdb->parameter_list_length, xcopy_length);

    if (sdp->xDebugFlag) {
	DumpXcopyData(sgp);
	Printf(sdp, "\n");
    }
    return (status);
}

/* ================================================================================== */

int
populate_token_init_devices(scsi_device_t *sdp)
{
    io_params_t *iop;
    scsi_generic_t *sgp;
    uint64_t max_lba = SCSI_MAX_LBA16, max_blocks = XCOPY_PT_MAX_BLOCKS;
    int device_index;
    int status = SUCCESS;

    /*
     * Still using NAA to detect the same LUN! (for intra-LUN testing)
     */ 
    for (device_index = 0; (device_index < sdp->io_devices); device_index++) {

	iop = &sdp->io_params[device_index];
	sgp = &iop->sg;
	sgp->restart_flag = True;

	if (!iop->designator_length && (sgp->fd != INVALID_HANDLE_VALUE) ) {
	    status = GetXcopyDesignator(sgp->fd, sgp->dsf, sgp->debug, FALSE, NULL,
					&iop->designator_id, &iop->designator_length,
					&iop->designator_type, sgp->tsp);
	    if (status == FAILURE) {
		ReportDeviceInformation(sdp, sgp);
		Fprintf(sdp, "Unable to retrieve Inquiry Designator ID for device %s!\n",
			sgp->dsf);
		return (status);
	    }
	}
    }

    sdp->rod_token_size = ROD_TOKEN_SIZE;
    if (!sdp->rod_token_data) {
	sdp->rod_token_data = Malloc(sdp, sdp->rod_token_size);
	if (sdp->rod_token_data == NULL) return (FAILURE);
	if (sdp->zero_rod_flag == True) {
	   rod_token_t *token = (rod_token_t *) sdp->rod_token_data;
	   HtoS(token->type, ZERO_ROD_TOKEN_TYPE);
	   HtoS(token->length, ZERO_ROD_TOKEN_LENGTH);
	   sdp->rod_token_valid = True;
	   if (sdp->compare_data == True) {
	       Printf(sdp, "WARNING: Data verification is disabled with Zero ROD Token operations!\n");
	       sdp->compare_data = False;
	   }
	}
    }
    if (sdp->rrti_data_buffer == NULL) {
	/* Note: Large enough for PT/WUT response data plus sense data! */
	sdp->rrti_data_length = (RRTI_PT_DATA_SIZE + RequestSenseDataLength);
	sdp->rrti_data_buffer = malloc_palign(sdp, sdp->rrti_data_length, 0);
	if (sdp->rrti_data_buffer == NULL) return (FAILURE);
    }

    for (device_index = 0; (device_index < sdp->io_devices); device_index++) {

	iop = &sdp->io_params[device_index];
	sgp = &iop->sg;

	/*
	 * Make a unique list identifer, if user did not specify one!
	 */ 
	if (!iop->list_identifier) {
	    iop->list_identifier = ((sdp->thread_number << 24) + (rand() & 0xffffff));
	}

	if ( !iop->device_size || !iop->device_capacity) {
	    status = GetCapacity(sdp, iop);
	    if (status != SUCCESS) return(status);
	}

	if (sgp->data_buffer && (sgp->data_length < iop->device_size) ) {
	    free_palign(sdp, sgp->data_buffer);
	    sgp->data_buffer = NULL;
	}
	sgp->data_dir = scsi_data_write;
	sgp->data_length = iop->device_size;
	iop->data_length = sgp->data_length;
	if (!sgp->data_buffer) {
	    sgp->data_buffer = malloc_palign(sdp, sgp->data_length, 0);
	    if (sgp->data_buffer == NULL) return(FAILURE);
	}
	sdp->iot_pattern = False;
	/* 
	 * Unlike non-token based xcopy, we always transfer the max blocks,
	 * then breakup this max into the ranges specified.
	 */
	if (iop->range_count == 0) {
	    iop->range_count = RangeCountDefault;
	}
	//iop->scale_count = iop->range_count;

	status = initialize_io_parameters(sdp, iop, max_lba, max_blocks);
	if (status != SUCCESS) return (status);
    }
    return (status);
}

int
write_using_token_complete_io(scsi_device_t *sdp)
{
    uint64_t max_lba = SCSI_MAX_LBA16, max_blocks = XCOPY_PT_MAX_BLOCKS;
    io_params_t *iop;
    int device_index;
    int status = SUCCESS;

    for (device_index = 0; (device_index < sdp->io_devices); device_index++) {
	iop = &sdp->io_params[device_index];
	if (iop->end_of_data == False) {
	    int device_status;
	    device_status = initialize_io_parameters(sdp, iop, max_lba, max_blocks);
	    if (device_status == END_OF_DATA) {
		status = device_status;
		break;	/* Don't need to go any further! */
	    } else if (device_status != SUCCESS) {
		status = device_status;
	    }
	}
    }
    return (status);
}

/*
 * populate_token_create() - Populate Token and Receive Result.
 *
 * Sequence:
 * 	Populate Token (PT)
 * 	Receive ROD Token Information (RRTI)
 * 
 * Inputs:
 * 	sdp = The SCSI device pointer.
 * 
 * Return Value:
 * 	Returns SUCCESS / FAILURE
 */ 
int
populate_token_create(scsi_device_t *sdp)
{
    io_params_t *iop = &sdp->io_params[IO_INDEX_SRC];
    scsi_generic_t *sgp = &iop->sg;
    populate_token_cdb_t *cdb = (populate_token_cdb_t *)sgp->cdb;
    populate_token_parameter_list_t *ptp;
    range_descriptor_t *rdp;
    uint32_t data_length;
    uint32_t blocks, blocks_per_range, blocks_left, blocks_resid;
    uint64_t lba;
    int range, range_count;
    int status = SUCCESS;

    /*
     * Setup the data length and CDB parameter list length.
     */ 
    range_count = (int)min((uint64_t)iop->range_count, iop->cdb_blocks);
    data_length = sizeof(*ptp) + (sizeof(*rdp) * range_count);

    if (data_length > iop->data_length) {
	free_palign(sdp, sgp->data_buffer);
	iop->data_length = data_length;
	sgp->data_buffer = malloc_palign(sdp, data_length, 0);
	if (sgp->data_buffer == NULL) return(FAILURE);
    }
    sgp->data_length = data_length;	/* Note: Overrides original length! */
    memset(sgp->data_buffer, '\0', sgp->data_length);

    ptp = (populate_token_parameter_list_t *)sgp->data_buffer;
    HtoS(ptp->data_length, (sgp->data_length - sizeof(ptp->data_length)));
    if (sdp->rod_inactivity_timeout) {
	HtoS(ptp->inactivity_timeout, sdp->rod_inactivity_timeout);
    }
    HtoS(ptp->range_descriptor_list_length, (sizeof(*rdp) * range_count));

    lba = iop->current_lba;
    blocks_per_range = (uint32_t)(iop->cdb_blocks / range_count);
    blocks_resid = (uint32_t)(iop->cdb_blocks - (blocks_per_range * range_count));
    blocks_left = (uint32_t)iop->cdb_blocks;

    /*
     * Populate each range descriptor.
     */ 
    rdp = (range_descriptor_t *)(ptp + 1);
    for (range = 0; (range < range_count); range++, rdp++) {
	blocks = min(blocks_per_range, blocks_left);
	if ( ((range + 1) == range_count) && blocks_resid) {
	    blocks += blocks_resid;
	}
	HtoS(rdp->lba, lba);
	HtoS(rdp->length, blocks);
	lba += blocks;
	blocks_left -= blocks;
    }

    if (sdp->xDebugFlag) {
	DumpPTData(sgp);
	Printf(sdp, "\n");
    }

    do {
	sgp->restart_flag = False;	/* It's Ok to retry PT operations! */
	status = PopulateToken(sgp, iop->list_identifier, sgp->data_buffer, sgp->data_length);
	if (status == SUCCESS) {
	    status = rrti_verify_response(sdp, iop);
	}
	/* Note: Only RTTI returns RESTART status! */
    } while (status == RESTART);
    iop->list_identifier++;
    return (status);
}

int
rrti_verify_response(scsi_device_t *sdp, io_params_t *iop)
{
    scsi_generic_t *csgp = NULL;
    scsi_generic_t *sgp = &iop->sg;
    int status = SUCCESS;

    if (sdp->rrti_data_buffer == NULL) {
	/* Note: Large enough for PT and WUT response data! */
	sdp->rrti_data_length = RRTI_PT_DATA_SIZE;
	sdp->rrti_data_buffer = malloc_palign(sdp, sdp->rrti_data_length, 0);
	if (sdp->rrti_data_buffer == NULL) return (FAILURE);
    }
    
    /* 
     * Copy the original SCSI generic data, since we're issuing an RRTI CDB.
     * Note: Retains original CDB, data buffer and length, etc.
     */
    csgp = Malloc(sdp, sizeof(*sgp));
    if (csgp == NULL) return(FAILURE);
    *csgp = *sgp;
    csgp->restart_flag = True;
    status = ReceiveRodTokenInfo(csgp, iop->list_identifier, sdp->rrti_data_buffer, sdp->rrti_data_length);
    if (status == SUCCESS) {
	status = rrti_process_response(sdp, iop, csgp);
    }
    /*
     * Kinda Micky Mouse, but spt wasn't designed for multiple different CDB's!
     */ 
    sgp->data_resid = csgp->data_resid;
    sgp->data_transferred = csgp->data_transferred;
    sgp->scsi_status = csgp->scsi_status;
    free(csgp);
    return (status);
}

int
rrti_process_response(scsi_device_t *sdp, io_params_t *iop, scsi_generic_t *sgp)
{
    unsigned char *bp;
    int status = SUCCESS;
    uint8_t copy_status;
    uint32_t rod_token_desc_length;
    uint32_t token_desc_offset, token_offset;
    rrti_parameter_data_t *rrtip;
    rod_token_parameter_data_t *rtpd;
    HANDLE fd;
    
    /* Note: Initial goal is to obtain the token, not decode! */
    
    if ( (bp = sgp->data_buffer) == NULL) {
	ReportDeviceInformation(sdp, sgp);
	Fprintf(sdp, "No data buffer, so no data to decode!\n");
	return (FAILURE);
    }

    rrtip = sgp->data_buffer;
    copy_status = rrtip->copy_operation_status;

    if (sdp->xDebugFlag) {
	DumpRRTIData(sgp);
    } else if ( (sgp->debug || sdp->sense_flag) &&
		((copy_status == COPY_STATUS_FAIL) ||
		 (copy_status == COPY_STATUS_TERMINATED)) ) {
	DumpRRTIData(sgp);
    }
    
    switch (rrtip->response_to_service_action) {

        case SCSI_RRTI_PT: /* Populate Token */
	    if (sgp->data_transferred < RRTI_PT_DATA_SIZE) {
		ReportDeviceInformation(sdp, sgp);
		Fprintf(sdp, "Expected at least %u data bytes, received %u!\n", 
			RRTI_PT_DATA_SIZE, sgp->data_transferred);
		return (FAILURE);
	    }
	    sdp->rod_token_valid = False;
	    /* Check Copy Status */
	    /* TODO: Handle residual? */
	    if (copy_status != COPY_STATUS_SUCCESS) {
		ReportDeviceInformation(sdp, sgp);
		Fprintf(sdp, "Populate Token failed with status %u\n", copy_status);
		return (FAILURE);
	    }
	    /* Populate Token not expected to have sense data, but handle! */
	    token_desc_offset = (sizeof(rrti_parameter_data_t) + rrtip->sense_data_length);
	    rtpd = (rod_token_parameter_data_t *)(bp + token_desc_offset);
	    rod_token_desc_length = (uint32_t)stoh(rtpd->rod_token_descriptors_length, 4);
	    rod_token_desc_length -= sizeof(rtpd->restricted_byte_4_5);
	    if (rod_token_desc_length != ROD_TOKEN_SIZE) {
		ReportDeviceInformation(sdp, sgp);
		Fprintf(sdp, "ROD token descriptor size %u not expected, %u bytes expected!\n", 
			rod_token_desc_length, ROD_TOKEN_SIZE);
		return (FAILURE);
	    }
#if 0
	    if (!sdp->rod_token_data) {
		sdp->rod_token_data = Malloc(sdp, sdp->rod_token_size);
		if (sdp->rod_token_data == NULL) return (FAILURE);
	    }
#endif
	    /* Offset to ROD token. */
	    token_offset = (token_desc_offset + sizeof(rod_token_parameter_data_t));
	    memcpy(sdp->rod_token_data, &bp[token_offset], sdp->rod_token_size);
	    sdp->rod_token_valid = True;
	    /* Optionally save the ROD token to a file. */
	    if (sdp->rod_token_file) {
		status = open_file(sdp, sdp->rod_token_file, OPEN_FOR_WRITING, &fd);
		if (status == SUCCESS) {
		    status = write_file(sdp, sdp->rod_token_file, fd, sdp->rod_token_data, sdp->rod_token_size);
		    (void)close_file(sdp, &fd);
		}
	    }
	    break;

        case SCSI_RRTI_WUT:
	    /* Check Copy Status */
	    /* TODO: Handle residual? */
	    if (copy_status != COPY_STATUS_SUCCESS) {
		ReportDeviceInformation(sdp, sgp);
		Fprintf(sdp, "Write Using Token failed with status %u\n", copy_status);
		status = FAILURE;
	    }
	    break;

	default:
	    ReportDeviceInformation(sdp, sgp);
	    Fprintf(sdp, "Unexpected service action %u receieved!\n",
		    rrtip->response_to_service_action);
	    status = FAILURE;
	    break;
    }
    return (status);
}

/*
 * Sequence:
 * 	Populate Token (PT) from Source
 * 	Receive ROD Token Information (RRTI) for PT
 * 	Write Using Token (WUT) to Destination
 * 	RRTI for WUT
 * 	Optionally Read and Verify data.
 */ 
int
write_using_token_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_DST];
    scsi_generic_t *sgp = &iop->sg;
    io_params_t *siop = &sdp->io_params[IO_INDEX_SRC];
    scsi_generic_t *ssgp = &siop->sg;
    write_using_token_cdb_t *cdb;
    unsigned char *bp;
    wut_parameter_list_t *wutp;
    wut_parameter_list_runt_t *runtp;
    range_descriptor_t *rdp;
    uint32_t data_length;
    uint32_t blocks, blocks_per_range, blocks_left, blocks_resid;
    uint64_t lba;
    int range, range_count;
    int status = SUCCESS;
    
    if (iop->first_time) {

	status = populate_token_init_devices(sdp);
	if (status != SUCCESS) return (status);

	if (sdp->zero_rod_flag == False) {
	    /*
	     * Check for and setup single/same LUN information.
	     */ 
	    extended_copy_handle_same_lun(sdp);
	}
    } else {
	/*
	 * When using onerr=continue, and the WUT failed (possibly due to
	 * an abort with short timeout), do *not* try to get its' response!
	 */ 
	if (sdp->status == SUCCESS) {
	    status = rrti_verify_response(sdp, iop);
	    if (status == FAILURE) return(status);
	    if (status == RESTART) goto create_token;
	    if (sdp->compare_data) {
		status = wut_extended_copy_verify_data(sdp);
		if (status != SUCCESS) return (status);
	    }
	}
	iop->list_identifier++;

	status = write_using_token_complete_io(sdp);
	/* Source or destination finished? */
	if (status == END_OF_DATA) {
	    restore_saved_parameters(sdp);
	    iop->end_of_data = True;
	    return (status);
	}
    }

create_token:
    /*
     * Create the next token, unless doing zero ROD token operation.
     */ 
    if (sdp->zero_rod_flag == False) {
	status = populate_token_create(sdp);
	if (status != SUCCESS) return(status);
    }

    /*
     * Setup the data length and CDB parameter list length.
     */ 
    range_count = (int)min((uint64_t)iop->range_count, iop->cdb_blocks);
    data_length = WUT_PARAM_SIZE + (sizeof(*rdp) * range_count);

    if (data_length > iop->data_length) {
	free_palign(sdp, sgp->data_buffer);
	sgp->data_buffer = malloc_palign(sdp, data_length, 0);
	if (sgp->data_buffer == NULL) return(FAILURE);
	iop->data_length = data_length;
    }
    sgp->data_length = data_length;	/* Note: Overrides original length! */
    memset(sgp->data_buffer, '\0', sgp->data_length);

    bp = sgp->data_buffer;
    cdb = (write_using_token_cdb_t *)sgp->cdb;
    HtoS(cdb->list_identifier, iop->list_identifier);
    HtoS(cdb->parameter_list_length, sgp->data_length);

    /* Now, the WUT parameter data! */
    wutp = (wut_parameter_list_t *)bp;
    HtoS(wutp->data_length, (sgp->data_length - sizeof(wutp->data_length)));

    sdp->rod_token_size = ROD_TOKEN_SIZE;

    bp += sizeof(*wutp);
    if (sdp->rod_token_valid) {
	memcpy(bp, sdp->rod_token_data, sdp->rod_token_size);
    } else {
	Printf(sdp, "ROD Token is NOT valid, bug!\n");
	return (FAILURE);
    }

    /* The range descriptor length follows the token ROD. */
    bp += sdp->rod_token_size;
    runtp = (wut_parameter_list_runt_t *)bp;
    HtoS(runtp->range_descriptor_list_length, (sizeof(*rdp) * range_count));

    /* Now the range descriptor(s). */
    bp += sizeof(*runtp);
    rdp = (range_descriptor_t *)bp;
    
    lba = iop->current_lba;
    blocks_per_range = (uint32_t)(iop->cdb_blocks / range_count);
    blocks_resid = (uint32_t)(iop->cdb_blocks - (blocks_per_range * range_count));
    blocks_left = (uint32_t)iop->cdb_blocks;

    /*
     * Populate each range descriptor.
     */ 
    for (range = 0; (range < range_count); range++, rdp++) {
	blocks = min(blocks_per_range, blocks_left);
	if ( ((range + 1) == range_count) && blocks_resid) {
	    blocks += blocks_resid;
	}
	HtoS(rdp->lba, lba);
	HtoS(rdp->length, blocks);
	lba += blocks;
	blocks_left -= blocks;
    }

    if (sdp->xDebugFlag) {
	DumpWUTData(sgp);
	Printf(sdp, "\n");
    }

    return (status);
}

int
wut_extended_copy_verify_data(scsi_device_t *sdp)
{
    io_params_t *iop = &sdp->io_params[IO_INDEX_DST];
    scsi_generic_t *sgp = &iop->sg;
    io_params_t *siop = &sdp->io_params[IO_INDEX_SRC];
    scsi_generic_t *ssgp = &siop->sg;
    uint32_t blocks;
    uint64_t src_starting_lba, dst_starting_lba;
    int status;

    /*
     * The source token can be larger than the destination blocks copied.
     */ 
    blocks = (uint32_t)min(siop->cdb_blocks, iop->cdb_blocks);
    src_starting_lba = siop->current_lba;
    dst_starting_lba = iop->current_lba;
    if (sdp->xDebugFlag) {
	Printf(sdp, "\n");
	Printf(sdp, "Starting Data Verification:\n");
	PrintDecHex(sdp, "Number of Blocks", blocks, PNL);
	PrintLongDecHex(sdp, "Source Block Device LBA", src_starting_lba, DNL);
	Printf(sdp, " (lba's " LUF " - " LUF ")\n", src_starting_lba, (src_starting_lba + blocks - 1));
	PrintLongDecHex(sdp, "Destination Block Device LBA", dst_starting_lba, DNL);
	Print(sdp, " (lba's " LUF " - " LUF ")\n", dst_starting_lba, (dst_starting_lba + blocks - 1));
	Printf(sdp, "\n");
    }

    status = extended_copy_read_and_compare(sdp, sgp, iop, ssgp, siop, blocks,
					    src_starting_lba, dst_starting_lba);
    return (status);
}

/* Older RRTI method, should go away one day! */

int
receive_rod_token_decode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    unsigned char *bp;
    int status = SUCCESS;
    uint32_t rod_token_desc_length;
    uint32_t token_desc_offset, token_offset;
    rrti_parameter_data_t *rrtip;
    rod_token_parameter_data_t *rtpd;
    HANDLE fd;
    
    if ( (bp = sgp->data_buffer) == NULL) {
	ReportDeviceInformation(sdp, sgp);
	Fprintf(sdp, "No data buffer, so no data to decode!\n");
	return (FAILURE);
    }
    rrtip = sgp->data_buffer;
    
    switch (rrtip->response_to_service_action) {

        case SCSI_RRTI_PT: /* Populate Token */
	    if (sgp->data_transferred < RRTI_PT_DATA_SIZE) {
		ReportDeviceInformation(sdp, sgp);
		Fprintf(sdp, "Expected at least %u data bytes, received %u!\n", 
			RRTI_PT_DATA_SIZE, sgp->data_transferred);
		return (FAILURE);
	    }
	    sdp->rod_token_valid = False;
	    /* Check Copy Status */
	    if (rrtip->copy_operation_status != COPY_STATUS_SUCCESS) {
		Fprintf(sdp, "Populate Token failed with status %u\n",
			rrtip->copy_operation_status);
		return (FAILURE);
	    }
	    /* Populate Token not expected to have sense data, but handle! */
	    token_desc_offset = (sizeof(rrti_parameter_data_t) + rrtip->sense_data_length);
	    rtpd = (rod_token_parameter_data_t *)(bp + token_desc_offset);
	    rod_token_desc_length = (uint32_t)stoh(rtpd->rod_token_descriptors_length, 4);
	    rod_token_desc_length -= sizeof(rtpd->restricted_byte_4_5);
	    if (rod_token_desc_length != ROD_TOKEN_SIZE) {
		ReportDeviceInformation(sdp, sgp);
		Fprintf(sdp, "ROD token descriptor size %u not expected, %u bytes expected!\n", 
			rod_token_desc_length, ROD_TOKEN_SIZE);
		return (FAILURE);
	    }
	    sdp->rod_token_size = ROD_TOKEN_SIZE;
	    if (!sdp->rod_token_data) {
		sdp->rod_token_data = Malloc(sdp, sdp->rod_token_size);
		if (sdp->rod_token_data == NULL) return (FAILURE);
	    }
	    /* Offset to ROD token. */
	    token_offset = (token_desc_offset + sizeof(rod_token_parameter_data_t));
	    memcpy(sdp->rod_token_data, &bp[token_offset], sdp->rod_token_size);
	    sdp->rod_token_valid = True;
	    /* Optionally save the ROD token to a file. */
	    if (sdp->rod_token_file) {
		status = open_file(sdp, sdp->rod_token_file, OPEN_FOR_WRITING, &fd);
		if (status == SUCCESS) {
		    status = write_file(sdp, sdp->rod_token_file, fd, sdp->rod_token_data, sdp->rod_token_size);
		    (void)close_file(sdp, &fd);
		}
	    }
	    break;
	default:
	    break;
    }
    return (status);
}

/* ======================================================================== */

int
setup_request_sense(scsi_device_t *sdp, scsi_generic_t *sgp)
{
    uint32_t data_length = 0xFF;

    memset(sgp->cdb, '\0', sizeof(sgp->cdb));
    sgp->cdb[0] = SOPC_REQUEST_SENSE; 
    sgp->data_dir = scsi_data_read;
    sgp->data_length = data_length;
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
request_sense_encode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    int status = SUCCESS;

    /*
     * The first time, we will allocate and initialize the page 
     * header, so the user does not need to specify these bytes. 
     */
    if (iop->first_time && (sgp->data_buffer == NULL) ) {
	size_t data_size;
	void *data_buffer;

	if ( (data_size = sgp->data_length) == 0) {
	    data_size = 0xFF;
	}
	data_buffer = malloc_palign(sdp, data_size, 0);
	if (data_buffer == NULL) {
	    return(FAILURE);
	}
	sgp->data_dir = iop->sop->data_dir;
	sgp->data_length = (unsigned int)data_size;
	sgp->data_buffer = data_buffer;
	iop->first_time = False;
    }

    if (sgp->data_length) {
	sgp->cdb[4] = sgp->data_length;
    }
    return (status);
}

int
request_sense_decode(void *arg)
{
    scsi_device_t *sdp = arg;
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    scsi_sense_t *ssp = sgp->data_buffer;
    int status = SUCCESS;

    DumpSenseData(sgp, False, ssp);
    return(status);
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
    {	SOPC_INQUIRY,		    0x89, ALL_DEVICE_TYPES, "Inquiry - ATA Information"		},
    {	SOPC_INQUIRY,		    0x8D, ALL_DEVICE_TYPES, "Inquiry - Power Consumption"	},
    {	SOPC_INQUIRY,		    0x8F, ALL_DEVICE_TYPES, "Inquiry - Third-party Copy"	},
    {	SOPC_INQUIRY,		    0xB0, ALL_RANDOM_DEVICES, "Inquiry - Block Limits"		},
    {	SOPC_INQUIRY,		    0xB1, ALL_RANDOM_DEVICES, "Inquiry - Block Device Characteristics" },
    {	SOPC_INQUIRY,		    0xB2, ALL_RANDOM_DEVICES, "Inquiry - Logical Block Provisioning" },
    {	SOPC_INQUIRY,		    0xB3, ALL_RANDOM_DEVICES, "Inquiry - Referrals"		},
    {	SOPC_INQUIRY,		    0xB4, ALL_RANDOM_DEVICES, "Inquiry - Supported Block Lengths And Protection Types" },
    {	SOPC_LOG_SELECT,	    0x00, ALL_DEVICE_TYPES, "Log Select",
	scsi_data_write, log_select_encode						},
    {	SOPC_LOG_SENSE,		    0x00, ALL_DEVICE_TYPES, "Log Sense",
	scsi_data_read, log_sense_encode, log_sense_decode				},
    {	SOPC_MODE_SELECT_6,	    0x00, ALL_DEVICE_TYPES, "Mode Select(6)"		},
    {	SOPC_MODE_SELECT_10,	    0x00, ALL_DEVICE_TYPES, "Mode Select(10)"		},
    {	SOPC_MODE_SENSE_6,	    0x00, ALL_DEVICE_TYPES, "Mode Sense(6)"		},
    {	SOPC_MODE_SENSE_10,	    0x00, ALL_DEVICE_TYPES, "Mode Sense(10)"		},
    {	SOPC_READ_BUFFER,	    0x00, ALL_DEVICE_TYPES, "Read Buffer"		},
    {	SOPC_RECEIVE_DIAGNOSTIC,    0x00, ALL_DEVICE_TYPES, "Receive Diagnostic",
	scsi_data_read, receive_diagnostic_encode, receive_diagnostic_decode		},
    {	SOPC_REQUEST_SENSE,	    0x00, ALL_DEVICE_TYPES, "Request Sense",
	scsi_data_read, request_sense_encode, request_sense_decode			},
    {	SOPC_SEND_DIAGNOSTIC,	    0x00, ALL_DEVICE_TYPES, "Send Diagnostic",
	scsi_data_write, send_diagnostic_encode, send_diagnostic_decode			},
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
    {	SOPC_REPORT_LUNS,	    0x00, ALL_DEVICE_TYPES, "Report Luns",
	scsi_data_read, report_luns_encode, report_luns_decode				},
    {	SOPC_MAINTENANCE_IN,	    0x00, ALL_DEVICE_TYPES, "Maintenance In"		},
    {	SOPC_MAINTENANCE_IN,	    0x05, ALL_DEVICE_TYPES, "Maintenance In - Report Device Identifier" },
    {	SOPC_MAINTENANCE_IN,	    0x06, ALL_DEVICE_TYPES, "Maintenance In - Report States" },
    {	SOPC_MAINTENANCE_IN,	    0x08, ALL_DEVICE_TYPES, "Maintenance In - Report Supported Configuration Method" },
    {	SOPC_MAINTENANCE_IN,	    0x09, ALL_DEVICE_TYPES, "Maintenance In - Report Unconfigured Capacity" },
    {	SOPC_MAINTENANCE_IN,	    0x0A, ALL_DEVICE_TYPES, "Maintenance In - Report Target Port Groups",
	scsi_data_read, rtpgs_encode, rtpgs_decode					},
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
    {	SOPC_READ_CAPACITY,	    0x00, ALL_RANDOM_DEVICES, "Read Capacity",
	scsi_data_read, read_capacity10_encode, read_capacity10_decode			},
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
    {	SOPC_UNMAP,		    0x00, ALL_RANDOM_DEVICES, "Unmap",
	scsi_data_write, random_unmap_encode, NULL, UNMAP_MAX_PER_RANGE			},
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
    {	SOPC_EXTENDED_COPY,	    SCSI_XCOPY_EXTENDED_COPY_LID1,
					  ALL_RANDOM_DEVICES, "Extended Copy",
	scsi_data_write, extended_copy_encode, NULL, XCOPY_MAX_BLOCKS_PER_SEGMENT	},
    {	SOPC_EXTENDED_COPY,	    SCSI_XCOPY_POPULATE_TOKEN,
					  ALL_RANDOM_DEVICES, "XCOPY - Populate Token",
	scsi_data_write, NULL, NULL, XCOPY_PT_MAX_BLOCKS				},
    {	SOPC_EXTENDED_COPY,	    SCSI_XCOPY_WRITE_USING_TOKEN,
					  ALL_RANDOM_DEVICES, "XCOPY - Write Using Token",
	scsi_data_write, write_using_token_encode, NULL, XCOPY_PT_MAX_BLOCKS		},
    {	SOPC_RECEIVE_COPY_RESULTS,  RECEIVE_COPY_RESULTS_SVACT_OPERATING_PARAMETERS,
					  ALL_RANDOM_DEVICES, "Receive Copy Results",
	scsi_data_read, NULL, NULL							},
    {	SOPC_RECEIVE_ROD_TOKEN_INFO,RECEIVE_ROD_TOKEN_INFORMATION,
					  ALL_RANDOM_DEVICES, "Receive ROD Token Information",
	scsi_data_read, NULL, receive_rod_token_decode					},
    {	SOPC_READ_16,		    0x00, ALL_RANDOM_DEVICES, "Read(16)",
	scsi_data_read, random_rw16_encode, NULL					},
    {	SOPC_WRITE_16,		    0x00, ALL_RANDOM_DEVICES, "Write(16)",
	scsi_data_write, random_rw16_encode, NULL					},
    {	SOPC_WRITE_AND_VERIFY_16,   0x00, ALL_RANDOM_DEVICES, "Write and Verify(16)",
	scsi_data_write, random_rw16_encode, NULL					},
    {	SOPC_VERIFY_16,		    0x00, ALL_RANDOM_DEVICES, "Verify(16)",
	scsi_data_none, random_rw16_encode, NULL, VERIFY_DATA_MAX_BLOCKS16		},
    {	SOPC_SYNCHRONIZE_CACHE_16,  0x00, ALL_RANDOM_DEVICES, "Synchronize Cache(16)"	},
    {	SOPC_WRITE_SAME_16,	    0x00, ALL_RANDOM_DEVICES, "Write Same(16)",
	scsi_data_write, random_rw16_encode, NULL, WRITE_SAME_MAX_BLOCKS16		},
    {	SOPC_SERVICE_ACTION_IN_16,  0x00, ALL_RANDOM_DEVICES, "Service Action In(16)",
	scsi_data_read									},
    {	SOPC_SERVICE_ACTION_IN_16,  0x10, ALL_RANDOM_DEVICES, "Read Capacity(16)",
	scsi_data_read, read_capacity16_encode, read_capacity16_decode			},
    {	SOPC_SERVICE_ACTION_IN_16,  0x12, ALL_RANDOM_DEVICES, "Get LBA Status(16)",
        scsi_data_read, get_lba_status_encode, get_lba_status_decode			},
    {	SOPC_COMPARE_AND_WRITE,	    0x00, ALL_RANDOM_DEVICES, "Compare and Write(16)",
	scsi_data_write, random_caw16_encode, NULL, CAW_DEFAULT_BLOCKS			},
    /*
     * ATA Operation Codes:
     */
    {	SOPC_ATA_PASS_THROUGH_12,    0x00, ALL_RANDOM_DEVICES, "ATA Pass-Through(12)"	},
    {	SOPC_ATA_PASS_THROUGH_16,    0x00, ALL_RANDOM_DEVICES, "ATA Pass-Through(16)"	},
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
ShowScsiOpcodes(scsi_device_t *sdp, char *opstr)
{
    scsi_opcode_t *sop = scsiOpcodeTable;
    int entrys;

    Print(sdp, "  Opcode  Subcode  Direction  Encode  Decode   Default   Opcode Name\n");
    Print(sdp, "  ------  -------  ---------  ------  ------  ---------  -----------\n");

    for (entrys = 0, sop = scsiOpcodeTable; entrys < scsi_opcodeEntries; sop++, entrys++) {
	char *data_dir;
	if (opstr && NESC(sop->opname, opstr)) {
	    continue;
	}
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
