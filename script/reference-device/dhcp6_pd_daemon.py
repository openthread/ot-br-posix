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
import dbus
import gi.repository.GLib as GLib
import subprocess
import threading
import os

from dbus.mainloop.glib import DBusGMainLoop
DBusGMainLoop(set_as_default=True)

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

bus = dbus.SystemBus()
intended_dhcp6pd_state = None

DHCP_CONFIG_FILE = "/etc/dhcpcd.conf"
DHCP_CONFIG_WITH_PD_FILE = "/etc/dhcpcd.conf.with-pd"
DHCP_CONFIG_NO_PD_FILE = "/etc/dhcpcd.conf.no-pd"

def restart_dhcpcd_with_pd_config():
    global intended_dhcp6pd_state
    if os.path.isfile(DHCP_CONFIG_WITH_PD_FILE):
        try:
            subprocess.run(["sudo", "cp", DHCP_CONFIG_WITH_PD_FILE, DHCP_CONFIG_FILE], check=True)
            subprocess.run(["sudo", "systemctl", "daemon-reload"], check=True)
            subprocess.run(["sudo", "service", "dhcpcd", "restart"], check=True)
            logging.info("Successfully restarted dhcpcd service with pd enabled.")
            intended_dhcp6pd_state = None
        except subprocess.CalledProcessError as e:
            logging.error(f"Error restarting dhcpcd service: {e}")
            intended_dhcp6pd_state = None
    else:
        logging.error(f"{DHCP_CONFIG_WITH_PD_FILE} not found. Cannot apply configuration with pd enabled.")
        intended_dhcp6pd_state = None

def restore_default_config():
    if os.path.isfile(DHCP_CONFIG_NO_PD_FILE):
        try:
            subprocess.run(["sudo", "cp", DHCP_CONFIG_NO_PD_FILE, DHCP_CONFIG_FILE], check=True)
            subprocess.run(["sudo", "systemctl", "daemon-reload"], check=True)
            subprocess.run(["sudo", "service", "dhcpcd", "restart"], check=True)
            logging.info("Successfully restarted dhcpcd service with pd disabled.")
        except subprocess.CalledProcessError as e:
            logging.error(f"Error restarting dhcpcd service: {e}")
    else:
        logging.error(f"{DHCP_CONFIG_NO_PD_FILE} not found. Cannot apply configuration with pd disabled.")

def properties_changed_handler(interface_name, changed_properties, invalidated_properties):
    global intended_dhcp6pd_state
    if "Dhcp6PdState" in changed_properties:
        new_state = changed_properties["Dhcp6PdState"]
        logging.info(f"Dhcp6PdState changed to: {new_state}")
        if new_state == "running" and intended_dhcp6pd_state != "running":
            intended_dhcp6pd_state = "running"
            thread = threading.Thread(target=restart_dhcpcd_with_pd_config)
            thread.start()
        elif new_state in ("stopped", "disabled") and intended_dhcp6pd_state is None:
            restore_default_config()

def connect_to_signal():
    try:
        dbus_obj = bus.get_object('io.openthread.BorderRouter.wpan0', '/io/openthread/BorderRouter/wpan0')
        properties_dbus_iface = dbus.Interface(dbus_obj, 'org.freedesktop.DBus.Properties')
        dbus_obj.connect_to_signal("PropertiesChanged", properties_changed_handler, dbus_interface=properties_dbus_iface.dbus_interface)
        logging.info("Connected to D-Bus signal.")
        return dbus_obj, properties_dbus_iface
    except dbus.DBusException as e:
        logging.error(f"Error connecting to D-Bus: {e}")
        return None, None

def check_and_reconnect(dbus_obj, properties_dbus_iface):
    if dbus_obj is None:
        dbus_obj, properties_dbus_iface = connect_to_signal()

def main():
    # Ensure dhcpcd is running in its last known state. In case it was killed or crashed previously, or
    # radvd is not yet enabled because of inactive network.target
    try:
        subprocess.run(["sudo", "systemctl", "reload-or-restart", "dhcpcd"], check=True)
        logging.info("Successfully restarting dhcpcd service.")
    except subprocess.CalledProcessError as e:
        logging.error(f"Error restarting dhcpcd service: {e}")

    loop = GLib.MainLoop()

    thread_dbus_obj, thread_properties_dbus_iface = connect_to_signal()

    GLib.timeout_add_seconds(5, check_and_reconnect, thread_dbus_obj, thread_properties_dbus_iface) 

    loop.run()

if __name__ == '__main__':
    main()

