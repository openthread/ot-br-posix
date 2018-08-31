#!/bin/bash

export LD_LIBRARY_PATH=/usr/local/lib:LD_LIBRARY_PATH

source /otbr/otbr_config
[ -n "$NCP_PATH" ] || NCP_PATH="/dev/ttyUSB0"
[ -n "$TUN_INTERFACE_NAME" ] || TUN_INTERFACE_NAME="wpan0"
[ -n "$AUTO_PREFIX_DEFAULT_ROUTE" ] || AUTO_PREFIX_DEFAULT_ROUTE=true
[ -n "$AUTO_PREFIX_SLAAC" ] || AUTO_PREFIX_SLACC=true
[ -n "$NAT64_PREFIX" ] || NAT64_PREFIX="64:ff9b::/96"

NAT64_PREFIX=${NAT64_PREFIX/\//\\\/}

sed -i "s/^prefix.*$/prefix $NAT64_PREFIX/" /etc/tayga.conf
sed -i "s/dns64/dns64 $NAT64_PREFIX/" /etc/bind/named.conf.options

service dbus start
service bind9 start
service tayga start 
wpantund \
    -o Config:NCP:SocketPath "$NCP_PATH" \
    -o Config:TUN:InterfaceName $TUN_INTERFACE_NAME \
    -o Daemon:SetDefaultRouteForAutoAddedPrefix $AUTO_PREFIX_DEFAULT_ROUTE \
    -o IPv6:SetSLAACForAutoAddedPrefix $AUTO_PREFIX_SLACC &

sleep 5

/usr/local/sbin/otbr-agent -I $TUN_INTERFACE_NAME &

/usr/local/sbin/otbr-web -I $TUN_INTERFACE_NAME -p 80 &

wait
