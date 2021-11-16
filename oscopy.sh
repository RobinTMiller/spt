#!/bin/bash

RGITHUB="romiller@rtm-rtp-rhel7.rtpvlab.nimblestorage.com:GitHub/spt"
TOOL="spt"

scp -p ${RGITHUB}/aix-6.1/${TOOL} aix-6.1/
scp -p ${RGITHUB}/hpux-ia64/${TOOL} hpux-ia64/
scp -p ${RGITHUB}/linux-rhel6x64/${TOOL} linux-rhel6x64/
scp -p ${RGITHUB}/linux-rhel7x64/${TOOL} linux-rhel7x64/
scp -p ${RGITHUB}/solaris-x64/${TOOL} solaris-x64/
scp -p ${RGITHUB}/solaris-sparc/${TOOL} solaris-sparc/
scp -p ${RGITHUB}/solaris-x86/${TOOL} solaris-x86/
