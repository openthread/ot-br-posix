#!/usr/bin/env python3
#
#  Copyright (c) 2024, The OpenThread Authors.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. Neither the name of the copyright holder nor the
#     names of its contributors may be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#

import logging
from dbus.mainloop.glib import DBusGMainLoop

import dbus
import gi.repository.GLib as GLib
import subprocess
import os

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

bus = dbus.SystemBus()
thread_dbus_obj = None
iface = None

def properties_changed_handler(interface_name, changed_properties, invalidated_properties):
    if "Dhcp6PdState" in changed_properties:
        new_state = changed_properties["Dhcp6PdState"]
        logging.info(f"Dhcp6PdState changed to: {new_state}")
        if new_state == "running":
            try:
                subprocess.run(["sudo", "service", "dhcpcd", "restart"], check=True)
                logging.info("Successfully restarted dhcpcd service.")
            except subprocess.CalledProcessError as e:
                logging.error(f"Error restarting dhcpcd service: {e}")
        elif new_state == "stopped":
            if os.path.isfile("/etc/dhcpcd.conf.orig"):
                try:
                    subprocess.run(["sudo", "cp", "/etc/dhcpcd.conf.orig", "/etc/dhcpcd.conf"], check=True)
                    logging.info("Successfully restored default dhcpcd config.")
                except subprocess.CalledProcessError as e:
                    logging.error(f"Error restoring dhcpcd config: {e}")
            else:
                logging.error("Backup configuration file not found. Cannot restore default.")

def connect_to_signal():
    global thread_dbus_obj, thread_properties_dbus_iface
    try:
        thread_dbus_obj = bus.get_object('io.openthread.BorderRouter.wpan0', '/io/openthread/BorderRouter/wpan0')
        thread_properties_dbus_iface = dbus.Interface(thread_dbus_obj, 'org.freedesktop.DBus.Properties')
        thread_dbus_obj.connect_to_signal("PropertiesChanged", properties_changed_handler, dbus_interface=thread_properties_dbus_iface.dbus_interface)
        logging.info("Connected to D-Bus signal.")
    except dbus.DBusException as e:
        logging.error(f"Error connecting to D-Bus: {e}")
        thread_dbus_obj = None
        thread_properties_dbus_iface = None
        

def check_and_reconnect():
    if thread_dbus_obj is None:
        connect_to_signal()

def main():
    dbus_loop = DBusGMainLoop(set_as_default=True)

    connect_to_signal()

    loop = GLib.MainLoop()

    GLib.timeout_add_seconds(5, check_and_reconnect)

    loop.run()

if __name__ == '__main__':
    main()

