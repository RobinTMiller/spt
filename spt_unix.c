/****************************************************************************
 *									    *
 *			  COPYRIGHT (c) 1988 - 2013			    *
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
 * Module:	spt_unix.c
 * Author:	Robin T. Miller
 *
 * Description:
 *	This module contains *unix OS specific functions.
 * 
 * Modification History: 
 *	Updated AIX mount function to lookup file system type.  
 */
#include "spt.h"

#include <fcntl.h>
#include <sys/statvfs.h>

char *
os_ctime(time_t *timep, char *time_buffer, int timebuf_size)
{
    char *bp;

    bp = ctime_r(timep, time_buffer);
    if (bp == NULL) {
	Perror(NULL, "ctime_r() failed");
	(int)sprintf(time_buffer, "<no time available>\n");
    } else {
	if (bp = strrchr(time_buffer, '\n')) {
	    *bp = '\0';
	}
    }
    return(time_buffer);
}

#if !defined(INLINE_FUNCS)

int
os_create_directory(char *dir_path, int permissions)
{
    return( mkdir(dir_path, permissions) );
}

int
os_remove_directory(char *dir_path)
{
    return( rmdir(dir_path) );
}

#endif /* !defined(INLINE_FUNCS) */

char *
os_getcwd(void)
{
    char path[PATH_BUFFER_SIZE];

    if ( getcwd(path, sizeof(path)) == NULL ) {
	return(NULL);
    } else {
	return ( strdup(path) );
    }
}

uint64_t
os_get_file_size(char *path, HANDLE handle)
{
    struct stat sb, *stp = &sb;
    int64_t filesize = -1LL;
    int status;

    if (handle == INVALID_HANDLE_VALUE) {
	status = stat(path, stp);
    } else {
	status = fstat(handle, stp);
    }
    if (status == SUCCESS) {
	return( (uint64_t)stp->st_size );
    } else {
	return( (uint64_t)filesize );
    }
}

char *
os_gethostname(void)
{
    char hostname[MAXHOSTNAMELEN];
    if (gethostname(hostname, sizeof(hostname)) == FAILURE) {
	Perror(NULL, "gethostname() failed");
	return(NULL);
    }
    return ( strdup(hostname) );
}

char *
os_getusername(void)
{
    size_t bufsize = STRING_BUFFER_SIZE;
    char username[STRING_BUFFER_SIZE];

    if (getlogin_r(username, bufsize) != SUCCESS) {
	Perror(NULL, "getlogin_r() failed");
	return(NULL);
    }
    return ( strdup(username) );
}

/* Note: Overloaded, thus new API's below! */
hbool_t
os_file_information(char *file, uint64_t *filesize, hbool_t *is_dir)
{
    struct stat sb;
    
    if (stat(file, &sb) == SUCCESS) {
	if (filesize) {
	    *filesize = (uint64_t)sb.st_size;
	}
	if (is_dir) {
	    if (sb.st_mode & S_IFDIR) {
		*is_dir = True;
	    } else {
		*is_dir = False;
	    }
	}
	return(True);
    } else {
	return(False);
    }
}

hbool_t
os_file_exists(char *file)
{
    struct stat sb;
    
    if (stat(file, &sb) == SUCCESS) {
	return(True);
    } else {
	return(False);
    }
}

hbool_t
os_isdir(char *dirpath)
{
    struct stat sb;
    
    if (stat(dirpath, &sb) == SUCCESS) {
	if (sb.st_mode & S_IFDIR) {
	    return(True);
	} else {
	    return(False);
	}
    } else {
	return(False);
    }
}

int
os_set_priority(scsi_device_t *sdp, HANDLE hThread, int priority)
{
    int status;

    status = nice(priority);
    return(status);
}

#if !defined(SYSLOG)

void
os_syslog(int priority, char *format, ...)
{
    return;
}

#endif /* !defined(SYSLOG) */

void
tPerror(scsi_device_t *sdp, int error, char *format, ...)
{
    char msgbuf[LOG_BUFSIZE];
    va_list ap;
    FILE *fp;

    if (sdp == NULL) sdp = master_sdp;
    fp = (sdp) ? sdp->efp : efp;
    va_start(ap, format);
    (void)vsprintf(msgbuf, format, ap);
    va_end(ap);
    LogMsg(sdp, sdp->efp, logLevelError, 0,
	   "%s, errno = %d - %s\n", msgbuf, errno, strerror(error));
    return;
}

#if defined(NEEDS_SETENV_API)

/* Note: Solaris 8 does NOT have this POSIX API! :-( */

int
setenv(const char *name, const char *value, int overwrite)
{
    /* Note: This is memory that will never be given back! */
    char *string = malloc(strlen(name) + strlen(value) + 2);
    if (string == NULL) return(-1);
    (void)sprintf("%s=%s", name, value);
    return ( putenv(string) );
}

#endif /* defined(NEEDS_SETENV_API) */

/*
 * os_isEof() - Determine if this is an EOF condition.
 *
 * Description:
 *	Generally, a read EOF is a count of 0, while writes are failed
 * with errno set to indicate ENOSPC. But, apparently POSIX does *not*
 * define this for direct disk and file system, thus this ugliness.
 * Note: Some of these extra errors are caused by seeks past EOM.
 */
hbool_t
os_isEof(ssize_t count, int error)
{
    if ( (count == 0) ||
	 ( (count < 0) &&
	   ( (error == ENOSPC) ||
	     (error == ENXIO)) ||
	     (error == EINVAL) ||
	     (error == EDQUOT) ) ) { /* For file systems, treat like EOF! */
	return(True);
    } else {
	return(False);
    }
}

int
os_lock_file(HANDLE fd, Offset_t start, Offset_t length, int type)
{
    struct flock fl;
    struct flock *flp = &fl;
    int status;

    memset(flp, '\0', sizeof(*flp));
    flp->l_whence = SEEK_SET;
    flp->l_start  = start;
    flp->l_len    = length;
    flp->l_type   = type;

    status = fcntl(fd, F_SETLK, flp);
    return(status);
}

int
os_unlock_file(HANDLE fd, Offset_t start, Offset_t length)
{
    struct flock fl;
    struct flock *flp = &fl;
    int status;

    memset(flp, '\0', sizeof(*flp));
    flp->l_whence = SEEK_SET;
    flp->l_start  = start;
    flp->l_len    = length;
    flp->l_type   = F_UNLCK;

    status = fcntl(fd, F_SETLK, flp);
    return(status);
}

uint64_t
os_create_random_seed(void)
{
    struct timeval time_val;
    struct timeval *tv = &time_val;

    if ( gettimeofday(tv, NULL) == SUCCESS ) {
	return( (uint64_t)tv->tv_sec + (uint64_t)tv->tv_usec );
    } else {
	return(0);
    }
}
