#!/usr/bin/python
"""
Created on December 15th, 2017

@author: Robin T. Miller

Simple script to call spt directly from Python.

Modification History:

June 11th, 2018 by Robin T. Miller
    Added option to collect e6logs only, and verify media option.
    With updated spt, display full FW version, rather than revision.

May 18th, 2018 by Robin T. Miller
    Update parsing of spts' show devices, to handle multi-path disks.

"""
from __future__ import print_function

# Simple script to execute spt commands via shared library.

EXIT_STATUS_SUCCESS = 0
EXIT_STATUS_FAILURE = -1

STDERR_BUFFER_SIZE = 65536      # 64k
STDOUT_BUFFER_SIZE = 262144     # 256k (some SES pages are very large!)
EMIT_BUFFER_SIZE = 8192         # 8k

THREAD_TIMEOUT_DEFAULT = 600    # Max time for thread to be active.

import argparse
import copy
import os
import sys
import json
import time
import subprocess
import threading
from ctypes import *
from datetime import datetime

EB_LOG_DIR  = (os.path.abspath(os.path.dirname(sys.argv[0]))+"/e6logs")

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
        self.e6logs = False
        self.log_directory = EB_LOG_DIR
        self.timeout = THREAD_TIMEOUT_DEFAULT           # Max time for thread to be active.
        self.verify = False                             # Controls SCSI verify media command.

    def _parse_options(self):
        """
        Parse options via argparse().
        """
        parser = argparse.ArgumentParser(prog=self.program,
                                         formatter_class=lambda prog: argparse.HelpFormatter(prog, max_help_position=30, width=132))
        parser.add_argument("--debug", action='store_true', default=self.debug, help="The debug flag. (Default: {0})".format(self.debug))
        parser.add_argument("--e6logs", action='store_true', default=self.e6logs, help="Collect e6logs. (Default: {0})".format(self.e6logs))
        parser.add_argument("--verify", action='store_true', default=self.verify, help="Do verify media. (Default: {0})".format(self.verify))
        args = parser.parse_args()
        self.args = args
        self.debug = args.debug
        self.e6logs = args.e6logs
        self.verify = args.verify

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
        cmd = "show devices dtype=direct"
        # Only select HGST drives for e6log collextion.
        if self.e6logs:
            cmd += " vendor=HGST"

        status = self.execute_spt(self.spt_args, cmd)
        if self.e6logs:
            # Create log directory if it doesn't already exist.
            cmd = "mkdir -p {0}".format(self.log_directory)
            process = subprocess.Popen(cmd.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            stdout, stderr = process.communicate()
            if process.returncode:
                print("Command \"{0}\" failed with return code {1}".format(cmd, process.returncode))
                raise RuntimeError

        # Remember: The JSON is already decoded!
        sdevices = self.spt_args['stdout_dict']

        devices = list()
        for entry in sdevices['SCSI Devices']['Device List']:
            device = dict()
            device['Firmware Version'] = "Unknown"
            if entry.get('Full Firmware Version') is not None:
                fwver = entry['Full Firmware Version']
                if not fwver.startswith('<not available>'):
                    device['Firmware Version'] = fwver
            # Parse various Linux paths.
            for path_type in entry['Path Types']:
                if path_type.get('Linux Device'):
                    # Handle multiple Linux device paths. (these are "sd" devices)
                    if device.get('Linux Device Name'):
                        new_device = copy.deepcopy(device)
                        devices.append(new_device)
                    device['Linux Device Name'] = path_type['Linux Device']
                    # Note: The latest spt places SCSI device with Linux device.
                    if path_type.get('SCSI Device'):
                        device['SCSI Device Name'] = path_type['SCSI Device']
                elif path_type.get('SCSI Device'):
                    # Handle multiple SCSI device paths. (now, "sg" devices only)
                    if device.get('SCSI Device Name') and path_type.get('SCSI Nexus'):
                        new_device = copy.deepcopy(device)
                        devices.append(new_device)
                    device['SCSI Device Name'] = path_type['SCSI Device']
                elif path_type.get('DMMP Device'):
                    device['DMMP Device Name'] = path_type['DMMP Device']
            devices.append(device)

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

            if not self.e6logs:
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
            status = self.execute_spt(spt_args, "dsf={dsf} inquiry page=serial".format(dsf=dsf))
            serial = None
            if (status == EXIT_STATUS_SUCCESS) and spt_args['stdout_dict']:
                serial = spt_args['stdout_dict']
                device['Serial Number'] = serial['Serial Number']['Product Serial Number'].strip()

            # Check for collection only e6logs, and omit other commands after collection.
            if self.e6logs and serial:
                now = datetime.now()
                # Generate format used by Reliability Engine for consistency.
                e6log_file = (self.log_directory + '/LogDump' + '_' +
                              str(device['Serial Number']) + '_' + str(now.year) + '-' + 
                              str(now.month).zfill(2) + '-' + str(now.day).zfill(2) + '_' +
                              str(now.hour).zfill(2) + '-' + str(now.minute).zfill(2) + '-' +
                              str(now.second).zfill(2) + '.bin')
                status = self.execute_spt(spt_args, "dsf={dsf} e6logs dout={e6log_file}"\
                                          .format(dsf=dsf, e6log_file=e6log_file, expected_failure=False))
                device['thread_status'] = status
                return status

            # Note: "spt show devices" reports Inquiry, Serial, etc, but this is for test!
            status = self.execute_spt(spt_args, "dsf={dsf} inquiry".format(dsf=dsf))
            if (status == EXIT_STATUS_SUCCESS) and spt_args['stdout_dict']:
                inquiry = spt_args['stdout_dict']
                device['Device Type Description'] = inquiry['Inquiry']['Peripheral Device Type Description']
                device['Product Identification'] = inquiry['Inquiry']['Product Identification'].strip()
                device['Vendor Identification'] = inquiry['Inquiry']['Vendor Identification'].strip()
                device['Revision Level'] = inquiry['Inquiry']['Firmware Revision Level'].strip()

            status = self.execute_spt(spt_args, "dsf={dsf} readcapacity16".format(dsf=dsf))
            if (status == EXIT_STATUS_SUCCESS) and spt_args['stdout_dict']:
                rdcap = spt_args['stdout_dict']
                device['Drive Capacity'] = int(rdcap['Read Capacity(16)']['Maximum Capacity'])
                device['Block Length'] = rdcap['Read Capacity(16)']['Block Length']

            # Do a short non-destructive media verify operation.
            # Note: We cannot execute multiple spt threads since each thread emits status.
            # TODO: Add dts' dynamic job control so we can emit job statistics!
            if self.verify:
                verify16 = '0x8f'
                status = self.execute_spt(spt_args, "dsf={dsf} cdb={opcode} bs=32k limit=1g threads=1"
                                          .format(dsf=dsf, opcode=verify16))

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
            print('{dsf:<10} {sdsf:<10} {vid:<8} {pid:<16} {fw:<8} {capacity:>12}  {blocklen:>4}  {serial:<14}'
                  .format(dsf=device['Linux Device Name'],
                          sdsf=device['SCSI Device Name'],
                          vid=device['Vendor Identification'],
                          pid=device['Product Identification'],
                          fw=device['Firmware Version'] if device['Firmware Version'] <> "Unknown" else device['Revision Level'],
                          capacity=device['Drive Capacity'],
                          blocklen=device['Block Length'],
                          serial=device['Serial Number']))

        print("\n")

if __name__ == "__main__":
    sptlib = SptLib()
    sptlib.main()
