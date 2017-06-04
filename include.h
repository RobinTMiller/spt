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
 * Modification History:
 * 
 * February 25th, 2016 by Robin T. Miller
 *  	Increased the args buffer size to accomodate very long command lines.
 * What's very long? how about ~45k of pout data! Therefore to support script 
 * files with lines this long, this size was increased from 32k to 64k! 
 * Note: This args buffer size is also used for the log file buffer size. 
 */
#if !defined(INCLUDE_H)
#define INCLUDE_H 1

#if (defined(__unix__) || defined(_AIX)) && !defined(__unix)
#define __unix
#endif

#if defined(__64BIT__) || defined(_LP64) || defined(__arch64__) || defined(_WIN64)
#  define QuadIsLong
#  define MACHINE_64BITS
#else /* assume !64bit compile! */
#  define QuadIsLongLong
#endif /* 64bit check */

#define ScriptExtension		".spt"

/*
 * These are used to define the max input sizes, but also get 
 * used for log buffer and string buffer sizes for formatting. 
 */
#define ARGS_BUFFER_SIZE	65536		/* Input buffer size.	*/
#define ARGV_BUFFER_SIZE	4096		/* Maximum arguments.	*/

/*
 * These buffer sizes are mainly for allocating stack space
 * for pre-formatting strings to display.  Different sizes
 * to prevent wasting stack space (my preference).
 */
#define SMALL_BUFFER_SIZE       32              /* Small buffer size.   */
#define MEDIUM_BUFFER_SIZE      64              /* Medium buffer size.  */
#define LARGE_BUFFER_SIZE       128             /* Large buffer size.   */
#define STRING_BUFFER_SIZE	4096		/* String buffer size.	*/
#define TIME_BUFFER_SIZE	32		/* ctime() buffer size. */


/*
 * Note: This should be the largest file path allowed for any Host OS.
 *	 On Linux, "errno = 36 - File name too long" occurs at 4096.
 *	 This will usually be used for short term stack allocated space.
 */
#define PATH_BUFFER_SIZE	8192		/* Max path size.	*/

#define TRUE		1			/* Boolean TRUE value.	*/
#define FALSE		0			/* Boolean FALSE value.	*/

/*
 * Declare a boolean data type.
 *
 * Note: Prefixed with 'h', to avoid redefinitions with OS includes.
 * [ Most (not all) OS's have a bool_t or boolean_t (rats!). ]
 */
typedef enum hbool {
    False = 0,
    True  = 1
} hbool_t;
typedef volatile hbool_t vbool_t;

typedef enum open_mode {
    OPEN_FOR_READING = 0,
    OPEN_FOR_WRITING = 1
} open_mode_t;

#if defined(_WIN32)
//#  define HANDLE                    (void *)
//#  define INVALID_HANDLE_VALUE      (void *)-1
#else /* !defined(_WIN32) */
#  define HANDLE                    int
#  define INVALID_HANDLE_VALUE      -1
#endif /* defined(_WIN32) */

/*
 * Define Maximum Values for Various Sizes:
 */
#define SVALUE8_MAX     127
#define VALUE8_MAX      255U
#define SVALUE16_MAX    32767
#define VALUE16_MAX     65535U
#if defined(MACHINE_64BITS)
#define SVALUE32_MAX    2147483647
#define VALUE32_MAX     4294967295U
#define SVALUE_MAX      9223372036854775807L
#define VALUE_MAX       18446744073709551615UL
#else /* !defined(MACHINE_64BITS) */
#define SVALUE32_MAX    2147483647L
#define VALUE32_MAX     4294967295UL
/* I love Windows differences like this! NOT! :-( */
#if defined(_WIN32)
#  define SVALUE_MAX      9223372036854775807i64
#  define VALUE_MAX       18446744073709551615ui64
#else /* !defined(_WIN32) */
#  define SVALUE_MAX      9223372036854775807LL
#  define VALUE_MAX       18446744073709551615ULL
#endif /* defined(_WIN32) */
#endif /* defined(MACHINE_64BITS) */

/*
 * Create some shorthands for volatile storage classes:
 */
typedef	volatile char	v_char;
typedef	volatile short	v_short;
typedef	volatile int	v_int;
typedef volatile long	v_long;
#if !defined(DEC)
/* These are defined in sys/types.h on DEC Alpha systems. */
typedef	volatile unsigned char	vu_char;
typedef	volatile unsigned short	vu_short;
typedef	volatile unsigned int	vu_int;
typedef volatile unsigned long	vu_long;
#endif /* !defined(__alpha) */

/* Newer *nix support these, Windows does not! */
#if defined(_WIN32)
typedef signed char             int8_t;
typedef short int               int16_t;
typedef signed int              int32_t;
typedef unsigned char           uint8_t;
typedef unsigned short int      uint16_t;
typedef unsigned int            uint32_t;
#  if defined(_WIN64)
/* Note: Unlike most Unix OS's, 64-bit Windows does *not* make a long 64-bits! */
//typedef long int                int64_t;
//typedef unsigned long int       uint64_t;
typedef LONG64			int64_t;
typedef ULONG64			uint64_t;
#  else /* !defined(_WIN64) */
typedef __int64			int64_t;
typedef unsigned __int64        uint64_t;
#  endif
#endif /* defined(_WIN32) */

#if defined(WIN32)

#define S64OF           "%I64d"
#define U64OF           "%I64u"
#define X64OF           "0x%I64x"
#define SLOF            "%ld"
#define ULOF            "%lu"
#define PTROF           "0x%lx"

#if defined(_LP64) || defined(__arch64__) || (_POSIX_V6_LP64_OFF64 - 0) == 1
#  define LUF	ULOF
#  define LDF	SLOF
#  define LXF	"%#lx"
#else
#  define LUF	U64OF
#  define LDF	S64OF
#  define LXF	X64OF
#endif

#else /* UNIX systems */

# if defined(_LP64) || defined(__arch64__) || (_POSIX_V6_LP64_OFF64 - 0) == 1

#  define LUF	"%lu"
#  define LDF	"%ld"
#  define LXF	"%#lx"

# else /* !defined(_LP64) && !defined(__arch64__) && !(_POSIX_V6_LP64_OFF64 - 0) == 1 */

/* 32-bit Definitions: */

#  define LUF   "%llu"
#  define LDF   "%lld"
#  define LXF   "%#llx"

# endif /* defined(_LP64) || defined(__arch64__) || (_POSIX_V6_LP64_OFF64 - 0) == 1 */
#endif /* defined(_WIN32) */

/*
 * Define some Architecture dependent types:
 */
#if defined(__alpha) || defined(__LP64__) || defined(_WIN64)
#if defined(_WIN64)
typedef ULONG64			ptr_t;
#else /* defined(_WIN64) */
typedef unsigned long		ptr_t;
#endif /* defined(_WIN64) */
typedef unsigned int		bool;
typedef volatile unsigned int	v_bool;

#else /* !defined(__alpha) && !defined(__LP64__) && !defined(_WIN64) */

typedef unsigned int		ptr_t;
//typedef unsigned char		bool;
typedef volatile unsigned char	v_bool;

#endif /* defined(__alpha) || defined(__LP64__) || defined(_WIN64) */

/*
 * For IOT block numbers, use this definition.
 * TODO: Extend this to 64-bits in the future!
 */
typedef unsigned int iotlba_t;

#define SUCCESS		0			/* Success status code. */
#define FAILURE		-1			/* Failure status code. */
#define WARNING		1			/* Warning status code.	*/
#define CONTINUE	WARNING			/* Continue operations.	*/
#define END_OF_FILE	254			/* End of file status.  */
#define END_OF_DATA	END_OF_FILE		/* End of test data.	*/
#define RESTART		253			/* Restart operations.  */
#define TRUE		1			/* Boolean TRUE value.	*/
#define FALSE		0			/* Boolean FALSE value.	*/
#define UNINITIALIZED	255			/* Uninitialized flag.	*/

#define FATAL_ERROR	255			/* Fatal error code.	*/

#define MSECS           1000                    /* Milliseconds value.  */

#define ANY_RADIX	0		/* Any radix string conversion.	*/
#define DEC_RADIX	10		/* Base for decimal conversion.	*/
#define HEX_RADIX	16		/* Base for hex str conversion.	*/

#define BLOCK_SIZE	512			/* Bytes in block.	*/
#define KBYTE_SIZE	1024			/* Kilobytes value.	*/
#define MBYTE_SIZE	1048576L		/* Megabytes value.	*/
#define GBYTE_SIZE	1073741824L	/* Gigabytes value.	*/
#if defined(_WIN32)
#  define TBYTE_SIZE      1099511627776i64	/* Terabytes value.	*/
#else /* !defined(_WIN32) */
#  define TBYTE_SIZE      1099511627776LL	/* Terabytes value.	*/
#endif /* defined(_WIN32) */

/* Avoid conflict with OS INFINITY definitions! */
//#undef INFINITY
#define MY_INFINITY VALUE_MAX 

#define SECS_PER_MIN	60
#define MINS_PER_HOUR	60
#define HOURS_PER_DAY	24
#define SECS_PER_HOUR	(SECS_PER_MIN * MINS_PER_HOUR)
#define SECS_PER_DAY	(SECS_PER_HOUR * HOURS_PER_DAY)
#define MSECS_PER_HOUR	(SECS_PER_HOUR * MSECS)
#define MSECS_PER_DAY	(SECS_PER_DAY * MSECS)
#define MSECS_PER_MIN   (SECS_PER_MIN * MSECS)
#define MSECS_PER_SEC   MSECS

/*********************************** macros **********************************/

#ifndef max
# define max(a,b)       ((a) > (b)? (a): (b))
#endif
#ifndef min
# define min(a,b)       ((a) < (b)? (a): (b))
#endif
#ifndef howmany
# define howmany(x, y)  (((x)+((y)-1))/(y))
#endif
#ifndef roundup
# define roundup(x, y)  ((((x)+((y)-1))/(y))*(y))
#endif
#ifndef rounddown
# define rounddown(x,y) (((x)/(y))*(y))
#endif
#ifndef ispowerof2
# define ispowerof2(x)  ((((x)-1)&(x))==0)
#endif

/*
 * External Variables:
 */
extern char *OurName;

#if defined(_WIN32)
# include "spt_win32.h"
#else /* !defined(_WIN32) */
# include "spt_unix.h"
#endif /* defined(_WIN32) */

/*
 * External Declarations:
 */
/* libscsi.c */
extern HANDLE OpenDevice(char *dsf);
extern int CloseDevice(HANDLE fd, char *dsf);

/* utilities.c */
extern uint32_t CvtStrtoValue(char *nstr, char **eptr, int base);
extern uint64_t CvtStrtoLarge(char *nstr, char **eptr, int base);
extern time_t CvtTimetoValue(char *nstr, char **eptr);
extern time_t CvtTimetoMsValue(char *nstr, char **eptr);

extern void InitBuffer( unsigned char	*buffer,
			size_t		count,
			uint32_t	pattern );

#endif /* !defined(INCLUDE_H) */
