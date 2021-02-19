#ifndef LIBSCSI_H
/****************************************************************************
 *                                                                          *
 *                      COPYRIGHT (c) 2006 - 2021                           *
 *                       This Software Provided                             *
 *                                  By                                      *
 *                      Robin's Nest Software Inc.                          *
 *                                                                          *
 * Permission to use, copy, modify, distribute and sell this software and   *
 * its documentation for any purpose and without fee is hereby granted,     *
 * provided that the above copyright notice appear in all copies and that   *
 * both that copyright notice and this permission notice appear in the      *
 * supporting documentation, and that the name of the author not be used    *
 * in advertising or publicity pertaining to distribution of the software   *
 * without specific, written prior permission.                              *
 *                                                                          *
 * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,        *
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN      *
 * NO EVENT SHALL HE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL   *
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR    *
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS  *
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF   *
 * THIS SOFTWARE.                                                           *
 *                                                                          *
 ****************************************************************************/
#define LIBSCSI_H

/*
 * These defines handle byte and bit ordering, used by many data structures
 * defined and used by Scu. While it's understood bit fields are not portable
 * these defines allow this nice (IMHO) feature to be utilized.  -Robin
 *
 * _LITTLE_ENDIAN / _BIG_ENDIAN:
 *  The natural byte order of the processor.  A pointer to an int points
 *  to the least/most significant byte of that int.
 *
 * _BITFIELDS_HIGH_TO_LOW_ / _BITFIELDS_LOW_TO_HIGH_:
 *  The C compiler assigns bit fields from the high/low to the low/high end
 *  of an int (most to least significant vs. least to most significant).
 */
#if defined(__hpux) || defined(__sparc) || defined(_AIX)
#  define _BIG_ENDIAN_    1
#  define _BITFIELDS_HIGH_TO_LOW_
#else /* !defined(__hpux) && !defined(__sparc) && !defined(_AIX) */
#  define _LITTLE_ENDIAN_ 1
#  define _BITFIELDS_LOW_TO_HIGH_
#endif /* defined(__hpux) || defined(__sparc) || defined(_AIX) */

#include "include.h"
#if defined(__IBMC__)
typedef unsigned bitfield_t;
#else /* !defined(__IBMC__) */
typedef unsigned char bitfield_t;
#endif /* defined(__IBMC__) */

//#include "common.h"
#include "inquiry.h"

/*
 * Local Defines:
 */
#define BITMASK(v)   (1L << v)          /* Convert value to bit mask.   */
#define ISSET(m,v)   (m & BITMASK(v))   /* Test bit set in bit mask.    */
#define ISCLR(m,v)   ((m & BITMASK(v)) == 0) /* Test bit clear in bit mask. */
#undef LTOB
#define LTOB(a,b)    ((a>>(b*8))&0xff)  /* Obtain byte from a long.     */
#define ALL_DEVICE_TYPES        0xffffU /* Supported for all devices.   */

#define MAX_CDB     64                  /* Maximum size of CDB.         */

#define ScsiRecoveryDelayDefault    2
#define ScsiRecoveryRetriesDefault  60
#define ScsiRecoveryFlagDefault     True
#define ScsiRestartFlagDefault      False
#define ScsiDebugFlagDefault        False
#define ScsiErrorFlagDefault        True

/* Note: These can be overridden by the consumer of this libary! */
#if !defined(SCSI_TIMEOUT_SECONDS)
# define SCSI_TIMEOUT_SECONDS   60  /* Default timeout in seconds.  */
#endif /* !defined(SCSI_TIMEOUT_SECONDS) */
#if !defined(ScsiDefaultTimeout)
# define ScsiDefaultTimeout (SCSI_TIMEOUT_SECONDS * MSECS)
                    /* Default timeout value (ms).  */
#endif /* !defined(ScsiDefaultTimeout) */
/*
 * Define Masks for SCSI Group Codes.
 */
#define SCSI_GROUP_0        0x00    /* SCSI Group Code 0.       */
#define SCSI_GROUP_1        0x20    /* SCSI Group Code 1.       */
#define SCSI_GROUP_2        0x40    /* SCSI Group Code 2.       */
#define SCSI_GROUP_3        0x60    /* SCSI Group Code 3.       */
#define SCSI_GROUP_4        0x80    /* SCSI Group Code 4.       */
#define SCSI_GROUP_5        0xA0    /* SCSI Group Code 5.       */
#define SCSI_GROUP_6        0xC0    /* SCSI Group Code 6.       */
#define SCSI_GROUP_7        0xE0    /* SCSI Group Code 7.       */
#define SCSI_GROUP_MASK     0xE0    /* SCSI Group Code mask.    */

/*
 * SCSI Address:
 *
 * Notes:
 *  General purpose for all OS's.
 *  Only Tru64 Unix (CAM) required b/t/l for SPT.
 */
typedef struct scsi_address {
    int scsi_bus;                       /* The bus number.                  */
    int scsi_chan;                      /* The channel number.              */
    int scsi_target;                    /* The target ID number.            */
    int scsi_lun;                       /* The Logical Unit Number.         */
    int scsi_path;                      /* The path number (for MPIO).      */
} scsi_addr_t;

typedef enum scsi_data_dir {
    scsi_data_none  = 0,                /* No data to be transferred.       */
    scsi_data_read  = 1,                /* Reading data from the device.    */
    scsi_data_write = 2                 /* Writing data to the device.      */
} scsi_data_dir_t;

/* Predefined I/O Types: */
typedef enum scsi_io_type {
    scsi_read6_cdb  = 0x08,
    scsi_read10_cdb = 0x28,
    scsi_read16_cdb = 0x88,
    scsi_write6_cdb = 0x0A,
    scsi_write10_cdb    = 0x2A,
    scsi_write16_cdb    = 0x8A,
    scsi_writev16_cdb   = 0x8E
} scsi_io_type_t;

/*
 * Optional SCSI Generic Control Flags:
 */
#define SG_INIT_ASYNC 0x01              /* Enable async mode (if possible). */
#define SG_INIT_SYNC  0x02              /* Initiate sync data transfers.    */
#define SG_INIT_WIDE  0x04              /* Initiate wide data transfers.    */
#define SG_NO_DISC    0x08              /* Disable disconnects.             */
#define SG_DIRECTIO   0x10              /* Enable direct I/O (no buffering).*/
#define SG_ADAPTER    0x20              /* Send SCSI CDB via HBA driver.    */

/*
 * Advanced Flags: (not currently implemented)
 * REVISIT: On AIX, there are others (I think).
 */
#define SG_ACA_Q      0x100             /* ACA task attribute.              */

#define SG_Q_CLEAR    0x2000            /* Clear adapter queue.             */
#define SG_Q_RESUME   0x4000            /* Resume queuing to device.        */
#define SG_CLEAR_ACA  0x8000            /* Clear ACA task request.          */

/*
 * Advanced Operations: (only supported on AIX at present)
 *
 * Note: These values match what AIX expects (at present).
 */
typedef enum scsi_qtag {
    SG_NO_Q      = 0,                   /* Don't use tagged queuing.        */
    SG_SIMPLE_Q  = 1,                   /* Simple queuing.                  */
    SG_HEAD_OF_Q = 2,                   /* Place at head of device queue.   */
    SG_ORDERED_Q = 3,                   /* Ordered queuing.                 */
    SG_HEAD_HA_Q = 4                    /* Head of HA queue (Solaris has).  */ 
} scsi_qtag_t;

struct scsi_generic;
/*
 * Tool Specific Data: (overrides SCSI library defaults)
 */
typedef struct tool_specific {
    void      *opaque;                  /* Opaque pointer for the caller.   */
    /*
     * This allows overriding the internal libExecuteCdb() function!
     */ 
    int       (*execute_cdb)(void *opaque, struct scsi_generic *sgp);
                                        /* Optional execute function.       */
    void      *params;                  /* Pointer to extra parameters.     */
} tool_specific_t;

typedef struct scsi_generic {
    HANDLE        fd;                   /* Device file descriptor.          */
    HANDLE        afd;                  /* Adapter file descriptor.         */
    char          *dsf;                 /* Device special file.             */
    char          *adsf;                /* Adapter device special file.     */
    hbool_t       dopen;                /* Open device and adapter files.   */
    hbool_t       mapscsi;              /* Map device name to SCSI device.  */
    unsigned int  flags;                /* Command control flags.           */
    unsigned int  sflags;               /* OS SCSI specific flags.          */
    scsi_addr_t   scsi_addr;            /* The SCSI address information.    */
    scsi_qtag_t   qtag_type;            /* The queue tag message type.      */
    char          *iface;               /* The interface name (cciss, etc). */
    hbool_t       error;                /* Indicates SCSI error occurred.   */
    hbool_t       errlog;               /* Controls logging messages.       */
    hbool_t       debug;                /* Enable debugging information.    */
    hbool_t       verbose;              /* Enable verbose infomormation.    */
    unsigned char cdb[MAX_CDB];         /* Command descriptor block.        */
    unsigned char cdb_size;             /* The CDB size.                    */
    char          *cdb_name;            /* The SCSI opcode name.            */
    unsigned int  scsi_status;          /* The SCSI status code.            */
    enum scsi_data_dir data_dir;        /* The SCSI data direction.         */
    void          *data_buffer;         /* Pointer to data buffer.          */
    unsigned int  data_length;          /* User data buffer length.         */
    unsigned int  data_resid;           /* Residual data not transferred.   */
    unsigned int  data_transferred;     /* The actual data transferred.     */
    unsigned int  data_dump_limit;      /* The data byte dump limit (debug).*/
    void          *sense_data;          /* Pointer to request sense data.   */
    unsigned int  sense_length;         /* Request sense buffer length.     */
    unsigned int  sense_resid;          /* Residual sense not transferred.  */
    unsigned int  sense_status;         /* Request sense SCSI status.       */
    hbool_t       sense_valid;          /* True if sense data is valid.     */
    unsigned int  timeout;              /* Command timeout in millisecs.    */
    unsigned int  aux_info;             /* Auxillary information (if any).  */
    unsigned int  duration;             /* Time taken by the command.       */
    unsigned int  host_status;          /* Addl host status (if any).       */
    unsigned int  driver_status;        /* Addl driver status (if any).     */
    unsigned int  os_error;             /* The OS specific error code.      */
    hbool_t       sense_flag;           /* Report full sense data flag.     */
    hbool_t       warn_on_error;        /* Reporting warning on errors.     */
    /* Tool Specific Parameters */
    tool_specific_t *tsp;               /* Tool specific information ptr.   */
    /* Recovery Parameters */
    hbool_t     recovery_flag;          /* The recovery control flag.       */
    hbool_t     restart_flag;           /* The restart control flag.        */
    uint32_t    recovery_delay;         /* The recovery delay (in secs)     */
    uint32_t    recovery_limit;         /* The recovery retry limit.        */
    uint32_t    recovery_retries;       /* The recovery retries.            */
} scsi_generic_t;

/*
 * SCSI Status Codes:
 */
#define SCSI_GOOD                  0x00 /* Command successfully completed.*/
#define SCSI_CHECK_CONDITION       0x02 /* Error, exception, or abnormal condition. */
#define SCSI_CONDITION_MET         0x04 /* Requested operation satisifed. */
#define SCSI_BUSY                  0x08 /* The target is BUSY.            */
#define SCSI_INTERMEDIATE          0x10 /* Linked commands successfully completed. */
#define SCSI_INTER_COND_MET        0x14 /* Intermediate-Condition met.    */
#define SCSI_RESERVATION_CONFLICT  0x18 /* Target reservation conflict.   */
#define SCSI_COMMAND_TERMINATED    0x22 /* Terminated by Terminate I/O Message. */
#define SCSI_QUEUE_FULL            0x28 /* Command tag queue is full.     */
#define SCSI_ACA_ACTIVE            0x30 /* ACA Active.                    */
#define SCSI_TASK_ABORTED          0x40 /* Task aborted.                  */

#define RequestSenseDataLength  255   /* Maximum Request Sense data size. */

/*
 * Error Code Definitions:
 */
#define ECV_CURRENT_FIXED       0x70  /* Current error, fixed format.     */
#define ECV_DEFERRED_FIXED      0x71  /* Deferred error fixed format.     */
#define ECV_CURRENT_DESCRIPTOR  0x72  /* Current error, descriptor format.*/
#define ECV_DEFERRED_DESCRIPTOR 0x73  /* Deferred error, descriptor format.*/
#define ECV_VENDOR_SPECIFIC     0x7f  /* Vendor specific sense data.      */

/*
 * Sense Key Codes:
 */
#define SKV_NOSENSE             0x0   /* No error or no sense info.       */
#define SKV_RECOVERED           0x1   /* Recovered error (success).       */
#define SKV_NOT_READY           0x2   /* Unit is not ready.               */
#define SKV_MEDIUM_ERROR        0x3   /* Nonrecoverable error.            */
#define SKV_HARDWARE_ERROR      0x4   /* Nonrecoverable hardware error.   */
#define SKV_ILLEGAL_REQUEST     0x5   /* Illegal CDB parameter.           */
#define SKV_UNIT_ATTENTION      0x6   /* Target has been reset.           */
#define SKV_DATA_PROTECT        0x7   /* Unit is write protected.         */
#define SKV_BLANK_CHECK         0x8   /* A no-data condition occured.     */
#define SKV_VENDOR_SPECIFIC     0x9   /* Vendor specific condition.       */
#define SKV_COPY_ABORTED        0xA   /* Copy command aborted.            */
#define SKV_ABORTED_CMD         0xB   /* Target aborted cmd, retry.       */
#define SKV_EQUAL               0xC   /* Vendor unique, not used.         */
#define SKV_VOLUME_OVERFLOW     0xD   /* Physical end of media detect.    */
#define SKV_MISCOMPARE          0xE   /* Source & medium data differ.     */
#define SKV_RESERVED            0xF   /* This sense key is reserved.      */

/*
 * Additional Sense Code/Qualifiers:
 */
/* Valid with SKV_NOT_READY */
#define ASC_NOT_READY           0x04  /* The device is Not Ready.       */
/* Valid with SKV_UNIT_ATTENTION */
#define ASC_RECOVERED_DATA      0x17  /* Recovered data. (success)      */
#define ASC_POWER_ON_RESET      0x29  /* A power on reset condition.    */
#define ASC_PARAMETERS_CHANGED  0x2A  /* Parameters changed condition.  */

/* (0x4, 0xb) - Logical unit not accessible, target port in standby state */
#define ASQ_STANDBY_STATE	0x0B  /* Target port in standby state.    */

/*
 * Generic Request Sense Data:
 */
typedef struct scsi_sense {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
        error_code  : 7,            /* Error code.                  [0] */
        info_valid  : 1;            /* Information fields valid.        */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
        info_valid  : 1,            /* Information fields valid.        */
        error_code  : 7;            /* Error code.                  [0] */
#else
#error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
  unsigned char obsolete;         /* Obsolete (was Segment number.[1] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
        sense_key       : 4,        /* Sense key.                   [2] */
        res_byte2_b4    : 1,        /* Reserved.                        */
        illegal_length  : 1,        /* Illegal length indicator.        */
        end_of_medium   : 1,        /* End of medium.                   */
        file_mark       : 1;        /* Tape filemark detected.          */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
        file_mark       : 1,        /* Tape filemark detected.          */
        end_of_medium   : 1,        /* End of medium.                   */
        illegal_length  : 1,        /* Illegal length indicator.        */
        res_byte2_b4    : 1,        /* Reserved.                        */
        sense_key       : 4;        /* Sense key.                   [2] */
#else
#error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
  unsigned char info_bytes[4];        /* Information bytes (MSB).   [3-6] */
  unsigned char addl_sense_len;       /* Additional sense length.     [7] */
  unsigned char cmd_spec_info[4];     /* Command specific info.    [8-11] */
  unsigned char asc;                  /* Additional sense code.      [12] */
  unsigned char asq;                  /* Additional sense qualifier. [13] */
  unsigned char fru_code;             /* Field replaceable unit.     [14] */
  unsigned char sense_key_specific[3];/* Sense key specific info. [15-17] */
  unsigned char addl_sense[RequestSenseDataLength-18]; /* Pad to max [18]*/
} scsi_sense_t;

/* --------------------------------------------------------------------------- */
/*
 * Sense Data Descriptor Format:
 */
typedef struct scsi_sense_desc {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
        error_code  : 7,          /* Error code.                  [0] */
        info_valid  : 1;          /* Information fields valid.        */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
        info_valid  : 1,          /* Information fields valid.        */
        error_code  : 7;          /* Error code.                  [0] */
#else
#error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
        sense_key       : 4,      /* Sense key.                   [1] */
        res_byte2_b4_b7 : 4;      /* Reserved.                        */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
        res_byte2_b4_b7 : 1,      /* Reserved.                        */
        sense_key       : 4;      /* Sense key.                   [1] */
#else
#error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
  unsigned char asc;                  /* Additional sense code.       [2] */
  unsigned char asq;                  /* Additional sense qualifier.  [3] */
  unsigned char reserved_byte4_6[3];  /* Reserved bytes.        [4-6] */
  unsigned char addl_sense_len;       /* Additional sense length.     [7] */
  unsigned char addl_sense[RequestSenseDataLength-7]; /* Pad to max [8-m]*/
} scsi_sense_desc_t;

/*
 * Sense Descriptor Types:
 */
#define INFORMATION_DESC_TYPE           0x00
#define COMMAND_SPECIFIC_DESC_TYPE      0x01
#define SENSE_KEY_SPECIFIC_DESC_TYPE    0x02
#define FIELD_REPLACEABLE_UNIT_DESC_TYPE 0x03
#define BLOCK_COMMAND_DESC_TYPE         0x05
#define ATA_STATUS_RETURN_DESC_TYPE     0x09

typedef struct sense_data_desc_header {
    unsigned char descriptor_type;
    unsigned char additional_length;
} sense_data_desc_header_t;

typedef struct information_desc_type {
    sense_data_desc_header_t header;    /* Descriptor header.         [0-1] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
        reserved_b0_b6  : 7,            /* Reserved.                    [2] */
        info_valid      : 1;            /* Information fields valid.        */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
        info_valid      : 1,            /* Information fields valid.        */
        reserved_b0_b6  : 7;            /* Resevred.                    [2] */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    unsigned char reserved_byte3;       /* Reserved.                    [3] */
    unsigned char information[8];       /* Information.              [4-11] */
} information_desc_type_t;

typedef struct command_specific_desc_type {
    sense_data_desc_header_t header;    /* Descriptor header.         [0-1] */
    unsigned char reserved_byte2_3;     /* Reserved.                  [2-3] */
    unsigned char information[8];       /* Command Information.      [4-11] */
} command_specific_desc_type_t;

typedef struct sense_key_specific_desc_type {
    sense_data_desc_header_t header;    /* Descriptor header.         [0-1] */
    unsigned char reserved_byte2;       /* Reserved.                    [2] */
    unsigned char reserved_byte3;       /* Reserved.                    [3] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
        sense_key_bits  : 7,            /* Sense key specific bits.     [4] */
        sksv            : 1;            /* Sense key valid.                 */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
        sksv            : 1,            /* Sense key valid.                 */
        sense_key_bits  : 7;            /* Sense key specific bits.     [4] */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    unsigned char sense_key_bytes[2];   /* Sense key specific bytes.  [5-6] */
    unsigned char reserved_byte7;       /* Reserved.                    [7] */
} sense_key_specific_desc_type_t;

typedef struct fru_desc_type {
    sense_data_desc_header_t header;    /* Descriptor header.         [0-1] */
    unsigned char reserved_byte2;       /* Reserved.                    [2] */
    unsigned char fru_code;             /* Field replaceable unit code. [3] */
} fru_desc_type_t;

typedef struct block_command_desc_type {
    sense_data_desc_header_t header;    /* Descriptor header.         [0-1] */
    unsigned char reserved_byte2;       /* Reserved.                    [2] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
        reserved_b0_b4  : 5,            /* Reserved.                    [3] */
        ili             : 1,            /* Information field value type.    */
        reserved_b6_b7  : 2;            /* Reserved.                        */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
        reserved_b6_b7  : 2,            /* Reserved.                        */
        ili             : 1,            /* Information field value type.    */
        reserved_b0_b4  : 5;            /* Reserved.                    [3] */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
} block_command_desc_type_t;

/*
 * Note: This is interpreted differently for ATA pass-through 12 vs 16.
 */
typedef struct ata_status_return_desc_type {
    sense_data_desc_header_t header;    /* Descriptor header.         [0-1] */
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t                          /*                              [2] */
        extend                  : 1,    /* Extend, ATA pass-through(16)(b0) */
        reserved_byte2_bits_1_7 : 7;    /* Reserved.                (b1:b7) */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t                          /*                              [2] */
        reserved_byte2_bits_1_7 : 7,    /* Reserved.                (b1:b7) */
        extend                  : 1;    /* Extend, ATA pass-through(16)(b0) */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t error;                      /* ATA error.                   [3] */
    uint8_t count[2];                   /* ATA sector count.          [4-5] */
    uint8_t lba[6];                     /* Logical block address.    [6-11] */
    uint8_t device;                     /* Device.                     [12] */
    uint8_t status;                     /* ATA status.                 [13] */
} ata_status_return_desc_type_t;

/* --------------------------------------------------------------------------- */
/*
 * Illegal Request Sense Specific Data:
 */
typedef struct scsi_sense_illegal_request {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
        bit_pointer     : 3,        /* Bit in error of the byte.    */
        bpv             : 1,        /* Bit pointer field valid.     */
        reserved_b4_2   : 2,        /* 2 bits reserved.             */
        c_or_d          : 1,        /* Error is in cmd or data.     */
        sksv            : 1;        /* Sense key specific valid.    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
        sksv            : 1,        /* Sense key specific valid.    */
        c_or_d          : 1,        /* Error is in cmd or data.     */
        reserved_b4_2   : 2,        /* 2 bits reserved.             */
        bpv             : 1,        /* Bit pointer field valid.     */
        bit_pointer     : 3;        /* Bit in error of the byte.    */
#else
#       error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
        uint8_t field_ptr1;         /* MSB of field pointer.        */
        uint8_t field_ptr0;         /* LSB of field pointer.        */
} scsi_sense_illegal_request_t;

/*
 * Copy Aborted Sense Specific Data:
 */ 
typedef struct scsi_sense_copy_aborted {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
        bit_pointer     : 3,        /* Bit in error of the byte.    */
        bpv             : 1,        /* Bit pointer field valid.     */
        sd              : 1,        /* Segment descriptor.          */
        reserved_b4     : 1,        /* Reserved bit.                */
        c_or_d          : 1,        /* Error is in cmd or data.     */
        sksv            : 1;        /* Sense key specific valid.    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
        sksv            : 1,        /* Sense key specific valid.    */
        c_or_d          : 1,        /* Error is in cmd or data.     */
        reserved_b4     : 1,        /* Reserved bit.                */
        sd              : 1,        /* Segment descriptor.          */
        bpv             : 1,        /* Bit pointer field valid.     */
        bit_pointer     : 3;        /* Bit in error of the byte.    */
#else
#       error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
        uint8_t field_ptr1;         /* MSB of field pointer.        */
        uint8_t field_ptr0;         /* LSB of field pointer.        */
} scsi_sense_copy_aborted_t;

/*
 * Additional Sense Bytes Format for "NOT READY" Sense Key for
 * progress indiction for Format Unit, Background Self-Test, and
 * Sanitize commands. Provides percentage complete.
 */
typedef struct scsi_sense_progress_indication {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
        res_byte15_b0_7 : 7,        /* Reserved.               [15] */
        sksv        : 1;            /* Sense key specific valid.    */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
        sksv        : 1,            /* Sense key specific valid.    */
        res_byte15_b0_7 : 7;        /* Reserved.               [15] */
#else
#   error "bitfield ordering is NOT defined!"
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
    uint8_t progress_indication[2]; /* Progress indicator.  [16-17] */
} scsi_sense_progress_indication_t;

/*
 * Recovered, Medium, or Hardware Sense Key Specific Data.
 */ 
typedef struct scsi_media_error_sense {
#if defined(_BITFIELDS_LOW_TO_HIGH_)
    bitfield_t
        reserved_b0_b6  : 7,        /* Reserved.                    */
        sksv            : 1;        /* Sense key specific valid.    */
    bitfield_t
        erp_type        : 4,        /* Error recovery type.         */
        secondary_step  : 4;        /* Secondary recovery step.     */
#elif defined(_BITFIELDS_HIGH_TO_LOW_)
    bitfield_t
        sksv            : 1,        /* Sense key specific valid.    */
        reserved_b0_b6  : 7;        /* Reserved.                    */
    bitfield_t
        secondary_step  : 4,        /* Secondary recovery step.     */
        erp_type        : 4;        /* Error recovery type.         */
#endif /* defined(_BITFIELDS_LOW_TO_HIGH_) */
        uint8_t actual_retry_count; /* Actual retry steps.          */
} scsi_media_error_sense_t;

/*
 * Additional Sense Code Table Entry Format:
 */
typedef struct sense_entry {
    unsigned char sense_code;       /* Additional sense code.       */
    unsigned char sense_qualifier;  /* Sense code qualifier.        */
    char    *sense_message;         /* Error message text.          */
} sense_entry_t;

/* ============================================================================ */

#if !defined(IMP_DESC_LIST_LEN)
#  define IMP_DESC_LIST_LEN		10	/* For allocation below, this varies. */
#endif /* !defined(IMP_DESC_LIST_LEN) */

/*
 * This is the application style data for receive copy operating parameters.
 */
typedef struct receive_copy_parameters {
    hbool_t snlid;				/* Supports no list ID.			*/
    uint16_t max_cscd_descriptor_count;		/* Maximum CSCD descriptor count.	*/
    uint16_t max_segment_descriptor_count;	/* Maximum segment desc count.		*/
    uint32_t maximum_descriptor_list_length;	/* Maximum desc list length.		*/
    uint32_t maximum_segment_length;		/* Maximum segment length.		*/
    uint32_t maximum_inline_data_length;	/* Maximum inline data length.  	*/
    uint32_t held_data_limit;			/* Held data limit;			*/
    uint32_t maximum_stream_transfer_size;	/* Maximum stream transfer size.	*/
    uint16_t total_concurrent_copies;		/* Total concurrent copies.		*/
    uint8_t maximum_concurrent_copies;		/* Maximum concurrent copies.		*/
    uint8_t data_segment_granularity;		/* Data segment granularity (log 2)	*/
    uint8_t inline_data_granularity;		/* Inline data granularity (log 2)	*/
    uint8_t held_data_granularity;		/* Held data granularity (log 2).	*/
    uint8_t implemented_desc_list_length;	/* Implemented desc list length.	*/
    uint8_t implemented_desc_list[IMP_DESC_LIST_LEN];/* List of implemented desc types. */
    						/* One byte for each desc type, ordered */
} receive_copy_parameters_t;

/* ============================================================================ */
/* 
 * Note: See inquiry.h for indivual inquiry page descriptions. 
 * These definitions is normallized for application layer usage. 
 */

/* 
 * Inquiry Third Party Copy (VPD Page 0x8F):
 */
typedef struct inquiry_third_party_copy {
    uint16_t descriptor_type;
    uint16_t max_range_descriptors;
    uint32_t max_inactivity_timeout;
    uint32_t default_inactivity_timeout;
    uint64_t max_token_transfer_size;
    uint64_t optimal_transfer_count;
} inquiry_third_party_copy_t;

/* ============================================================================ */
/*
 * Inquiry Block Limits (VPD Page 0xB0):
 */
typedef struct inquiry_block_limits {
    hbool_t  wsnz;
    uint8_t  max_caw_len;
    uint16_t opt_xfer_len_granularity;
    uint32_t max_xfer_len;
    uint32_t opt_xfer_len;
    uint32_t max_prefetch_xfer_len;
    uint32_t max_unmap_lba_count;
    uint32_t max_unmap_descriptor_count;
    uint32_t optimal_unmap_granularity;
    hbool_t  unmap_granularity_alignment_valid;
    uint32_t unmap_granularity_alignment;
    uint64_t max_write_same_len;
} inquiry_block_limits_t;

/* ============================================================================ */
/*
 * Provisioning Types:
 */
#define PROVISIONING_TYPE_FULL  0
#define PROVISIONING_TYPE_THIN  2

/*
 * Inquiry Logical Block Provisioning (VPD Page 0xB2):
 */
typedef struct logical_block_provisioning {
    uint8_t threshold_exponent;
    hbool_t lbpu;
    hbool_t lbpws;
    hbool_t lbpws10;
    hbool_t lbprz;
    hbool_t anc_sup;
    hbool_t dp;
    uint8_t provisioning_type;
} inquiry_logical_block_provisioning_t;

/* ============================================================================ */

#include "scsilib.h"                  /* OS specific declarations. */

extern scsi_generic_t *init_scsi_generic(tool_specific_t *tsp);
extern void init_scsi_defaults(scsi_generic_t *sgp, tool_specific_t *tsp);
extern hbool_t libIsRetriable(scsi_generic_t *sgp);
extern int libExecuteCdb(scsi_generic_t *sgp);
extern void libReportIoctlError(scsi_generic_t *sgp, hbool_t warn_on_error);
extern void libReportScsiError(scsi_generic_t *sgp, hbool_t warn_on_error);
extern void libReportScsiSense(scsi_generic_t *sgp, int scsi_status, scsi_sense_t *ssp);
extern int Inquiry(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
                   scsi_addr_t *sap, scsi_generic_t **sgpp,
                   void *data, unsigned int len, unsigned char page,
                   unsigned int sflags, unsigned int timeout, tool_specific_t *tsp);
extern int verify_inquiry_header(inquiry_t *inquiry, inquiry_header_t *inqh, unsigned char page);

extern char *GetDeviceIdentifier(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
                                 scsi_addr_t *sap, scsi_generic_t **sgpp,
				 void *inqp, unsigned int timeout, tool_specific_t *tsp);
extern char *DecodeDeviceIdentifier(void *opaque, inquiry_t *inquiry,
                    inquiry_page_t *inquiry_page, hbool_t hyphens);
extern int GetXcopyDesignator(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
			      scsi_generic_t **sgpp, uint8_t **designator_id,
			      int *designator_len, int *designator_type, tool_specific_t *tsp);
extern char *GetTargetPortIdentifier(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
				     scsi_generic_t **sgpp, void *inqp,
				     unsigned int timeout, tool_specific_t *tsp);
extern char *DecodeTargetPortIdentifier(void *opaque, inquiry_t *inquiry, inquiry_page_t *inquiry_page);
extern char *GetSerialNumber(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
                             scsi_addr_t *sap, scsi_generic_t **sgpp,
			     void *inqp, unsigned int timeout, tool_specific_t *tsp);
extern char *GetMgmtNetworkAddress(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
				   scsi_addr_t *sap, scsi_generic_t **sgpp,
				   void *inqp, unsigned int timeout, tool_specific_t *tsp);

typedef enum id_type {
  IDT_NONE, IDT_DEVICEID, IDT_SERIALID
} idt_t;
#define IDT_BOTHIDS IDT_NONE
extern idt_t GetUniqueID(HANDLE fd, char *dsf, char **identifier, idt_t idt,
             hbool_t debug, hbool_t errlog, unsigned int timeout, tool_specific_t *tsp);
/* 
 * Application API's to return normalized data (already decoded):
 */
extern int ReceiveCopyParameters(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
				 receive_copy_parameters_t *copy_parameters, tool_specific_t *tsp);
extern int GetThirdPartyCopy(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
			     inquiry_third_party_copy_t *third_party_copy, tool_specific_t *tsp);
extern int GetBlockLimits(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
			  inquiry_block_limits_t *block_limits, tool_specific_t *tsp);
/* TODO: This API is NOT implemented yet! */
extern int GetLogicalBlockProvisioning(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
				       inquiry_logical_block_provisioning_t *block_provisioning,
				       tool_specific_t *tsp);
/* ATA API's */
extern char *AtaGetDriveFwVersion(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
                                  scsi_addr_t *sap, scsi_generic_t **sgpp,
                                  void *inqp, unsigned int timeout, tool_specific_t *tsp);
extern int AtaIdentify(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
                       scsi_addr_t *sap, scsi_generic_t **sgpp,
                       void *data, unsigned int len,
                       unsigned int sflags, unsigned int timeout, tool_specific_t *tsp);

extern int ReadCapacity10(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
			  scsi_addr_t *sap, scsi_generic_t **sgpp,
			  void *data, unsigned int len, unsigned int sflags,
			  unsigned int timeout, tool_specific_t *tsp);
extern int ReadCapacity16(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
			  scsi_addr_t *sap, scsi_generic_t **sgpp,
			  void *data, unsigned int len, unsigned int sflags,
			  unsigned int timeout, tool_specific_t *tsp);
int ReadData(scsi_io_type_t read_type, scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes);
extern int Read6(scsi_generic_t *sgp, uint32_t lba, uint8_t length, uint32_t bytes);
extern int Read10(scsi_generic_t *sgp, uint32_t lba, uint16_t length, uint32_t bytes);
extern int Read16(scsi_generic_t *sgp, uint64_t lba, uint32_t length, uint32_t bytes);
int WriteData(scsi_io_type_t write_type, scsi_generic_t *sgp, uint64_t lba, uint32_t blocks, uint32_t bytes);
extern int Write6(scsi_generic_t *sgp, uint32_t lba, uint8_t length, uint32_t bytes);
extern int Write10(scsi_generic_t *sgp, uint32_t lba, uint16_t length, uint32_t bytes);
extern int Write16(scsi_generic_t *sgp, uint64_t lba, uint32_t length, uint32_t bytes);
extern int PopulateToken(scsi_generic_t *sgp, unsigned int listid, void *data, unsigned int bytes);
extern int ReceiveRodTokenInfo(scsi_generic_t *sgp, unsigned int listid, void *data, unsigned int bytes);
extern int TestUnitReady(HANDLE fd, char *dsf, hbool_t debug, hbool_t errlog,
                         scsi_addr_t *sap, scsi_generic_t **sgpp,
			 unsigned int timeout, tool_specific_t *tsp) ;

#define StoH(fptr)      stoh(fptr, sizeof(fptr))
#define HtoS(fptr,val)  htos(fptr, val, sizeof(fptr))

extern uint64_t stoh(unsigned char *bp, size_t size);
extern void htos(unsigned char *bp, uint64_t value, size_t size);
extern int GetCdbLength (unsigned char opcode);

/* scsidata.c */

extern void DumpCdbData(scsi_generic_t *sgp);
extern void GetSenseErrors(scsi_sense_t *ssp, unsigned char *sense_key,
               unsigned char *asc, unsigned char *asq);
extern void *GetSenseDescriptor(scsi_sense_desc_t *ssdp, uint8_t desc_type);
extern void GetSenseInformation(scsi_sense_t *ssp, uint8_t *info_valid, uint64_t *info_value);
extern void GetSenseCmdSpecific(scsi_sense_t *ssp, uint64_t *cmd_spec_value);
extern void GetSenseFruCode(scsi_sense_t *ssp, uint8_t *fru_value);
extern void MapSenseDescriptorToFixed(scsi_sense_t *ssp);
extern void DumpSenseData(scsi_generic_t *sgp, hbool_t recursive, scsi_sense_t *sdp);
extern void DumpSenseDataDescriptor(scsi_generic_t *sgp, scsi_sense_desc_t *ssdp);
extern void DumpSenseDescriptors(scsi_generic_t *sgp, scsi_sense_desc_t *ssdp, int sense_length);
extern void DumpInformationSense(scsi_generic_t *sgp, information_desc_type_t *idtp);
extern void DumpCommandSpecificSense(scsi_generic_t *sgp, command_specific_desc_type_t *csp);
extern void DumpSenseKeySpecificSense(scsi_generic_t *sgp, sense_key_specific_desc_type_t *sksp);
extern void DumpIllegalRequestSense(scsi_generic_t *sgp, scsi_sense_illegal_request_t *sirp);
extern void DumpProgressIndication(scsi_generic_t *sgp, scsi_sense_progress_indication_t *skp);
extern void DumpMediaErrorSense(scsi_generic_t *sgp, scsi_media_error_sense_t *mep);
extern void DumpFieldReplaceableUnitSense(scsi_generic_t *sgp, fru_desc_type_t *frup);
extern void DumpBlockCommandSense(scsi_generic_t *sgp, block_command_desc_type_t *bcp);
extern void DumpAtaStatusReturnSense(scsi_generic_t *sgp, ata_status_return_desc_type_t *asp);

extern void print_scsi_status(scsi_generic_t *sgp, uint8_t scsi_status, uint8_t sense_key, uint8_t asc, uint8_t ascq);
extern char *ScsiAscqMsg(unsigned char asc, unsigned char asq);
extern char *SenseKeyMsg(uint8_t sense_key);
extern char *SenseCodeMsg(uint8_t error_code);
extern int LookupSenseKey(char *sense_key_name);
extern char *ScsiStatus(unsigned char scsi_status);
extern int LookupScsiStatus(char *status_name);

#include "scsi_cdbs.h"

extern void DumpXcopyData(scsi_generic_t *sgp);
extern void DumpParameterListDescriptor(scsi_generic_t *sgp, xcopy_lid1_parameter_list_t *paramp, unsigned offset);
extern void DumpTargetDescriptor(scsi_generic_t *sgp, xcopy_id_cscd_ident_desc_t *tgtdp, int target_number, unsigned offset);
/* TODO: Re-add this after code is updated for multiple descriptor types! */
//extern void DumpTargetDescriptor(scsi_generic_t *sgp, xcopy_id_cscd_desc_t *tgtdp, int target_number, unsigned offset);
extern void DumpSegmentDescriptor(scsi_generic_t *sgp, xcopy_b2b_seg_desc_t *segdpi, int segment_number, unsigned offset);

extern void DumpPTData(scsi_generic_t *sgp);
extern char *RRTICopyStatus(unsigned char copy_status);
extern void DumpRRTIData(scsi_generic_t *sgp);
extern void DumpWUTData(scsi_generic_t *sgp);
extern void DumpRangeDescriptor(scsi_generic_t *sgp, range_descriptor_t *rdp, int descriptor_number, unsigned offset);

extern void GenerateSptCmd(scsi_generic_t *sgp);

extern char *FindSegmentTypeMsg(uint8_t descriptor_type);
extern char *FindTargetTypeMsg(uint8_t target_descriptor_type);
extern char *GetDescriptorTypeMsg(char **descriptor_type, uint8_t descriptor_type_code);

#endif /* LIBSCSI_H */
