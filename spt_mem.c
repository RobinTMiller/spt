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
 * Module:	spt_mem.c
 * Author:	Robin T. Miller
 * Date:	August 17th, 2013
 *
 * Description:
 *	Memory allocation functions.
 *
 * Modification History:
 *
 * August 17th, 2013 by Robin T Miller
 * 	Moving memory allocation functions here.
 */
#include "spt.h"

#if 0
/* Note: Some compilers won't allow inline functions like this! */
/*
 * Note: Callers better handle NULL return and ENOMEM or we'll crash & burn!
 */
__inline void*
Malloc(scsi_device_t *sdp, size_t bytes)
{
    void *ptr = malloc(bytes);
    if (ptr) {
	memset(ptr, '\0', bytes);
    } else {
	report_nomem(sdp, bytes);
    }
    return(ptr);
}

__inline void *
Realloc(scsi_device_t *sdp, void *ptr, size_t bytes)
{
    ptr = realloc(ptr, bytes);
    if (ptr) {
	memset(ptr, '\0', bytes);
    } else {
	report_nomem(sdp, bytes);
    }
    return(ptr);
}
#endif /* 0 */

void
report_nomem(scsi_device_t *sdp, size_t bytes)
{
    Fprintf(sdp, "Failed to allocated %u bytes!\n", bytes);
    return;
}

#if !defined(INLINE_FUNCS)

/*
 * Note: To trace more memory issues, we'de need our own Strdup() as well!
 */
void
Free(scsi_device_t *sdp, void *ptr)
{
    if (mDebugFlag) {
	Printf(sdp, "Free: Deallocating buffer at address %p...\n", ptr);
    }
    free(ptr);
    return;
}

/*
 * Malloc() - Allocate memory with appropriate error checking.
 *
 * Description:
 *      This function allocates the requested memory, performs the
 * necessary error checking/reporting, and then zeros the allocated
 * memory space.
 *
 * Inputs:
 *	bytes = The number of bytes to allocate.
 * 
 * Return Value:
 *	Returns pointer to buffer allocated, NULL on failure.
 *	Note: We terminate on memory failures, until callers handle!
 */
void *
Malloc(scsi_device_t *sdp, size_t bytes)
{
    void *bp;

    if (!bytes) {
        LogMsg(sdp, efp, logLevelDiag, 0,
               "Malloc: FIXME -> Trying to allocate %u bytes.\n", bytes);
	return(NULL);
    }
    if ( (bp = malloc(bytes)) == NULL) {
        Perror(sdp, "malloc() failed allocating %u bytes.\n", bytes);
	terminate(sdp, FAILURE);
	//return(NULL);
    } else if (mDebugFlag) {
	Printf(sdp, "Malloc: Allocated buffer at address %p of %u bytes, end %p...\n",
	       bp, bytes, ((char *)bp + bytes) );
    }
    memset(bp, '\0', bytes);
    return(bp);
}

void *
Realloc(scsi_device_t *sdp, void *bp, size_t bytes)
{
    if ( (bp = realloc(bp, bytes)) == NULL) {
        Perror(sdp, "realloc() failed allocating %u bytes.\n", bytes);
	terminate(sdp, FAILURE);
	//return(NULL);
    } else if (mDebugFlag) {
	Printf(sdp, "Realloc: Allocated buffer at address %p of %u bytes...\n", bp, bytes);
    }
    memset(bp, '\0', bytes);
    return(bp);
}

#endif /* defined(INLINE_FUNCS) */

/* ========================================================================= */

#define NEED_PAGESIZE	1	/* Define to obtain page size. */

/*
 * This structure is used by the page align malloc/free support code. These
 * "working sets" will contain the malloc-ed address and the page aligned
 * address for the free*() call.
 */
typedef struct mpa_ws {
    struct mpa_ws *next;	/* for the next on the list */
    void *palign_addr;		/* the page aligned address that's used */
    void *malloc_addr;		/* the malloc-ed address to free */
} MPA_WS;

/* Initialized and uninitialized data. */

static pthread_mutex_t malloc_mutex;
static int malloc_mutex_inited = 0;

static MPA_WS mpa_qhead;	/* local Q head for the malloc stuctures */

/*
 * This is a local allocation routine to alloc and return to the caller a
 * system page aligned buffer.  Enough space will be added, one more page, to
 * allow the pointers to be adjusted to the next page boundry.  A local linked
 * list will keep copies of the original and adjusted addresses.  This list 
 * will be used by free_palign() to free the correct buffer.
 *
 * Inputs:
 * 	bytes = The number of bytes to allocate.
 * 	offset = The offset to misalign from page aligned memory.
 *
 * Return Value:
 *	Returns page aligned buffer or NULL if no memory available.
 */
void *
malloc_palign(scsi_device_t *sdp, size_t bytes, int offset)
{
    int error;
    size_t alloc_size;
    MPA_WS *ws;		/* pointer for the working set */
#if defined(NEED_PAGESIZE)
    int page_size;	/* for holding the system's page size */
#endif /* defined(NEED_PAGESIZE) */

    if (!bytes) {
        LogMsg(sdp, efp, logLevelDiag, 0,
               "malloc_palign: FIXME -> Trying to allocate %u bytes.\n", bytes);
	return(NULL);
    }
    if ( !malloc_mutex_inited ) {
	error = pthread_mutex_init(&malloc_mutex, NULL);
	if ( error ) {
	    tPerror(sdp, error, "pthread_mutex_init() failed!");
	}
	malloc_mutex_inited++;
    }
    /*
     * The space for the working set structure that will go on the queue
     * is allocated first.
     */
    ws = (MPA_WS *)Malloc(sdp, sizeof(MPA_WS) );
    if ( ws == (MPA_WS *)NULL ) {
	return( (char *)NULL );
    }

#if defined(NEED_PAGESIZE)
    page_size = getpagesize();
#endif /* defined(NEED_PAGESIZE) */

    alloc_size = (bytes + page_size + offset);
    /*
     * Using the requested size, from the argument list, and the page size
     * from the system allocate enough space to page align the requested 
     * buffer.  The original request will have the space of one system page
     * added to it.  The pointer will be adjusted.
     */
    ws->malloc_addr = Malloc(sdp, alloc_size);
    if ( ws->malloc_addr == NULL ) {
	Free(sdp, ws);
	return( NULL );
    } else {
	(void)memset(ws->malloc_addr, 0, alloc_size);
    }

    error = pthread_mutex_lock(&malloc_mutex);
    if ( error ) {
	tPerror(sdp, error, "pthread_mutex_lock() failed!");
	return (NULL);
    }
    /*
     * Now align the allocated address to a page alignment and offset.
     */
    ws->palign_addr = (void *)( ((ptr_t)ws->malloc_addr + page_size) & ~(page_size-1) );
    ws->palign_addr = (void *)((ptr_t)ws->palign_addr + offset);

    /*
     * Put the working set onto the linked list so that the original malloc-ed
     * buffer can be freeed when the user program is done with it.
     */
    ws->next = mpa_qhead.next;	    /* just put it at the head */
    mpa_qhead.next = ws;

    error = pthread_mutex_unlock(&malloc_mutex);
    if ( error ) {
	tPerror(sdp, error, "pthread_mutex_unlock() failed!");
    }

    if (mDebugFlag) {
	Printf(sdp, "malloc_palign: Allocated buffer at address %p of %u bytes...\n",
	       ws->palign_addr, (bytes + offset));
    }
    return( ws->palign_addr );
}

/*
 * This is a local free routine to return to the system a previously alloc-ed
 * buffer.  A local linked list keeps copies of the original and adjusted
 * addresses.  This list is used by this routine to free the correct buffer.
 *
 * Inputs:
 *	pa_addr = The page aligned buffer to free.
 *
 * Return Value:
 *	void
 */
void
free_palign(scsi_device_t *sdp, void *pa_addr)
{
    int error;
    MPA_WS *p, *q;

    if (mDebugFlag) {
	Printf(sdp, "free_palign: Freeing buffer at address %p...\n", pa_addr);
    }

    error = pthread_mutex_lock(&malloc_mutex);
    if (error) {
	tPerror(sdp, error, "pthread_mutex_lock() failed!");
	/* race noted... */
    }
    /*
     * Walk along the malloc-ed memory linked list, watch for a match on
     * the page aligned address.  If a match is found break out of the loop.
     */
    p = mpa_qhead.next;	    /* set the pointers */
    q = NULL;

    while ( p != NULL ) {
	if ( p->palign_addr == pa_addr ) /* found the buffer */
	    break;
	q = p;		    /* save current */
	p = p->next;		/* get next */
    }

    /*
     * After falling out of the loop the pointers are at the place where
     * some work has to be done, (this could also be at the beginning).
     */
    if ( p != NULL ) {

	/* Where on the list is it, check for making it empty. */

	if ( q == NULL ) {	   /* at the front */
	    mpa_qhead.next = p->next;	/* pop off front */
	} else {		/* inside the list */
	    q->next = p->next;	    /* pop it */
	}

	Free(sdp, p->malloc_addr);

	/* Now free up the working set, it is not needed any more. */

	Free(sdp, p);
    } else { /* Should never happen, if we're coded right! */
        LogMsg(sdp, efp, logLevelError, 0,
	       "BUG: Did not find buffer at address %p...\n", pa_addr);
    }
    error = pthread_mutex_unlock(&malloc_mutex);
    if ( error ) {
	tPerror(sdp, error, "pthread_mutex_unlock() failed!");
    }
    return;
}
