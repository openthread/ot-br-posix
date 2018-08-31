#!/bin/bash

export LD_LIBRARY_PATH=/usr/local/lib:LD_LIBRARY_PATH

function parse_args()
{
    while [ $# -gt 0 ]
    do
        echo $1
        case $1 in
        --ncp_path)
            NCP_PATH=$2
            shift
            shift
            ;;
        --interface|-I)
            TUN_INTERFACE_NAME=$2
            shift
            shift
            ;;
        --nat64_prefix)
            NAT64_PREFIX=$2
            shift
            shift
            ;;
        --disable_default_prefix_route)
            AUTO_PREFIX_ROUTE=false
            shift
            ;;
        --disable_default_prefix_slaac)
            AUTO_PREFIX_SLAAC=false
            shift
            ;;
        *)
            shift
            ;;
        esac
    done
}

parse_args "$@"

[ -n "$NCP_PATH" ] || NCP_PATH="/dev/ttyUSB0"
[ -n "$TUN_INTERFACE_NAME" ] || TUN_INTERFACE_NAME="wpan0"
[ -n "$AUTO_PREFIX_ROUTE" ] || AUTO_PREFIX_ROUTE=true
[ -n "$AUTO_PREFIX_SLAAC" ] || AUTO_PREFIX_SLAAC=true
[ -n "$NAT64_PREFIX" ] || NAT64_PREFIX="64:ff9b::/96"

echo "NCP_PATH:" $NCP_PATH
echo "TUN_INTERFACE_NAME:" $TUN_INTERFACE_NAME
echo "NAT64_PREFIX:" $NAT64_PREFIX
echo "AUTO_PREFIX_ROUTE:" $AUTO_PREFIX_ROUTE
echo "AUTO_PREFIX_SLAAC:" $AUTO_PREFIX_SLAAC

NAT64_PREFIX=${NAT64_PREFIX/\//\\\/}

sed -i "s/^prefix.*$/prefix $NAT64_PREFIX/" /etc/tayga.conf
sed -i "s/dns64/dns64 $NAT64_PREFIX/" /etc/bind/named.conf.options

service dbus start
service bind9 start
service tayga start 
wpantund \
    -o Config:NCP:SocketPath "$NCP_PATH" \
    -o Config:TUN:InterfaceName $TUN_INTERFACE_NAME \
    -o Daemon:SetDefaultRouteForAutoAddedPrefix $AUTO_PREFIX_ROUTE \
    -o IPv6:SetSLAACForAutoAddedPrefix $AUTO_PREFIX_SLAAC &

sleep 5

/usr/local/sbin/otbr-agent -I $TUN_INTERFACE_NAME &

/usr/local/sbin/otbr-web -I $TUN_INTERFACE_NAME -p 80 &

wait
