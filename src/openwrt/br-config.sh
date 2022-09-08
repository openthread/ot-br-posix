#!/bin/sh

# Exit if the Thread interface already exists
uci get network.thread.ifname && \
	logger -t br-config Thread network interface and firewall already configured! && \
	exit 0  

logger -t br-config Configuring Thread network interface and firewall rules...

uci set network.thread=interface
uci set network.thread.ifname='wpan0'
uci set network.thread.proto='static'

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

