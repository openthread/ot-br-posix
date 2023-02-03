#!/bin/sh

# Get the randomly-generated ULA prefix from uci
ula_prefix=$(uci get network.globals.ula_prefix)

# Assuming the prefix is a /48, create the Thread ...:1::/64 prefix
thread_prefix=$(echo $ula_prefix | sed s!::/48!:1::/64!)

# Add the prefix using ot-ctl - the border routing code will see this
# when it runs and pick this prefix as the Off-Mesh Prefix

logger -t br-omr-enable Adding $thread_prefix for Off-Mesh Routing...

ot-ctl prefix add $thread_prefix paos high

logger -t br-omr-enable Prefix $thread_prefix added!

# Do not route link-local packets to the Thread network - they cannot
# be responded to by Thread devices
ip -6 route del fe80::/64 dev wpan0

