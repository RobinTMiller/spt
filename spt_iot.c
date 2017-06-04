/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2014			    *
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
 * Module:	spt_iot.c
 * Author:	Robin T. Miller
 * Date:	June 16th, 2014
 *
 * Description:
 *	IOT related functions. Note: Taken from dtiot.c
 *
 * Modification History:
 */
#include "spt.h"
#include <ctype.h>

#define SPT_FIELD_WIDTH "%30.30s: "

/*
 * Forward Reference:
 */
hbool_t	is_iot_data(scsi_device_t *sdp, uint8_t *rptr, size_t rsize, int rprefix_size, int *iot_offset, iotlba_t *rlbn);

uint32_t
init_iotdata(
	scsi_device_t	*sdp,
	io_params_t	*iop, 
    	void		*buffer,
	uint32_t	count,
	uint32_t	lba,
	uint32_t	iot_seed)
{
    register int i, wperb;
    register uint32_t *bptr = buffer;
    register uint32_t lba_pattern;
    register int32_t bytes = count;

    wperb = (iop->device_size / sizeof(lba));

    /*
     * Initialize the buffer with the IOT test pattern.
     */
    while (bytes > 0) {
        lba_pattern = lba++;
	if (bytes < (int32_t)iop->device_size) {
	    wperb = (bytes / sizeof(lba));
	}
        for (i = 0; (i < wperb); i++) {
#if _BIG_ENDIAN_
            init_swapped(sdp, bptr++, sizeof(lba), lba_pattern);
#else /* !_BIG_ENDIAN_ */
            *bptr++ = lba_pattern;
#endif /* _BIG_ENDIAN_ */
            lba_pattern += iot_seed;
        }
	bytes -= iop->device_size;
    }
    return(lba);
}

void
process_iot_data(scsi_device_t *sdp, io_params_t *iop, uint8_t *pbuffer, uint8_t *vbuffer, size_t bcount)
{
    int status;

    status = AcquirePrintLock(sdp);

    analyze_iot_data(sdp, iop, pbuffer, vbuffer, bcount);
    display_iot_data(sdp, iop, pbuffer, vbuffer, bcount);

    if (status == SUCCESS) {
        status = ReleasePrintLock(sdp);
    }
    return;
}

void
report_bad_sequence(scsi_device_t *sdp, io_params_t *iop, int start, int length, Offset_t offset)
{
    Offset_t pos = (offset + ((start-1) * iop->device_size));
    Fprintf(sdp, SPT_FIELD_WIDTH "%d\n",
	  "Start of corrupted blocks", start);
    Fprintf(sdp, SPT_FIELD_WIDTH "%d (%d bytes)\n",
	  "Length of corrupted blocks", length, (length * iop->device_size));
    Fprintf(sdp, SPT_FIELD_WIDTH LUF " (lba %u)\n",
	   "Corrupted blocks file offset", pos, (uint32_t)(pos / iop->device_size));
    return;
}

void
report_good_sequence(scsi_device_t *sdp, io_params_t *iop, int start, int length, Offset_t offset)
{
    Offset_t pos = (offset + ((start-1) * iop->device_size));
    Fprintf(sdp, SPT_FIELD_WIDTH "%d\n",
	    "Start of good blocks", start);
    Fprintf(sdp, SPT_FIELD_WIDTH "%d (%d bytes)\n",
	    "Length of good blocks", length, (length * iop->device_size));
    Fprintf(sdp, SPT_FIELD_WIDTH LUF " (lba %u)\n",
	    "Good blocks file offset", pos, (uint32_t)(pos / iop->device_size));
    return;
}

void
analyze_iot_data(scsi_device_t *sdp, io_params_t *iop, uint8_t *pbuffer, uint8_t *vbuffer, size_t bcount)
{
    register uint8_t *pptr = pbuffer;
    register uint8_t *vptr = vbuffer;
    register size_t count = bcount;
    int bad_blocks = 0, good_blocks = 0;
    int bad_start = 0,  good_start = 0;
    int zero_blocks = 0;
    uint32_t block = 1;
    int blocks = (int)(count / iop->device_size);
    Offset_t record_offset = (iop->current_lba * iop->device_size);

#if 0
    /*
     * TODO: Save starting offset in the sdp!
     */
    if (sdp->aio_flag || sdp->mmap_flag) {
	record_offset = sdp->offset - bcount;
    } else {
	record_offset = get_position(sdp) - bcount;
    }
#endif /* 0 */

    
    Fprintf(sdp, "\n");
    Fprintf(sdp, "Analyzing IOT Record Data: (Note: Block #'s are relative to start of record!)\n");
    Fprintf(sdp, "\n");
    Fprintf(sdp, SPT_FIELD_WIDTH "%d\n",
	    "IOT block size", iop->device_size);
    Fprintf(sdp, SPT_FIELD_WIDTH "%d (%u bytes)\n",
	    "Total number of blocks", blocks, count);
    Fprintf(sdp, SPT_FIELD_WIDTH "0x%08x (pass %u)\n",
	    "Current IOT seed value", sdp->iot_seed_per_pass, (sdp->iot_seed_per_pass / IOT_SEED));
    if (sdp->iot_seed_per_pass != IOT_SEED) {
        Fprintf(sdp, SPT_FIELD_WIDTH "0x%08x (pass %u)\n",
		"Previous IOT seed value", (sdp->iot_seed_per_pass - IOT_SEED),
		((sdp->iot_seed_per_pass - IOT_SEED) / IOT_SEED));
    }

    /*
     * Compare one lbdata sized block at a time.
     */
    while (blocks) {
	int result = 0;
	if (sdp->timestamp_flag) {
	    if (sdp->fprefix_size) {
		result = memcmp(pptr, vptr, sdp->fprefix_size);
	    }
	    if (result == 0) {
		int doff = (sdp->fprefix_size + sizeof(iotlba_t));
		result = memcmp( (pptr + doff), (vptr + doff),
				 (iop->device_size - doff) );
	    }
	} else {
	    result = memcmp(pptr, vptr, iop->device_size);
	}
	if (result == 0) {
	    good_blocks++;
	    if (good_start == 0) {
		good_start = block;
	    }
	    if (bad_start) {
		report_bad_sequence(sdp, iop, bad_start, (block - bad_start), record_offset);
		bad_start = 0;
	    }
	} else {
	    uint32_t i;
	    bad_blocks++;
	    if (bad_start == 0) {
		bad_start = block;
	    }
	    for (i = 0; (i < iop->device_size); i++) {
		if (vptr[i] != '\0') {
		    break;
		}
	    }
	    if (i == iop->device_size) {
		zero_blocks++;
	    }
	    if (good_start) {
		report_good_sequence(sdp, iop, good_start, (block - good_start), record_offset);
		good_start = 0;
	    }
	}
	block++; blocks--;
	pptr += iop->device_size;
	vptr += iop->device_size;
    }
    if (bad_start) {
	report_bad_sequence(sdp, iop, bad_start, (block - bad_start), record_offset);
	bad_start = 0;
    }
    if (good_start) {
	report_good_sequence(sdp, iop, good_start, (block - good_start), record_offset);
	good_start = 0;
    }
    Fprintf(sdp, SPT_FIELD_WIDTH "%d\n",
	    "Number of corrupted blocks", bad_blocks);
    Fprintf(sdp, SPT_FIELD_WIDTH "%d\n",
	    "Number of good blocks found", good_blocks);
    Fprintf(sdp, SPT_FIELD_WIDTH "%d\n",
	    "Number of zero blocks found", zero_blocks);
    return;
}

#define BITS_PER_BYTE		8
#define BYTES_PER_LINE		16
#define BYTE_EXPECTED_WIDTH	55
#define WORD_EXPECTED_WIDTH	43

/* Loop through received data to determine if it contains any IOT data pattern. */
hbool_t
is_iot_data(scsi_device_t *sdp, uint8_t *rptr, size_t rsize, int rprefix_size, int *iot_offset, iotlba_t *rlbn)
{
    uint32_t received_word0, received_word1, received_iot_seed;
    int doff = (rprefix_size + sizeof(iotlba_t));
    int seed_word = 1;

    /* Format: <optional prefix><lbn or timestamp><lbn + IOT_SEED>...*/
    /* Loop through received data looking for a valid IOT seed. */
    for (; ((doff + sizeof(iotlba_t)) < rsize); doff += sizeof(iotlba_t) ) {
	received_word0 = get_lbn( (rptr + doff) );
	received_word1 = get_lbn( (rptr + doff + sizeof(iotlba_t)) );
	received_iot_seed = (received_word1 - received_word0);
	if ( (received_iot_seed && received_word0 && received_word1) &&
	     (received_iot_seed % IOT_SEED) == 0) {
	    /* Assume matches IOT data. */
	    if (iot_offset) *iot_offset = doff;
	    if (rlbn) *rlbn = (received_word0 - (received_iot_seed * seed_word));
	    return(True);
	}
	seed_word++;
    }
    return(False);
}

void
display_iot_block(scsi_device_t *sdp, io_params_t *iop, int block, Offset_t block_offset, uint8_t *pptr, uint8_t *vptr, size_t bsize)
{
    char str[LARGE_BUFFER_SIZE];
    char astr[PATH_BUFFER_SIZE];	/* Prefix strings can be rather long! */
    register char *sbp = str;
    register char *abp = astr;
    uint8_t *tend = NULL, *tptr = NULL;
    int aprefix_size = 0;
    int rprefix_size = 0;
    int raprefix_size = 0;
    int boff = 0, rindex, i;
    int match, width;
    register int bytes_per_line;
    int expected_width;
    uint32_t expected_lbn, received_lbn;
    size_t limit = min(bsize,sdp->dump_limit);
    uint32_t received_word0, received_word1, received_iot_seed;

    Fprintf(sdp, "\n");
    Fprintf(sdp, SPT_FIELD_WIDTH "%u\n", "Record block", block);
    Fprintf(sdp, SPT_FIELD_WIDTH LUF " (lba %u)\n", "Record block offset",
	    block_offset, (uint32_t)(block_offset / iop->device_size));

    if (sdp->fprefix_size) {
	int result = 0;
	unsigned char byte;
	aprefix_size = (int)strlen(sdp->fprefix_string);
	rprefix_size = sdp->fprefix_size;
	/* 
	 * Note: The formated prefix size includes the terminating NULL.
	 * and is also rounded up to the sizeof(unsigned int). Therefore,
	 * we are comparing the ASCII prefix string + NULL bytes!
	 */
	result = memcmp(pptr, vptr, (size_t)sdp->fprefix_size);
        Fprintf(sdp, SPT_FIELD_WIDTH "%s\n", "Prefix string compare",
		(result == 0) ? "correct" : "incorrect");
	/* If the prefix is incorrect, display prefix information. */
	if (result != 0) {
	    int printable = 0;
	    abp = astr;
	    rindex = 0;
	    /* Note: IOT data can look printable, so check start of block. */
	    if ( is_iot_data(sdp, vptr, (sizeof(iotlba_t) * 3), rindex, NULL, NULL) == False ) {
		/* Ensure the received prefix string is printable. */
		for (rindex = 0; (rindex < aprefix_size); rindex++) {
		    byte = vptr[rindex];
		    if (byte == '\0') break; /* Short prefix string. */
		    *abp++ = isprint((int)byte) ? (char)byte : ' ';
		    if ( isprint((int)byte) ) {
			printable++;
		    }
		}
	    }
	    if (rindex == 0) {
		/* We did not find a prefix string, started woth zero byte. */
		raprefix_size = rprefix_size = rindex;
	    } else if (rindex < aprefix_size) {
		/* The prefix string is shorter than the expected! */
		raprefix_size = rprefix_size = rindex;
		rprefix_size++; /* Include the terminating NULL. */
		rprefix_size = roundup(rprefix_size, sizeof(uint32_t));
	    } else if ( (rindex != aprefix_size) || (vptr[rindex] != '\0') ) {
		/* The prefix string is longer than the expected! */
		for (; (rindex < (int)bsize); rindex++) {
		    byte = vptr[rindex];
		    if (byte == '\0') break; /* End of the prefix. */
		    *abp++ = isprint((int)byte) ? (char)byte : ' ';
		    if ( isprint((int)byte) ) {
			printable++;
		    }
		}
		/* Note: If prefix takes up the entire block, we need more work! */
		if (rindex < (int)bsize) {
		    raprefix_size = rprefix_size = rindex;
		    rprefix_size++; /* Include the terminating NULL. */
		    rprefix_size = roundup(rprefix_size, sizeof(uint32_t));
		} else { /* Assume this is NOT a prefix. */
		    raprefix_size = rprefix_size = 0;
		    printable = 0;
		}
	    } else { /* Expected and received are the same length! */
		raprefix_size = rindex;
	    }
	    Fprintf(sdp, SPT_FIELD_WIDTH "%s\n", "Expected prefix string", pptr);
	    Fprintf(sdp, SPT_FIELD_WIDTH, "Received prefix string: ");
	    if (printable) {
		*abp = '\0';
                Fprint(sdp, "%s\n", astr);
	    } else {
                Fprint(sdp, "<non-printable string>\n");
	    }
	    if (rprefix_size != sdp->fprefix_size) {
		Fprintf(sdp, SPT_FIELD_WIDTH "%d\n", "Expected prefix length", sdp->fprefix_size);
		Fprintf(sdp, SPT_FIELD_WIDTH "%d\n", "Received prefix length", rprefix_size);
	    } else if (raprefix_size != aprefix_size) {
		  Fprintf(sdp, SPT_FIELD_WIDTH "%d\n", "Expected ASCII prefix length", aprefix_size);
		  Fprintf(sdp, SPT_FIELD_WIDTH "%d\n", "Received ASCII prefix length", raprefix_size);
	    }
	}
    }

    /* Note: The pattern buffer *always* has the correct expected block number. */
    expected_lbn = get_lbn((pptr + sdp->fprefix_size));
    received_word0 = get_lbn( (vptr + rprefix_size + (sizeof(received_lbn) * 1)) );
    received_word1 = get_lbn( (vptr + rprefix_size + (sizeof(received_lbn) * 2)) );

    if (sdp->timestamp_flag) {
	time_t seconds;
	tptr = (vptr + rprefix_size);
	tend = (tptr + sizeof(iotlba_t));
	seconds = (time_t)stoh((vptr + rprefix_size), sizeof(iotlba_t));
        Fprintf(sdp, SPT_FIELD_WIDTH "0x%08x\n", "Block timestamp value",
		get_lbn( (vptr + sdp->fprefix_size) ));
	/* Check for invalid time values, with upper limit including fudge factor! */
	if ( (seconds == (time_t)0) || (seconds > (time((time_t)0) + 300)) ) {
	    Fprintf(sdp, SPT_FIELD_WIDTH "%s\n",
		    "Data block written on", "<invalid time value>");
	} else {
	    char    time_buffer[TIME_BUFFER_SIZE];
	    Fprintf(sdp, SPT_FIELD_WIDTH "%s\n", "Data block written on",
	    	    os_ctime(&seconds, time_buffer, sizeof(time_buffer)));
	}
	/*
	 * Since timestamp overwrites the lba, calculate the seed and lba.
	 */
	received_iot_seed = (received_word1 - received_word0);
	received_lbn = (received_word0 - received_iot_seed);
    } else {
	received_lbn = get_lbn((vptr + rprefix_size));
	received_iot_seed = (received_word0 - received_lbn);
    }
    Fprintf(sdp, SPT_FIELD_WIDTH "%u (0x%08x)\n", "Expected block number", expected_lbn, expected_lbn);
    Fprintf(sdp, SPT_FIELD_WIDTH "%u (0x%08x)\n", "Received block number", received_lbn, received_lbn);

    /*
     * Analyze the IOT data:
     * Steps:
     *  - Detect stale IOT data (most common case, past or future)
     * 	- Detect wrong IOT data (valid IOT data, but wrong block)
     * 	- Detect IOT data/seed anywhere within the data block.
     */
    if ( ((expected_lbn != received_lbn) ||
	  (sdp->iot_seed_per_pass != received_iot_seed)) ) {
	/* Does this look like a valid IOT seed? */
	if ( (received_iot_seed && received_word0 && received_word1) &&
	     (received_word1 == (received_word0 + received_iot_seed)) &&
	     ((received_iot_seed % IOT_SEED) == 0) ) {
	    /* Ok, this looks like valid IOT data. */
            Fprintf(sdp, SPT_FIELD_WIDTH "%u\n",
		    "Data written during pass", (received_iot_seed / IOT_SEED));
            Fprintf(sdp, SPT_FIELD_WIDTH "0x%08x (%s)\n",
		    "Received data is from seed", received_iot_seed,
		    (expected_lbn == received_lbn) ? "stale data" : "wrong data");
	} else { /* Let's search for the IOT seed! */
	    /* Format: <optional prefix><lbn or timestamp><lbn + IOT_SEED>...*/
	    int doff = (rprefix_size + sizeof(expected_lbn));
	    int seed_word = 1;
	    /* Loop through data looking for a valid IOT seed. */
	    for (; ((doff + sizeof(expected_lbn)) < bsize); doff += sizeof(expected_lbn) ) {
		received_word0 = get_lbn( (vptr + doff) );
		received_word1 = get_lbn( (vptr + doff + sizeof(expected_lbn)) );
		received_iot_seed = (received_word1 - received_word0);
		if ( (received_iot_seed && received_word0 && received_word1) &&
		     (received_iot_seed % IOT_SEED) == 0) {
                    Fprintf(sdp, SPT_FIELD_WIDTH "%u (0x%x) (word %u, zero based)\n",
			    "Seed detected at offset", doff, doff, (doff / sizeof(expected_lbn)));
                    Fprintf(sdp, SPT_FIELD_WIDTH "%u\n",
			    "Data written during pass", (received_iot_seed / IOT_SEED));
		    received_lbn = (received_word0 - (received_iot_seed * seed_word));
                    Fprintf(sdp, SPT_FIELD_WIDTH "%u (0x%08x)\n", "Calculated block number",
			    received_lbn, received_lbn);
		    /* Since part of the block is corrupt, always report wrong data. */
                    Fprintf(sdp, SPT_FIELD_WIDTH "0x%08x (%s)\n",
			    "Received data is from seed", received_iot_seed, "wrong data");
			    //(expected_lbn == received_lbn) ? "stale data" : "wrong data");
		    break; /* Stop upon 1st valid IOT data. */
		}
		seed_word++;
#if 0
		Fprint(sdp, SPT_FIELD_WIDTH "0x%08x\n", "Seed word 0", received_word0);
		Fprint(sdp, SPT_FIELD_WIDTH "0x%08x\n", "Seed word 1", received_word1);
		Fprint(sdp, SPT_FIELD_WIDTH "0x%08x\n", "Difference", (received_word1 - received_word0));
		Fprint(sdp, "offset = %d, seed word = %d\n", doff, seed_word);
#endif
	    }
	}
    }
    Fprintf(sdp, "\n");
    width = sprintf(sbp, "Byte Expected: address %p", pptr);
    sbp += width;
    if (sdp->data_format == BYTE_FMT) {
	expected_width = BYTE_EXPECTED_WIDTH;
    } else {
	expected_width = WORD_EXPECTED_WIDTH;
    }
    while (width++ < expected_width) {
	sbp += sprintf(sbp, " ");
    }
    sbp += sprintf(sbp, "Received: address %p\n", vptr); 
    Fprintf(sdp, "%s", str);
    while (limit > 0) {
	sbp = str;
	bytes_per_line = (int)min(bsize, BYTES_PER_LINE);
	if (sdp->boff_format == DEC_FMT) {
	    sbp += sprintf(sbp, "%04d ", boff);
	} else {
	    sbp += sprintf(sbp, "%04x ", boff);
	}
	abp = NULL;
	if (aprefix_size && (boff < aprefix_size)) {
	    abp = astr;
	    abp += sprintf(abp, "     ");
	}
	/* Handle timestamp within this byte range. */
	if (tptr && (vptr < tend)) {
	    uint8_t *pp = pptr;
	    uint8_t *vp = vptr;
	    match = 0;
	    for (i = 0; (i < bytes_per_line); i++, pp++, vp++) {
		if ( (vp >= tptr) && (vp < tend) ) {
		    continue; /* Skip the timestamp. */
		}
		if ( *pp != *vp ) {
		    match = 1;
		    break;
		}
	    }
	} else {
	    match = memcmp(pptr, vptr, bytes_per_line);
	}
	if (sdp->data_format == BYTE_FMT) {
	    unsigned char byte;
	    for (i = 0; (i < bytes_per_line); i++) {
		byte = pptr[i];
		sbp += sprintf(sbp, "%02x ", byte);
		if (abp) abp += sprintf(abp, " %c ", isprint((int)byte) ? byte : ' ');
	    }
	    sbp += sprintf(sbp, "%c ", (match == 0) ? ' ' : '*');
	    if (abp) abp += sprintf(abp, "  ");
	    for (i = 0; (i < bytes_per_line); i++) {
		byte = vptr[i];
		sbp += sprintf(sbp, "%02x ", byte);
		if (abp) abp += sprintf(abp, " %c ", isprint((int)byte) ? byte : ' ');
	    }
	} else {
	    uint32_t data;
	    for (i = 0; (i < bytes_per_line); i += sizeof(data)) {
		data = get_lbn((pptr + i));
		sbp += sprintf(sbp, "%08x ", data);
		if (abp) {
		    unsigned char byte;
		    int x = sizeof(data);
		    while (x--) {
			byte = (data >> (x * BITS_PER_BYTE));
			abp += sprintf(abp, " %c", isprint((int)byte) ? byte : ' ');
		    }
		    abp += sprintf(abp, " ");
		}
	    }
	    sbp += sprintf(sbp, "%c ", (match == 0) ? ' ' : '*');
	    if (abp) abp += sprintf(abp, "  ");
	    for (i = 0; (i < bytes_per_line); i += sizeof(data)) {
		data = get_lbn((vptr + i));
		sbp += sprintf(sbp, "%08x ", data);
		if (abp) {
		    unsigned char byte;
		    int x = sizeof(data);
		    while (x--) {
			byte = (data >> (x * BITS_PER_BYTE));
			abp += sprintf(abp, " %c", isprint((int)byte) ? byte : ' ');
		    }
		    abp += sprintf(abp, " ");
		}
	    }
	}
	sbp += sprintf(sbp, "\n");
        Fprintf(sdp, str);
	if (abp) {
	    abp += sprintf(abp, "\n");
            Fprintf(sdp, astr);
	}
	limit -= bytes_per_line;
	boff += bytes_per_line;
	pptr += bytes_per_line;
	vptr += bytes_per_line;
    }
    return;
}

void
display_iot_data(scsi_device_t *sdp, io_params_t *iop, uint8_t *pbuffer, uint8_t *vbuffer, size_t bcount)
{
    register uint8_t *pptr = pbuffer;
    register uint8_t *vptr = vbuffer;
    register size_t count = bcount;
    int block = 0, blocks = (int)(count / iop->device_size);
    unsigned int bad_blocks = 0;
    Offset_t block_offset, record_offset;

    block_offset = record_offset = (iop->current_lba * iop->device_size);
#if 0
    if (sdp->aio_flag || sdp->mmap_flag) {
	block_offset = record_offset = sdp->offset - bcount;
    } else {
	block_offset = record_offset = get_position(sdp) - bcount;
    }
#endif /* 0 */

    Fprintf(sdp, "\n");
    Fprintf(sdp, SPT_FIELD_WIDTH LUF "\n", "File offset", record_offset);
    Fprintf(sdp, SPT_FIELD_WIDTH "%u (%#x)\n", "Transfer count", bcount, bcount);
    Fprintf(sdp, SPT_FIELD_WIDTH "%p\n", "Read buffer address", vptr);
    Fprintf(sdp, SPT_FIELD_WIDTH "%p\n", "Pattern base address", pptr);
    if (sdp->fprefix_size) {
	int aprefix_size = (int)strlen(sdp->fprefix_string);
        Fprintf(sdp, SPT_FIELD_WIDTH "%s\n", "Prefix string", sdp->fprefix_string);
        Fprintf(sdp, SPT_FIELD_WIDTH "%d bytes (0x%x) plus %d zero bytes\n", "Prefix length",
		sdp->fprefix_size, sdp->fprefix_size, sdp->fprefix_size - aprefix_size);
    }
    Fprintf(sdp, SPT_FIELD_WIDTH "%s\n", "Note", "Incorrect data is marked with asterisk '*'");

    /*
     * Compare one lbdata sized block at a time.
     * 
     * TODO: This does NOT handle any partial IOT blocks! (assumes full IOT data blocks)
     * This is *not* generally a problem, but partial blocks can occur with file system full,
     * and the file offset is not modulo the block size (crossing file system blocks).
     */
    while (blocks) {
	int result = 0;
	if (sdp->timestamp_flag) {
	    if (sdp->fprefix_size) {
		result = memcmp(pptr, vptr, sdp->fprefix_size);
	    }
	    if (result == 0) {
		int doff = (sdp->fprefix_size + sizeof(iotlba_t));
		result = memcmp( (pptr + doff), (vptr + doff),
				 (iop->device_size - doff) );
	    }
	} else {
	    result = memcmp(pptr, vptr, iop->device_size);
	}
	if (result == 0) {
	    if (sdp->dumpall_flag) {
		display_iot_block(sdp, iop, block, block_offset, pptr, vptr, iop->device_size);
	    }
	} else {
	    if (sdp->dumpall_flag ||
		(sdp->max_bad_blocks && (bad_blocks < sdp->max_bad_blocks) )) {
		display_iot_block(sdp, iop, block, block_offset, pptr, vptr, iop->device_size);
	    }
	    bad_blocks++;
	}
	block++;
	blocks--;
	pptr += iop->device_size;
	vptr += iop->device_size;
	block_offset += iop->device_size;
    }
    /*
     * Warn user (including me), that some of the IOT data was NOT displayed!
     */ 
    if ((count % iop->device_size) != 0) {
	Fprint(sdp, "\n");
	Wprintf(sdp, "A partial IOT data block of %u bytes was NOT displayed!\n",
	       (count % iop->device_size) );
    }
    return;
}
