/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 2006 - 2013			    *
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
#if !defined(_SPT_UNIX_H_)
#define _SPT_UNIX_H_ 1

/* Note: stdint.h does NOT exist on HP-UX 11.11 (very old now).    */
/*	 Nor does Solaris 8! (sigh) Time to retire these old OS's! */
/*	 But that said, required typedef's are in sys/types.h      */
#include <sys/types.h>

#if defined(__linux__)
#  include <stdint.h>
#  define XFS_DIO_BLOCK_SIZE 4096
#endif /* defined(__linux__) */
#include <poll.h>

#if defined(DEC)
#  define LOG_DIAG_INFO	1
/* Tru64 Unix */
#  if defined(O_DIRECTIO)
#    define O_DIRECT	O_DIRECTIO
#  endif
#endif /* defined(DEC) */

/*
 * Define unused POSIX open flag for enabling Direct I/O in OS open.
 * Note: This is only used on Solaris and MacOS (at present).
 */
#if !defined(O_DIRECT)
# if defined(__sun)
/* To enable the equivalent of direct I/O. (unbuffered I/O) */
#  define O_DIRECT	0x400000	/* Direct disk access. */
# elif defined(MacDarwin)
#  define O_DIRECT	0x800000	/* F_NOCACHE on MacOS! */
# else /* unknown OS, force new code, no assumptions! */
#  error "Please define O_DIRECT for this operating system!"
/* Note: For Linux, this is defined as: O_DIRECT 040000 */
/*	 and _GNU_SOURCE MUST be specified to define this! */
//#  define O_DIRECT	0		/* No Direct I/O. */
# endif
#endif /* !defined(O_DIRECT) */

/*
 * Note: FreeBSD does NOT defined O_DSYNC, but O_SYNC is required for POSIX!
 */
#if !defined(O_DSYNC)
#  define O_DSYNC   O_SYNC
#endif /* !defined(O_DSYNC) */

#define HANDLE			int
#define INVALID_HANDLE_VALUE	-1

#define DIRSEP		'/'
#define DEV_PREFIX	"/dev/"		/* Physical device name prefix.	*/
#define DEV_LEN		5		/* Length of device name prefix.*/

#define DEV_DIR_PREFIX		"/dev/"
#define DEV_DIR_LEN		(sizeof(DEV_DIR_PREFIX) - 1)
#define DEV_DEVICE_LEN		128

#if defined(__hpux) || defined(SOLARIS)
#  define DEV_BDIR_PREFIX	"/dev/dsk/"
#  define DEV_BDIR_LEN		(sizeof(DEV_BDIR_PREFIX) - 1)
#  define DEV_RDIR_PREFIX	"/dev/rdsk/"
#  define DEV_RDIR_LEN		(sizeof(DEV_RDIR_PREFIX) - 1)
#endif /* defined(__hpux) || defined(SOLARIS) */

#define TEMP_DIR		"/var/tmp/"
#define TEMP_DIR_NAME		TEMP_DIR
#define TEMP_DIR_LEN		(sizeof(TEMP_DIR_NAME) - 1)

#define TRIGGER_SCRIPT	"/x/eng/localtest/noarch/bin/dt_noprog_script.ksh"

#define OS_OPEN_FILE_OP			"open"
#define OS_CLOSE_FILE_OP		"close"
#define OS_DELETE_FILE_OP		"unlink"
#define OS_FLUSH_FILE_OP		"fsync"
#define OS_READ_FILE_OP			"read"
#define OS_WRITE_FILE_OP		"write"
#define OS_PREAD_FILE_OP		"pread"
#define OS_PWRITE_FILE_OP		"pwrite"
#define OS_RENAME_FILE_OP		"rename"
#define OS_SEEK_FILE_OP			"lseek"
#define OS_TRUNCATE_FILE_OP		"ftruncate"
#define OS_CREATE_DIRECTORY_OP		"mkdir"
#define OS_REMOVE_DIRECTORY_OP		"rmdir"
#define OS_GET_FILE_ATTR_OP		"stat"
#define OS_GET_FS_INFO_OP		"statvfs"
#define OS_GET_FILE_SIZE_OP		"fstat"
#define OS_LINK_FILE_OP			"link"
#define OS_UNLINK_FILE_OP		OS_DELETE_FILE_OP
#define OS_SYMLINK_FILE_OP		"symlink"
#define OS_LOCK_FILE_OP			"lock"
#define OS_UNLOCK_FILE_OP		"unlock"
#define OS_SET_END_OF_FILE_OP		"SetEndOfFile"

#define OS_READONLY_MODE	O_RDONLY
#define OS_WRITEONLY_MODE	O_WRONLY
#define OS_READWRITE_MODE	O_RDWR

typedef off_t Offset_t;

# define os_open_file		open
#define os_close_file		close
#define os_seek_file		lseek
#define os_read_file		read
#define os_write_file		write
#define os_pread_file		pread
#define os_pwrite_file		pwrite
#define os_delete_file		unlink
#define os_flush_file		fsync
#define os_rename_file		rename
#define os_truncate_file	ftruncate
#define os_link_file		link
#define os_unlink_file		os_delete_file
#define os_symlink_file		symlink
#define os_getpid		getpid
#define os_set_random_seed	srandom
#define os_symlink_supported()	True

#define os_isCancelled(error)	( (error == ECANCELED) ? True : False)
#define os_isIoError(error)	( (error == EIO) ? True : False)
#define os_isFileNotFound(error) ( (error == ENOENT) ? True : False)
#define os_isDiskFull(error)	( ((error == ENOSPC) || (error == EDQUOT)) ? True : False)
#define os_isLocked(error)	( ((error == EACCES) || (error == EAGAIN)) ? True : False)
#define os_getDiskFullMsg(error) (error == ENOSPC) ? "No space left on device (ENOSPC)" : "Quota exceeded (EDQUOT)"
#define os_mapDiskFullError(error) error

#define os_perror		Perror
#define os_tperror		tPerror
#define os_get_error()		errno
#define os_get_error_msg(error)	strerror(error)
#define os_free_error_msg(msg)	

#if !defined(SYSLOG)

#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_INFO        6       /* informational */

//#define syslog	os_syslog
extern void os_syslog(int priority, char *format, ...);

#endif /* !defined(SYSLOG) */

#define os_sleep(value)		(void)poll(0, 0, (int)(value*MSECS))
#define os_msleep(value)	(void)poll(0, 0, (int)value)
//#define os_usleep(value)	(void)poll(0, 0, (int)(value/MSECS))
/* Note: Signals will terminate usleep() prematurely. */
#if defined(__useconds_t_defined)
# define os_usleep(value)	(void)usleep((useconds_t)value)
#else
# define os_usleep(value)	(void)usleep(value)
#endif
/* TODO: nanosleep() - high resolution sleep */
//#define os_nanosleep(value)

#define os_set_timer_resolution(value)      True
#define os_reset_timer_resolution(value)    True

#if defined(INLINE_FUNCS)
#define os_create_directory(dir_path, permissions) \
			    mkdir(dir_path, permissions)

#define os_remove_directory(dir_path) \
			    rmdir(dir_path)
#else /* !defined(INLINE_FUNCS) */
extern int os_create_directory(char *dir_path, int permissions);
extern int os_remove_directory(char *dir_path);
#endif /* defined(INLINE_FUNCS) */

extern int os_lock_file(HANDLE fd, Offset_t start, Offset_t length, int lock_type);
extern int os_unlock_file(HANDLE fd, Offset_t start, Offset_t length);

extern char *os_gethostname(void);
extern char *os_getusername(void);
extern char *ConvertBlockToRawDevice(char *block_device);
extern char *ConvertDeviceToScsiDevice(char *device);

extern hbool_t os_isEof(ssize_t count, int error);

#if defined(NEEDS_SETENV_API)

/* Older versions of HP-UX and Solaris do NOT have this API! */
extern int setenv(const char *name, const char *value, int overwrite);

#endif /* defined(NEEDS_SETENV_API) */

#endif /* !defined(_SPT_UNIX_H_) */
