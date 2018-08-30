#!/bin/bash

export LD_LIBRARY_PATH=/usr/local/lib:LD_LIBRARY_PATH

source /otbr/otbr_config
[ -n "$NCP_PATH" ] || NCP_PATH="/dev/ttyUSB0"
[ -n "$TUN_INTERFACE_NAME" ] || TUN_INTERFACE_NAME="wpan0"
[ -n "$AUTO_PREFIX_DEFAULT_ROUTE" ] || AUTO_PREFIX_DEFAULT_ROUTE=true
[ -n "$AUTO_PREFIX_SLAAC" ] || AUTO_PREFIX_SLACC=true

service dbus start
service tayga start 
wpantund \
    -o Config:NCP:SocketPath "$NCP_PATH" \
    -o Config:TUN:InterfaceName $TUN_INTERFACE_NAME \
    -o Daemon:SetDefaultRouteForAutoAddedPrefix $AUTO_PREFIX_DEFAULT_ROUTE \
    -o IPv6:SetSLAACForAutoAddedPrefix $AUTO_PREFIX_SLACC &

echo "wpantund done"

sleep 5

echo "start otbr agent"

/usr/local/sbin/otbr-agent -I $TUN_INTERFACE_NAME &

/usr/local/sbin/otbr-web -I $TUN_INTERFACE_NAME -p 80 &

wait
