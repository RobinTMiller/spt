#!/usr/bin/python
"""
Created on November 9th, 2017

@author: Robin T. Miller

Note: Tools required: spt, and smartctl (expected in PATH).

Modification History:

August 10th, 2018 by Robin T. Miller
    Add logic and option for a flexible path to the spt tool.

June 27th, 2018 by Robin T. Miller
    Switch to spt to acquire ATA temperature, since smartctl is slow.
    Note: smartctl is only used to acquire power on hours, off by default.

June 25th, 2018 by Robin T. Miller
    Added --power_on_hours option, to reduce time due to ATA w/smartctl.
    This script does not display power on hours, except in JSON output,
but since smartctl is rather slow for many ATA drives, omitting power on
hours (by default), helps with the overall execution times.

June 22nd, 2018 by Robin T. Miller
    Added --exclude= option to exclude drives from being accessed.
    Added support for disks with device type of 'Managed Host', which
is being used for SMR drives.

June 18, 2018 by Robin T. Miller
    Added --firmware_version option to filter by FW version.
    Added --drives= option to select specific drives to display.
    Update spt to honor vendor, product, serial, and target options.

June 7th, 2018 by Robin T. Miller
    With updated spt, use the FW version acquired by spt.

May 17th, 2018 by Robin T. Miller
    Update the device path parsing, now that spt has been updated.
What changed? spt correlates (maps) "sg" to "sd", and added per path
target ports, since this varies by path (of course).

May 15th, 2018 by Robin T. Miller
    Add logic to handle multiple device paths when parsing spt output.

April 13th, 2018 by Robin T. Miller
    Adding back the 'lsscsi' method of device lookup, currently required
to properly handle multi-path (DM-MP) devices. Note: spt to be updated!
    Widen 'Enc Slot Description' for Mount Madonna, due to this format:
      "SLOT 000,A019B085" since the drive serial number is included.
    Note: Standard output already includes the drive serial number.
    Nonetheless, leaving both for consistency and useful for verification.

February 20th, 2018 by Robin T. Miller
    Add parsing for several filters, to reduce desired output.

February 16th, 2018 by Robin T. Miller
    Switch to using spt for acquiring devices, rather than using lsscsi.

December 12th, 2017 by Robin T. Miller
    Switched to subprocess pipes due to poor pexpect performance when
tools emit alot of output. This is a known pexpect issue, not fixed yet!

December 9th, 2017 by Robin T. Miller
    Added pexect method to communicate with spt via pipes.
    Note: pexpect output processing found to be ~5 times slower! ;(

December 1st, 2017 by Robin T. Miller
    Add Test Unit Ready with retries to clear any initial retriable errors.
    Example: "Logical unit is in process of becoming ready" is retryable.

November 22nd, 2017 by Robin T. Miller
    Updated disks with enclosure display to include enclosure device name.
    This latter information is very helpful when there are multiple enclosures.
    Updated lsscsi parsing to account for merged fields: [15:0:53803:0]enclosu

November 15th, 2017 by Robin T. Miller
    For spt commands, use the SCSI device rather than Linux device.
    For SAS disks, switch to spt for acquiring drive temperature.
    When executing processes, enable stdout logging bu default.
    
November 14th, 2017 by Robin T. Miller
    Switched to updated versions of run_command/Command class.
    Update requesting serial number, since VM OS's don't support.

"""

from __future__ import print_function

import argparse
import copy
import errno
import json
import logging
import os
import re
import signal
import socket
import subprocess
import sys
import threading

from IPython.core import ultratb
sys.excepthook = ultratb.FormattedTB(mode='Verbose', color_scheme='Linux', call_pdb=1)

#try:
#    import pexpect
#    pexpect_available = True
#
#except ImportError:
#    print("Please install pexpect via pip, then restart!")
#    pexpect_available = False

# Switched away from pexpect for now! pexpect emulated via subprocess pipes!
pexpect_available = True

#import pdb; pdb.set_trace()

TOOL_PROMPT_INDEX = 0
TOOL_UNIQUE_INDEX = 1
TOOL_STATUS_INDEX = 2


class ShowDisks(object):
    """
    """
    # Constants:
    EXIT_STATUS_SUCCESS = 0
    EXIT_STATUS_FAILURE = -1
    EXIT_STATUS_WARNING = 1         # spts' warning exit status.
    EXIT_STATUS_USAGE_ERROR = 0
    EXIT_STATUS_RUNTIME_ERROR = 1
    EXIT_STATUS_TIMEOUT_EXPIRED = 7

    # My own log level to print to stdout! (too many things using info)
    _LOG_NAME = "LOG"
    _LOG_LEVEL = 25

    def __init__(self):
        """
        """
        self._devices = list()
        self._drives = list()
        self._exclude = list()
        self._program = "lsdisks"
        self._package_version = "1.06"
        self._sw_version = "1.0.10"
        self._json_format = False
        self._logger_name = "lsdisks"
        self._timeout = 60
        self._debug = False
        self._top_level_directory = os.path.dirname(os.path.realpath(__file__))
        #os.chdir(self._top_level_directory)
        self._tmp_directory = "/var/tmp"
        #self._tools_directory = "./bin"
        self._log_directory = "/var/tmp"
        self._hostname = socket.gethostname()
        self._force_spt = False
        self._json_format = False
        self._long_format = False
        self._include_enclosures = True
        self._power_on_hours = False
        self._report_header = True
        self._use_lsscsi = False
        # Internal variables:
        self._number_drives = 0
        self._number_enclosures = 0
        self._sas_addresses = 0
        # spt filters:
        self.firmware_version = None
        self.product_name = None
        self.vendor_name = None
        self.serial_number = None
        self.target_port = None
        # pexpect variables:
        self._pexpect_available = pexpect_available
        self.tc = None
        # Allow for flexible spt tool path locations.
        self.tool = os.environ.get('SPT_PATH')
        if not self.tool:
            if os.path.exists('./spt'):
                self.tool = './spt'
        if not self.tool:
            spt_path = self._top_level_directory + '/spt'
            if os.path.exists(spt_path):
                self.tool = spt_path
        if not self.tool:
            self.tool = 'spt'      # Expect spt to be in PATH.
        self.tool_prompt = None
        self.tool_start = None
        self.tf = None
        self.tool_prompt_index = 0
        self.tool_unique_index = 0
        self.tool_status_index = 0

    def main(self):
        """ Perform system verification and/or health checks based on the command line parameters. """

        try:
            if os.getuid():
                print("**** Error: This script (%s) must be run as root!" % sys.argv[0])
                raise RuntimeError

            exit_status = self.EXIT_STATUS_SUCCESS
            self._parse_options()
            self._init_logger()
            logger = self._logger

            # TODO: Switch to spts' shared onject API!
            if self._pexpect_available:
                self.tool_prompt='spt> \? .*\n'
                #self.tool_prompt='spt> ? .*\r\n'
                self.tool_start='enable=pipes'
                self.tool_prompt_index = TOOL_PROMPT_INDEX
                self.tool_unique_index = TOOL_UNIQUE_INDEX
                self.tool_status_index = TOOL_STATUS_INDEX
                self.start_tool(tool=self.tool, start_cmd=self.tool_start)

            self.get_devices()
            self.get_drive_information()
            self.get_enclosure_information()
            self.get_drive_enclosure_information()

            if self._number_drives:
                self.show_device_information()
            else:
                self._logger.warning("No SCSI disk drives found or none match your criteria!")

        except KeyboardInterrupt:
            pass

        except IOError as e:
            if e.errno == errno.EPIPE:
                pass

        except RuntimeError:
            exit_status = self.EXIT_STATUS_RUNTIME_ERROR
            if self._debug:
                logger.debug("Exiting with status code {0}, RuntimeError".format(exit_status))

        except TimeoutExpiredError:
            exit_status = self.EXIT_STATUS_TIMEOUT_EXPIRED
            if self._debug:
                logger.debug("Exiting with status code {0}, TimeoutExpiredError".format(exit_status))

        if self.tc:
            self.tc.terminate()
        sys.exit(exit_status)

    def _parse_options(self):
        """
        Parse options via argparse().
        """
        parser = argparse.ArgumentParser(prog=self._program,
                                         formatter_class=lambda prog: argparse.HelpFormatter(prog, max_help_position=30, width=132))
        parser.add_argument("--debug", action='store_true', default=self._debug, help="The debug flag. (Default: {0})".format(self._debug))
        parser.add_argument("--drives", default=None, help="The drives to display. (Default: {0})".format(self._drives))
        parser.add_argument("--exclude", default=None, help="The drives to exclude. (Default: {0})".format(self._exclude))
        parser.add_argument("--force_spt", action='store_true', help="Force using spt (debug). (Default: {0})".format(self._force_spt))
        parser.add_argument("--json", action='store_true', default=self._json_format, help="Enable JSON format. (Default: {0})".format(self._json_format))
        parser.add_argument("--long", action='store_true', default=self._long_format, help="Enable long format. (Default: {0})".format(self._long_format))
        parser.add_argument("--noencs", action='store_false', default=self._include_enclosures, help="Exclude enclosures. (Default: {0})".format(not self._include_enclosures))
        parser.add_argument("--noheader", action='store_false', default=self._report_header, help="Exclude headers. (Default: {0})".format(not self._report_header))
        parser.add_argument("--power_on_hours", action='store_true', default=self._power_on_hours, help="Include power on hours. (Default: {0})".format(not self._power_on_hours))
        # Filters for spt:
        parser.add_argument("--firmware_version", default=None, help="The firmware version. (Default: {0})".format(self.firmware_version))
        parser.add_argument("--product_name", default=None, help="The product name. (Default: {0})".format(self.product_name))
        parser.add_argument("--vendor_name", default=None, help="The vendor name. (Default: {0})".format(self.vendor_name))
        parser.add_argument("--serial_number", default=None, help="The serial number. (Default: {0})".format(self.serial_number))
        parser.add_argument("--sas_address", default=None, help="The SAS address. (Default: {0})".format(self.target_port))
        parser.add_argument("--target_port", default=None, help="The target port. (Default: {0})".format(self.target_port))
        parser.add_argument("--use_lsscsi", action='store_true', help="Find devices via lsscsi. (Default: {0})".format(self._use_lsscsi))
        parser.add_argument("--spt_path", default=None, help="The spt tool path. (Default: {0})".format(self.tool))

        args = parser.parse_args()

        self._debug = args.debug
        if self._debug:
            self.log_level = logging.DEBUG
        self._json_format = args.json
        self._long_format = args.long
        if args.drives:
            self._drives = args.drives.split(',')
        if args.exclude:
            self._exclude = args.exclude.split(',')
        if not args.noencs:
            self._include_enclosures = False
        if not args.noheader:
            self._report_header = False
        if args.power_on_hours:
            self._power_on_hours = True
        if args.firmware_version:
            self.firmware_version = args.firmware_version
        if args.product_name:
            self.product_name = args.product_name
        if args.vendor_name:
            self.vendor_name = args.vendor_name
        if args.serial_number:
            self.serial_number = args.serial_number
        if args.sas_address:
            self.target_port = args.sas_address
        if args.target_port:
            self.target_port = args.target_port
        if args.force_spt:
            self._force_spt = args.force_spt
        if args.use_lsscsi:
            self._use_lsscsi = args.use_lsscsi
        if args.spt_path:
            self.tool = args.spt_path

    def get_devices(self):
        """ Find SCSI devices (disks and enclosures). """

        """
        # Note: This code is no longer required with the latest spt updates.
        #       But that said, leaving for now so I don't risk breaking folks!
        if not self._use_lsscsi:
            message = "Find Number of IOM's"
            command = "lsscsi | fgrep enclo | egrep 'HGST|WDC' | wc -l"
            pdata = self._run_command(command=command, message=message, logger=self._logger, shell=True)
            ioms = (int)(pdata['stdout'].strip())
            if ioms > 1:
                self._use_lsscsi = True
            if not self._use_lsscsi and os.path.exists('/etc/multipath.conf'):
                self._use_lsscsi = True
        """
        # Allow above logic or options to override lsscsi vs. spt usage.
        if not self._use_lsscsi or self._force_spt:
            self.get_devices_spt()
        else:
            self.get_devices_lsscsi()
        return

    def get_devices_lsscsi(self):
        """ Find SCSI devices using lsscsi (disks and enclosures). """

        try:
            message = "Find SCSI Devices"
            if self._include_enclosures:
                command = "lsscsi --generic --transport | egrep 'disk|0x14|enclo'"
            else:
                command = "lsscsi --generic --transport | fgrep 'disk|0x14'"
            pdata = self._run_command(command=command, message=message, logger=self._logger, shell=True)
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
            # Evidently, the author of lsscsi did not think of consistent output! ;(
            #
            for line in pdata['stdout'].splitlines():
                dinfo = line.split()
                device = dict()
                if len(dinfo) < 5:
                    m = re.search('(?P<device>disk|\(0x14\)|enclosu)', dinfo[0])
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

                # lsscsi does not understand 'Host Managed' device type.
                if '0x14' in device['Device Type']:
                    device['Device Type'] = 'disk'

                # Parse remaining information.
                if 'sas:' in dinfo[sas_index]:
                    device['SAS Address'] = dinfo[sas_index][4:]
                    self._sas_addresses += 1
                else:
                    device['SAS Address'] = ""

                # Note: Enclosure has no driver, so reports '-' for name.
                if '/dev/' in dinfo[dev_index]:
                    if self._drives and not dinfo[dev_index] in self._drives:
                        continue
                    if self._exclude and dinfo[dev_index] in self._exclude:
                        continue
                    device['Linux Device Name'] = dinfo[dev_index]
                else:
                    device['Linux Device Name'] = ""
                if '/dev/sg' in dinfo[sg_index]:
                    device['SCSI Device Name'] = dinfo[sg_index]
                else:
                    device['SCSI Device Name'] = ""

                self._devices.append(device)

        except RuntimeError as exc:
            self._logger.error("Failed to acquire SCSI devices: {0}".format(exc))
            raise exc

    def get_devices_spt(self):
        """ Find SCSI devices using spt (disks and enclosures). """

        #import pdb; pdb.set_trace()
        if self._drives or self.firmware_version or self.product_name or self.vendor_name or \
           self.serial_number or self.target_port:
            user_options = True
        else:
            user_options = False
        try:
            # Note: Extra logic to optimize spt device directory scanning.
            if not user_options:
                if self._include_enclosures:
                    message = "Find SCSI Devices"
                    command = "{tool} show devices dtype=direct,hostmanaged,enclosure".format(tool=self.tool)
                else:
                    message = "Find SCSI Disk Drives"
                    command = "{tool} show devices dtype=direct,hostmanaged".format(tool=self.tool)
                # Use common execute below.
            else:
                # Request enclosures separately.
                if self._include_enclosures:
                    message = "Find SCSI Enclosures"
                    command = "{tool} show devices dtype=enclosure ofmt=json".format(tool=self.tool)
                    pdata = self._run_command(command=command, message=message,
                                              logger=self._logger, shell=False, expected_failure=True)
                    if pdata['exit_code'] == self.EXIT_STATUS_SUCCESS and pdata['stdout']:
                        devices = json.loads(pdata['stdout'])
                        self.parse_devices_spt(devices)

                message = "Find SCSI Disk Drives"
                # Selective drives or all direct access (disk drives).
                if self._drives:
                    command = "{tool} show edt dtype=direct,hostmanaged devices={drives}"\
                              .format(tool=self.tool, drives=",".join(self._drives))
                else:
                    command = "{tool} show devices dtype=direct,hostmanaged".format(tool=self.tool)
                # Apply optional parameters.
                if self.product_name:
                    command += " pid={product}".format(product=self.product_name)
                if self.vendor_name:
                    command += " vid={vendor}".format(vendor=self.vendor_name)
                if self.serial_number:
                    command += " serial={serial}".format(serial=self.serial_number)
                if self.target_port:
                    command += " tport={target}".format(target=self.target_port)
                if self.firmware_version:
                    command += " fw_version={firmware}".format(firmware=self.firmware_version)

            # Add common spt options, we want JSON output!
            if self._exclude:
                command += " exclude={drives}".format(drives=",".join(self._exclude))
            command += " ofmt=json"
            # Finally, execute spt and parse its' JSON output (if any).
            pdata = self._run_command(command=command, message=message,
                                      logger=self._logger, shell=False, expected_failure=True)
            # spt emits warning status (1) and no JSON output if no devices found.
            if pdata['exit_code'] == self.EXIT_STATUS_SUCCESS and pdata['stdout']:
                devices = json.loads(pdata['stdout'])
                self.parse_devices_spt(devices)

        except RuntimeError as exc:
            self._logger.error("Failed to acquire SCSI devices: {0}".format(exc))
            raise exc

        except ValueError as exc:
            self._logger.error("Failed to parse spts' JSON output: {0}".format(exc))
            raise exc

    def parse_devices_spt(self, devices=None):
        """ Parse SCSI devices from spt (disks and enclosures). """

        if not devices:
            self._logger.warning("The devices list is empty, so no devices parsed!")
            return
        try:
            for entry in devices['SCSI Devices']['Device List']:
                device_type = entry['Peripheral Device Type Description']
                if self._include_enclosures:
                    if not device_type.startswith('Direct') and \
                       not device_type.startswith('Host Managed') and \
                       not device_type.startswith('Enclosure'):
                        continue
                else:
                    if not device_type.startswith('Direct') and \
                       not device_type.startswith('Host Managed'):
                        continue

                # Parse remaining information.
                if device_type.startswith('Direct') or device_type.startswith('Host Managed'):
                    device_type = 'disk'
                    if self.product_name and not self.product_name in entry['Product Identification'].strip():
                        continue;
                    if self.vendor_name and not self.vendor_name in entry['Vendor Identification'].strip():
                        continue;
                    if self.serial_number and not self.serial_number in entry['Product Serial Number'].strip():
                        continue;
                    if self.target_port and not self.target_port in entry['Device Target Port']:
                        continue;
                elif device_type.startswith('Enclosure'):
                    device_type = 'enclosure'

                device = dict()
                device['Device Type'] = device_type

                device['Device Type Description'] = entry['Peripheral Device Type Description']
                device['Product Identification'] = entry['Product Identification'].strip()
                device['Vendor Identification'] = entry['Vendor Identification'].strip()
                device['Revision Level'] = entry['Firmware Revision Level'].strip()

                if entry.get('Full Firmware Version') is not None:
                    fwver = entry['Full Firmware Version']
                    if not fwver.startswith('<not available>'):
                        device['Firmware Version'] = fwver

                serial = entry['Product Serial Number']
                device['Serial Number'] = serial.strip()

                # Note: Not currently displayed. (WWN == LUN Device Identification)
                wwn = entry['Device World Wide Name']
                if wwn.startswith('<not available>'):
                    wwn = ""
                device['Device World Wide Name'] = wwn

                sas_address = entry['Device Target Port']
                if not sas_address.startswith('<not available>'):
                    device['SAS Address'] = sas_address
                    self._sas_addresses += 1
                else:
                    device['SAS Address'] = ""

                # Note: There's probably a better Pythonic way to do this?
                device['Linux Device Name'] = ""
                device['SCSI Device Name'] = ""
                device['DMMP Device Name'] = ""

                # Parse the device paths.
                for path_type in entry['Path Types']:
                    if path_type.get('Linux Device'):
                        # Handle multiple Linux device paths. (these are "sd" devices)
                        if device.get('Linux Device Name') and path_type.get('SCSI Nexus'):
                            new_device = copy.deepcopy(device)
                            self._devices.append(new_device)
                            # Fall through to update this device entry.
                        # Initialize information for this (or next) device.
                        device['Linux Device Name'] = path_type['Linux Device']
                        device['Linux SCSI Nexus'] = path_type['SCSI Nexus']
                        if path_type.get('SCSI Device'):
                            device['SCSI Device Name'] = path_type['SCSI Device']
                        if path_type.get('Device Target Port'):
                            device['SAS Address'] = path_type['Device Target Port']

                    elif path_type.get('SCSI Device'):
                        # Handle multiple SCSI device paths. (now, "sg" devices only)
                        if device.get('SCSI Device Name') and path_type.get('SCSI Nexus'):
                            new_device = copy.deepcopy(device)
                            self._devices.append(new_device)
                            # Fall through to update this device entry.
                        # Initialize information for this (or next) device.
                        device['SCSI Device Name'] = path_type['SCSI Device']
                        device['SCSI Nexus'] = path_type['SCSI Nexus']
                        if path_type.get('Device Target Port'):
                            device['SAS Address'] = path_type['Device Target Port']

                    elif path_type.get('DMMP Device') is not None:
                        # Initialize information for this device. (limited)
                        device['DMMP Device Name'] = path_type['DMMP Device']

                # Hack: We don't find a SCSI device if there's no serial number or device ID (WWN).
                # This is observed on Linux VM's, so not common, but we still wish to handle this!
                if not len(device['SCSI Device Name']):
                    # Funky DM-MP names are skipped! (we deal with sd and/or sg devices only)
                    #  /dev/mapper/centos_cos--lab--vm01-root
                    if not len(device['Linux Device Name']):
                        continue

                self._devices.append(device)

        except RuntimeError as exc:
            self._logger.error("Failed to acquire SCSI devices: {0}".format(exc))
            raise exc

    def find_scsi_device(self, path_types, nexus):
        """ Find the SCSI device path via SCSI nexus information. """

        for path_type in path_types:
            if path_type.get('SCSI Device'):
                if path_type['SCSI Nexus'] == nexus:
                    return path_type['SCSI Device']
        return None

    def get_target_port(self, dsf):
        """ Get the Target Port Address. """

        try:
            message = "Get The Target Port Address"
            command = "spt dsf={dsf} inquiry page=deviceid output-format=json emit=".format(dsf=dsf)
            pdata = self._run_command(command=command, message=message, logger=self._logger, shell=False)
            deviceid = json.loads(pdata['stdout'])

            for desc in deviceid['Device Identification']['Identifier Descriptor List']:
                if ( desc.get('Protocol Identifier Description') and
                     desc['Protocol Identifier Description'] == 'SAS Serial SCSI Protocol' ):
                    return desc['IEEE Registered Identifier']

            return None

        except RuntimeError as exc:
            self._logger.error("Failed to acquire SCSI Inquiry Device ID page: {0}".format(exc))
            raise exc

    def get_drive_information(self):
        """ Get Drive Information. """

        try:
            self._logger.info("Gathering Drive Information")

            for device in self._devices:
                if not device['Device Type'].startswith("disk"):
                    continue
                self._number_drives += 1
                dsf = device['Linux Device Name']
                sdsf = device['SCSI Device Name']
                if not len(sdsf):
                    sdsf = dsf

                if device.get('Device Type Description') is None:
                    message = "Get Inquiry Information"
                    command = "spt dsf={dsf} inquiry output-format=json emit=".format(dsf=sdsf)
                    pdata = self._run_command(command=command, message=message, logger=self._logger, shell=False)
                    inquiry = json.loads(pdata['stdout'])
                    device['Device Type Description'] = inquiry['Inquiry']['Peripheral Device Type Description']
                    device['Product Identification'] = inquiry['Inquiry']['Product Identification'].strip()
                    device['Vendor Identification'] = inquiry['Inquiry']['Vendor Identification'].strip()
                    # The version is returned for ATA drives, so use it.
                    if device.get('Revision Level') is None:
                        device['Revision Level'] = inquiry['Inquiry']['Firmware Revision Level'].strip()

                message = "Test Unit Ready w/retries to clear errors"
                command = "spt dsf={dsf} cdb=0 emit= enable=recovery".format(dsf=sdsf)
                pdata = self._run_command(command=command, message=message,
                                          logger=self._logger, shell=False, expected_failure=True)

                # Note: Using spt, we already acquired the serial number, but bypassed Inquiry.
                if device.get('Serial Number') is None:
                    # HGST/Sandisk drives have the serial number at the end of the standard Inquiry.
                    device['Serial Number'] = ""
                    if inquiry['Inquiry'].get('Serial Number'):
                        serial = inquiry['Inquiry']['Serial Number']
                        device['Serial Number'] = serial.strip()
                    elif "ATA" not in device['Vendor Identification']:
                        message = "Get Drive Serial Number"
                        command = "spt dsf={dsf} inquiry page=serial output-format=json emit=".format(dsf=sdsf)
                        pdata = self._run_command(command=command, message=message,
                                                  logger=self._logger, shell=False, expected_failure=True)
                        # Note: Some VM OS disks do NOT support serial numbers! (sigh)
                        if pdata['exit_code'] == self.EXIT_STATUS_SUCCESS:
                            page80 = json.loads(pdata['stdout'])
                            device['Serial Number'] = page80['Serial Number']['Product Serial Number'].strip()

                device['Power On Hours'] = ""
                if "ATA" in device['Vendor Identification']:
                    if device.get('Firmware Version') is None:
                        # Note: This method works for all ATA drives!
                        message = "Get ATA Firmware Version & Serial Number"
                        command = 'spt dsf={dsf} cdb="85 08 0e 00 00 00 01 00 00 00 00 00 00 40 ec 00"'\
                                  ' dir=read length=512 enable=sata disable=verbose unpack="%C:20:20 %C:46:8\\n"'.format(dsf=sdsf)
                        #command = 'spt dsf={dsf} cdb="85 08 0e 00 00 00 01 00 00 00 00 00 00 40 ec 00"'\
                        #          ' dir=read length=512 enable=sata disable=verbose unpack="%C:20:20 %C:46:8"'.format(dsf=sdsf)
                        pdata = self._run_command(command=command, message=message, logger=self._logger, shell=True)
                        serial, firmware = pdata['stdout'].split()
                        device['Firmware Version'] = firmware.strip()
                        # Note: Not all Sandisk drives are returning a serial number!
                        device['Serial Number'] = serial.strip()

                    # Optionally get the power-on hours, used by manufacturing and test automation.
                    if self._power_on_hours:
                        # smartctl format: (for ATA drives)
                        #   9 Power_On_Hours          0x0032   100   100   000    Old_age   Always       -       13796
                        message = "Get ATA Power On Hours"
                        command = "smartctl --attributes --log=error {dsf} | fgrep 'Power_On_Hours' | awk '{{ print $10 }}'".format(dsf=dsf)
                        pdata = self._run_command(command=command, message=message, logger=self._logger, shell=True)
                        device['Power On Hours'] = pdata['stdout'].strip()
                elif "HGST" in device['Vendor Identification'] and "SDL" in device['Product Identification']:
                    if device.get('Firmware Version') is None:
                        device['Firmware Version'] = device['Revision Level']
                elif device.get('Firmware Version') is None:
                    device['Firmware Version'] = device['Revision Level']

                message = "Get Disk Capacity"
                command = "spt dsf={dsf} readcapacity16 output-format=json emit=".format(dsf=sdsf)
                pdata = self._run_command(command=command, message=message, logger=self._logger, shell=False)
                rdcap = json.loads(pdata['stdout'])
                device['Drive Capacity'] = int(rdcap['Read Capacity(16)']['Maximum Capacity'])
                device['Block Length'] = rdcap['Read Capacity(16)']['Block Length']

                device['Current Temperature'] = ""
                #
                # Note: Using smartctl sucks, since output formats differ, as do log types supported!
                # FWIW: Dealing with the ATA versus SAS difference has been very time consuming! ;(
                # Also Note: Using smartctl with --all or --xall creates a large amount of overhead!
                #
                if "ATA" in device['Vendor Identification']:
                    # smartctl format: (for ATA drives)
                    # Current Temperature:                    39 Celsius
                    message = "Get Current Drive Temperature"
                    #command = "smartctl --attributes --log=scttemp {dsf} | fgrep 'Current Temperature:' | awk '{{ print $3 }}'".format(dsf=dsf)
                    #pdata = self._run_command(command=command, message=message, logger=self._logger, shell=True)
                    command = "spt dsf={dsf} cdb='85,08,0e,00,d5,00,01,00,e0,00,4f,00,c2,00,b0,00' dir=read "\
                               "length=512 disable=verbose unpack='%B:200\\n'".format(dsf=dsf)
                    pdata = self._run_command(command=command, message=message, logger=self._logger)
                    temperature = pdata['stdout'].strip()
                    if len(temperature):
                        device['Current Temperature'] = "{temp} C".format(temp=temperature)
                else:
                    message = "Get Current Drive Temperature"
                    command = "spt dsf={dsf} logsense page=temperature ofmt=json rfmt=brief emit=".format(dsf=sdsf)
                    pdata = self._run_command(command=command, message=message,
                                              logger=self._logger, shell=False, expected_failure=True)
                    # Note: Some VM OS disks do NOT support log pages! (limited)
                    if pdata['exit_code'] == self.EXIT_STATUS_SUCCESS:
                        log_temp = json.loads(pdata['stdout'])
                        temperature, celcius = log_temp['Temperature']['Current Temperature'].split()
                        device['Current Temperature'] = "{temp} C".format(temp=temperature)

        except ValueError as exc:
            self._logger.error("ValueError caught while acquiring drive information: {0}".format(exc))
            raise exc

        except (RuntimeError, TimeoutExpiredError) as exc:
            self._logger.error("Exception caught while acquiring drive information: {0}".format(exc))
            raise exc

    def get_enclosure_information(self):
        """ Get Enclosure Information. """

        try:
            self._logger.info("Gathering Enclosure Information")

            for device in self._devices:
                # Note: Match is adequate for lsscsi or spt!
                if not device['Device Type'].startswith("enclosu"):
                    continue
                self._number_enclosures += 1
                sdsf = device['SCSI Device Name']
                message = "Get Inquiry Information"
                command = "spt dsf={sdsf} inquiry output-format=json emit=".format(sdsf=sdsf)
                pdata = self._run_command(command=command, message=message, logger=self._logger, shell=False)
                inquiry = json.loads(pdata['stdout'])
                device['Device Type Description'] = inquiry['Inquiry']['Peripheral Device Type Description']
                device['Product Identification'] = inquiry['Inquiry']['Product Identification'].strip()
                device['Vendor Identification'] = inquiry['Inquiry']['Vendor Identification'].strip()
                device['Firmware Version'] = inquiry['Inquiry']['Firmware Revision Level'].strip()
                #
                # Note: HGST Enclosures have a serial number, Quanta enclosures do not!
                #
                device['Serial Number'] = ""
                message = "Get Enclosure Serial Number"
                command = "spt dsf={dsf} inquiry page=serial output-format=json emit=".format(dsf=sdsf)
                pdata = self._run_command(command=command, message=message,
                                          logger=self._logger, shell=False, expected_failure=True)
                if pdata['exit_code'] == self.EXIT_STATUS_SUCCESS:
                    page80 = json.loads(pdata['stdout'])
                    device['Serial Number'] = page80['Serial Number']['Product Serial Number'].strip()
                #
                # Now, gather SES pages of interest.
                #
                message = "Get SES Element Information"
                command = "spt dsf={sdsf} rcvdiag page=element etype=array output-format=json emit=".format(sdsf=sdsf)
                pdata = self._run_command(command=command, message=message, logger=self._logger, shell=False)
                device['Element Information'] = json.loads(pdata['stdout'])

                message = "Get SES Additional Element Information"
                command = "spt dsf={sdsf} rcvdiag page=addl_element_status etype=array output-format=json emit=".format(sdsf=sdsf)
                pdata = self._run_command(command=command, message=message, logger=self._logger, shell=False)
                device['Additional Element Information'] = json.loads(pdata['stdout'])

        except (RuntimeError, TimeoutExpiredError) as exc:
            self._logger.error("Exception caught while acquiring enclosure information: {0}".format(exc))
            raise exc

    def get_drive_enclosure_information(self):
        """ Get the Drive Enclosure Information. """

        for device in self._devices:
            if not device['Device Type'].startswith("disk"):
                continue
            enc_device, device_slot, element_index = self.get_device_slot(device['SAS Address'])
            device['Enclosure Device'] = enc_device
            device['Enclosure Slot'] = device_slot
            device['Slot Description'] = self.get_array_desc_text(enc_device, element_index)

    def get_device_slot(self, sas):
        """ Get the Device Slot Number. """

        for device in self._devices:
            if not device['Device Type'].startswith("enclosu"):
                continue
            array_devices = device['Additional Element Information']['Additional Element Status']['Array Device Slot']
            for array_device in array_devices['Descriptor List']:
                for phy_desc in array_device['Descriptor List']:
                    if sas == phy_desc['SAS Address']:
                        element_index = int(array_device['Element Index'])
                        # Denali vs. Pikespeak Difference.
                        if not array_device['Element Index Includes Overall']:
                            # Adjust for 'Overall Status Descriptor' in Element page.
                            element_index += 1
                        return device['SCSI Device Name'], array_device['Device Slot Number'], element_index
        return "", "", -1

    def get_array_desc_text(self, enc_device, element_index):
        """ Get the Array Slot Descriptor Text. """

        for device in self._devices:
            if not device['Device Type'].startswith("enclosu"):
                continue
            if enc_device != device['SCSI Device Name']:
                continue
            array_devices = device['Element Information']['Element Descriptor']['Array Device Slot']
            return array_devices['Descriptor List'][element_index]['Descriptor Text'].strip()
        return ""

    def show_device_information(self):
        """ Show the drive information collected. """

        if self._json_format:
            print(json.dumps(self._devices, indent=4, separators=(',', ': ')))
            return

        if self._long_format:
            self.show_device_information_long()
        elif self._include_enclosures and self._number_enclosures:
            self.show_device_information_enclosures()
        else:
            self.show_device_information_only()

    def show_device_information_enclosures(self):
        """ Show drive information with enclosure information. """

        if self._report_header:
            print("\n")
            print("  Linux       SCSI                              Firmware    Drive     Block  Curr                                    Enc SCSI  Enc      Enc Slot")
            print("  Device     Device    Vendor       Product     Revision   Capacity   Length Temp Serial Number     SAS Address       Device   Slot    Description")
            print("---------- ---------- -------- ---------------- -------- ------------ ------ ---- -------------- ------------------ ---------- ---- -----------------")

        for device in self._devices:
            if not device['Device Type'].startswith("disk"):
                continue
            print('{dsf:<10} {sdsf:<10} {vid:<8} {pid:<16} {fw:<8} {capacity:>12}  {blocklen:>4}  {temp:<4} {serial:<14} {sas:<18} {edsf:<10}  {slot:<3} {text:<16}'
                  .format(dsf=device['Linux Device Name'],
                          sdsf=device['SCSI Device Name'],
                          vid=device['Vendor Identification'],
                          pid=device['Product Identification'],
                          fw=device['Firmware Version'],
                          capacity=device['Drive Capacity'],
                          blocklen=device['Block Length'],
                          temp=device['Current Temperature'],
                          serial=device['Serial Number'],
                          sas=device['SAS Address'],
                          edsf=device['Enclosure Device'],
                          slot=device['Enclosure Slot'],
                          text=device['Slot Description']))

        if self._report_header:
            print("\n")

    def show_device_information_only(self):
        """ Show drive information without enclosure information. """

        # TODO: Optimize formatting later!
        if self._sas_addresses:
            if self._report_header:
                print("\n")
                print("  Linux       SCSI                              Firmware    Drive     Block  Curr")
                print("  Device     Device    Vendor       Product     Revision   Capacity   Length Temp Serial Number     SAS Address")
                print("---------- ---------- -------- ---------------- -------- ------------ ------ ---- -------------- ------------------")

            for device in self._devices:
                if not device['Device Type'].startswith("disk"):
                    continue
                print('{dsf:<10} {sdsf:<10} {vid:<8} {pid:<16} {fw:<8} {capacity:>12}  {blocklen:>4}  {temp:<4} {serial:<14} {sas:<18}'
                      .format(dsf=device['Linux Device Name'],
                              sdsf=device['SCSI Device Name'],
                              vid=device['Vendor Identification'],
                              pid=device['Product Identification'],
                              fw=device['Firmware Version'],
                              capacity=device['Drive Capacity'],
                              blocklen=device['Block Length'],
                              temp=device['Current Temperature'],
                              serial=device['Serial Number'],
                              sas=device['SAS Address']))
        else:
            if self._report_header:
                print("\n")
                print("  Linux       SCSI                              Firmware    Drive     Block  Curr")
                print("  Device     Device    Vendor       Product     Revision   Capacity   Length Temp Serial Number")
                print("---------- ---------- -------- ---------------- -------- ------------ ------ ---- -------------")

            for device in self._devices:
                if not device['Device Type'].startswith("disk"):
                    continue
                print('{dsf:<10} {sdsf:<10} {vid:<8} {pid:<16} {fw:<8} {capacity:>12}  {blocklen:>4}  {temp:<4} {serial:<14}'
                      .format(dsf=device['Linux Device Name'],
                              sdsf=device['SCSI Device Name'],
                              vid=device['Vendor Identification'],
                              pid=device['Product Identification'],
                              fw=device['Firmware Version'],
                              capacity=device['Drive Capacity'],
                              blocklen=device['Block Length'],
                              temp=device['Current Temperature'],
                              serial=device['Serial Number']))

        if self._report_header:
            print("\n")

    def show_device_information_long(self):
        """ Show device infromation in long format. """

        for device in self._devices:
            print("")
            if device['Device Type'].startswith("enclosu"):
                if device.get('Device Type'):
                    print("{0:>32}: {1}".format("Device Type", device['Device Type']))
                if device['Device Type Description']:
                    print("{0:>32}: {1}".format("Device Description", device['Device Type Description']))
                if device.get('SCSI Device Name'):
                    print("{0:>32}: {1}".format("SCSI Device Name", device['SCSI Device Name']))
                if device.get('Product Identification'):
                    print("{0:>32}: {1}".format("Product Identification", device['Product Identification']))
                if device.get('Vendor Identification'):
                    print("{0:>32}: {1}".format("Vendor Identification", device['Vendor Identification']))
                if device.get('Firmware Version'):
                    print("{0:>32}: {1}".format("Firmware Version", device['Firmware Version']))
                if device.get('Serial Number'):
                    print("{0:>32}: {1}".format("Serial Number", device['Serial Number']))
                if device.get('SAS Address'):
                    print("{0:>32}: {1}".format("SAS Address", device['SAS Address']))
            else:
                if device.get('Device Type'):
                    print("{0:>32}: {1}".format("Device Type", device['Device Type']))
                if device['Device Type Description']:
                    print("{0:>32}: {1}".format("Device Description", device['Device Type Description']))
                if device.get('Linux Device Name'):
                    print("{0:>32}: {1}".format("Linux Device Name", device['Linux Device Name']))
                if device.get('SCSI Device Name'):
                    print("{0:>32}: {1}".format("SCSI Device Name", device['SCSI Device Name']))
                if device.get('Product Identification'):
                    print("{0:>32}: {1}".format("Product Identification", device['Product Identification']))
                if device.get('Vendor Identification'):
                    print("{0:>32}: {1}".format("Vendor Identification", device['Vendor Identification']))
                if device.get('Firmware Version'):
                    print("{0:>32}: {1}".format("Firmware Version", device['Firmware Version']))
                if device.get('Serial Number'):
                    print("{0:>32}: {1}".format("Serial Number", device['Serial Number']))
                if device.get('Drive Capacity'):
                    print("{0:>32}: {1}".format("Drive Capacity", device['Drive Capacity']))
                if device.get('Block Length'):
                    print("{0:>32}: {1}".format("Block Length", device['Block Length']))
                if device.get('Power On Hours'):
                    print("{0:>32}: {1}".format("Power On Hours", device['Power On Hours']))
                if device.get('Current Temperature'):
                    print("{0:>32}: {1}".format("Current Temperature", device['Current Temperature']))
                if device.get('SAS Address'):
                    print("{0:>32}: {1}".format("SAS Address", device['SAS Address']))
                if device.get('Enclosure Device'):
                    print("{0:>32}: {1}".format("Enclosure Device", device['Enclosure Device']))
                if device.get('Enclosure Slot'):
                    print("{0:>32}: {1}".format("Enclosure Slot", device['Enclosure Slot']))
                if device.get('Slot Description'):
                    print("{0:>32}: {1}".format("Slot Desciption", device['Slot Description']))

        if len(self._devices):
            print("")

    @staticmethod
    def _create_directory(logger=None, directory=None):
        """ Create a set of directory paths, handling errors accordingly

        Arguments:
            logger = The optional logger object (None uses print)
            directory = The directory path to create.

        Expect file exist error, otherwise raise RuntimeError.
        """
        try:
            # For Python 3, we can add "exist_ok=True".
            os.makedirs(directory)

        except OSError as exc:
            if exc.errno == errno.EEXIST and os.path.isdir(directory):
                pass
            else:
                if logger:
                    logger.error("Failed to create servers directory {0}: {1}".format(directory, exc))
                else:
                    print("Failed to create servers directory {0}: {1}".format(directory, exc))
                raise RuntimeError()

    def _init_logger(self):
        """ Initialize the log file and logger. """
        # Create log directory, if it doesn't already exist.
        self._create_directory(directory=self._log_directory)
        log_filename = "{0}/{1}.log".format(self._log_directory, self._program)

        # Add the date to the log file names.
        logging.basicConfig(
            filename=log_filename,
            filemode='w',
            level=logging.DEBUG,
            format='%(asctime)s|%(name)s|%(levelname)-5s| %(message)s',
            datefmt='%Y-%m-%d %I:%M:%S %p')

        # define a Handler which writes LOG messages or higher to the sys.stderr
        console = logging.StreamHandler()
        #
        # Note: Anything above the logging level is displayed to stdout.
        #
        # Level    Numeric value
        # CRITICAL	50
        # ERROR 	40
        # WARNING	30
        # LOG       25 (our log level)
        # INFO	    20
        # DEBUG 	10
        # NOTSET	0
        #
        # Add a logging level to always display to stderr.
        logging.addLevelName(self._LOG_LEVEL, self._LOG_NAME)
        if self._debug:
            console.setLevel(logging.DEBUG)
        else:
            console.setLevel(self._LOG_LEVEL)
        # Set a format which is simpler for console use.
        formatter = logging.Formatter('%(name)s|%(levelname)-5s| %(message)s')
        console.setFormatter(formatter)
        # Add the handler to the root logger.
        logging.getLogger('').addHandler(console)
        self._logger = logging.getLogger()

    def _run_command(self, command=None, message=None, logger=None, log_stdout=True, shell=False, timeout=None, expected_failure=False):
        """
        Run a command and handle failures.

        :param command: The command to execute.
        :param message: The message to log for this command, or None.
        :param logger: The logger for logging messages.
        :param log_stdout: Boolean to control logging standard output.
        :param shell: Boolean to conrol execute via the shell.
        :param timeout: The timeout value.
        :param expected_failure: Boolean indicating if error is expected.

        :return: Dictionary with process information
        :raises: AttributeError, TimeoutExpiredError, or RuntimeError
        :rtype: dict {'exit_code': value, 'stdout': text, 'stdout': text}
        """
        try:
            if not timeout:
                timeout = self._timeout

            if self.tc and 'spt' in command:
                pdata = self.tool_communicate(command=command, message=message, logger=logger,
                                              log_output=log_stdout, expected_failure=expected_failure)
            else:
                cmd = Command(command)
                pdata = cmd.run(message=message, logger=logger, log_stdout=log_stdout,
                                shell=shell, timeout=timeout, expected_failure=expected_failure)
            return pdata

        except TimeoutExpiredError as exc:
            raise exc

        except (RuntimeError, AttributeError):
            raise RuntimeError

    def tool_communicate(self, command=None, message=None, logger=None, log_output=True, expected_failure=False):
        """ Send spt command via pipe interface. """
        # Strip spt path, since spt is already running via pipes.
        cmd = re.sub('^.*spt ', '', command)
        # Remove any user defined emit status string.
        cmd = re.sub(' emit=', '', cmd)
        pdata = self.send_command(command=cmd, message=message, logger=logger,
                                  log_output=log_output, expected_failure=expected_failure)
        return {'exit_code': pdata['status'],
                'stdout': pdata['output'],
                'stderr': ""}

    def start_tool(self, tool=None, start_cmd=None, maxread=65536):
        """
        Start tool via pexpect.

        :param tool: The tool to start.
        :param start_cmd: The initial command.
        :param maxread: The max characters per read. (pexpects' default is 2000)

        :return: Returns dictionary of {'status': value, 'output': text (stdout and stderr)
        """
        # TODO: Use pxssh for SSH sessions!
        if not tool or not start_cmd:
            print("Require the tool name and start command!")
            raise RuntimeError
        if self._debug:
            print('Starting: {tool} {cmd}'.format(tool=tool, cmd=start_cmd))
        self.tc = subprocess.Popen([tool, start_cmd],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=1)
        #self.tc = pexpect.spawn('{tool} {cmd}'.format(tool=tool, cmd=start_cmd), echo=False, maxread=maxread)
        self.get_prompt()

    def get_prompt(self, timeout=30):
        """Wait for and parse tool prompt."""
        #self.tc.expect(self.tool_prompt, timeout=timeout)
        #self.tf = self.tc.after.split()
        #return {'status': int(self.tf[self.tool_status_index]), 'output': self.tc.before}
        output = ""
        # Loop until we receive the special spt prompt while in pipe mode.
        while True:
            line = self.tc.stdout.readline()
            if re.search(self.tool_prompt, line):
                self.tf = line.split()
                break
            elif not len(line):
                # We've reached EOF or spt exited abnormally, usually a core dump!
                raise RuntimeError
            else:
                output += line
        #if self._debug:
        #    print('Response: {0}'.format(self.tf))
        #    print('Output: {0}'.format(output), end=None)
        #import pdb; pdb.set_trace()
        return {'status': int(self.tf[self.tool_status_index]), 'output': output}

    def send_command(self, command=None, message=None, logger=None, log_output=True, expected_failure=False, timeout=30):
        """Send a tool command."""
        try:
            if message:
                logger.info("Command: {0}".format(message))
            logger.info("Command:   {tool} {cmd}".format(tool=self.tool, cmd=command))

            self.tc.stdin.write(command + '\n')
            pdata = self.get_prompt(timeout=timeout)
            if log_output:
                logger.info('{output}'.format(output=pdata['output'], end=""))
                logger.info('Response: {0}'.format(self.tf))
            if pdata['status'] and not expected_failure:
                raise RuntimeError
            return pdata

        except Exception as e:
            logger.info('send_command() exception: {0}'.format(e))
            raise e


class Command(object):
    """Run a command with timeout, then on timeout kill processes based on abort flag. """
    def __init__(self, command):
        self.command = command
        self.process = None
        self.stdout = None
        self.stderr = None
        self.timedout = False
        self.raise_error = False
        self.error_occurred = False

    def run(self, message=None, logger=None, log_stdout=True, shell=True, timeout=None, abort=True, expected_failure=False):
        """
        This thread executes the command and logs results according to parameters.

        :param message: The message to log for this command, or None.
        :param logger: The logger for logging messages.
        :param log_stdout: Boolean to control logging standard output.
        :param shell: Boolean to control execute via the shell.
        :param timeout: The timeout value.
        :param abort: Boolean to control aborting process on timeout.
        :param expected_failure: Boolean indicating if error is expected.

        :return: Dictionary with process information
        :raises: AttributeError, TimeoutExpiredError, or RuntimeError
        :rtype: dict {'exit_code': value, 'stdout': text, 'stdout': text}
        """
        def target():
            if message:
                logger.info("Message: {0}".format(message))
            logger.info("Command:   {0}".format(repr(self.command)))

            if shell is False:
                command = self.command.split()
            else:
                command = self.command
            # Note: The setpgrp() is required so killpg() kills all processes started (shell).
            self.process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                            shell=shell, preexec_fn=os.setpgrp)
            self.stdout, self.stderr = self.process.communicate()
            # Note: We control stdout logging to avoid huge logs, but always log errors.
            if logger and self.stdout and log_stdout:
                logger.info(self.stdout.decode('utf-8'))
            if self.stderr:
                # Writing to stderr usually indicates an error (but see below).
                self.error_occurred = True
                if expected_failure is False:
                    # Only flag errors when they are unexpected (as requested).
                    # Note: Some tools write to stderr on success! (e.g. sg_ses --verbose)(sigh)
                    # Therefore, we will use the exit status to indicate error conditions.
                    if self.process.returncode:
                        self.raise_error = True
                    if logger:
                        logger.info(self.stderr.decode('utf-8'))

            self.exit_code = self.process.returncode
            if self.process.returncode:
                self.error_occurred = True
                if not expected_failure:
                    if logger:
                        logger.error("Command '{0}' failed with return code {1}".format(self.command, self.process.returncode))
                    if not self.timedout:
                        self.raise_error = True

        try:
            thread = threading.Thread(target=target)
            thread.start()
            thread.join(timeout)
            if self.raise_error:
                raise RuntimeError
            if thread.is_alive():
                self.timedout = True
                if abort is False:
                    logger.error('Process timed out after {0} seconds!'.format(timeout))
                else:
                    # Note: When using the shell, the signal is sent to the shell process!
                    logger.error('Killing process {0}, which timed out after {1} seconds!'.format(self.process.pid, timeout))
                    os.killpg(os.getpgid(self.process.pid), signal.SIGTERM)
                    thread.join()
                    if self.process.returncode is None:
                        logger.error("Process {0} has NOT terminated after being killed!".format(self.process.pid))
                raise TimeoutExpiredError(self.command, timeout)
            else:
                return {'exit_code': self.process.returncode,
                        'stdout': self.stdout.decode('utf-8'),
                        'stderr': self.stderr.decode('utf-8')}

        except RuntimeError as exc:
            raise exc


class TimeoutExpiredError(Exception):
    """This exception is raised when the timeout expires while waiting for a
    child process.
    """
    def __init__(self, cmd, timeout, output=None):
        self.cmd = cmd
        self.timeout = timeout
        self.output = output

    def __str__(self):
        return "Command '%s' timed out after %s seconds" % (self.cmd, self.timeout)


if __name__ == "__main__":
    showdisks = ShowDisks()
    showdisks.main()
