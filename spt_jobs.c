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
 * Module:	spt_jobs.c
 * Author:	Robin T. Miller
 *
 * Description:
 *	This file contains functions to handle spt jobs.
 *
 * Modification History:
 * 
 * January 12th, 2021 by Robin T. Miller
 * 	Initial creation.
 * 
 */
#include "spt.h"

/*
 * Globals for Tracking Job Information:
 */ 
job_id_t job_id = 0;		/* The next job ID. */
job_info_t jobsList;		/* The jobs list header. */
job_info_t *jobs = NULL;	/* List of active jobs. */
pthread_mutex_t jobs_lock;	/* Job queue lock. */
pthread_mutexattr_t jobs_lock_attr; /* The jobs lock attributes. */

#define QUEUE_EMPTY(jobs)	(jobs->ji_flink == jobs)

#define JOB_WAIT_DELAY		1	/* The job wait delay (in secs) */

char *job_state_table[] = {
    "stopped", "running", "finished", "paused", "terminating", "cancelling", NULL};

char *thread_state_table[] = {
    "stopped", "starting", "running", "finished", "joined", "paused", "terminating", "cancelling", NULL};

/*
 * Forward References:
 */
void *a_job(void *arg);
int acquire_jobs_lock(scsi_device_t *sdp);
int release_jobs_lock(scsi_device_t *sdp);
int jobs_ne_state(scsi_device_t *sdp, jstate_t job_state);
int jobs_eq_state(scsi_device_t *sdp, jstate_t job_state);
int jobs_finished(scsi_device_t *sdp);

job_info_t *find_job_by_id(scsi_device_t *sdp, uint32_t job_id, hbool_t lock_jobs);
job_info_t *find_job_by_tag(scsi_device_t *sdp, char *tag, hbool_t lock_jobs);
job_info_t *find_jobs_by_tag(scsi_device_t *sdp, char *tag, job_info_t *pjob, hbool_t lock_jobs);

job_info_t *create_job(scsi_device_t *sdp);
int insert_job(scsi_device_t *sdp, job_info_t *job);
int remove_job(scsi_device_t *msdp, job_info_t *job, hbool_t lock_jobs);

/*
 * Start of Job Functions:
 */ 
int
initialize_jobs_data(scsi_device_t *sdp)
{
    pthread_mutexattr_t *attrp = &jobs_lock_attr;
    int status;

    if ( (status = pthread_mutexattr_init(attrp)) !=SUCCESS) {
	tPerror(sdp, status, "pthread_mutexattr_init() of jobs mutex attributes failed!");
	return(FAILURE);
    }
    if ( (status = pthread_mutexattr_settype(attrp, PTHREAD_MUTEX_ERRORCHECK)) != SUCCESS) {
	tPerror(sdp, status, "pthread_mutexattr_settype() of jobs mutex type failed!");
	return(FAILURE);
    }
    if ( (status = pthread_mutex_init(&jobs_lock, attrp)) != SUCCESS) {
	tPerror(sdp, status, "pthread_mutex_init() of jobs lock failed!");
    }
    jobs = &jobsList;
    jobs->ji_flink = jobs;
    jobs->ji_blink = jobs;
    //sdp->di_job = jobs;
    return(status);
}

int
acquire_jobs_lock(scsi_device_t *sdp)
{
    int status = pthread_mutex_lock(&jobs_lock);
    if (status != SUCCESS) {
	tPerror(sdp, status, "Failed to acquire jobs mutex!");
    }
    return(status);
}

int
release_jobs_lock(scsi_device_t *sdp)
{
    int status = pthread_mutex_unlock(&jobs_lock);
    if (status != SUCCESS) {
	tPerror(sdp, status, "Failed to unlock jobs mutex!");
    }
    return(status);
}

int
jobs_active(scsi_device_t *sdp)
{
    int count;

    count = jobs_ne_state(sdp, JS_FINISHED);
    return(count);
}

int
jobs_ne_state(scsi_device_t *sdp, jstate_t job_state)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int count = 0, status;

    if ( QUEUE_EMPTY(jobs) ) return(count);
    if ( (status = acquire_jobs_lock(sdp)) != SUCCESS) {
	return(count);
    }
    do {
	/* This state is set prior to thread exit. */
	if (job->ji_job_state != job_state) {
	    count++;
	}
    } while ( ((job = job->ji_flink) != jhdr) );
    (void)release_jobs_lock(sdp);
    return(count);
}

int
jobs_eq_state(scsi_device_t *sdp, jstate_t job_state)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int count = 0, status;

    if ( QUEUE_EMPTY(jobs) ) return(count);
    if ( (status = acquire_jobs_lock(sdp)) != SUCCESS) {
	return(count);
    }
    do {
	/* This state is set prior to thread exit. */
	if (job->ji_job_state == job_state) {
	    count++;
	}
    } while ( ((job = job->ji_flink) != jhdr) );
    (void)release_jobs_lock(sdp);
    return(count);
}

int
jobs_finished(scsi_device_t *sdp)
{
    job_info_t *jhdr = jobs;
    job_info_t *jptr = jhdr->ji_flink;
    job_info_t *job;
    int status = SUCCESS;

    if ( QUEUE_EMPTY(jobs) ) return(status);
    if ( (status = acquire_jobs_lock(sdp)) != SUCCESS) {
	return(status);
    }
    do {
	job = jptr;
	jptr = job->ji_flink;
	if (job->ji_job_state == JS_FINISHED) {
	    if (job->ji_job_status == FAILURE) status = job->ji_job_status;
	    if (job->ji_job_tag) {
		Printf(sdp, "Job %u (%s) completed with status %d\n",
		       job->ji_job_id, job->ji_job_tag, status);
	    } else {
		Printf(sdp, "Job %u completed with status %d\n", job->ji_job_id, status);
	    }
	    remove_job(sdp, job, False);
	    /* next job, please! */
	}
    } while ( jptr != jhdr );
    (void)release_jobs_lock(sdp);
    return(status);
}

/*
 * find_job_by_id() - Find a job by its' job ID.
 *
 * Inputs:
 *	sdp = The SCSI device pointer.
 *	job_id = The job ID to find.
 *	lock_jobs = Flag controlling job lock.
 *	  True = acquire lock, False = do not lock.
 *
 * Outputs:
 *	Returns a job if found, else NULL if not found.
 *	When job is found, the jobs lock is still held.
 *	Therefore, the caller *must* release the lock!
 */
job_info_t *
find_job_by_id(scsi_device_t *sdp, job_id_t job_id, hbool_t lock_jobs)
{
    job_info_t *jhdr = jobs;
    job_info_t *jptr = jhdr->ji_flink;
    job_info_t *job = NULL;

    if ( QUEUE_EMPTY(jobs) ) return(job);
    if (lock_jobs == True) {
	if (acquire_jobs_lock(sdp) != SUCCESS) {
	    return(job);
	}
    }
    do {
	/* Find job entry. */
	if (jptr->ji_job_id == job_id) {
	    job = jptr;
	    break;
	}
    } while ( (jptr = jptr->ji_flink) != jhdr );

    if ( (lock_jobs == True) && (job == NULL) ) {
	(void)release_jobs_lock(sdp);
    }
    return(job);
}

/*
 * find_job_by_tag() - Find a job by its' job tag.
 *
 * Inputs:
 *	sdp = The SCSI device pointer.
 *	job_tag = The job tag to find.
 *	lock_jobs = Flag controlling job lock.
 *	  True = acquire lock, False = do not lock.
 *
 * Outputs:
 *	Returns a job if found, else NULL if not found.
 *	When job is found, the jobs lock is still held.
 *	Therefore, the caller *must* release the lock!
 */
job_info_t *
find_job_by_tag(scsi_device_t *sdp, char *tag, hbool_t lock_jobs)
{
    job_info_t *jhdr = jobs;
    job_info_t *jptr = jhdr->ji_flink;
    job_info_t *job = NULL;

    if ( QUEUE_EMPTY(jobs) ) return(job);
    if (lock_jobs == True) {
	if (acquire_jobs_lock(sdp) != SUCCESS) {
	    return(job);
	}
    }
    do {
	/* Find job entry. */
	if ( jptr->ji_job_tag && (strcmp(jptr->ji_job_tag,tag) == 0) ) {
	    job = jptr;
	    break;
	}
    } while ( (jptr = jptr->ji_flink) != jhdr );

    if ( (lock_jobs == True) && (job == NULL) ) {
	(void)release_jobs_lock(sdp);
    }
    return(job);
}

/*
 * find_job_by_tag() - Find a job by its' job tag.
 *
 * Inputs:
 *	sdp = The SCSI device pointer.
 * 	tag = The job tag to find.
 * 	pjob = The previous job (context for next job).
 *	lock_jobs = Flag controlling job lock.
 *	  True = acquire lock, False = do not lock.
 *
 * Outputs:
 *	Returns a job if found, else NULL if not found.
 *	When job is found, the jobs lock is still held.
 *	Therefore, the caller *must* release the lock!
 */
job_info_t *
find_jobs_by_tag(scsi_device_t *sdp, char *tag, job_info_t *pjob, hbool_t lock_jobs)
{
    job_info_t *jhdr = jobs;
    job_info_t *jptr = jhdr;
    job_info_t *job = NULL;
    int status = SUCCESS;

    if (lock_jobs == True) {
	if ( (status = acquire_jobs_lock(sdp)) != SUCCESS) {
	    return(job);
	}
    }
    if (pjob) {
	jptr = pjob;	/* Start at the previous job. */
    }
    if ( QUEUE_EMPTY(jptr) ) {
	if (lock_jobs == True) {
	    (void)release_jobs_lock(sdp);
	}
	return(job);
    }
    while ( (jptr = jptr->ji_flink) != jhdr ) {
	/* Find job entry by tag. */
	if ( jptr->ji_job_tag && (strcmp(jptr->ji_job_tag,tag) == 0) ) {
	    job = jptr;
	    break;
	}
    }

    if ( (lock_jobs == True) && (job == NULL) ) {
	(void)release_jobs_lock(sdp);
    }
    return(job);
}

job_info_t *
create_job(scsi_device_t *sdp)
{
    job_info_t *job = Malloc(sdp, sizeof(job_info_t));

    if (job) {
	job->ji_job_id = sdp->job_id;
	job->ji_job_tag = sdp->job_tag;
	sdp->job_tag = NULL;
    }
    return(job);
}

int
insert_job(scsi_device_t *sdp, job_info_t *job)
{
    job_info_t *jhdr = jobs, *jptr;
    int status;
    
    /*
     * Note: Job threads started, so queue even if lock fails!
     *       May revert later, but recent bug was misleading! ;(
     */
    status = acquire_jobs_lock(sdp);
    jptr = jhdr->ji_blink;
    jptr->ji_flink = job;
    job->ji_blink = jptr;
    job->ji_flink = jhdr;
    jhdr->ji_blink = job;
    if (status == SUCCESS) {
	status = release_jobs_lock(sdp);
    }
    return(status);
}

int
remove_job(scsi_device_t *msdp, job_info_t *job, hbool_t lock_jobs)
{
    job_info_t *jptr;
    int status = SUCCESS;
    int lock_status;

    if (lock_jobs == True) {
	if ( (lock_status = acquire_jobs_lock(msdp)) != SUCCESS) {
	    return(lock_status);
	}
    }
    jptr = job->ji_blink;
    jptr->ji_flink = job->ji_flink;
    job->ji_flink->ji_blink = jptr;

    /* Note: dt uses a cleanup_job() but does alot more than this! */
    /* Beware: Our wait_for_threads() has already cleaned up devices. */
    if (job->ji_job_tag) {
        FreeStr(msdp, job->ji_job_tag);
        job->ji_job_tag = NULL;
    }
    if (job->ji_tinfo) {
	FreeMem(sdp, job->ji_tinfo, sizeof(*job->ji_tinfo));
	job->ji_tinfo = NULL;
    }
    FreeMem(msdp, job, sizeof(*job));
    if ( (lock_jobs == True) && (lock_status == SUCCESS) ) {
	(void)release_jobs_lock(msdp);
    }
    return(status);
}

void *
a_job(void *arg)
{
    job_info_t *job = arg;
    threads_info_t *tip = job->ji_tinfo;

    job->ji_job_status = wait_for_threads(tip);
    job->ji_job_state = JS_FINISHED;
    /* Note: Cleanup occurs after waiting for the job. */
    pthread_exit(tip);
    return(NULL);
}

/*
 * Note: This is only required for async jobs today! 
 *  
 * Unlike dt, the threads have already been started, and we are only 
 * initiate the job  so we can wait for these outstanding threads. 
 * One day, we may switch to dt jobs method, but today it's partial! 
 */
int
execute_job(scsi_device_t *msdp, threads_info_t *tip)
{
    job_info_t *job;
    int pstatus, status = SUCCESS;

    job = create_job(msdp);
    if (job == NULL) return(FAILURE);
    job->ji_tinfo = tip;
    (void)insert_job(msdp, job);
    job->ji_job_state = JS_RUNNING;

    /* Create a job thread to wait for and complete the job/threads. */
    pstatus = pthread_create( &msdp->thread_id, tdattrp, a_job, job );
    if (pstatus != SUCCESS) {
	errno = pstatus;
	Perror (msdp, "pthread_create() failed");
	(void)remove_job(msdp, job, True);
	return(FAILURE);
    }
    return(status);
}

/* ========================================================================= */

int
show_jobs(scsi_device_t *sdp, job_id_t job_id, char *job_tag, hbool_t verbose)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int status = SUCCESS;

    if ( QUEUE_EMPTY(jobs) ) {
	Wprintf(sdp, "There are no jobs active!\n");
	return(status);
    }
    if (job_id) {
	status = show_job_by_id(sdp, job_id);
    } else if (job_tag) {
	status = show_jobs_by_tag(sdp, job_tag);
    } else {
	if ( (status = acquire_jobs_lock(sdp)) != SUCCESS) {
	    return(status);
	}
	do {
	    show_job_info(sdp, job, verbose);
	} while ( (job = job->ji_flink) != jhdr);
	(void)release_jobs_lock(sdp);
    }
    return(status);
}

int
show_job_by_id(scsi_device_t *sdp, job_id_t job_id)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_id(sdp, job_id, True)) {
	show_job_info(sdp, job, True);
	(void)release_jobs_lock(sdp);
    } else {
	Eprintf(sdp, "Job %u does *not* exist!\n", job_id);
	status = FAILURE;
    }
    return(status);
}

int
show_job_by_tag(scsi_device_t *sdp, char *job_tag)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    
    if (job = find_job_by_tag(sdp, job_tag, True)) {
	show_job_info(sdp, job, True);
	(void)release_jobs_lock(sdp);
    } else {
	Eprintf(sdp, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    }
    return(status);
}

int
show_jobs_by_tag(scsi_device_t *sdp, char *job_tag)
{
    job_info_t *job = NULL;
    int jobs_found = 0;
    int status = SUCCESS;
    hbool_t lock_jobs = True;
    
    while (job = find_jobs_by_tag(sdp, job_tag, job, lock_jobs)) {
	jobs_found++;
	show_job_info(sdp, job, True);
	lock_jobs = False;
    }
    if (jobs_found == 0) {
	Eprintf(sdp, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    } else {
	(void)release_jobs_lock(sdp);
    }
    return(status);
}

void
show_job_info(scsi_device_t *sdp, job_info_t *job, hbool_t show_threads_flag)
{
    char fmt[STRING_BUFFER_SIZE];
    char *bp = fmt;

    if (job->ji_job_tag) {
	bp += sprintf(bp, "Job %u (%s) is %s (%d thread%s)",
		      job->ji_job_id, job->ji_job_tag,
		      job_state_table[job->ji_job_state],
		      job->ji_tinfo->ti_threads,
		      (job->ji_tinfo->ti_threads > 1) ? "s" : "");
    } else {
	bp += sprintf(bp, "Job %u is %s (%d thread%s)",
		      job->ji_job_id, job_state_table[job->ji_job_state],
		      job->ji_tinfo->ti_threads,
		      (job->ji_tinfo->ti_threads > 1) ? "s" : "");
    }
    if (job->ji_job_state == JS_FINISHED) {
	bp += sprintf(bp, ", with status %d\n", job->ji_job_status);
    } else {
	bp += sprintf(bp, "\n");
    }
    PrintLines(sdp, fmt);
    /* Note: The threads information may have been freed already! */
    if (show_threads_flag && (job->ji_job_state != JS_FINISHED)) {
	show_threads_info(sdp, job->ji_tinfo);
    }
    return;
}

void
show_threads_info(scsi_device_t *msdp, threads_info_t *tip)
{
    scsi_device_t *sdp;
    int thread;

    for (thread = 0; (thread < tip->ti_threads); thread++) {
	char fmt[PATH_BUFFER_SIZE];
	char *bp = fmt;
	sdp = &tip->ti_sds[thread];
	bp += sprintf(bp, "  Thread: %d, State: %s, Devices: %d\n",
		      sdp->thread_number, thread_state_table[sdp->thread_state], sdp->io_devices);
	if (sdp->cmd_line) {
	    /* Skip the dt path. */
	    char *cmd = strchr(sdp->cmd_line, ' ');
	    if (cmd) {
		cmd++;
	    } else {
		cmd = sdp->cmd_line;
	    }
	    bp += sprintf(bp, "  -> %s\n", cmd);
	}
	PrintLines(msdp, fmt);
    }
    return;
}

/*
 * wait_for_jobs() - Wait for all jobs.
 */ 
int
wait_for_jobs(scsi_device_t *sdp, job_id_t job_id, char *job_tag)
{
    job_info_t *jhdr = jobs;
    job_info_t *job = jhdr->ji_flink;
    int status = SUCCESS;
    hbool_t first_time = True;
    int count = 0;

    if ( QUEUE_EMPTY(jobs) ) {
	Wprintf(sdp, "There are no active jobs!\n");
	return(status);
    }
    if (job_id) {
	status = wait_for_job_by_id(sdp, job_id);
    } else if (job_tag) {
	status = wait_for_jobs_by_tag(sdp, job_tag);
    } else {
	hbool_t first_time = True;
	while ( (count = jobs_active(sdp)) ) {
	    if (CmdInterruptedFlag) break;
	    if (first_time || sdp->jDebugFlag) {
		Printf(sdp, "Waiting on %u job%s to complete...\n",
		       count, (count > 1) ? "s" : "");
		first_time = False;
	    }
	    (void)os_sleep(JOB_WAIT_DELAY);
	}
	status = jobs_finished(sdp);
    }
    return(status);
}

int
wait_for_job_by_id(scsi_device_t *sdp, job_id_t job_id)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    hbool_t first_time = True;
    int job_found = 0, job_finished = 0;
    
    while (job = find_job_by_id(sdp, job_id, True)) {
	job_found++;
	if (job->ji_job_state != JS_FINISHED) {
	    if (first_time || sdp->jDebugFlag) {
		Printf(sdp, "Waiting for Job %u, active threads %u...\n",
		       job->ji_job_id, job->ji_tinfo->ti_threads);
		first_time = False;
	    }
	    (void)release_jobs_lock(sdp);
            //if (CmdInterruptedFlag) break;
	    (void)os_sleep(JOB_WAIT_DELAY);
	    continue;
	}
	job_finished++;
	status = job->ji_job_status;
	(void)remove_job(sdp, job, False);
	(void)release_jobs_lock(sdp);
	break;
    }
    if (job_found == 0) {
	Eprintf(sdp, "Job %u does *not* exist!\n", job_id);
	status = FAILURE;
    } else if (job_finished == 0) {
	Eprintf(sdp, "Job %u did *not* finish!\n", job_id);
	status = FAILURE;
    }
    return(status);
}

int
wait_for_job_by_tag(scsi_device_t *sdp, char *job_tag)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    hbool_t first_time = True;
    int job_found = 0, job_finished = 0;
    
    while (job = find_job_by_tag(sdp, job_tag, True)) {
	job_found++;
	if (job->ji_job_state != JS_FINISHED) {
	    if (first_time || sdp->jDebugFlag) {
		Printf(sdp, "Waiting for Job %u (%s), active threads %u...\n",\
		       job->ji_job_id, job->ji_job_tag, job->ji_tinfo->ti_threads);
		first_time = False;
	    }
	    (void)release_jobs_lock(sdp);
            //if (CmdInterruptedFlag) break;
	    (void)os_sleep(JOB_WAIT_DELAY);
	    continue;
	}
	job_finished++;
	status = job->ji_job_status;
	(void)remove_job(sdp, job, False);
	(void)release_jobs_lock(sdp);
	break;
    }
    if (job_found == 0) {
	Eprintf(sdp, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    } else if (job_finished == 0) {
	Eprintf(sdp, "Jobs with tag %s did *not* finish!\n", job_tag);
	status = FAILURE;
    }
    return(status);
}

int
wait_for_jobs_by_tag(scsi_device_t *sdp, char *job_tag)
{
    job_info_t *job = NULL;
    int status = SUCCESS;
    hbool_t first_time = True;
    int jobs_found = 0, jobs_finished = 0;
    
    /* Find first or next job. */
    while (job = find_job_by_tag(sdp, job_tag, True)) {
	jobs_found++;
	if (job->ji_job_state != JS_FINISHED) {
	    if (first_time || sdp->jDebugFlag) {
		Printf(sdp, "Waiting for Job %u (%s), active threads %u...\n",
		       job->ji_job_id, job->ji_job_tag, job->ji_tinfo->ti_threads);
		first_time = False;
	    }
	    (void)release_jobs_lock(sdp);
            //if (CmdInterruptedFlag) break;
	    (void)os_sleep(JOB_WAIT_DELAY);
	    continue;
	}
	first_time = True;
	jobs_finished++;
	/* Set status and remove this job. */
	if (job->ji_job_status == FAILURE) {
	    status = job->ji_job_status;
	}
	(void)release_jobs_lock(sdp);
	(void)remove_job(sdp, job, True);
    }
    if (jobs_found == 0) {
	Eprintf(sdp, "Job tag %s does *not* exist!\n", job_tag);
	status = FAILURE;
    } else if (jobs_finished == 0) {
	Eprintf(sdp, "Jobs with tag %s did *not* finish!\n", job_tag);
	status = FAILURE;
    }
    return(status);
}

int
wait_for_threads(threads_info_t *tip)
{
    scsi_device_t 	*sdp;
    scsi_generic_t	*sgp;
    io_params_t		*iop;
    void		*thread_status = NULL;
    int			thread, pstatus, status = SUCCESS;

    /*
     * Now, wait for each thread to complete.
     */
    for (thread = 0; (thread < tip->ti_threads); thread++) {
	sdp = &tip->ti_sds[thread];
	iop = &sdp->io_params[IO_INDEX_BASE];
	sgp = &iop->sg;
	pstatus = pthread_join( sdp->thread_id, &thread_status );
	tip->ti_finished++;
	if (pstatus != SUCCESS) {
	    errno = pstatus;
	    Perror(sdp, "pthread_join() failed");
	    /* continue waiting for other threads */
	} else {
	    sdp->thread_state = TS_FINISHED;
#if !defined(WIN32)
            /* Note: Thread status is 0 (NULL) on Windows! */
            if ( (thread_status == NULL) || (long)thread_status == -1 ) {
		Fprintf(sdp, "Sanity check of thread status failed for device %s!\n", sgp->dsf);
		Fprintf(sdp, "Thread status is NULL or -1, assuming cancelled, setting FAILURE status!\n");
		status = FAILURE;   /* Assumed canceled, etc. */
	    } else {
		if (sdp != (scsi_device_t *)thread_status) {
		    Fprintf(sdp, "Sanity check of thread status failed for device %s!\n", sgp->dsf);
		    Fprintf(sdp, "Expected sdp = %p, Received: %p\n", sdp, thread_status);
		    abort(); /* sanity */
		}
	    }
#endif /* !defined(WIN32) */
	    if (sdp->status == FAILURE) {
		status = sdp->status;
	    }
	    /* Note: We may need to delay cleanup until the job is removed, like dt! */
	    /* But that said, we cannot do this until all execution is done via jobs! */
	    cleanup_devices(sdp, False);
	}
    }
    free(tip->ti_sds);
    /* Note: This is now delayed for async job support! */
    //free(tip);
    return(status);
}
