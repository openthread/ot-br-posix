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

logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s - %(levelname)s - %(message)s')

bus = dbus.SystemBus()
intended_dhcp6pd_state = None

DHCP_CONFIG_PATH = "/etc/dhcpcd.conf"
DHCP_CONFIG_PD_PATH = "/etc/dhcpcd.conf.pd"
DHCP_CONFIG_NO_PD_PATH = "/etc/dhcpcd.conf.no-pd"


def restart_dhcpcd_service(config_path):
    if not os.path.isfile(config_path):
        logging.error(f"{config_path} not found. Cannot apply configuration.")
        return
    try:
        subprocess.run(["cp", config_path, DHCP_CONFIG_PATH], check=True)
        subprocess.run(["systemctl", "daemon-reload"], check=True)
        subprocess.run(["service", "dhcpcd", "restart"], check=True)
        logging.info(
            f"Successfully restarted dhcpcd service with {config_path}.")
    except subprocess.CalledProcessError as e:
        logging.error(f"Error restarting dhcpcd service: {e}")


def restart_dhcpcd_with_pd_config():
    global intended_dhcp6pd_state
    restart_dhcpcd_service(DHCP_CONFIG_PD_PATH)
    intended_dhcp6pd_state = None


def restart_dhcpcd_with_no_pd_config():
    restart_dhcpcd_service(DHCP_CONFIG_NO_PD_PATH)


def properties_changed_handler(interface_name, changed_properties,
                               invalidated_properties):
    global intended_dhcp6pd_state
    if "Dhcp6PdState" not in changed_properties:
        return
    new_state = changed_properties["Dhcp6PdState"]
    logging.info(f"Dhcp6PdState changed to: {new_state}")
    if new_state == "running" and intended_dhcp6pd_state != "running":
        intended_dhcp6pd_state = "running"
        thread = threading.Thread(target=restart_dhcpcd_with_pd_config)
        thread.start()
    elif new_state in ("stopped", "idle",
                       "disabled") and intended_dhcp6pd_state is None:
        restart_dhcpcd_with_no_pd_config()


def connect_to_signal():
    try:
        dbus_obj = bus.get_object('io.openthread.BorderRouter.wpan0',
                                  '/io/openthread/BorderRouter/wpan0')
        properties_dbus_iface = dbus.Interface(
            dbus_obj, 'org.freedesktop.DBus.Properties')
        dbus_obj.connect_to_signal(
            "PropertiesChanged",
            properties_changed_handler,
            dbus_interface=properties_dbus_iface.dbus_interface)
        logging.info("Connected to D-Bus signal.")
    except dbus.DBusException as e:
        logging.error(f"Error connecting to D-Bus: {e}")


def handle_name_owner_changed(new_owner):
    if new_owner:
        logging.info(f"New D-Bus owner({new_owner}) assigned, connecting...")
        connect_to_signal()


def main():
    # Ensure dhcpcd is running in its last known state. This addresses a potential race condition
    # during system startup due to the loop dependency in dhcpcd-radvd-network.target.
    #
    #   - network.target activation relies on the completion of dhcpcd start
    #   - during bootup, dhcpcd tries to start radvd with PD enabled before network.target is
    #     active, which leads to a timeout failure
    #   - so we will prevent radvd from starting before target.network is active
    #
    # By restarting dhcpcd here, we ensure it runs after network.target is active, allowing
    # radvd to start correctly and dhcpcd to configure the interface.
    try:
        subprocess.run(["systemctl", "reload-or-restart", "dhcpcd"], check=True)
        logging.info("Successfully restarting dhcpcd service.")
    except subprocess.CalledProcessError as e:
        logging.error(f"Error restarting dhcpcd service: {e}")

    loop = GLib.MainLoop()

    bus.watch_name_owner(bus_name='io.openthread.BorderRouter.wpan0',
                         callback=handle_name_owner_changed)

    loop.run()


if __name__ == '__main__':
    main()
