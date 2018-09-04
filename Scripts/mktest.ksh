#!/usr/bin/ksh
#
# Author: Robin T. Miller
# Date:   April 28, 2011
#
# Description:
#       Script to generate I/O via the SCSI pass-through (spt) tool.
#
# Modification History:
#
# January 18th, 2012 by Robin T. Miller
#   Add useScuFlag to force using Scu instead of defaulting to sanlun.
#
# December 7th, 2011 by Robin T. Miller
#   Add support for new (latest) sanlun output format (header changes).
#   Catch signals so script cleanup (delete) can be done (as chosen).
#
#       Allow environment variables to override script parameters.
#       Make resetFlag 0 by default (wish to run I/O by default)
#       Add flag to delete spt script when test is complete (default 1)
#

#
# Tool Aliases:
#
#alias spt=/usr/software/test/bin/spt.current
#alias scu=/usr/software/test/bin/scu
alias spt=/u/rtmiller/Tools/tspt.d/linux2.6-x86/spt
alias scu=/u/rtmiller/Tools/scu.d-WIP/linux2.6-x86/scu

#
# Local Definitions: (override defaults via: # export variable=value)
#
typeset runtime=${runtime:-"1h"}
typeset sleepTime=${sleepTime:-"$runtime+10"}
# May wish to adjust threads based on disks under test.
typeset threads=${threads:-"16"}
typeset deleteFlag=${deleteFlag:-"1"}
typeset executeFlag=${executeFlag:-"1"}
typeset readFlag=${readFlag:-"1"}
typeset writeFlag=${writeFlag:-"0"}
typeset resetFlag=${resetFlag:-"0"}
typeset resetSleep=${resetSleep:-"30s"}
typeset sptScript=${sptScript:-"/var/tmp/script-$$.spt"}
typeset useScuFlag=${useScuFlag:-"0"}

#
# This script uses sanlun if it's available to obtain SCSI disks to test.
# Otherwise it defaults to using scu (expected in PATH).
#
if [[ -z "$sanlunPath" && $useScuFlag -eq 0 ]]; then
  typeset sanlunPath=$(whence sanlun)
fi
if [[ -n "$sanlunPath" ]]; then
# robin-ptc# sanlun lun show
#  filer:          lun-pathname         device filename  adapter  protocol          lun size         lun state
#      vs0:  /vol/vsim2_vol0/linux_lun3  /dev/sdy         host4    iSCSI         32m (33554432)       GOOD     
#      vs0:  /vol/vsim1_vol0/linux_lun3  /dev/sdx         host4    iSCSI         32m (33554432)       GOOD     
#                       ...
# robin-ptc# nsanlun lun show (new sanlun output)
# controller:         lun-pathname         device filename  adapter  protocol          lun size         lun state
#         vs0:  /vol/vsim2_vol0/linux_lun3  /dev/sdy         host4    iSCSI         32m (33554432)       GOOD     
#         vs0:  /vol/vsim1_vol0/linux_lun3  /dev/sdx         host4    iSCSI         32m (33554432)       GOOD     
#
# Most recent sanlun!
# robin-ptc# /usr/sbin/sanlun lun show
# controller(7mode)/                              device          host                  lun    
# vserver(Cmode)       lun-pathname               filename        adapter    protocol   size    mode 
# --------------------------------------------------------------------------------------------------
# vs0                  /vol/vsim3_vol0/linux_lun1 /dev/sdt        host92     iSCSI      250m    C    
#                       ...
#
  dsfs=`sanlun lun show | egrep -v "filer|controller|vserver" | awk '{ print $3 }'`
else
  # robin-ptc# scu show edt serial raw | fgrep 'B0003$-7fT29'
  # /dev/sdi /dev/sg8|2|0|7|Direct|SPC-3|NETAPP  |LUN C-Mode      |8100|B0003$-7fT29
  # /dev/sdq /dev/sg16|3|0|7|Direct|SPC-3|NETAPP  |LUN C-Mode      |8100|B0003$-7fT29
  # /dev/sdy /dev/sg24|4|0|7|Direct|SPC-3|NETAPP  |LUN C-Mode      |8100|B0003$-7fT29
  # robin-ptc# 
  #
  # Note: Search for a serial number(s), to limit to specific disk(s).
  #dsfs=`scu show edt serial raw | fgrep 'B0003$-7fT29' | awk '{ print $1 }'`
  #
  # We will select all NetApp disks for testing.
  #
  dsfs=`scu show edt serial raw | fgrep 'NETAPP' | awk '{ print $1 }'`
fi

if [[ -z "$dsfs" ]]; then
  print -u2 "mktest: Did not find any disks to test, exiting!"
  exit 1
fi

#
# Let user know our parameters and what we're testing:
#
echo ""
echo "                     The SCSI read flag is (readFlag): $readFlag"
echo "                   The SCSI write flag is (writeFlag): $writeFlag"
echo "               The SCSI reset LUN flag is (resetFlag): $resetFlag"
echo "                The sleep after reset is (resetSleep): $resetSleep"
echo "                The SCSI command runtime is (runtime): $runtime"
echo "   The time waiting for threads active is (sleepTime): $sleepTime"
echo "       The number of threads per command is (threads): $threads"
echo "             The script execute flag is (executeFlag): $executeFlag"
echo "               The script delete flag is (deleteFlag): $deleteFlag"
echo "              The spt script file name is (sptScript): $sptScript"
echo "                                 The spt tool path is: $(whence spt)"
echo "                                 The scu tool path is: $(whence scu)"
if [[ -n "$sanlunPath" ]]; then
echo "                              The sanlun tool path is: ${sanlunPath}"
fi
echo ""
echo "         Override (variable)'s via: # export variable=value"
echo ""

#
# Create a script with disks to test.
#
rm -f $sptScript
for dsf in $dsfs
do
  if [[ $readFlag -ne 0 ]]; then
    echo "dsf=$dsf dir=read length=64k cdb='8 0 0 0 80 0' disable=verbose threads=$threads runtime=$runtime enable=async" >> $sptScript
  fi
  if [[ $writeFlag -ne 0 ]]; then
    echo "dsf=$dsf dir=write length=64k cdb='a 0 0 0 80 0' disable=verbose threads=$threads runtime=$runtime enable=async" >> $sptScript
  fi
done
# For disruptive testing, wait for threads to start, then add LUN reset.
if [[ $resetFlag -ne 0 ]]; then
  echo "# Wait for threads to start" >> $sptScript
  echo "sleep=$resetSleep" >> $sptScript
  # Reset I/O on all LUNs!
  for dsf in $dsfs
  do
    echo "dsf=$dsf op=lun_reset" >> $sptScript
  done
  echo "# Wait for threads to complete" >> $sptScript
  echo "sleep=$resetSleep" >> $sptScript
else
  echo "# Wait for threads to complete" >> $sptScript
  echo "sleep=$sleepTime" >> $sptScript
fi
echo "exit" >> $sptScript

#
# Catch signals, so we can cleanup and exit with proper status.
#
intrFlag=0
trap '
# Just continue to do cleanup...
#echo "Caught signal..."
intrFlag=1
exitStatus=2
' HUP INT QUIT TERM

#
# Ok, now execute the spt commands.
#
if [[ $executeFlag -ne 0 ]]; then
  echo "Executing script file $sptScript, please be patient..."
  spt script=$sptScript
  [[ $intrFlag -eq 0 ]] && exitStatus=$?
  if [[ $deleteFlag -ne 0 ]]; then
    rm $sptScript
  fi
else
  echo "Created script file $sptScript"
  exitStatus = 0
fi
exit $exitStatus
