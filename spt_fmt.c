/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2018			    *
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
 * Module:	spt_fmt.c
 * Author:	Robin T. Miller
 * Date:	August 17th, 2013
 *
 * Description:
 *	File system operations.
 *
 * Modification History:
 *
 * November 8th, 2017 by Robin T. Miller
 *      Add "%adsf" to format the alternate device name.
 * 
 * November 3rd, 2017 by Robin T. Miller
 * 	Update emit control strings to support descriptor sense format.
 *      Add support for unpacking SATA ASCII data (byte swap words).
 * 
 * October 13th, 2017 by Robin T. Miller
 *      Handle sense data in both fixed and descriptor formats.
 *      Note: Still need to handle individual sense descriptors!
 *      This affects: %info* %cspec_data %fru %ili sense formats.
 *      TODO: Also need to add control strings for HGST specific.
 * 
 * September 29th, 2017 by Robin T. Miller
 *      Add support for unpacking bit fields.
 * 
 * September 14th, 2016 by Robin T. Miller
 *      When formatting emit status string, handle no sense data.
 * 
 * June 15th, 2014 by Robin T. Miller
 * 	Add %dir and %length to formatting of emit status.
 *
 * December 16th, 2013 by Robin T Miller
 * 	Integrating dt's formatting functions into spt.
 */
#include "spt.h"
#include <ctype.h>

/*
 * Forward References:
 */
clock_t	get_elapsed_ticks(scsi_device_t *sdp);
time_t get_elapsed_time(scsi_device_t *sdp);
uint64_t get_total_bytes_transferred(scsi_device_t *sdp, time_t *secs);
uint64_t get_total_blocks_transferred(scsi_device_t *sdp, time_t *secs);
uint64_t get_total_operations(scsi_device_t *sdp, time_t *secs);

int verify_unpack_range(scsi_device_t *sdp, int offset, int size, int limit);

/*
 * FmtEmitStatus() - Format the Exit Status String.
 *
 * Exit Status Control Keywords:
 *	%progname = Our program name.
 * 	%status = The command (IOCTL) status.
 * 	%status_msg = The status message.
 * 	%scsi_status = The SCSI status.
 * 	%scsi_msg = The SCSI status message.
 *	%host_status = The host status.
 *	%host_msg = The host status message.
 *	%driver_status = The driver status.
 *	%driver_msg = The driver status message.
 * 	%sense_code = The sense error code.
 * 	%sense_msg = The sense code message.
 * 	%sense_key = The sense key.
 * 	%skey_msg = The sense key message.
 *	%info_valid = The information valid bit.
 *	%info_data = The information field data.
 *	%cspec_data = The cmd specific info data.
 *	%ili = Illegal length indicator.
 *	%eom = End of medium.
 *	%fm = Tape file mark.
 *	%ascq = The asc/ascq pair.
 *	%asc = The additional sense code.
 * 	%asq = The additional sense qualifier.
 * 	%ascq_msg = The asc/asq message.
 *	%fru = The field replaceable unit code.
 * 	%resid = The residual bytes.
 * 	%blocks = The blocks transferred.
 * 	%cdb = The SCSI CDB executed.
 * 	%dir = The data direction.
 * 	%length = The data length;
 *	%xfer = The bytes transferred. (or %bytes)
 *	%sense_data = All the sense data.
 * 	%scsi_name = The SCSI opcode name.
 *      %adsf = The alternate device.
 * 	%dsf = The 1st device special file.
 * 	%dsf1 = The 2nd device special file.
 * 	%src = The source device(s).
 * 	%dst = The destination device.
 * 	%iterations = The interations executed.
 * 	%operations = The operations executed.
 *	%timeout = The command timeout (in ms).
 * 	%thread = The thread number.
 *	%total_blocks = The total blocks.
 * 	%total_xfer = The total transferred. (or %total_bytes)
 * 	%deallocated = The deallocated blocks.
 * 	%mapped = The mapped_blocks.
 * 	%starting = The starting logical block.
 * 	%ending = The ending logical block.
 * 	%capacity = The device capacity.
 * 	%device_size = The device size.
 *
 * Time Keywords:
 * 	%date = The current date/time.
 * 	%seconds = The time in seconds.
 * 	%start_time = The test start time.
 * 	%end_time = The test end time.
 * 	%elapsed_time = The elapsed time.
 *
 * Performance Keywords:
 *      %bps  = The bytes per second.
 *      %lbps = The blocks per second.
 *      %kbps = The kilobytes per second.
 *      %mbps = The megabytes per second.
 *      %iops = The I/O's per second.
 *      %spio = The seconds per I/O.
 *
 * Inputs:
 *	sdp = The SCSI device pointer.
 *	format = The exit status format string.
 *	buffer = Buffer for formatted message.
 *
 * Outputs:
 *	Returns the formatted prefix buffer length.
 */
int
FmtEmitStatus(scsi_device_t *sdp, io_params_t *uiop, scsi_generic_t *usgp, char *format, char *buffer)
{
    char    *from = format;
    char    *to = buffer;
    ssize_t length = strlen(format);
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    scsi_sense_t *ssp = sgp->sense_data;

    /* Allow caller to select the initial iop and sgp. */
    if (uiop) {
	iop = uiop;
	sgp = &iop->sg;
    }
    if (usgp) {
	sgp = usgp;
    }
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
	    if (strncasecmp(key, "progname", 8) == 0) {
		slen = Sprintf(to, "%s", OurName);
		to += slen;
		from += 9;
		length -= 8;
		continue;
	    } else if (strncasecmp(key, "thread", 6) == 0) {
		slen = Sprintf(to, "%d", sdp->thread_number);
		to += slen;
		from += 7;
		length -= 6;
		continue;
	    } else if (strncasecmp(key, "adsf", 4) == 0) {
		if (sgp->adsf) {
		    slen = Sprintf(to, "%s", sgp->adsf);
		    to += slen;
		}
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "dsf1", 4) == 0) {
		/* Switch to dsf1. (mirror device) */
		iop = &sdp->io_params[IO_INDEX_DSF1];
		sgp = &iop->sg;
		ssp = sgp->sense_data;
		if (sgp->dsf) {
		    slen = Sprintf(to, "%s", sgp->dsf);
		    to += slen;
		}
		from += 5;
		length -= 4;
		continue;
	    } else if ( (strncasecmp(key, "dsf", 3) == 0) ||
			(strncasecmp(key, "dst", 3) == 0) ) {
		/* Switch to dsf (default device). */
		iop = &sdp->io_params[IO_INDEX_BASE];
		sgp = &iop->sg;
		ssp = sgp->sense_data;
		if (sgp->dsf) {
		    slen = Sprintf(to, "%s", sgp->dsf);
		    to += slen;
		}
		from += 4;
		length -= 3;
		continue;
	    } else if (strncasecmp(key, "srcs", 4) == 0) {
		int device_index = IO_INDEX_SRC;
		
		for (; (device_index < sdp->io_devices); device_index++) {
		    io_params_t *siop = &sdp->io_params[device_index];
		    scsi_generic_t *ssgp = &siop->sg;
		    slen = Sprintf(to, "%s%s", ssgp->dsf,
				   ((device_index + 1) < sdp->io_devices) ? " " : "");
		    to += slen;
		}
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "src1", 4) == 0) {
		/* Switch to source device 1. */
		iop = &sdp->io_params[IO_INDEX_SRC + 1];
		sgp = &iop->sg;
		ssp = sgp->sense_data;
		if (sgp->dsf) {
		    slen = Sprintf(to, "%s", sgp->dsf);
		    to += slen;
		}
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "src2", 4) == 0) {
		/* Switch to source device 2. */
		iop = &sdp->io_params[IO_INDEX_SRC + 2];
		sgp = &iop->sg;
		ssp = sgp->sense_data;
		if (sgp->dsf) {
		    slen = Sprintf(to, "%s", sgp->dsf);
		    to += slen;
		}
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "src", 3) == 0) {
		/* Switch to source device 0. */
		iop = &sdp->io_params[IO_INDEX_SRC];
		sgp = &iop->sg;
		ssp = sgp->sense_data;
		if (sgp->dsf) {
		    slen = Sprintf(to, "%s", sgp->dsf);
		    to += slen;
		}
		from += 4;
		length -= 3;
		continue;
	    } else if (strncasecmp(key,  "cdb",  3) == 0) {
		int i;
		for (i = 0; (i < sgp->cdb_size); i++) {
		    slen = Sprintf(to, "%02x ",  sgp->cdb[i]);
		    to += slen;
		}
		if (i) {
		    slen--; to--; *to = '\0';
		}
		from += 4;
		length -= 3;
		continue;
	    } else if (strncasecmp(key,  "dir",  3) == 0) {
		if (sgp->data_dir == scsi_data_none) {
		    slen = Sprintf(to, "%s", "none");
		} else if (sgp->data_dir == scsi_data_read) {
		    slen = Sprintf(to, "%s", "read");
		} else if (sgp->data_dir == scsi_data_write) {
		    slen = Sprintf(to, "%s", "write");
		} else {
		    slen = 0;
		}
		to += slen;
		from += 4;
		length -= 3;
		continue;
	    } else if (strncasecmp(key,  "length",  6) == 0) {
		to += Sprintf(to, "%u",  sgp->data_length);
		from += 7;
		length -= 6;
		continue;
	    } else if (strncasecmp(key, "status_msg", 10) == 0) {
		char *status_msg;
		if (sdp->status == SUCCESS) {
		    status_msg = "SUCCESS";
		} else if (sdp->status == FAILURE) {
		    status_msg = "FAILURE";
		} else {
		    status_msg = "<unknown>";
		}
		slen = Sprintf(to, "%s", status_msg);
		to += slen;
		from += 11;
		length -= 10;
		continue;
	    } else if (strncasecmp(key, "status", 6) == 0) {
		slen = Sprintf(to, "%d", sdp->status);
		to += slen;
		from += 7;
		length -= 6;
		continue;
	    } else if (strncasecmp(key, "scsi_name", 9) == 0) {
		if (sdp->scsi_name) {
		    slen = Sprintf(to, "%s", sdp->scsi_name);
		} else {
		    slen = Sprintf(to, "%s", "<unknown>");
		}
		to += slen;
		from += 10;
		length -= 9;
		continue;
	    } else if (strncasecmp(key, "scsi_msg", 8) == 0) {
		char *scsi_msg = ScsiStatus(sgp->scsi_status);
		slen = Sprintf(to, "%s", scsi_msg);
		to += slen;
		from += 9;
		length -= 8;
		continue;
	    } else if (strncasecmp(key, "scsi_status", 11) == 0) {
		slen = Sprintf(to, "%x", sgp->scsi_status);
		to += slen;
		from += 12;
		length -= 11;
		continue;
	    } else if (strncasecmp(key, "host_status", 11) == 0) {
		slen = Sprintf(to, "%x", sgp->host_status);
		to += slen;
		from += 12;
		length -= 11;
		continue;
	    } else if (strncasecmp(key, "host_msg", 8) == 0) {
		char *host_msg = os_host_status_msg(sgp);
		slen = Sprintf(to, "%s", host_msg);
		to += slen;
		from += 9;
		length -= 8;
		continue;
	    } else if (strncasecmp(key, "driver_status", 13) == 0) {
		slen = Sprintf(to, "%x", sgp->driver_status);
		to += slen;
		from += 14;
		length -= 13;
		continue;
	    } else if (strncasecmp(key, "driver_msg", 10) == 0) {
		char *driver_msg = os_driver_status_msg(sgp);
		slen = Sprintf(to, "%s", driver_msg);
		to += slen;
		from += 11;
		length -= 10;
		continue;
	    } else if (strncasecmp(key, "sense_code", 10) == 0) {
		uint8_t error_code = (ssp) ? ssp->error_code : 0;
		slen = Sprintf(to, "%x", error_code);
		to += slen;
		from += 11;
		length -= 10;
		continue;
	    } else if (strncasecmp(key, "sense_msg", 9) == 0) {
		char *sense_msgp = (ssp) ? SenseCodeMsg(ssp->error_code) : "None";
		slen = Sprintf(to, "%s", sense_msgp);
		to += slen;
		from += 10;
		length -= 9;
		continue;
	    } else if (strncasecmp(key, "resid", 5) == 0) {
		slen = Sprintf(to, "%u", sgp->data_resid);
		to += slen;
		from += 6;
		length -= 5;
		continue;
	    } else if (strncasecmp(key, "timeout", 7) == 0) {
		slen = Sprintf(to, "%u", sgp->timeout);
		to += slen;
		from += 8;
		length -= 7;
		continue;
	    } else if (strncasecmp(key, "blocks", 6) == 0) {
		uint64_t blocks_transferred = 0;
		if (iop->cdb_blocks) {
		    blocks_transferred = iop->cdb_blocks;
		} else if (iop->device_size) {
		    blocks_transferred = howmany(sgp->data_transferred, iop->device_size);
		}
		slen = Sprintf(to, LUF, blocks_transferred);
		to += slen;
		from += 7;
		length -= 6;
		continue;
	    } else if (strncasecmp(key, "starting", 8) == 0) {
		slen = Sprintf(to, LUF, iop->starting_lba);
		to += slen;
		from += 9;
		length -= 8;
		continue;
	    } else if (strncasecmp(key, "ending", 6) == 0) {
		slen = Sprintf(to, LUF, iop->ending_lba);
		to += slen;
		from += 7;
		length -= 6;
		continue;
	    } else if (strncasecmp(key, "capacity", 8) == 0) {
		slen = Sprintf(to, LUF, iop->device_capacity);
		to += slen;
		from += 9;
		length -= 8;
		continue;
	    } else if (strncasecmp(key, "device_size", 11) == 0) {
		slen = Sprintf(to, "%u", iop->device_size);
		to += slen;
		from += 12;
		length -= 11;
		continue;
	    } else if (strncasecmp(key, "deallocated", 11) == 0) {
		slen = Sprintf(to, LUF, iop->deallocated_blocks);
		to += slen;
		from += 12;
		length -= 11;
		continue;
	    } else if (strncasecmp(key, "mapped", 6) == 0) {
		slen = Sprintf(to, LUF, iop->mapped_blocks);
		to += slen;
		from += 7;
		length -= 6;
		continue;
	    } else if (strncasecmp(key,  "iterations",  10) == 0) {
		slen = Sprintf(to, LUF, sdp->iterations);
		to += slen;
		from += 11;
		length -= 10;
		continue;
	    } else if (strncasecmp(key,  "operations",  10) == 0) {
		slen = Sprintf(to, LUF, iop->operations);
		to += slen;
		from += 11;
		length -= 10;
		continue;
	    } else if (strncasecmp(key, "xfer", 4) == 0) {
		//slen = Sprintf(to, "%u", (sgp->data_length - sgp->data_resid));
		slen = Sprintf(to, "%u", sgp->data_transferred);
		to += slen;
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "bytes", 5) == 0) {
		slen = Sprintf(to, "%u", sgp->data_transferred);
		to += slen;
		from += 4;
		length -= 5;
		continue;
	    } else if (strncasecmp(key, "total_blocks", 12) == 0) {
		uint64_t total_blocks = 0;
		total_blocks = get_total_blocks_transferred(sdp, NULL);
		slen = Sprintf(to, LUF, total_blocks);
		to += slen;
		from += 13;
		length -= 12;
		continue;
	    } else if (strncasecmp(key, "total_operations", 16) == 0) {
		uint64_t total_operations = 0;
		total_operations = get_total_operations(sdp, NULL);
		slen = Sprintf(to, LUF, total_operations);
		to += slen;
		from += 17;
		length -= 16;
		continue;
	    } else if (strncasecmp(key, "total_xfer", 10) == 0) {
		uint64_t total_bytes = 0;
		total_bytes = get_total_bytes_transferred(sdp, NULL);
		slen = Sprintf(to, LUF, total_bytes);
		to += slen;
		from += 11;
		length -= 10;
		continue;
	    } else if (strncasecmp(key, "total_bytes", 11) == 0) {
		uint64_t total_bytes = 0;
		total_bytes = get_total_bytes_transferred(sdp, NULL);
		slen = Sprintf(to, LUF, total_bytes);
		to += slen;
		from += 12;
		length -= 11;
		continue;
	    } else if (strncasecmp(key, "sense_data", 10) == 0) {
		unsigned char *bp = sgp->sense_data;
		int sense_len = (ssp) ? (8 + ssp->addl_sense_len) : 8;
		if (bp) {
		    while (sense_len--) {
			slen = Sprintf(to, "%02x ", *bp++);
			to += slen;
		    }
		    --to; *to = '\0'; /* remove last space */
		}
		from += 11;
		length -= 10;
		continue;
	    } else if (strncasecmp(key, "cspec_data", 10) == 0) {
		uint64_t cmd_spec_value = 0;
		if (ssp) {
		    GetSenseCmdSpecific(ssp, &cmd_spec_value);
		}
		slen = Sprintf(to, LUF, cmd_spec_value);
		to += slen;
		from += 11;
		length -= 10;
		continue;
	    } else if (strncasecmp(key, "info_valid", 10) == 0) {
		uint8_t info_valid = 0;
		uint64_t info_value = 0;
		if (ssp) {
		    GetSenseInformation(ssp, &info_valid, &info_value);
		}
		slen = Sprintf(to, "%u", info_valid);
		to += slen;
		from += 11;
		length -= 10;
		continue;
	    } else if (strncasecmp(key, "info_data", 9) == 0) {
		uint8_t info_valid = 0;
		uint64_t info_value = 0;
		if (ssp) {
		    GetSenseInformation(ssp, &info_valid, &info_value);
		}
		slen = Sprintf(to, LUF, info_value);
		to += slen;
		from += 10;
		length -= 9;
		continue;
	    } else if (strncasecmp(key, "sense_key", 9) == 0) {
		uint8_t sense_key = 0, asc = 0, asq = 0;
		if (ssp) {
		    GetSenseErrors(ssp, &sense_key, &asc, &asq);
		}
		slen = Sprintf(to, "%x", sense_key);
		to += slen;
		from += 10;
		length -= 9;
		continue;
	    } else if (strncasecmp(key, "skey_msg", 8) == 0) {
		uint8_t sense_key = 0, asc = 0, asq = 0;
		char *skey_msg = "None";
		if (ssp) {
		    GetSenseErrors(ssp, &sense_key, &asc, &asq);
		    skey_msg = SenseKeyMsg(sense_key);
		}
		slen = Sprintf(to, "%s", skey_msg);
		to += slen;
		from += 9;
		length -= 8;
		continue;
	    } else if (strncasecmp(key, "ili", 3) == 0) {
		uint8_t illegal_length = (ssp) ? ssp->illegal_length : 0;
		slen = Sprintf(to, "%u", illegal_length);
		to += slen;
		from += 4;
		length -= 3;
		continue;
	    } else if (strncasecmp(key, "eom", 3) == 0) {
		uint8_t end_of_medium = (ssp) ? ssp->end_of_medium : 0;
		slen = Sprintf(to, "%u", end_of_medium);
		to += slen;
		from += 4;
		length -= 3;
		continue;
	    } else if (strncasecmp(key, "fm", 2) == 0) {
		uint8_t file_mark = (ssp) ? ssp->file_mark : 0;
		slen = Sprintf(to, "%u", file_mark);
		to += slen;
		from += 3;
		length -= 2;
		continue;
	    } else if (strncasecmp(key, "ascq_msg", 8) == 0) {
		uint8_t sense_key = 0, asc = 0, asq = 0;
		char *ascq_msg = "None";
		if (ssp) {
		    GetSenseErrors(ssp, &sense_key, &asc, &asq);
		    ascq_msg = ScsiAscqMsg(asc, asq);
		}
		slen = Sprintf(to, "%s", ascq_msg);
		to += slen;
		from += 9;
		length -= 8;
		continue;
	    } else if (strncasecmp(key, "ascq", 4) == 0) {
		uint8_t sense_key = 0, asc = 0, asq = 0;
		if (ssp) {
		    GetSenseErrors(ssp, &sense_key, &asc, &asq);
		}
		slen = Sprintf(to, "%02x%02x", asc, asq);
		to += slen;
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "asc", 3) == 0) {
		uint8_t sense_key = 0, asc = 0, asq = 0;
		if (ssp) {
		    GetSenseErrors(ssp, &sense_key, &asc, &asq);
		}
		slen = Sprintf(to, "%02x", asc);
		to += slen;
		from += 4;
		length -= 3;
		continue;
	    } else if (strncasecmp(key, "asq", 3) == 0) {
		uint8_t sense_key = 0, asc = 0, asq = 0;
		if (ssp) {
		    GetSenseErrors(ssp, &sense_key, &asc, &asq);
		}
		slen = Sprintf(to, "%02x", asq);
		to += slen;
		from += 4;
		length -= 3;
		continue;
	    } else if (strncasecmp(key, "fru", 3) == 0) {
		uint8_t fru_code = 0;
		if (ssp) {
		    GetSenseFruCode(ssp, &fru_code);
		}
		slen = Sprintf(to, "%x", fru_code);
		to += slen;
		from += 4;
		length -= 3;
		continue;
	    /*
	     * Time Keywords:
	     */
	    } else if (strncasecmp(key, "date", 4) == 0) {
		char time_buffer[32];
		time_t current_time = time((time_t *) 0);
		memset(time_buffer, '\0', sizeof(time_buffer));
		os_ctime(&current_time, time_buffer, sizeof(time_buffer));
		to += Sprintf(to, "%s", time_buffer);
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "seconds", 7) == 0) {
		time_t secs = get_elapsed_time(sdp);
		to += Sprintf(to, "%d", secs);
		from += 8;
		length -= 7;
		continue;
	    } else if (strncasecmp(key, "start_time", 10) == 0) {
		char time_buffer[32];
		memset(time_buffer, '\0', sizeof(time_buffer));
		os_ctime(&sdp->start_time, time_buffer, sizeof(time_buffer));
		to += Sprintf(to, "%s", time_buffer);
		from += 11;
		length -= 10;
		continue;
	    } else if (strncasecmp(key, "end_time", 8) == 0) {
		char time_buffer[32];
		memset(time_buffer, '\0', sizeof(time_buffer));
		os_ctime(&sdp->end_time, time_buffer, sizeof(time_buffer));
		to += Sprintf(to, "%s", time_buffer);
		from += 9;
		length -= 8;
		continue;
	    } else if (strncasecmp(key, "elapsed_time", 12) == 0) {
		clock_t et;
		if (sdp->end_ticks) {
		    et = (sdp->end_ticks - sdp->start_ticks);
		} else {
		    et = get_elapsed_ticks(sdp);
		}
		if (!sdp->start_ticks) et = 0;
		to += FormatElapstedTime(to, et);
		from += 13;
		length -= 12;
		continue;
	    /*
	     * Performance Keywords:
	     */
	    } else if (strncasecmp(key, "bps", 3) == 0) {
		time_t		secs;
		uint64_t	bytes;
		bytes = get_total_bytes_transferred(sdp, &secs);
		if (secs) {
		    to += Sprintf(to, "%.3f", ((double)bytes / (double)secs));
		} else {
		    to += Sprintf(to, "0.000");
		}
		length -= 3;
		from += 4;
		continue;
	    } else if (strncasecmp(key, "lbps", 4) == 0) {
		time_t		secs;
		uint64_t	blocks;
		blocks = get_total_blocks_transferred(sdp, &secs);
		if (secs) {
		    to += Sprintf(to, "%.3f", ((double)blocks / (double)secs));
		} else {
		    to += Sprintf(to, "0.000");
		}
		length -= 4;
		from += 5;
		continue;
	    } else if (strncasecmp(key, "kbps", 4) == 0) {
		time_t		secs;
		uint64_t	bytes;
		bytes = get_total_bytes_transferred(sdp, &secs);
		if (secs) {
		    to += Sprintf(to, "%.3f", ((double)bytes / (double)KBYTE_SIZE) / secs);
		} else {
		    to += Sprintf(to, "0.000");
		}
		length -= 4;
		from += 5;
		continue;
	    } else if (strncasecmp(key, "mbps", 4) == 0) {
		time_t		secs;
		uint64_t	bytes;
		bytes = get_total_bytes_transferred(sdp, &secs);
		if (secs) {
		    to += Sprintf(to, "%.3f", ((double)bytes / (double)MBYTE_SIZE) / secs);
		} else {
		    to += Sprintf(to, "0.000");
		}
		length -= 4;
		from += 5;
		continue;
	    } else if (strncasecmp(key, "iops", 4) == 0) {
		time_t secs;
		uint64_t operations = get_total_operations(sdp, &secs);
		if (secs) {
		    to += Sprintf(to, "%.3f", ((double)operations / (double)secs));
		} else {
		    to += Sprintf(to, "0.000");
		}
		length -= 4;
		from += 5;
		continue;
	    } else if (strncasecmp(key, "spio", 4) == 0) {
		time_t secs;
		uint64_t operations = get_total_operations(sdp, &secs);
		if (operations) {
		    to += Sprintf(to, "%.4f", ((double)secs / (double)operations));
		} else {
		    to += Sprintf(to, "0.0000");
		}
		length -= 4;
		from += 5;
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
    return ( (int)strlen(buffer) );
}

clock_t
get_elapsed_ticks(scsi_device_t *sdp)
{
    clock_t ticks;
    struct tms end_times;
    clock_t current_ticks = times(&end_times);
    ticks = (current_ticks - sdp->start_ticks);
    return(ticks);
}

/* Note: This matchs dt, but clearly could use time() for secs! */
time_t
get_elapsed_time(scsi_device_t *sdp)
{
    time_t secs;
    time_t current_secs = time((time_t *) 0);
    //struct tms end_times;
    //clock_t current_ticks = times(&end_times);
    //secs = ((current_ticks - sdp->start_ticks) / hertz);
    secs = (current_secs - sdp->start_time);
    return(secs);
}

uint64_t
get_total_bytes_transferred(scsi_device_t *sdp, time_t *secs)
{
    scsi_generic_t	*sgp;
    io_params_t		*iop;
    int			device_index;
    uint64_t		total_bytes_transferred = 0;

    for (device_index = 0; device_index < sdp->io_devices; device_index++) {
	iop = &sdp->io_params[device_index];
	sgp = &iop->sg;
	total_bytes_transferred += iop->total_transferred;
    }
    if (secs) {
	*secs = get_elapsed_time(sdp);
    }
    return(total_bytes_transferred);
}

uint64_t
get_total_blocks_transferred(scsi_device_t *sdp, time_t *secs)
{
    scsi_generic_t	*sgp;
    io_params_t		*iop;
    int			device_index;
    uint64_t		total_blocks_transferred = 0;

    for (device_index = 0; device_index < sdp->io_devices; device_index++) {
	iop = &sdp->io_params[device_index];
	sgp = &iop->sg;
	total_blocks_transferred += iop->total_blocks;
    }
    if (secs) {
	*secs = get_elapsed_time(sdp);
    }
    return(total_blocks_transferred);
}

uint64_t
get_total_operations(scsi_device_t *sdp, time_t *secs)
{
    scsi_generic_t	*sgp;
    io_params_t		*iop;
    int			device_index;
    uint64_t		total_operations = 0;

    for (device_index = 0; device_index < sdp->io_devices; device_index++) {
	iop = &sdp->io_params[device_index];
	sgp = &iop->sg;
	total_operations += iop->operations;
    }
    if (secs) {
	*secs = get_elapsed_time(sdp);
    }
    return(total_operations);
}

/*
 * FmtString() - Format a String Based on Control Strings.
 *
 * Description:
 *	This function is used for formatting control strings for file paths
 * as well as the log prefix. More control strings are available using keepalive,
 * but these are deemed sufficient for file paths and the log prefix (so far).
 *
 * Special Format Control Strings:
 *
 *      %date = The date string.
 *      %et = The elapsed time.
 *      %tod = The time of day.
 *      %etod = Elapsed time of day.
 *      %dsf = The device name only.
 *	%dfs = The directory file separator. (Unix: '/' or Windows: '\')
 *	%host = The host name.
 * 	%job = The job ID.
 * 	%ymd = The year, month, day.
 * 	%hms = The hour, minute, second.
 *	%secs = Seconds since start.
 * 	%seq = The sequence number.
 * 	%src = The source devices.
 * 	%prog = Our program name.
 * 	%pid = The process ID.
 *	%tag = The job tag.
 * 	%tid = The thread ID.
 * 	%thread = The thread number.
 * 	%tmpdir = The temporary directory.
 *	%user = The user (login) name.
 *
 * Inputs:
 *	sdp = The SCSI device pointer.
 *	format = Pointer to format strings.
 *	filepath_flag = Formatting a file path.
 *
 * Return Value:
 * 	Returns a pointer to the allocated formatted string.
 */
char *
FmtString(scsi_device_t *sdp, char *format, hbool_t filepath_flag)
{
    io_params_t *iop = &sdp->io_params[IO_INDEX_BASE];
    scsi_generic_t *sgp = &iop->sg;
    char	buffer[LOG_BUFSIZE];
    char	*from = format;
    char	*to = buffer;
    int		length = (int)strlen(format);
    int		ifs = DIRSEP;

    *to = '\0';
    while (length--) {
	int slen = 0;
        if (*from == '%') {
            char *key = (from + 1);
	    if (strncasecmp(key, "date", 4) == 0) {
		char time_buffer[32];
		time_t current_time = time((time_t *) 0);
		memset(time_buffer, '\0', sizeof(time_buffer));
		os_ctime(&current_time, time_buffer, sizeof(time_buffer));
		to += Sprintf(to, "%s", time_buffer);
		from += 5;
		length -= 4;
		continue;
		/*
		 * Note: This device stuff is duplicated, need common parsing!
		 */
	    } else if (strncasecmp(key, "dsf1", 4) == 0) {
		/* Switch to dsf1. (mirror device) */
		iop = &sdp->io_params[IO_INDEX_DSF1];
		sgp = &iop->sg;
		if (sgp->dsf) {
		    slen = Sprintf(to, "%s", sgp->dsf);
		    to += slen;
		}
		from += 5;
		length -= 4;
		continue;
	    } else if ( (strncasecmp(key, "dsf", 3) == 0) ||
			(strncasecmp(key, "dst", 3) == 0) ) {
		/* Switch to dsf (default device). */
		iop = &sdp->io_params[IO_INDEX_BASE];
		sgp = &iop->sg;
		if (sgp->dsf) {
		    slen = Sprintf(to, "%s", sgp->dsf);
		    to += slen;
		}
		from += 4;
		length -= 3;
		continue;
	    } else if (strncasecmp(key, "srcs", 4) == 0) {
		int device_index = IO_INDEX_SRC;
		
		for (; (device_index < sdp->io_devices); device_index++) {
		    io_params_t *siop = &sdp->io_params[device_index];
		    scsi_generic_t *ssgp = &siop->sg;
		    slen = Sprintf(to, "%s%s", ssgp->dsf,
				   ((device_index + 1) < sdp->io_devices) ? " " : "");
		    to += slen;
		}
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "src1", 4) == 0) {
		/* Switch to source device 1. */
		iop = &sdp->io_params[IO_INDEX_SRC + 1];
		sgp = &iop->sg;
		if (sgp->dsf) {
		    slen = Sprintf(to, "%s", sgp->dsf);
		    to += slen;
		}
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "src2", 4) == 0) {
		/* Switch to source device 2. */
		iop = &sdp->io_params[IO_INDEX_SRC + 2];
		sgp = &iop->sg;
		if (sgp->dsf) {
		    slen = Sprintf(to, "%s", sgp->dsf);
		    to += slen;
		}
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "src", 3) == 0) {
		/* Switch to source device 0. */
		iop = &sdp->io_params[IO_INDEX_SRC];
		sgp = &iop->sg;
		if (sgp->dsf) {
		    slen = Sprintf(to, "%s", sgp->dsf);
		    to += slen;
		}
		from += 4;
		length -= 3;
		continue;
	    } else if (strncasecmp(key, "dfs", 3) == 0) {
		to += Sprintf(to, "%c", sdp->dir_sep);
		length -= 3;
		from += 4;
		continue;
            } else if (strncasecmp(key, "host", 4) == 0) {
                char *p, *hostname = os_gethostname();
		if (hostname) {
		    if (p = strchr(hostname, '.')) {
			*p = '\0';
		    }
		    to += Sprintf(to, "%s", hostname);
		    free(hostname);
		}
                length -= 4;
                from += 5;
                continue;
	    } else if (strncasecmp(key, "job", 3) == 0) {
		if (sdp->job) {
		    to += Sprintf(to, "%u", sdp->job->ji_job_id);
		} else {
		    to += Sprintf(to, "%u", sdp->job_id); /* TEMP */
		}
		length -= 3;
		from += 4;
		continue;
	    } else if (strncasecmp(key, "ymd", 3) == 0) {
		struct tm time_data;
		struct tm *tmp = &time_data;
		time_t now;
		(void)time(&now);
		if ( localtime_r(&now, tmp) ) {
		    tmp->tm_year += 1900;
		    tmp->tm_mon++;
		    /* Format: yyyymmdd */
		    to += sprintf(to, "%04d%02d%02d",
				  tmp->tm_year, tmp->tm_mon, tmp->tm_mday);
		}
		length -= 3;
		from += 4;
		continue;
	    } else if (strncasecmp(key, "hms", 3) == 0) {
		struct tm time_data;
		struct tm *tmp = &time_data;
		time_t now;
		(void)time(&now);
		if ( localtime_r(&now, tmp) ) {
		    tmp->tm_year += 1900;
		    tmp->tm_mon++;
		    /* Format: hhmmss */
		    to += sprintf(to, "%02d%02d%02d",
				  tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
		}
		length -= 3;
		from += 4;
		continue;
	    } else if (strncasecmp(key, "level", 5) == 0) {
		to += Sprintf(to, "%d", sdp->log_level);
		length -= 5;
		from += 6;
		continue;
	    } else if (strncasecmp(key, "secs", 4) == 0) {
		time_t secs = get_elapsed_time(sdp);
		to += Sprintf(to, "%08d", secs);
		from += 5;
		length -= 4;
		continue;
	    } else if (strncasecmp(key, "seq", 3) == 0) {
		to += Sprintf(to, "%8d", sdp->sequence);
		from += 4;
		length -= 3;
		continue;
            } else if (strncasecmp(key, "tod", 3) == 0) {
                *&sdp->ptod = *&sdp->gtod;
                (void)gettimeofday(&sdp->gtod, NULL);
                to += Sprintf(to, "%d.%06d", sdp->gtod.tv_sec, sdp->gtod.tv_usec);
                if (sdp->ptod.tv_sec == 0) {
                    *&sdp->ptod = *&sdp->gtod;
                }
                from += 4;
                length -= 3;
                continue;
            } else if (strncasecmp(key, "etod", 4) == 0) {
                int secs, usecs;
                secs = sdp->gtod.tv_sec;
                usecs = sdp->gtod.tv_usec;
                if (usecs < sdp->ptod.tv_usec) {
                    secs--;
                    usecs += uSECS_PER_SEC;
                }
                to += Sprintf(to, "%d.%06d", (secs - sdp->ptod.tv_sec), (usecs - sdp->ptod.tv_usec));
                from += 5;
                length -= 4;
                continue;
	    } else if (strncasecmp(key, "et", 2) == 0) {
		clock_t et = get_elapsed_ticks(sdp);
		if (!sdp->start_ticks) et = 0;
		to += FormatElapstedTime(to, et);
		from += 3;
		length -= 2;
		continue;
            } else if (strncasecmp(key, "prog", 4) == 0) {
                to += Sprintf(to, "%s", OurName);
                length -= 4;
                from += 5;
                continue;
            } else if (strncasecmp(key, "pid", 3) == 0) {
                pid_t pid = getpid();
                to += Sprintf(to, "%d", pid);
                length -= 3;
                from += 4;
                continue;
	    } else if (strncasecmp(key, "tag", 3) == 0) {
		if (sdp->job && sdp->job->ji_job_tag) {
		    to += Sprintf(to, "%s", sdp->job->ji_job_tag);
		}
		length -= 3;
		from += 4;
		continue;
	    } else if (strncasecmp(key, "tid", 3) == 0) {
		//to += Sprintf(to, "%p", sdp->thread_id);
		to += Sprintf(to, "%p", pthread_self());
		length -= 3;
		from += 4;
		continue;
	    } else if (strncasecmp(key, "thread", 6) == 0) {
		to += Sprintf(to, "%u", sdp->thread_number);
		length -= 6;
		from += 7;
		continue;
	    } else if (strncasecmp(key, "tmpdir", 6) == 0) {
		to += Sprintf(to, "%s", TEMP_DIR_NAME);
		length -= 6;
		from += 7;
		continue;
            } else if (strncasecmp(key, "user", 4) == 0) {
		char *username = os_getusername();
                if (username) {
                    to += Sprintf(to, "%s", username);
		    free(username);
                }
                length -= 4;
                from += 5;
                continue;
            }
        }
        *to++ = *from++;
    }
    *to = '\0';
    return( strdup(buffer) );
}

/*
 * FmtUnpackString() - Format Unpack String. 
 *  
 * Description: 
 *      This function unpacks received data based on the unpack format string.
 *  
 * Special Format Control Strings:
 *  
 *      %O[FFSET]:value = The data offset.
 *      %B[YTE][:index] = Decode a 8-bit value.
 *      %S[HORT][:index] = Decode a 16-bit value.
 *      %W[ORD][:index] = Decode a 32-bit value.
 *      %L[ONG][:index] = Decode a 64-bit value.
 *      %C[HAR][:index]:length = Decode length characters.
 *      %F[IELD][:index]:start:length = Extract a bit field.
 *  
 * Inputs: 
 *      sdp = The SCSI device pointer.
 *      format = The unpack format control string.
 *      data = The received data.
 *      count = The received data count.
 */
char *
FmtUnpackString(scsi_device_t *sdp, char *format, unsigned char *data, size_t count)
{
    char	buffer[LOG_BUFSIZE];
    char	*from = format;
    char	*to = buffer;
    ssize_t	length = strlen(format);
    char	*eptr = NULL;
    uint32_t	offset = 0;

    *to = '\0';
    while (length--) {
	if (*from == '%') {
	    char *key = (from + 1);
	    /* Format: %C[HAR]:index:length */
	    if ( match(&key, "C:") || match(&key, "CHAR:") ) {
		int i, len;
		/* The index is optional, so '::length' is valid. */
		if (*key != ':') {
		    offset = strtoul(key, &eptr, 10);
		    if (eptr) key = eptr;
		}
		/* The length is required. */
		if ( match(&key, ":") ) {
		    len = strtol(key, &eptr, 10);
		    key = eptr;
		} else {
		    Eprintf(sdp, "Missing ':', format is: %%C[HAR]:index:length\n");
		    return(NULL);
		}
		from = key;
		length -= (key - from);
		if (sdp->sata_device_flag) {
		    uint16_t *words = (uint16_t *)&data[offset];
		    union {
			uint8_t bytes[sizeof(uint16_t)];
			uint16_t word;
		    } w;
		    for (i = 0; i < len; i += 2) {
			int ii;
			w.word = *words++;
			/* Swap the SATA ASCII bytes. */
			for (ii = sizeof(uint16_t); ii; ii--) {
			    uint8_t byte = w.bytes[ii - 1];
			    if (isprint(byte)) {
				*to++ = byte;
			    } else {
				/* Show hex for non-printable characters. */
				to += sprintf(to, "%02x", byte);
			    }
			}
		    }
		    offset += len;
		} else {
		    for (i = 0; i < len; i++) {
			if (isprint(data[offset])) {
			    *to++ = data[offset++];
			} else {
			    /* Show hex for non-printable characters. */
			    to += sprintf(to, "%02x", data[offset++]);
			}
		    }
		}
		continue;
	    } else if ( match(&key, "F:") || match(&key, "FIELD:") ) {
		uint8_t mask, bits, max_mask = 0xff, value8 = 0;
		int len, start = 0, max_bits = 8;
		/* The index is optional, so '::start:end' is valid. */
		if (*key != ':') {
		    offset = strtoul(key, &eptr, 10);
		    if (eptr) key = eptr;
		}
		if ( verify_unpack_range(sdp, offset, sizeof(uint8_t), (int)count) ) {
		    return(NULL);
		}
		/* The starting bit is optional, defailts to 0. */
		if ( match(&key, ":") && *key != ':' ) {
		    start = strtol(key, &eptr, 10);
		    key = eptr;
		}
		/* The length is required. */
		if ( match(&key, ":") ) {
		    len = strtol(key, &eptr, 10);
		    if (len > max_bits) {
			Eprintf(sdp, "Bit field length is too large, max is %u bits\n", max_bits);
			return(NULL);
		    }
		    key = eptr;
		} else {
		    Eprintf(sdp, "Missing ':end', format is: %%F[IELD]:index:start:length\n");
		    return(NULL);
		}
		from = key;
		length -= (key - from);
		value8 = data[offset];
		mask = (max_mask >> (max_bits - len));
		bits = ((value8 >> start) & mask);
		to += sprintf(to, (sdp->unpack_data_fmt == DEC_FMT) ? "%u" : "0x%x", bits);
		/* We don't adjust offset, since we may extract other bit fields. */
		continue;
	    } else if ( match(&key, "O:") || match(&key, "OFFSET:") ) {
		if ( verify_unpack_range(sdp, offset, 0, (int)count) ) {
		    return(NULL);
		}
		offset = strtoul(key, &eptr, 10);
		if (eptr) key = eptr;
		from = key;
		length -= (key - from);
		continue;
	    } else if ( match(&key, "B") ) {
		/* Format: %B[YTE][:index] */
		uint8_t value8 = 0;
		(void)match(&key, "YTE");
		if (match(&key, ":")) {
		    offset = strtol(key, &eptr, 10);
		    if (eptr) key = eptr;
		}
		if ( verify_unpack_range(sdp, offset, sizeof(uint8_t), (int)count) ) {
		    return(NULL);
		}
		value8 = data[offset];
		to += sprintf(to, (sdp->unpack_data_fmt == DEC_FMT) ? "%u" : "0x%x", value8);
		from = key;
		length -= (key - from);
		offset += sizeof(value8);
		continue;
	    } else if ( match(&key, "S") ) {
		/* Format: %S[HORT][:index] */
		uint16_t value16 = 0;
		(void)match(&key, "HORT");
		if ( match(&key, ":") ) {
		    offset = strtol(key, &eptr, 10);
		    if (eptr) key = eptr;
		}
		if ( verify_unpack_range(sdp, offset, sizeof(uint16_t), (int)count) ) {
		    return(NULL);
		}
		value16 = (uint16_t)stoh(&data[offset], sizeof(value16));
		to += sprintf(to, (sdp->unpack_data_fmt == DEC_FMT) ? "%u" : "0x%x", value16);
		from = key;
		length -= (key - from);
		offset += sizeof(value16);
		continue;
	    } else if (  match(&key, "W") ) {
		/* Format: %W[ORD][:index] */
		uint32_t value32 = 0;
		(void)match(&key, "ORD");
		if ( match(&key, ":") ) {
		    offset = strtol(key, &eptr, 10);
		    if (eptr) key = eptr;
		}
		if ( verify_unpack_range(sdp, offset, sizeof(uint32_t), (int)count) ) {
		    return(NULL);
		}
		value32 = (uint32_t)stoh(&data[offset], sizeof(value32));
		to += sprintf(to, (sdp->unpack_data_fmt == DEC_FMT) ? "%u" : "0x%x", value32);
		from = key;
		length -= (key - from);
		offset += sizeof(value32);
		continue;
	    } else if ( match(&key, "L") ) {
		/* Format: %L[ONG][:index] */
		uint64_t value64 = 0;
		(void)match(&key, "ONG");
		if ( match(&key, ":") ) {
		    offset = strtol(key, &eptr, 10);
		    if (eptr) key = eptr;
		}
		if ( verify_unpack_range(sdp, offset, sizeof(uint64_t), (int)count) ) {
		    return(NULL);
		}
		value64 = stoh(&data[offset], sizeof(value64));
		to += sprintf(to, (sdp->unpack_data_fmt == DEC_FMT) ? LUF : LXF, value64);
		from = key;
		length -= (key - from);
		offset += sizeof(value64);
		continue;
	    }
	}
	/* Not '%' nor a match for the above. */
	if (*from == '\\') {
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
    }
    *to = '\0';
    return( strdup(buffer) );
}

int
verify_unpack_range(scsi_device_t *sdp, int offset, int size, int limit)
{
    if ( (offset + size) > limit) {
	Eprintf(sdp, "The offset %d + size %d, exceeds the range of 0-%d!\n", offset, size, (limit - 1));
	return(FAILURE);
    } else {
	return(SUCCESS);
    }
}
