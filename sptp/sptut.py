#!/usr/bin/python
"""
Created on December 15th, 2017

@author: Robin T. Miller

Simple script to call spt directly from Python.

"""
from __future__ import print_function

# Simple script to execute spt commands via shared library.

EXIT_STATUS_SUCCESS = 0
EXIT_STATUS_FAILURE = -1

STDERR_BUFFER_SIZE = 65536      # 64k
STDOUT_BUFFER_SIZE = 262144     # 256k (some SES pages are very large!)
EMIT_BUFFER_SIZE = 8192         # 8k

import argparse
import os
import sys
import json
import time
import subprocess
import threading
from ctypes import *

#import pdb; pdb.set_trace()

class SptLib(object):
    """Define spt library class."""

    def __init__(self):
        """Initialize spt variables."""
        # Note: This will be packaged with spt and copied to /usr/local/bin
        self.libspt = CDLL('./libspt.so')
        self.sptapi = self.libspt.spt
        self.program = "sptut"
        #
        # int
        # spt(char *stdin_buffer,
        #     char *stderr_buffer, int stderr_length,
        #     char *stdout_buffer, int stdout_length,
        #     char *emit_status_buffer, int emit_status_length)
        #
        self.sptapi.restype = c_int
        self.sptapi.argtypes = [c_char_p, c_char_p, c_int, c_char_p, c_int, c_char_p, c_int]
        self.spt_args = {}
        self.spt_args['stderr_length'] = STDERR_BUFFER_SIZE
        self.spt_args['stdout_length'] = STDOUT_BUFFER_SIZE
        self.spt_args['emit_status_length'] = EMIT_BUFFER_SIZE
        self.spt_args['stderr_buffer'] = create_string_buffer('\000' * self.spt_args['stderr_length'])
        self.spt_args['stdout_buffer'] = create_string_buffer('\000' * self.spt_args['stdout_length'])
        self.spt_args['emit_status_buffer'] = create_string_buffer('\000' * self.spt_args['emit_status_length'])
        self.spt_args['stdout_dict'] = None
        self.spt_args['emit_dict'] = None
        self.error_code = EXIT_STATUS_SUCCESS
        self.debug = False
        self.timeout = 600          # Max time for thread to be active.

    def _parse_options(self):
        """
        Parse options via argparse().
        """
        parser = argparse.ArgumentParser(prog=self.program,
                                         formatter_class=lambda prog: argparse.HelpFormatter(prog, max_help_position=30, width=132))
        parser.add_argument("--debug", action='store_true', default=self.debug, help="The debug flag. (Default: {0})".format(self.debug))
        args = parser.parse_args()
        self.debug = args.debug

    def main(self):
        """Simple example of executing spt via shared library."""

        self._parse_options()
        # Verify we can execute a simple spt command.
        status = self.execute_spt(self.spt_args, "version")
        if status == EXIT_STATUS_SUCCESS:
            # Decode the tool version information.
            tvi = self.spt_args['stdout_dict']
            print("Author: {author}, Date: {date}, Version: {version}"\
                  .format(author=tvi['Author'], date=tvi['Date'], version=tvi['Version']))
        else:
            print("version command failed, status = {status}".format(status=status))

        #status = self.execute_spt(self.spt_args, "help")
        """
        # TODO: Merge in spt "show devices" support!
        status = self.execute_spt(self.spt_args, "show devices dtype=direct")
        # Remember: The JSON is already decoded!
        sdevices = self.spt_args['stdout_dict']

        devices = list()
        for entry in sdevices['SCSI Devices']['Device List']:
            device = dict()
            for path_type in entry['Path Types']:
                if path_type.get('Linux Device'):
                    device['Linux Device Name'] = path_type['Linux Device']
                elif path_type.get('SCSI Device'):
                    device['SCSI Device Name'] = path_type['SCSI Device']
                elif path_type.get('DMMP Device'):
                    device['DMMP Device Name'] = path_type['DMMP Device']
            devices.append(device)
        """
        # Get a list of drives.
        cmd = "lsscsi --generic --transport | fgrep 'disk'"
        drives = subprocess.check_output(cmd, shell=True)
        devices = list()
        try:
            #
            # Format:
            # $ lsscsi --generic --transport
            #    [0]         [1]          [2]                       [3]        [4]
            # [0:0:0:0]    disk    sas:0x5000cca25103b471          /dev/sda   /dev/sg0
            # [0:0:1:0]    disk    sas:0x5000cca251029301          /dev/sdb   /dev/sg1
            #    ...
            # [0:0:14:0]   enclosu sas:0x5001636001caa0bd          -          /dev/sg14
            # [7:0:0:0]    cd/dvd  usb: 1-1.3:1.2                  /dev/sr0   /dev/sg15
            #
            # Special Case:
            # Handle lines without a transport (spaces only). (screen scrapping danger)
            # [0:0:10:0]   enclosu sas:0x50030480091d71fd          -         /dev/sg10
            # [1:0:0:0]    disk        <spaces>                 /dev/sdk  /dev/sg11 <- INTEL disk!
            #
            # Another SNAFU! (and why I hate screen scrapping!!!)
            # [15:0:53597:0]disk    sas:0x5000cca23b359649          /dev/sdg   /dev/sg6
            # [15:0:53598:0]disk    sas:0x5000cca23b0c0a99          /dev/sdh   /dev/sg7
            # [15:0:53599:0]disk    sas:0x5000cca23b0b7531          /dev/sdi   /dev/sg8
            #       ...
            # [15:0:53686:0]enclosu sas:0x5000ccab040001bc          -          /dev/sg165
            # [15:0:53766:0]enclosu sas:0x5000ccab040001fc          -          /dev/sg144
            #
            for line in drives.splitlines():
                dinfo = line.split()
                device = dict()
                if len(dinfo) < 5:
                    m = re.search('(?P<device>disk|enclo)', dinfo[0])
                    if m:
                        device['Device Type'] = m.group('device')
                        sas_index = 1
                        dev_index = 2
                        sg_index = 3
                    else:
                        continue
                else:
                    device['Device Type'] = dinfo[1]
                    sas_index = 2
                    dev_index = 3
                    sg_index = 4
                # Parse remaining information.
                if 'sas:' in dinfo[sas_index]:
                    device['SAS Address'] = dinfo[sas_index][4:]
                else:
                    device['SAS Address'] = ""
                # Note: Enclosure has no driver, so reports '-' for name.
                if '/dev/' in dinfo[dev_index]:
                    device['Linux Device Name'] = dinfo[dev_index]
                else:
                    device['Linux Device Name'] = ""
                if '/dev/sg' in dinfo[sg_index]:
                    device['SCSI Device Name'] = dinfo[sg_index]
                else:
                    device['SCSI Device Name'] = ""
                devices.append(device)

        except RuntimeError as exc:
            self._logger.error("Failed to acquire SCSI devices: {0}".format(exc))
            raise exc

        try:
            # Ensure spt properly handles switching devices.
            threads = list()
            for device in devices:
                dsf = device['Linux Device Name']
                print("Starting thread for {dsf}...".format(dsf=dsf))
                # Note: The device disctionary will be updated by each thread.
                thread = threading.Thread(target=self.spt_thread, args=(device,), name=dsf)
                thread.start()
                threads.append(thread)

            for thread in threads:
                thread.join(self.timeout)

            for device in devices:
                #print('{0}'.format(device))
                if device['thread_status'] != EXIT_STATUS_SUCCESS:
                    self.error_code = device['thread_status']

            self.show_device_information(devices)

        except RuntimeError as e:
            status = EXIT_STATUS_FAILURE
            print("RuntimeError exception: {exc}".format(exc=e))

        print("Finished spt commands, exiting with status {status}...".format(status=self.error_code))
        sys.exit(self.error_code)

    def spt_thread(self, device):
        """ Execute spt commands for a specified drive. """
        dsf = device['Linux Device Name']
        sdsf = device['SCSI Device Name']
        device['thread_status'] = EXIT_STATUS_FAILURE
        # Create separate args so we are thread safe.
        spt_args = dict()
        spt_args['stderr_length'] = STDERR_BUFFER_SIZE
        spt_args['stdout_length'] = STDOUT_BUFFER_SIZE
        spt_args['emit_status_length'] = EMIT_BUFFER_SIZE
        spt_args['stderr_buffer'] = create_string_buffer('\000' * spt_args['stderr_length'])
        spt_args['stdout_buffer'] = create_string_buffer('\000' * spt_args['stdout_length'])
        spt_args['emit_status_buffer'] = create_string_buffer('\000' * spt_args['emit_status_length'])
        spt_args['stdout_dict'] = None
        spt_args['emit_dict'] = None

        try:
            # Note: The spt output format defaults to JSON, so ofmt=json is NOT required!
            # Issue a Test Unit Ready with retries, waiting for the drive to become ready.
            status = self.execute_spt(spt_args, "dsf={dsf} cdb=0 status=good retry=100 msleep=100 enable=wait".format(dsf=dsf))
            # Note: "spt show devices" reports Inquiry, Serial, etc, but this is for test!
            status = self.execute_spt(spt_args, "dsf={dsf} inquiry".format(dsf=dsf))
            if (status == EXIT_STATUS_SUCCESS) and spt_args['stdout_dict']:
                inquiry = spt_args['stdout_dict']
                device['Device Type Description'] = inquiry['Inquiry']['Peripheral Device Type Description']
                device['Product Identification'] = inquiry['Inquiry']['Product Identification'].strip()
                device['Vendor Identification'] = inquiry['Inquiry']['Vendor Identification'].strip()
                device['Revision Level'] = inquiry['Inquiry']['Firmware Revision Level'].strip()

            status = self.execute_spt(spt_args, "dsf={dsf} inquiry page=serial".format(dsf=dsf))
            if (status == EXIT_STATUS_SUCCESS) and spt_args['stdout_dict']:
                serial = spt_args['stdout_dict']
                device['Serial Number'] = serial['Serial Number']['Product Serial Number'].strip()

            status = self.execute_spt(spt_args, "dsf={dsf} readcapacity16".format(dsf=dsf))
            if (status == EXIT_STATUS_SUCCESS) and spt_args['stdout_dict']:
                rdcap = spt_args['stdout_dict']
                device['Drive Capacity'] = int(rdcap['Read Capacity(16)']['Maximum Capacity'])
                device['Block Length'] = rdcap['Read Capacity(16)']['Block Length']


            # Do a short non-destructive media verify operation.
            # Note: We cannot execute multiple spt threads since each thread emits status.
            # TODO: We need dts' dynamic job control so we can emit job statistics!
            #verify16 = '0x8f'
            #status = self.execute_spt(spt_args, "dsf={dsf} cdb={opcode} bs=32k limit=1g threads=1"
            #                          .format(dsf=dsf, opcode=verify16))

        except RuntimeError as e:
            status = EXIT_STATUS_FAILURE
            print("RuntimeError exception: {exc}".format(exc=e))

        #if status != EXIT_STATUS_SUCCESS and self.error_code == EXIT_STATUS_SUCCESS:
        #    self.error_code = status

        device['thread_status'] = status
        return status

    def execute_spt(self, spt_args, command, expected_failure=False):
        """Execute an spt command and report results."""

        # Note: The debug should be removed, and left to the caller! (need for testing, for now)
        print("Executing: {cmd}".format(cmd=command))
        status = self.sptapi(command,
                             spt_args['stderr_buffer'], spt_args['stderr_length'],
                             spt_args['stdout_buffer'], spt_args['stdout_length'],
                             spt_args['emit_status_buffer'], spt_args['emit_status_length'])

        # For debug, show everything returned!
        spt_args['stdout_dict'] = {}
        if len(spt_args['stdout_buffer'].value):
            if self.debug:
                print('Stdout: {output}'.format(output=spt_args['stdout_buffer'].value))
            # Note: Not all commands emit JSON output.
            try:
                spt_args['stdout_dict'] = json.loads(spt_args['stdout_buffer'].value)
                if self.debug:
                    print("Stdout Dict: {output}".format(output=spt_args['stdout_dict']))
            except ValueError:
                # Assume output is NOT in JSON, but rather ASCII!
                pass

        if self.debug and len(spt_args['stderr_buffer'].value):
            # Note: All error messages are in ASCII only today.
            print('Stderr: {output}'.format(output=spt_args['stderr_buffer'].value))

        # Note: Commands such as 'help' and 'version' do not emit status!
        spt_args['emit_dict'] = {}
        if len(spt_args['emit_status_buffer'].value):
            if self.debug:
                print('Emit Status: {output}'.format(output=spt_args['emit_status_buffer'].value))
            spt_args['emit_dict'] = json.loads(spt_args['emit_status_buffer'].value)
            if self.debug:
                print("Emit Dict: {output}".format(output=spt_args['emit_dict']))

        if (status != EXIT_STATUS_SUCCESS) and not expected_failure:
            raise RuntimeError

        return status


    def show_device_information(self, devices=None):
        """ Show drive information without enclosure information. """

        print("\n")
        print("  Linux       SCSI                              Firmware    Drive     Block ")
        print("  Device     Device    Vendor       Product     Revision   Capacity   Length Serial Number")
        print("---------- ---------- -------- ---------------- -------- ------------ ------ -------------")

        for device in devices:
            print('{dsf:<10} {sdsf:<10} {vid:<8} {pid:<16}   {rev:<4}   {capacity:>12}  {blocklen:>4}  {serial:<14}'
                  .format(dsf=device['Linux Device Name'],
                          sdsf=device['SCSI Device Name'],
                          vid=device['Vendor Identification'],
                          pid=device['Product Identification'],
                          rev=device['Revision Level'],
                          capacity=device['Drive Capacity'],
                          blocklen=device['Block Length'],
                          serial=device['Serial Number']))

        print("\n")

if __name__ == "__main__":
    sptlib = SptLib()
    sptlib.main()
