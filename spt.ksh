#!/bin/ksh
#/****************************************************************************
# *                                                                          *
# *                       COPYRIGHT (c) 2006 - 2007                          *
# *                        This Software Provided                            *
# *                                  By                                      *
# *                       Robin's Nest Software Inc.                         *
# *                                                                          *
# * Permission to use, copy, modify, distribute and sell this software and   *
# * its documentation for any purpose and without fee is hereby granted      *
# * provided that the above copyright notice appear in all copies and that   *
# * both that copyright notice and this permission notice appear in the      *
# * supporting documentation, and that the name of the author not be used    *
# * in advertising or publicity pertaining to distribution of the software   *
# * without specific, written prior permission.                              *
# *                                                                          *
# * THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,        *
# * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN      *
# * NO EVENT SHALL HE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL   *
# * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR    *
# * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS  *
# * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF   *
# * THIS SOFTWARE.                                                           *
# *                                                                          *
# ****************************************************************************/
#
# Author:  Robin Miller
# Date:    August 11th, 2007
#
# Description:
#	Korn shell wrapper for 'spt' program to allow commands
# to be sent to detached 'spt' through pipes.  This method makes
# 'spt' appear as though it's a Korn shell builtin command.
#
# Caveats/Notes:
#	The stderr stream is redirected to stdout stream.
#	Exit status of 'spt' is return value from functions.
#	You cannot pipe output to pager (writing is to a pipe).
#
typeset -fx spt sptIntr sptGetPrompt sptPromptUser sptStartup sptSetStatus
typeset -i sptIntrFlag=0
typeset -x HISTFILE=${HISTFILE:-$HOME/.spt_history}
typeset -x HISTSIZE=${HISTSIZE:-100}
typeset -x VISUAL=${VISUAL:-"emacs"}
typeset -x SPT_PATH=${SPT_PATH:-`whence spt`}
typeset -x SPT_PATH=${SPT_PATH:-/sbin/spt}
typeset -i SPT_PID
typeset -x sptCmd
typeset -i CmdStatus HostStatus DriverStatus ScsiStatus
typeset -i SenseCode SenseKey SenseAscq ScsiResid ScsiXfer

#readonly spt sptIntr sptGetPrompt
#
# Check for arrow keys being defined for editing.
#
whence __A > /dev/null ||
{
    # These first 4 allow emacs history editing to use the arrow keys
    alias __A="" \
	  __B="" \
	  __C="" \
	  __D=""
}

#
# Catch signals, and forward on to 'spt' process.
#
function sptIntr
{
	sptIntrFlag=1
	kill -INT $SPT_PID
}

#
# This function loops reading input from the 'spt' process
# until we read the prompt string.  This is important so we
# keep things in sync between 'spt' commands for the main loop.
#
function sptGetPrompt
{
	status=1
	while read -r -u1 -p
	do
	    case $REPLY in

		spt\>\ \?\ *)
			sptSetStatus $REPLY
			status=$?
			break;;
		*)
			print -r - "$REPLY"
			;;
	    esac
	done
	return $status
}

#
# This function is used to get input from the terminal to
# parse and/or send to the 'spt' process.  It _must_ be a
# simple function (as it is), so signals get delivered to
# our trap handler properly.  Basically signals are _not_
# delivered until a function returns (as opposed to async
# signal delivery in C programs.
#
function sptPromptUser
{
	read -s sptCmd?"$1"
	return $?
}

#
#            $1    $2   $3         $4            $5
# Expect: progname> ? %status %scsi_status %sense_code \
#           $6        $7     $8    $9       $10            $11
#         %sense_key %ascq %resid %xfer %host_status %driver_status
#
function sptSetStatus
{
	saved_IFS=$IFS
	IFS=" "
	set -- $*
	CmdStatus=$3
	ScsiStatus=$4
	SenseCode=$5
	SenseKey=$6
	SenseAscq=$7
	ScsiResid=$8
	ScsiXfer=$9
        shift 9
	HostStatus=$1
	DriverStatus=$2
	IFS=$saved_IFS
	return $CmdStatus
}

function spt
{
	trap 'sptIntr' HUP INT QUIT TERM
	print -p - "$*" || return $?
	IFS=''
	status=0
	sptIntrFlag=0
	while read -r -u1 -p
	do
	    case $REPLY in

		spt\>\ \?\ *)
			sptSetStatus $REPLY
			status=$?
			break;;

		*)	#[[ $sptIntrFlag -eq 1 ]] && break
			print -r - "$REPLY"
			;;
	    esac
	    sptIntrFlag=0
	done
	IFS="$SavedIFS"
	sptIntrFlag=0
	trap - HUP INT QUIT TERM
	return $status
}

function sptStartup
{
	$SPT_PATH enable=pipes $* 2<&1 |&
	[[ $? -ne 0 ]] && return $?
	SPT_PID=$!
	sptGetPrompt
	return $?
}

#
# This is main()...
#
set +o nounset
sptStartup
#sptStartup $*
typeset -x SavedIFS="$IFS "
return $?
