#!/bin/bash


# Note: This method is required since auto-mounter and DNS are NOT configured on each OS! :-(

declare -A OSdir
declare -A OShost
declare -A OSuser

OSs="AIX HPUX LINUX SOLARIS_SPARC SOLARIS_X86 SOLARIS_X64"

OSdir[AIX]='aix-6.1'
OShost[AIX]='rtp-ps814-dev01p1.rtplab.nimblestorage.com'
OSuser[AIX]='root'

OSdir[HPUX]='hpux-ia64'
OShost[HPUX]='rh-d8-u24.rtplab.nimblestorage.com'
OSuser[HPUX]='root'

OSdir[LINUX]='linux-rhel6x64'
OShost[LINUX]='rtm-rtp-centos6'
OSuser[LINUX]='romiller'

OSdir[SOLARIS_SPARC]='solaris-sparc'
OShost[SOLARIS_SPARC]='rs-d7-u22.rtplab.nimblestorage.com'
OSuser[SOLARIS_SPARC]='root'

OSdir[SOLARIS_X86]='solaris-x86'
OShost[SOLARIS_X86]='rtm-rtp-solaris11x86.rtpvlab.nimblestorage.com'
OSuser[SOLARIS_X86]='root'

OSdir[SOLARIS_X64]='solaris-x64'
OShost[SOLARIS_X64]='rtm-rtp-solaris11x86.rtpvlab.nimblestorage.com'
OSuser[SOLARIS_X64]='root'

GITHUB="/auto/home.nas04/romiller/GitHub/spt"
SPTDIR="/var/tmp/romiller/GitHub/spt"
##OSBIN='/usr/bin/'
# Please configure ssh keys on each OS server for easier updates!

for os in ${OSs};
do
    OSDIR=${OSdir[${os}]}
    OSHOST=${OShost[${os}]}
    OSUSER=${OSuser[${os}]}
    if [[ "${os}" == "LINUX" ]]; then
	BUILD_DIR="${GITHUB}/${OSDIR}"
    else
        BUILD_DIR="${SPTDIR}/${OSDIR}"
    fi
    echo "Host: ${OSHOST}, OS Build Directory: ${OSDIR}, User: ${OSUSER}"
    # Do build, test, and copy.
    ssh ${OSUSER}@${OSHOST} "( uname -a ; cd ${BUILD_DIR} ; ./domake )"
    if [ $? -eq  0 ]; then
        # Quick test to ensure built Ok.
        ssh ${OSUSER}@${OSHOST} "( cd ${BUILD_DIR} ; ./spt version )"
        if [ $? -eq  0 ]; then
            if [ "${os}" != "LINUX" ]; then
                # Update the OS binary directory.
                ##ssh ${OSUSER}@${OSHOST} cp -p ${BUILD_DIR}/spt ${OSBIN}
                # Update the build tree in my home GitHub directory.
                scp -p ${OSUSER}@${OSHOST}:${BUILD_DIR}/spt ${GITHUB}/${OSDIR}/
            fi
        fi
    fi
done

# Windows (Robin's Laptop using Visual Studio Community Edition 2017) Manual Build!
# millerob@OVCQP3UPGX ~/spt.v23
# $ scp -p windows/x64/Release/spt.exe romiller@rtm-rtp-rhel7.rtpvlab.nimblestorage.com:Windows/

# Note: Update tools on San Jose Cycle Server: sjccycl.lab.nimblestorage.com

find . -name spt -ls
find . -name spt | xargs file

