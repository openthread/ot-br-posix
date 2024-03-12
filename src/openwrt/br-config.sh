#!/bin/sh

# Exit if the Thread interface already exists
uci get network.thread.ifname && \
	logger -t br-config Thread network interface and firewall already configured! && \
	exit 0  

logger -t br-config Configuring Thread network interface...

uci set network.thread=interface
uci set network.thread.ifname='wpan0'
uci set network.thread.proto='static'

uci show firewall | grep thread && \
	logger -t br-config Thread firewall rules already configured! && \
	exit 0

logger -t br-config Configuring Thread firewall rules...

uci add firewall zone > /dev/null
uci set firewall.@zone[-1].name='thread'
uci add_list firewall.@zone[-1].network='thread'
uci set firewall.@zone[-1].input='ACCEPT'
uci set firewall.@zone[-1].output='ACCEPT'
uci set firewall.@zone[-1].forward='ACCEPT'

uci add firewall forwarding > /dev/null
uci set firewall.@forwarding[-1].src='thread'
uci set firewall.@forwarding[-1].dest='lan'

uci add firewall forwarding > /dev/null
uci set firewall.@forwarding[-1].src='lan'
uci set firewall.@forwarding[-1].dest='thread'

uci commit
reload_config

# given that this is the first-time config, make u-boot boot silent & faster
fw_setenv silent 1
fw_setenv bootdelay 5

# fix for software update while UART is disabled
fw_setenv stdout serial
fw_setenv stderr serial

# fix bootloader - holding down reset button should now work
fw_setenv ipaddr 192.168.1.1
fw_setenv recovery httpd

# preserve OpenThread details across software upgrades
grep -qF cascoda /etc/sysupgrade.conf || echo "/.local/share/cascoda/" >> /etc/sysupgrade.conf

# preserve IoT Router application program across software upgrades
grep -qF kir_new /etc/sysupgrade.conf || echo "/usr/bin/knx_iot_router/kir_new/" >> /etc/sysupgrade.conf && echo "/kir_new/" >> /etc/sysupgrade.conf
