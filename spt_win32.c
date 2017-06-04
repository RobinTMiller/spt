/*
 * WIN32 specific interfaces, includes:
 *      + pthread wrapper routines
 *      + I/O including async I/O routines
 *      + misc utilities routines 
 *
 * Modification History:
 * 
 */
#include "spt.h"
#include "spt_win32.h"
#include <stdio.h>
#include <signal.h>
#include <winbase.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <io.h>

/*
 * Per-thread data. See Microsoft VC++ documentation on when
 * this can be used (basically static data)
 */
#define Thread __declspec(thread)

/*
 * Local Storage:
 */
/* Note: Left from original code, but not settable! */
static int use_async_io = 0;

int 
win32_noop()
{
    return (0);
}

/*
 * Fake pthread implementation using Windows threads. Windows threads
 * are generally a superset of pthreads, so there is no lost functionality.
 * 
 * Note: Lots of Windows documentation/links added by Robin, who does not
 * do sufficient Windows programming to feel confident in his theads knowledge.
 */
int
pthread_attr_init(pthread_attr_t *attr)
{
    return PTHREAD_NORMAL_EXIT;
}

int
pthread_attr_setscope(pthread_attr_t *attr, unsigned type)
{
    return PTHREAD_NORMAL_EXIT;
}

int
pthread_attr_setdetachstate(pthread_attr_t *attr, int type)
{
    return PTHREAD_NORMAL_EXIT;
}

/* 
 * Reference:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms682453(v=vs.85).aspx
 *
 * Remarks:
 * The number of threads a process can create is limited by the available virtual
 * memory. By default, every thread has one megabyte of stack space. Therefore,
 * you can create at most 2,048 threads. If you reduce the default stack size, you
 * can create more threads. However, your application will have better performance
 * if you create one thread per processor and build queues of requests for which
 * the application maintains the context information. A thread would process all
 * requests in a queue before processing requests in the next queue.
 */ 
int
pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
    *stacksize = MBYTE_SIZE;
    return PTHREAD_NORMAL_EXIT;
}

int
pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
    return PTHREAD_NORMAL_EXIT;
}

/* 
 * Windows Notes from:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms682659(v=vs.85).aspx
 * A thread in an executable that is linked to the static C run-time library  
 * (CRT) should use _beginthread and _endthread for thread management rather  
 * than CreateThread and ExitThread. Failure to do so results in small memory  
 * leaks when the thread calls ExitThread. Another work around is to link the  
 * executable to the CRT in a DLL instead of the static CRT.  
 * Note that this memory leak only occurs from a DLL if the DLL is linked to  
 * the static CRT *and* a thread calls the DisableThreadLibraryCalls function.  
 * Otherwise, it is safe to call CreateThread and ExitThread from a thread in  
 * a DLL that links to the static CRT.  
 *  
 * Robin's Note: September 2012
 * Since we are NOT calling DisableThreadLibraryCalls(), I'm assuming we  
 * do not need to worry about using these alternate thread create/exit API's!
 * Note: The code has left, but not enabled since _MT changes to _MTx.
 * Update: Reenabling _MT method for threads, otherwise hangs occur!
 * Note: The hangs occur while waiting for the parent thread to exit.
 *  
 * A different warning...
 * If you are going to call C run-time routines from a program built
 * with Libcmt.lib, you must start your threads with the _beginthread
 * or _beginthreadex function. Do not use the Win32 functions ExitThread
 * and CreateThread. Using SuspendThread can lead to a deadlock when more
 * than one thread is blocked waiting for the suspended thread to complete
 * its access to a C run-time data structure.
 * URL: http://msdn.microsoft.com/en-us/library/7t9ha0zh(VS.80).aspx
 *
 * Note: My testing proved this to be true, but hangs occured regardless
 * of using these alternate thread API's with thread suspend/resume!
 *
 * Outputs:
 *	tid for Windows is actually the Thread handle, NOT the thread ID!
 */
int
pthread_create(pthread_t *tid, pthread_attr_t *attr,
	       void *(*func)(void *), void *arg)
{
    DWORD dwTid;
#if defined(_MT)
    /* 
     * uintptr_t _beginthreadex( 
     *   void *security,
     *   unsigned stack_size,
     *   unsigned ( *start_address )( void * ),
     *   void *arglist,
     *   unsigned initflag,
     *   unsigned *thrdaddr );
     */
#define myTHREAD_START_ROUTINE unsigned int (__stdcall *)(void *)
    *tid = (pthread_t *)_beginthreadex(NULL, 0, (myTHREAD_START_ROUTINE)func, arg, 0, &dwTid);
#else /* !defined(_MT) */
    /*
     * Use CreateThread so we have a real thread handle
     * to synchronize on
     * HANDLE WINAPI CreateThread(
     *  _In_opt_   LPSECURITY_ATTRIBUTES lpThreadAttributes,
     *  _In_       SIZE_T dwStackSize,
     *  _In_       LPTHREAD_START_ROUTINE lpStartAddress,
     *  _In_opt_   LPVOID lpParameter,
     *  _In_       DWORD dwCreationFlags,
     *  _Out_opt_  LPDWORD lpThreadId
     * );
     */
    *tid = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, &dwTid);
#endif /* defined(_MT) */
    if (*tid == NULL) {
	return GetLastError();
    }
    return PTHREAD_NORMAL_EXIT;
}

void
pthread_exit(void *status)
{
#if defined(_MT)
    /* 
     * void _endthreadex(unsigned retval);
     */
    _endthreadex( (unsigned)status );
#else /* !defined(_MT) */
    ExitThread( (DWORD)status );
#endif /* defined(_MT) */
}

int
pthread_cancel(pthread_t thread)
{
    int status = SUCCESS;
    if ( !TerminateThread((HANDLE)thread, ERROR_SUCCESS) ) {
	status = (int)GetLastError();
    }
    return(status);
}
    
void
pthread_kill(pthread_t thread, int sig)
{
    if (sig == SIGKILL) {
	TerminateThread ((HANDLE)thread, sig);
    } else {
	; //Fprintf(NULL, "Oops, can't kill threads nicely yet\n");
    }
}

int
pthread_mutex_init(pthread_mutex_t *lock, void *attr)
{
    /*
     * HANDLE WINAPI CreateMutex(
     *  _In_opt_  LPSECURITY_ATTRIBUTES lpMutexAttributes,
     * _In_      BOOL bInitialOwner,
     * _In_opt_  LPCTSTR lpName
     * );
     */
    *lock = CreateMutex(NULL, False, NULL);

    if (*lock == NULL) {
	return GetLastError();
    }
    return PTHREAD_NORMAL_EXIT;
}

int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    if (CloseHandle(*mutex) == False) {
	return GetLastError();
    }
    return PTHREAD_NORMAL_EXIT;
}

/* 
 * the diff b/w this and mutex_lock is that this one returns
 * if any thread including itself has the mutex object locked
 * (man pthread_mutex_trylock on Solaris)
 */
int
pthread_mutex_trylock(pthread_mutex_t *lock)
{
    DWORD dw = WaitForSingleObject (*lock, INFINITE);

    /* TODO - errors and return values? */
    return PTHREAD_NORMAL_EXIT;
}

int
pthread_mutex_lock(pthread_mutex_t *lock)
{
    DWORD dw = WaitForSingleObject(*lock, INFINITE);

    switch (dw) {
	case WAIT_ABANDONED:    /* owning thread gone without unlocking */
	case WAIT_TIMEOUT:  
	    /* should never occur in this case since we called 
	     * with INFINITE time */
	    break;
	case WAIT_FAILED: /* TODO - set errno */
	    return 1;
    }
    return PTHREAD_NORMAL_EXIT;
}

int
pthread_mutex_unlock(pthread_mutex_t *lock)
{
    /*
     * BOOL WINAPI ReleaseMutex( _In_  HANDLE hMutex );
     *
     * Note: The caller *must* be the owner (thread) of the lock!
     */
    if (ReleaseMutex(*lock) == False) {
	return GetLastError();
    }
    return PTHREAD_NORMAL_EXIT;
}

int
pthread_cond_init(pthread_cond_t * cv, const void *dummy)
{
    /* 
     * I'm not sure the broadcast thang works - untested 
     * I had to tweak this to use SetEvent when signalling
     * the SIGNAL event 
     */

    /* Create an auto-reset event */
    cv->events_[SIGNAL] = CreateEvent (NULL,/* no security */
				       False,	/* auto-reset event */
				       False,	/* non-signaled initially */
				       NULL);	/* unnamed */

    /* Create a manual-reset event. */
    cv->events_[BROADCAST] = CreateEvent (NULL,	/* no security */
					  True,	/* manual-reset */
					  False,/* non-signaled initially */
					  NULL);/* unnamed */

    /* TODO - error handling */
    return PTHREAD_NORMAL_EXIT;
}

/* Note: This should return pthread_t, but cannot due to type mismatch! */
unsigned long
pthread_self()
{
    /* DWORD WINAPI GetCurrentThreadId(void); */
    return (unsigned long)GetCurrentThreadId();
}

/* Release the lock and wait for the other lock
 * in one move.
 *
 * N.B.
 *        This isn't strictly pthread_cond_wait, but it works
 *        for this program without any race conditions.
 *
 */ 
int
pthread_cond_wait(pthread_cond_t *cv, pthread_mutex_t *lock)
{
    DWORD dwRes = 0;
    int ret = 0;

    dwRes = SignalObjectAndWait(*lock, cv->events_[SIGNAL], INFINITE, True);

    switch (dwRes) {
	case WAIT_ABANDONED:
	    //printf("SignalObjectAndWait - Wait Abandoned \n");
	    return -1;
    
	    /* MSDN says this is one of the ret values, but I get compile errors */
	//case WAIT_OBJECT_O:
	//	printf("SignalObjectAndWait thinks object is signalled\n");
	//	break;
    
	case WAIT_TIMEOUT:
	    //printf("SignalObjectAndWait timed out\n");
	    break;
    
	case 0xFFFFFFFF:
	    os_perror(NULL, "SignalObjectAndWait failed");
	    return -1;
    }

    /* reacquire the lock */
    WaitForSingleObject(*lock, INFINITE);

    return ret;
}

/* 
 * Try to release one waiting thread. 
 */
int
pthread_cond_signal(pthread_cond_t *cv)
{
    if (!SetEvent (cv->events_[SIGNAL])) {
	return GetLastError();
    }
    return PTHREAD_NORMAL_EXIT;
}

/* 
 * Try to release all waiting threads. 
 */
int
pthread_cond_broadcast(pthread_cond_t *cv)
{
    if (!PulseEvent (cv->events_[BROADCAST])) {
	return GetLastError();
    }
    return PTHREAD_NORMAL_EXIT;
}

int
pthread_join(pthread_t thread, void **exit_value)
{
    HANDLE handle = (HANDLE)thread;
    DWORD status = PTHREAD_NORMAL_EXIT;
    DWORD thread_status = PTHREAD_NORMAL_EXIT;
    DWORD wait_status;

    if (GetCurrentThread() == thread) {
	return -1;
    }
    wait_status = WaitForSingleObject(handle, INFINITE);
    if (wait_status == WAIT_FAILED) {
	status = GetLastError();
    } else if (GetExitCodeThread(handle, &thread_status) == False) {
	status = GetLastError();
    }
    if (CloseHandle(handle) == False) {
	status = GetLastError();
    }
    if (exit_value) {
	*(int *)exit_value = (int)thread_status;
    }
    return((int)status);
}

int
pthread_detach(pthread_t thread)
{
    if (CloseHandle((HANDLE)thread) == False) {
	return (int)GetLastError();
    } else {
	return PTHREAD_NORMAL_EXIT;
    }
}

/*
 * I/O routines
 */

HANDLE
os_open_file(char *name, int oflags, int perm)
{
    HANDLE Handle;
    DWORD DesiredAccess;
    DWORD CreationDisposition;
    DWORD FlagAndAttributes = 0;
    DWORD ShareMode;

    /*
     * FILE_SHARE_DELETE = 0x00000004
     *   Enables subsequent open operations on a file or device to request
     * delete access. Otherwise, other processes cannot open the file or device
     * if they request delete access. If this flag is not specified, but the
     * file or device has been opened for delete access, the function fails.
     *    Note: Delete access allows both delete and rename operations.
     */
    if (oflags & O_EXCL) {
	/*
	 * Value 0 - Prevents other processes from opening a file or device
	 * if they request delete, read, or write access.
	 */
	ShareMode = 0;
    } else {
	ShareMode = (FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE);
    }

    /*
     * We map Unix style flags to the Windows equivalent (as best we can).
     */
    if (oflags & O_RDONLY) {
	DesiredAccess = FILE_READ_DATA;
    } else if (oflags & O_WRONLY) {
	DesiredAccess = FILE_WRITE_DATA;
    } else {
	DesiredAccess = (FILE_READ_DATA | FILE_WRITE_DATA);
    }
    if (oflags & O_APPEND) {
	DesiredAccess |= FILE_APPEND_DATA;
    }

    if (oflags & O_CREAT) {
	/*
	 * Note: This logic is required to match Unix create file behavior!
	 */
	if (oflags & O_EXCL) {
	    /*
	     * CREATE_NEW = 1 - Creates a new file, only if it does not already exist.
	     *   If the specified file exists, the function fails and the last-error
	     * code is set to ERROR_FILE_EXISTS (80). If the specified file does not
	     * exist and is a valid path to a writable location, a new file is created.
	     */
	    CreationDisposition = CREATE_NEW;
#if 0
	} else if (os_file_exists(name) == False) {
	    /*
	     * CREATE_ALWAYS = 2 - Creates a new file, always.
	     *   If the specified file exists and is writable, the function overwrites the
	     * file, the function succeeds, and last-error code is set to ERROR_ALREADY_EXISTS (183).
	     * If the specified file does not exist and is a valid path, a new file is created,
	     * the function succeeds, and the last-error code is set to zero.
	     * Note: The overwrite sets the file length to 0, effectively truncating!
	     */
 	    CreationDisposition = CREATE_ALWAYS;
#endif /* 0 */
	} else {
	    /*
	     * OPEN_ALWAYS = 4 - Opens a file, always.
	     *   If the specified file exists, the function succeeds and the last-error
	     * code is set to ERROR_ALREADY_EXISTS (183).
	     *   If the specified file does not exist and is a valid path to a writable
	     * location, the function creates a file and the last-error code is set to zero.
	     */
	    CreationDisposition = OPEN_ALWAYS;
	}
    } else if (oflags & O_TRUNC) {
        /*
         * TRUNCATE_EXISTING = 5
         *   Opens a file and truncates it so that its size is zero bytes, only
         * if it exists. If the specified file does not exist, the function fails
         * and the last-error code is set to ERROR_FILE_NOT_FOUND (2).
         *   The calling process must open the file with the GENERIC_WRITE bit set
         * as part of the dwDesiredAccess parameter.
         */
	if (os_file_exists(name) == True) {
	    CreationDisposition = TRUNCATE_EXISTING;
	} else {
	    CreationDisposition = OPEN_ALWAYS;
	}
    } else {
	/*
	 * OPEN_EXISTING = 3 - Opens a file or device, only if it exists.
	 *   If the specified file or device does not exist, the function
	 * fails and the last-error code is set to ERROR_FILE_NOT_FOUND (2).
	 */
 	CreationDisposition = OPEN_EXISTING;
    }

    if (oflags & O_DSYNC) {
        FlagAndAttributes |= FILE_FLAG_WRITE_THROUGH;
    }
    if (oflags & O_DIRECT) {
        FlagAndAttributes |= (FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH);
    }
    if (oflags & O_RDONLY) {
	FlagAndAttributes |= FILE_ATTRIBUTE_READONLY;
    }
    if (oflags & O_RANDOM) {
	FlagAndAttributes |= FILE_FLAG_RANDOM_ACCESS;
    } else if (oflags & O_SEQUENTIAL) {
        FlagAndAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;
    }
    if (oflags & O_ASYNC) {
	FlagAndAttributes |= FILE_FLAG_OVERLAPPED;
    }
    if (FlagAndAttributes == 0) {
	FlagAndAttributes = FILE_ATTRIBUTE_NORMAL;
    }

    /*
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa363858(v=vs.85).aspx
     *
     * HANDLE WINAPI CreateFile(
     *  _In_      LPCTSTR lpFileName,
     *  _In_      DWORD dwDesiredAccess,
     *  _In_      DWORD dwShareMode,
     *  _In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
     *  _In_      DWORD dwCreationDisposition,
     *  _In_      DWORD dwFlagsAndAttributes,
     *  _In_opt_  HANDLE hTemplateFile
     *);
     */
    Handle = CreateFile(name, DesiredAccess, ShareMode,
			NULL, CreationDisposition, FlagAndAttributes, NULL);
    return( Handle );
}

__inline ssize_t
os_read_file(HANDLE handle, void *buffer, size_t size)
{
    DWORD bytesRead;

    /*
     * BOOL WINAPI ReadFile(
     *	_In_         HANDLE hFile,
     *	_Out_        LPVOID lpBuffer,
     *	_In_         DWORD nNumberOfBytesToRead,
     * 	_Out_opt_    LPDWORD lpNumberOfBytesRead,
     * 	_Inout_opt_  LPOVERLAPPED lpOverlapped
     * );
     */
    if (ReadFile(handle, buffer, (DWORD)size, (LPDWORD)&bytesRead, NULL) == False) {
       bytesRead = -1;
    }
    return( (ssize_t)(int)bytesRead );
}

__inline ssize_t
os_write_file(HANDLE handle, void *buffer, size_t size)
{
    DWORD bytesWritten;

    /*
     * BOOL WINAPI WriteFile(
     *	_In_         HANDLE hFile,
     *	_In_         LPCVOID lpBuffer,
     *	_In_         DWORD nNumberOfBytesToWrite,
     *	_Out_opt_    LPDWORD lpNumberOfBytesWritten,
     *	_Inout_opt_  LPOVERLAPPED lpOverlapped
     * );
     */
    if (WriteFile(handle, buffer, (DWORD)size, (LPDWORD)&bytesWritten, NULL) == False) {
       bytesWritten = -1;
    }
    return( (ssize_t)(int)bytesWritten );
}

/*    Unix whence values: SEEK_SET(0), SEEK_CUR(1), SEEK_END(2) */
static int seek_map[] = { FILE_BEGIN, FILE_CURRENT, FILE_END };

/*
 * Note that this is a 64-bit seek, Offset_t better be 64 bits
 */
Offset_t
os_seek_file(HANDLE handle, Offset_t offset, int whence)
{
    LARGE_INTEGER liDistanceToMove;
    LARGE_INTEGER lpNewFilePointer;
	
    /*
     * BOOL WINAPI SetFilePointerEx(
     *  _In_       HANDLE hFile,
     *  _In_       LARGE_INTEGER liDistanceToMove,
     *  _Out_opt_  PLARGE_INTEGER lpNewFilePointer,
     *  _In_       DWORD dwMoveMethod
     * );
     */
    liDistanceToMove.QuadPart = offset;
    if ( SetFilePointerEx(handle, liDistanceToMove, &lpNewFilePointer, seek_map[whence]) == False) {
	lpNewFilePointer.QuadPart = -1;
    }
    return( (Offset_t)(lpNewFilePointer.QuadPart) );
}

ssize_t
os_pread_file(HANDLE handle, void *buffer, size_t size, Offset_t offset)
{
    DWORD bytesRead;
    OVERLAPPED ol;
    BOOL res;
    LARGE_INTEGER li;

    li.QuadPart = offset;
    ol.Offset = li.LowPart;
    ol.OffsetHigh = li.HighPart;
    if (use_async_io) {
	ol.hEvent = CreateEvent(NULL, True, True, NULL);
	if (ol.hEvent==NULL) {
	    return(FAILURE);
	}
    } else {
	ol.hEvent = NULL;
    }

    res = ReadFile(handle, buffer, (DWORD)size, (LPDWORD)&bytesRead, &ol);
    if (res == False) {
	if (GetLastError() == ERROR_IO_PENDING) {
	    res = GetOverlappedResult(handle, &ol, &bytesRead, True);
	}
	if (!res) {
	    bytesRead = FAILURE;
	}
    }

    if (ol.hEvent != NULL) {
	CloseHandle(ol.hEvent);
    }
    return( (ssize_t)(int)bytesRead );
}

ssize_t
os_pwrite_file(HANDLE handle, void *buffer, size_t size, Offset_t offset)
{
    DWORD bytesWrite;
    OVERLAPPED ol;
    BOOL res;
    LARGE_INTEGER li;

    li.QuadPart = offset;
    ol.Offset = li.LowPart;
    ol.OffsetHigh = li.HighPart;
    if (use_async_io) {
	ol.hEvent = CreateEvent(NULL, True, False, NULL);
	if (ol.hEvent == NULL) {
	    return(FAILURE);
	}
    } else {
	ol.hEvent = NULL;
    }

    res = WriteFile(handle, buffer, (DWORD)size, (LPDWORD)&bytesWrite, &ol);
    if (res == False) {
	if (GetLastError() == ERROR_IO_PENDING) {
	    res = GetOverlappedResult(handle, &ol, &bytesWrite, True);
	}
	if (!res) {
	    bytesWrite = FAILURE;
	}
    }
    if (ol.hEvent != NULL) {
	CloseHandle(ol.hEvent);
    }
    return( (ssize_t)(int)bytesWrite );
}

/*
 * Async IO using completion ports
 * this allows any thread to process any thread's I/O
 * This is more suited for cruisio since we have reaper threads
 */
static HANDLE hCompPort = NULL;
#define COMPKEY 1

int
win32_async_init(int *fhs, int nfiles, int nthreads)
{
    int i;

    hCompPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 
				       COMPKEY, nthreads);

    if (hCompPort == NULL) {
	return False;
    }

    for (i = 0; i < nfiles; i++) {
	HANDLE fh = (HANDLE) fhs[i];
	HANDLE h = CreateIoCompletionPort(fh, hCompPort, 
					  COMPKEY, nthreads);
	if (h != hCompPort) {
	    return False;
	}
    }
    return True;
}

void
win32_async_term()
{
    CloseHandle(hCompPort);
}

static int
win32_error_to_errno(DWORD dwErr)
{
    int err = EAGAIN; // default error! 

    switch (dwErr) {
    case WAIT_TIMEOUT:
//		err = ETIMEDOUT;
	break;
    case ERROR_INVALID_USER_BUFFER:
	err = EFAULT;
	break;
    case ERROR_NOT_ENOUGH_MEMORY:
	err = ENOMEM;
	break;
    case ERROR_NOT_ENOUGH_QUOTA:
	err = EAGAIN;
	break;
    case ERROR_INVALID_HANDLE:
	err = EBADF;
    }
    return err;
}

int
win32_async_read(HANDLE han, caddr_t buf, ULONG size, Offset_t offset,
		 int whence, struct aio_result_t *aiores)
{
    OVERLAPPED *ol = &(aiores->overlapped);
    DWORD dwBytes = 0;
    BOOL fOk;
    LARGE_INTEGER li;
    int result = 0;

    if (whence != SEEK_SET) {
	fprintf (stderr, "aioread only supports SEEK_SET currently\n");
	return -1;
    }

    aiores->aio_return = -1;
    aiores->aio_errno = 0;

    li.QuadPart = offset;
    ol->Offset = li.LowPart;
    ol->OffsetHigh = li.HighPart;
    ol->hEvent = NULL;

    fOk = ReadFile(han, buf, size, &dwBytes, ol);
    if (!fOk) {
	DWORD dwErr= GetLastError();
	switch (dwErr) {
	case ERROR_IO_PENDING:
	    break;
	default:
	    errno = win32_error_to_errno(dwErr);
	    //win32_error ("ReadFile fails with %s\n");
	    result = -1;
	}
    } else {
	PostQueuedCompletionStatus(hCompPort, dwBytes, COMPKEY, ol);
    }
    return result;
}

int
win32_async_write(HANDLE han, caddr_t buf, ULONG size, Offset_t offset,
		  int whence, struct aio_result_t *aiores)
{
    DWORD dwBytes = 0;
    OVERLAPPED *ol = &(aiores->overlapped);
    BOOL fOk;
    LARGE_INTEGER li;
    int result = 0;

    li.QuadPart = offset;
    ol->Offset = li.LowPart;
    ol->OffsetHigh = li.HighPart;
    ol->hEvent = NULL;

    aiores->aio_return = -1;
    aiores->aio_errno = 0;

    fOk = WriteFile (han, buf, size, &dwBytes, ol);
    if (!fOk) {
	DWORD dwErr= GetLastError();
	switch (dwErr) {
	case ERROR_IO_PENDING:
	    break;
	default:
	    errno = win32_error_to_errno(dwErr);
	    //win32_error ("ReadFile fails with %s\n");
	    result = -1;
	}
    } else {
	PostQueuedCompletionStatus(hCompPort, dwBytes, COMPKEY, ol);
    }
    return result;
}

void
win32_async_post(int nthreads)
{
    int i;

    for (i=0; i < nthreads; i++) {
	PostQueuedCompletionStatus(hCompPort, 0, 0, NULL);
    }
}

struct aio_result_t *
win32_async_wait(struct timeval *tv)
{
    DWORD msecs = INFINITE;
    DWORD dwBytes;
    ULONG_PTR dwCompKey;
    LPOVERLAPPED lpOverlapped;
    BOOL fOk = False;
    struct aio_result_t *aiores;

    if (tv != NULL) {
	msecs = (tv->tv_sec * 1000) + tv->tv_usec/1000;
    }

    /*
     * BOOL WINAPI GetQueuedCompletionStatus(
     *  _In_   HANDLE CompletionPort,
     *  _Out_  LPDWORD lpNumberOfBytes,
     *  _Out_  PULONG_PTR lpCompletionKey,
     *  _Out_  LPOVERLAPPED *lpOverlapped,
     *  _In_   DWORD dwMilliseconds
     * );
     */
    fOk = GetQueuedCompletionStatus(hCompPort, &dwBytes, 
				    &dwCompKey, &lpOverlapped, msecs);

    if (fOk == True) {
	if (dwCompKey != COMPKEY) {
	    /* someone used PostQueuedCompletionStatus to signal us
	     * of an event other than an I/O - probably to terminate
	     * us 
	     */
	    errno = EINVAL;
	    return(struct aio_result_t *) -1;
	}
	/* succesful completion of I/O */
	aiores = (struct aio_result_t *) lpOverlapped;
	aiores->aio_return = dwBytes;
	aiores->aio_errno = 0;
	errno = 0;
	return aiores;
    } else {
	DWORD dwError = GetLastError();

	if (lpOverlapped != NULL) {
	    /* 
	     * Failed completed I/O request.
	     * dwError has reason for failure 
	     */
	    aiores = (struct aio_result_t *) lpOverlapped;
	    aiores->aio_return = -1;
	    aiores->aio_errno = dwError;
	} else if (dwError == WAIT_TIMEOUT) {
	    /* Time-out msecs expired wait for completed I/O */
	    return(struct aio_result_t *) 0;
	} else {
	    /* 
	     * Bad call to GetQueuedCompletionStatus.
	     * dwError has reason for bad call
	     * Currently this is handled the same way as a failed IO
	     */
	}
	errno = dwError;
	//win32_error("I/O call to GetQueuedCompletion failed %s\n");
	return(struct aio_result_t *) -1;
    }
}

/*
 * Async I/O Handling using Alertable I/O Asynchronous Procedure Calls (APC)
 * Note that the thread issuing the I/O must process it as per Advanced Windows
 * (The Richter Book chapter 15)
 * Much of this depends on the per-thread EX-completion routines.
 * Be careful of changing.
 */
#ifdef USE_WIN32_APC

static Thread struct aio_result_t *io_completion_root = NULL;
static Thread struct aio_result_t *io_completion_tail = NULL;

static void
init_aiores (struct aio_result_t *aiores)
{
    aiores->magic = AIO_MAGIC;
    aiores->hEvent = CreateEvent (NULL, True, False, "AIO_EVENT");
    if (aiores->hEvent == NULL) {
	fprintf (stderr, "Unable to get aio event\n");
	//exit (1);
    }
    aiores->next = NULL;
}

/*
 * Completion callback handler for asynch I/O
 */
VOID CALLBACK
aioCompletion (DWORD errorCode, DWORD numBytes, OVERLAPPED * olp)
{
    struct aio_result_t *aiores = (struct aio_result_t *) olp;

    if (errorCode == 0) {
	aiores->aio_return = numBytes;
    } else {
	aiores->aio_return = -1;
    }

    aiores->aio_errno = errorCode;
    if (io_completion_tail != NULL) {
	io_completion_tail->next = aiores;
    } else {
	io_completion_root = aiores;
    }

    aiores->next = NULL;
    io_completion_tail = aiores;
}

int
win32_aioread (HANDLE han, caddr_t buf, ULONG size, Offset_t offset,
	       int whence, struct aio_result_t *aiores)
{
    OVERLAPPED *ol = &(aiores->overlapped);
    BOOL res;
    int result = 0;
    LARGE_INTEGER li;

    if (whence != SEEK_SET) {
	fprintf (stderr, "aioread only supports SEEK_SET currently\n");
	return -1;
    }
    if (aiores->magic != AIO_MAGIC) {
	init_aiores (aiores);
    }

    li.QuadPart = offset;
    ol->Offset = li.LowPart;
    ol->OffsetHigh = li.HighPart;
    ol->hEvent = aiores->hEvent;

    res = ReadFileEx (han, buf, size, ol, aioCompletion);
    if (!res) {
	int error = GetLastError();
	switch (error) {
	case ERROR_IO_PENDING:
	    /* I/O Still in progress */
	    break;

	default:
	    //win32_error ("ReadFileEx fails with %s\n");
	    result = -1;
	    break;
	}
    }
    return result;
}


int
win32_aiowrite (HANDLE han, caddr_t buf, ULONG size, Offset_t offset,
		int whence, struct aio_result_t *aiores)
{
    OVERLAPPED *ol = &(aiores->overlapped);
    BOOL res;
    int result = 0;
    LARGE_INTEGER li;

    if (whence != SEEK_SET) {
	fprintf (stderr, "aiowrite only supports SEEK_SET currently\n");
	return -1;
    }

    if (aiores->magic != AIO_MAGIC) {
	init_aiores (aiores);
    }
    li.QuadPart = offset;
    ol->Offset = li.LowPart;
    ol->OffsetHigh = li.HighPart;
    ol->hEvent = aiores->hEvent;

    res = WriteFileEx (han, buf, size, ol, aioCompletion);
    if (!res) {
	int error = GetLastError();
	switch (error) {
	case ERROR_IO_PENDING:
	    /* I/O Still in progress, */
	    break;
	default:
	    //win32_error ("WriteFileEx fails with %s\n");
	    result = -1;
	    break;
	}
    }
    return result;
}

struct aio_result_t *
win32_aiowait(struct timeval *tv) {
    DWORD millisecs;
    struct aio_result_t *aiores;

    /*
     * If there are already outstanding completions
     * from this thread (a SleepEx may cause allow 
     * multiple completion handlers to run) then
     * get one
     */
    if (io_completion_root != NULL) {
	aiores = io_completion_root;
	io_completion_root = io_completion_root->next;
	if (io_completion_root == NULL)
	    io_completion_tail = NULL;
	return aiores;
    }

    if (tv != NULL) {
	millisecs = (tv->tv_sec * 1000) + tv->tv_usec/1000;
    } else {
	millisecs = INFINITE;
    }

    if (SleepEx(millisecs, True) == WAIT_IO_COMPLETION) {
	/*
	 * We got a completion for this thread.
	 * One or more I/Os may have completed.
	 */
	aiores = io_completion_root;
	io_completion_root = io_completion_root->next;
	if (io_completion_root == NULL) {
	    io_completion_tail = NULL;
	}
	return aiores;
    }
    return(struct aio_result_t *) -1;
}
#endif /* USE_WIN32_APC */

HANDLE
win32_dup(HANDLE handle)
{
    HANDLE hDup = (HANDLE)-1;

    /* http://msdn.microsoft.com/en-us/library/ms724251(VS.85).aspx */

    if ( !DuplicateHandle(GetCurrentProcess(), 
			  handle, 
			  GetCurrentProcess(),
			  &hDup, 
			  0,
			  False,
			  DUPLICATE_SAME_ACCESS) ) {
	DWORD dwErr = GetLastError();
	//win32_error("DuplicateHandle failed because %s\n");
	errno = EINVAL;
    }
    return (hDup);
}

/* ============================================================================== */

hbool_t
IsDriveLetter(char *device)
{
    /* Check for drive letters "[a-zA-Z]:" */
    if ((strlen(device) >= 2) && (device[1] == ':') &&
        ((device[0] >= 'a') && (device[0] <= 'z') ||
         (device[0] >= 'A') && (device[0] <= 'Z'))) {
        return(True);
    }
    return(False);
}

char *
setup_scsi_device(scsi_device_t *sdp, char *path)
{
    char *scsi_device;

    scsi_device = Malloc(sdp, (DEV_DIR_LEN * 2) );
    if (scsi_device == NULL) return(scsi_device);
    /* Format: \\.\[A-Z]: */
    strcpy(scsi_device, DEV_DIR_PREFIX);
    scsi_device[DEV_DIR_LEN] = path[0];		/* The drive letter. */
    scsi_device[DEV_DIR_LEN+1] = path[1];	/* The ':' terminator. */
    return(scsi_device);
}

char *
os_ctime(time_t *timep, char *time_buffer, int timebuf_size)
{
    int error;

    error = ctime_s(time_buffer, timebuf_size, timep);
    if (error) {
	tPerror(NULL, error, "_ctime_s() failed");
	(int)sprintf(time_buffer, "<no time available>\n");
    } else {
	char *bp;
	if (bp = strrchr(time_buffer, '\n')) {
	    *bp = '\0';
	}
    }
    return(time_buffer);
}

char *
os_gethostname(void)
{
    char hostname[MAXHOSTNAMELEN];
    DWORD len = MAXHOSTNAMELEN;

    if ( (GetComputerNameEx(ComputerNameDnsFullyQualified,
				hostname, &len)) == 0) {
	  os_perror(NULL, "GetComputerNameEx() failed");
	  return(NULL);
    }
    return ( strdup(hostname) );
}

char *
os_getusername(void)
{
    DWORD size = STRING_BUFFER_SIZE;
    TCHAR username[STRING_BUFFER_SIZE];

    if ( !GetUserName(username, &size) ) {
	os_perror(NULL, "GetUserName() failed");
	return(NULL);
    }
    return ( strdup(username) );
}

int
getpagesize(void)
{
    SYSTEM_INFO sysinfo;

    GetSystemInfo(&sysinfo);
    return ( sysinfo.dwPageSize );
}

int
setenv(const char *name, const char *value, int overwrite)
{
    return ( _putenv_s(name, value) );
}

/*
 * Windows equivalent...
 *	we need both since POSIX and Windows API's are both used! (yet)
 */
/*VARARGS*/
void
os_perror(scsi_device_t *sdp, char *format, ...)
{
    char msgbuf[LOG_BUFSIZE];
    DWORD error = GetLastError();
    LPVOID emsg = os_get_error_msg(error);
    va_list ap;
    FILE *fp;

    if (sdp == NULL) sdp = master_sdp;
    fp = (sdp) ? sdp->efp : efp;
    va_start(ap, format);
    (void)vsprintf(msgbuf, format, ap);
    va_end(ap);
    LogMsg(sdp, fp, logLevelError, 0, "%s, error = %d - %s\n", msgbuf, error, emsg);
    os_free_error_msg(emsg);
    return;
}

void
tPerror(scsi_device_t *sdp, int error, char *format, ...)
{
    char msgbuf[LOG_BUFSIZE];
    LPVOID emsg = os_get_error_msg(error);
    va_list ap;
    FILE *fp;

    if (sdp == NULL) sdp = master_sdp;
    fp = (sdp) ? sdp->efp : efp;
    va_start(ap, format);
    (void)vsprintf(msgbuf, format, ap);
    va_end(ap);
    LogMsg(sdp, fp, logLevelError, 0, "%s, error = %d - %s\n", msgbuf, error, emsg);
    os_free_error_msg(emsg);
    return;
}

/*
 * os_get_error_msg() - Get OS (Windows) Error Message.
 *
 * Inputs:
 *	error = The error code, from GetLastError()
 *
 * Return Value:
 *	A pointer to a dynamically allocated message.
 *	The buffer should be freed via LocalFree().
 */
char *
os_get_error_msg(int error)
{
    char *msgbuf;

    if ( FormatMessage (
		       FORMAT_MESSAGE_ALLOCATE_BUFFER |
		       FORMAT_MESSAGE_FROM_SYSTEM,
		       NULL,
		       error,
		       MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
		       (LPSTR) &msgbuf,
		       0, NULL) == 0) {
	Fprintf(NULL, "FormatMessage() failed with %d\n", GetLastError());
	return(NULL);
    } else {
	char *bp = strrchr(msgbuf, '\r'); /* Terminated with \r\n */
	if (bp) *bp = '\0';		  /* Just the message please! */
	return(msgbuf);
    }
}

Offset_t
SetFilePtr(HANDLE hf, Offset_t distance, DWORD MoveMethod)
{
    LARGE_INTEGER seek;

    /*
     * DWORD WINAPI SetFilePointer(
     *  _In_         HANDLE hFile,
     *  _In_         LONG lDistanceToMove,
     *  _Inout_opt_  PLONG lpDistanceToMoveHigh,
     *  _In_         DWORD dwMoveMethod
     * );
     */
    seek.QuadPart = distance;
    seek.LowPart = SetFilePointer(hf, seek.LowPart, &seek.HighPart, MoveMethod);
    if ( (seek.LowPart == INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR) ) {
	seek.QuadPart = -1;
    }
    return( (Offset_t)seek.QuadPart );
}

/*
 * syslog() - Windows API to Emulate Unix Syslog API.
 *
 * Inputs:
 *	priority = The message priority.
 *	format = Format control string.
 *	(optional arguments ...)
 *
 * Return Value:
 *	void
 */
void
syslog(int priority, char *format, ...)
{
    LPCSTR sourceName = "System";	// The event source name.
    DWORD dwEventID = 999;              // The event identifier.
    WORD cInserts = 1;                  // The count of insert strings.
    HANDLE h; 
    char msgbuf[LOG_BUFSIZE];
    LPCSTR bp = msgbuf;
    va_list ap;
    hbool_t debug_flag = False;

    va_start(ap, format);
    (void)vsprintf(msgbuf, format, ap);
    va_end(ap);

    /*
     * Get a handle to the event log.
     */
    h = RegisterEventSource(NULL,        // Use local computer. 
                            sourceName); // Event source name. 
    if (h == NULL) {
	if (debug_flag) {
	    Fprintf(NULL, "RegisterEventSource() failed, error %d", GetLastError());
	}
        return;
    }

    /*
     * Report the event.
     */
    if (!ReportEvent(h,           // Event log handle. 
            priority,             // Event type. 
            0,                    // Event category.  
            dwEventID,            // Event identifier. 
            (PSID) 0,             // No user security identifier. 
            cInserts,             // Number of substitution strings. 
            0,                    // No data. 
            &bp,                  // Pointer to strings. 
            NULL))                // No data. 
    {
	if (debug_flag) {
	    printf(NULL, "ReportEvent() failed, error %d", GetLastError());
	}
    }
    DeregisterEventSource(h); 
    return;
}

/*
 * Taken from URL: 
 *  http://msdn.microsoft.com/en-us/library/windows/desktop/ms686277(v=vs.85).aspx
 */
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
# define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
# define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif
 
int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
    FILETIME ft;
    unsigned __int64 tmpres = 0;
    static int tzflag;

    if (NULL != tv) {
	/* Precision is 10-15ms. */
	GetSystemTimeAsFileTime(&ft);
	tmpres |= ft.dwHighDateTime;
	tmpres <<= 32;
	tmpres |= ft.dwLowDateTime;

	/*converting file time to unix epoch*/
	tmpres -= DELTA_EPOCH_IN_MICROSECS; 
	tmpres /= 10;  /*convert into microseconds*/
	tv->tv_sec = (long)(tmpres / 1000000UL);
	tv->tv_usec = (long)(tmpres % 1000000UL);
    }

    if (NULL != tz) {
	if (!tzflag) {
	    _tzset();
	    tzflag++;
	}
	tz->tz_minuteswest = _timezone / 60;
	tz->tz_dsttime = _daylight;
    }
    return 0;
}

/*
 * localtime_r() - Get Local Time.
 * 
 * The arguments are reversed and the return value is different.
 * 
 * Unix:
 *  struct tm *localtime_r(const time_t *timep, struct tm *result);
 *
 * Windows:
 *  errno_t localtime_s(struct tm* _tm,	const time_t *time);
 * );
 *
 * Return Value:
 * 	Returns tm on Success or NULL on Failure.
 */
struct tm *
localtime_r(const time_t *timep, struct tm *tm)
{
    if ( localtime_s(tm, timep) == SUCCESS ) {
	return(tm);
    } else {
	return(NULL);
    }
}

__inline clock_t
times(struct tms *buffer)
{
    return ( (clock_t)(time(0) * hertz) );
}

uint64_t
os_create_random_seed(void)
{
    LARGE_INTEGER PerformanceCount;
    /*
     * BOOL WINAPI QueryPerformanceCounter(
     *  _Out_  LARGE_INTEGER *lpPerformanceCount
     * );
     */
    if ( QueryPerformanceCounter(&PerformanceCount) ) {
	return( (uint64_t)(PerformanceCount.QuadPart) );
    } else {
	return(0);
    }
}

__inline int
os_create_directory(char *dir_path, int permissions)
{
    /* BOOL WINAPI CreateDirectory(
     *  _In_      LPCTSTR lpPathName,
     *  _In_opt_  LPSECURITY_ATTRIBUTES lpSecurityAttributes
     * );
     */
    return ( ( CreateDirectory(dir_path, (LPSECURITY_ATTRIBUTES) 0) ) ? SUCCESS : FAILURE );
}


__inline int
os_remove_directory(char *dir_path)
{
    /* BOOL WINAPI RemoveDirectory( _In_ LPCTSTR lpPathName );	*/
    return ( ( RemoveDirectory(dir_path) ) ? SUCCESS : FAILURE );
}

__inline int
os_close_file(HANDLE handle)
{
    /* BOOL WINAPI CloseHandle(	_In_ HANDLE hObject );	*/
    return ( ( CloseHandle(handle) ) ? SUCCESS : FAILURE );
}

__inline int
os_delete_file(char *file)
{
    /* BOOL WINAPI DeleteFile( _In_ LPCTSTR lpFileName	); */
    return ( ( DeleteFile(file) ) ? SUCCESS : FAILURE );
}

__inline int
os_flush_file(HANDLE *handle)
{
    /* BOOL WINAPI FlushFileBuffers( _In_ HANDLE hFile	); */
    return ( (FlushFileBuffers(handle) ) ? SUCCESS : FAILURE );
}

int
os_truncate_file(HANDLE handle, Offset_t offset)
{
    int status = SUCCESS;

    if (os_seek_file(handle, offset, SEEK_END) == (Offset_t) -1) {
	status = FAILURE;
    } else {
	/* BOOL WINAPI SetEndOfFile( _In_ HANDLE hFile	); */
	if ( SetEndOfFile(handle) == False ) {
	    status = FAILURE;
	}
    }
    return(status);
}

hbool_t
os_file_information(char *file, uint64_t *filesize, hbool_t *is_dir)
{
    WIN32_FILE_ATTRIBUTE_DATA fad, *fadp = &fad;

    /*
     * See if the file exists, and what it's size is. 
     *  
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa364946(v=vs.85).aspx
     * 
     * BOOL WINAPI GetFileAttributesEx(
     *  _In_   LPCTSTR lpFileName,
     *  _In_   GET_FILEEX_INFO_LEVELS fInfoLevelId,
     *  _Out_  LPVOID lpFileInformation
     *);
     */
    if ( GetFileAttributesEx(file, GetFileExInfoStandard, fadp) ) {
	/* Assuming a regular file (for now). */
	if (filesize) {
	    /* Setup the 64-bit file size please! */
	    *filesize = (uint64_t)(((uint64_t)fadp->nFileSizeHigh << 32L) + fadp->nFileSizeLow);
	}
	if (is_dir) {
	    if (fadp->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
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
    WIN32_FILE_ATTRIBUTE_DATA fad, *fadp = &fad;

    /*
     * See if the file exists, and what it's size is. 
     *  
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa364946(v=vs.85).aspx
     * 
     * BOOL WINAPI GetFileAttributesEx(
     *  _In_   LPCTSTR lpFileName,
     *  _In_   GET_FILEEX_INFO_LEVELS fInfoLevelId,
     *  _Out_  LPVOID lpFileInformation
     *);
     */
    if ( GetFileAttributesEx(file, GetFileExInfoStandard, fadp) ) {
	return(True);
    } else {
	return(False);
    }
}

hbool_t
os_isdir(char *dirpath)
{
    WIN32_FILE_ATTRIBUTE_DATA fad, *fadp = &fad;

    /*
     * See if this is a directory.
     *  
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa364946(v=vs.85).aspx
     * 
     * BOOL WINAPI GetFileAttributesEx(
     *  _In_   LPCTSTR lpFileName,
     *  _In_   GET_FILEEX_INFO_LEVELS fInfoLevelId,
     *  _Out_  LPVOID lpFileInformation
     *);
     */
    if ( GetFileAttributesEx(dirpath, GetFileExInfoStandard, fadp) ) {
	if (fadp->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
	    return(True);
	} else {
	    return(False);
	}
    } else {
	return(False);
    }
}

char *
os_getcwd(void)
{
    char path[PATH_BUFFER_SIZE];

    /*
     * DWORD WINAPI GetCurrentDirectory(
     *  _In_   DWORD nBufferLength,
     *  _Out_  LPTSTR lpBuffer
     * );
     */
    if ( GetCurrentDirectory(sizeof(path), path) == 0 ) {
	return(NULL);
    } else {
	return ( strdup(path) );
    }
}

uint64_t
os_get_file_size(char *path, HANDLE handle)
{
    uint64_t filesize = -1LL;

    if (handle == INVALID_HANDLE_VALUE) {
	/*
	 * BOOL WINAPI GetFileAttributesEx(
	 *  _In_   LPCTSTR lpFileName,
	 *  _In_   GET_FILEEX_INFO_LEVELS fInfoLevelId,
	 *  _Out_  LPVOID lpFileInformation
	 * );
	*/
	WIN32_FILE_ATTRIBUTE_DATA fad, *fadp = &fad;
	if ( GetFileAttributesEx(path, GetFileExInfoStandard, fadp) ) {
	    filesize = (uint64_t)(((uint64_t)fadp->nFileSizeHigh << 32L) + fadp->nFileSizeLow);
	    return (filesize);
	}
    } else {
	/*
	 * BOOL WINAPI GetFileSizeEx(
	 *  _In_   HANDLE hFile,
	 *  _Out_  PLARGE_INTEGER lpFileSize
	 * );
	 */
	LARGE_INTEGER FileSize;
	if ( GetFileSizeEx(handle, &FileSize) ) {
	    return ( (uint64_t)FileSize.QuadPart );
	}
    }
    return (filesize);
}

#pragma comment(lib, "mpr.lib")

char *
os_get_universal_name(char *drive_letter)
{
    DWORD cbBuff = PATH_BUFFER_SIZE;
    TCHAR szBuff[PATH_BUFFER_SIZE];
    REMOTE_NAME_INFO *prni = (REMOTE_NAME_INFO *)&szBuff;
    UNIVERSAL_NAME_INFO *puni = (UNIVERSAL_NAME_INFO *)&szBuff;
    DWORD result;

    /*
     * DWORD WNetGetUniversalName(
     *  _In_     LPCTSTR lpLocalPath,
     *  _In_     DWORD dwInfoLevel,
     *  _Out_    LPVOID lpBuffer,
     *  _Inout_  LPDWORD lpBufferSize
     * );
     */
    result = WNetGetUniversalName((LPTSTR)drive_letter,
				  UNIVERSAL_NAME_INFO_LEVEL,
				  (LPVOID)&szBuff, &cbBuff);
    if (result == NO_ERROR) {
	return( strdup(puni->lpUniversalName) );
    } else {
	return(NULL);
    }
//    _tprintf(TEXT("Universal Name: \t%s\n\n"), puni->lpUniversalName); 
#if 0
    if ( (res = WNetGetUniversalName((LPTSTR)drive_letter, 
				     REMOTE_NAME_INFO_LEVEL, 
				     (LPVOID)&szBuff,
				     &cbBuff)) != NO_ERROR) {
	return(NULL);
    }
//    _tprintf(TEXT("Universal Name: \t%s\nConnection Name:\t%s\nRemaining Path: \t%s\n"),
//          prni->lpUniversalName, 
//          prni->lpConnectionName, 
//          prni->lpRemainingPath);
    return(NULL);
#endif
}

char *
os_get_volume_path_name(char *path)
{
    BOOL	bStatus;
    CHAR	VolumePathName[PATH_BUFFER_SIZE];

    /*
     * BOOL WINAPI GetVolumePathName(
     *  _In_   LPCTSTR lpszFileName,
     *  _Out_  LPTSTR lpszVolumePathName,
     *  _In_   DWORD cchBufferLength
     * );
     */
    bStatus = GetVolumePathName(path, VolumePathName, sizeof(VolumePathName));
    if (bStatus == True) {
	return ( strdup(VolumePathName) );
    } else {
	return (NULL);
    }
}

int
os_set_priority(scsi_device_t *sdp, HANDLE hThread, int priority)
{
    int status = SUCCESS;

    /*
     * BOOL WINAPI SetThreadPriority(_In_ HANDLE hThread, _In_ int nPriority);
     */ 
    if ( SetThreadPriority(hThread, priority) == False ) {
	status = FAILURE;
    }
    return(status);
}

/*
 * os_isEof() - Determine if this is an EOF condition.
 *
 * Description:
 *	Well clearly, we're checking for more than EOF errors below.
 * These additional checks are made, since the algorithm for finding
 * capacity (seek/read), and our step option, can cause one of these
 * other errors when reading past end of media (EOM).
 *
 *	Now that said, I did not do the original Windows port, so I'm
 * trusting these other error checks for won't mask *real* errors. But,
 * this may need revisited at some point.
 */
hbool_t
os_isEof(ssize_t count, int error)
{
    if ( (count == 0) ||
	 ( (count < 0) && 
	   ( (error == ERROR_DISK_FULL)		||
	     (error == ERROR_HANDLE_EOF)	||
	     (error == ERROR_SECTOR_NOT_FOUND) ) ) ) { 
	return(True);
    } else {
	return(False);
    }
}

__inline int
os_lock_file(HANDLE fd, Offset_t start, Offset_t length, int type)
{
    /*
     * BOOL WINAPI LockFile(
     *  _In_  HANDLE hFile,
     *  _In_  DWORD dwFileOffsetLow,
     *  _In_  DWORD dwFileOffsetHigh,
     *  _In_  DWORD nNumberOfBytesToLockLow,
     *  _In_  DWORD nNumberOfBytesToLockHigh
     * );
     */
    return( ( LockFile(fd, (DWORD)start, (DWORD)(start >> 32), (DWORD)length, (DWORD)(length >> 32)) ) ? SUCCESS : FAILURE);
}

__inline int
os_unlock_file(HANDLE fd, Offset_t start, Offset_t length)
{
    /*
     * BOOL WINAPI UnlockFile(
     *  _In_  HANDLE hFile,
     *  _In_  DWORD dwFileOffsetLow,
     *  _In_  DWORD dwFileOffsetHigh,
     *  _In_  DWORD nNumberOfBytesToUnlockLow,
     *  _In_  DWORD nNumberOfBytesToUnlockHigh
     * );
     */
    return( ( UnlockFile(fd, (DWORD)start, (DWORD)(start >> 32), (DWORD)length, (DWORD)(length >> 32)) ) ? SUCCESS : FAILURE);
}

__inline int
os_rename_file(char *oldpath, char *newpath)
{
    /*
     * Unix rename() behavior is different than Windows: 
     *    If newpath already exists it will be atomically replaced (subject to
     * a few conditions), so that there is no point at which another process
     * attempting to access newpath will find it missing. 
     *  
     * Therefore, for Winodws we must remove the newpath first!
     */
    if ( os_file_exists(newpath) ) {
	int status = os_delete_file(newpath);
	if (status == FAILURE) return(status);
    }

    /* 
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa365239(v=vs.85).aspx 
     *  
     * BOOL WINAPI MoveFile(
     *  _In_  LPCTSTR lpExistingFileName,
     *  _In_  LPCTSTR lpNewFileName
     * );
     */
    return ( (MoveFile(oldpath, newpath) == True) ? SUCCESS : FAILURE);
}

__inline int
os_link_file(char *oldpath, char *newpath)
{
    /* 
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa363866(v=vs.85).aspx 
     *  
     * BOOL WINAPI CreateHardLink(
     *  _In_        LPCTSTR lpFileName,
     *  _In_        LPCTSTR lpExistingFileName,
     *  _Reserved_  LPSECURITY_ATTRIBUTES lpSecurityAttributes
     * );
     */
    return ( (CreateHardLink(newpath, oldpath, NULL) == True) ? SUCCESS : FAILURE);
}

hbool_t
os_symlink_supported(void)
{
    LUID luid;
    LPCTSTR Name = "SeCreateSymbolicLinkPrivilege";
    PTOKEN_PRIVILEGES tpp = NULL;
    TOKEN_INFORMATION_CLASS tc = TokenPrivileges;
    DWORD ReturnLength, priv_index;
    HANDLE hToken;
    BOOL result;

    /*
     * URL: http://msdn.microsoft.com/en-us/library/windows/desktop/aa379180(v=vs.85).aspx
     * 
     * BOOL WINAPI LookupPrivilegeValue(
     *  _In_opt_  LPCTSTR lpSystemName,
     *  _In_      LPCTSTR lpName,
     *  _Out_     PLUID lpLuid
     * );
     */
    result = LookupPrivilegeValue(NULL, Name, &luid);
    if (result == False) return(result);

    /*
     * BOOL WINAPI OpenProcessToken(
     *  _In_   HANDLE ProcessHandle,
     *  _In_   DWORD DesiredAccess,
     *  _Out_  PHANDLE TokenHandle
     * );
     */
    result = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken);
    if (result == False) return(result);

    /*
     * BOOL WINAPI GetTokenInformation(
     *  _In_       HANDLE TokenHandle,
     *  _In_       TOKEN_INFORMATION_CLASS TokenInformationClass,
     *  _Out_opt_  LPVOID TokenInformation,
     *  _In_       DWORD TokenInformationLength,
     *  _Out_      PDWORD ReturnLength
     * );
     */
    result = GetTokenInformation(hToken, tc, NULL, (DWORD)0, &ReturnLength);
    // Yes, the result is failure, but the ReturnLength is filled in!
    tpp = malloc(ReturnLength);
    if (tpp == NULL) goto error_return;
    result = GetTokenInformation(hToken, tc, tpp, ReturnLength, &ReturnLength);
    if (result == False) {
	//os_perror(NULL, "GetTokenInformation() failed");
	goto error_return;
    }
    /*
     * typedef struct _TOKEN_PRIVILEGES {
     *  DWORD               PrivilegeCount;
     *  LUID_AND_ATTRIBUTES Privileges[ANYSIZE_ARRAY];
     * } TOKEN_PRIVILEGES, *PTOKEN_PRIVILEGES;
     * 
     * typedef struct _LUID_AND_ATTRIBUTES {
     *  LUID  Luid;
     *  DWORD Attributes;
     * } LUID_AND_ATTRIBUTES, *PLUID_AND_ATTRIBUTES;
     * 
     * typedef struct _LUID {
     *  DWORD LowPart;
     *  LONG  HighPart;
     * } LUID, *PLUID;
     */
    result = False;
    for (priv_index = 0; priv_index < tpp->PrivilegeCount; priv_index++) {
	PLUID puidp = &tpp->Privileges[priv_index].Luid;
	if ( (puidp->LowPart == luid.LowPart) &&
	     (puidp->HighPart == luid.HighPart) ) {
	    result = True;	/* Symbolic link privilege is supported! */
	    break;
	}
    }
    free(tpp);
error_return:
    (void)CloseHandle(hToken);
    return(result);
}

__inline int
os_symlink_file(char *oldpath, char *newpath)
{
    /* 
     * http://msdn.microsoft.com/en-us/library/windows/desktop/aa363860(v=vs.85).aspx 
     *  
     * BOOLEAN WINAPI CreateSymbolicLink(
     *  _In_  LPTSTR lpSymlinkFileName,
     *  _In_  LPTSTR lpTargetFileName,
     *  _In_  DWORD dwFlags
     * );
     * 
     * Note: Requires SE_CREATE_SYMBOLIC_LINK_NAME privilege.
     * This function requires the SE_CREATE_SYMBOLIC_LINK_NAME (defined as
     * "SeCreateSymbolicLinkPrivilege" in <WinNT.h>), otherwise the function
     * will fail and GetLastError will return ERROR_PRIVILEGE_NOT_HELD (1314).
     * This means the process must be run in an elevated state.
     */
    return ( (CreateSymbolicLink(newpath, oldpath, 0) == True) ? SUCCESS : FAILURE);
}
