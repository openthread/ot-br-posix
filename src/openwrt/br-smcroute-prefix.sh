#!/bin/sh

# Get the randomly-generated ULA prefix from uci
ula_prefix=$(uci get network.globals.ula_prefix)

# Plop in the prefix into the configuration scripts
sed -i s!%CASCODA_ULA_PREFIX%!$ula_prefix!g /etc/smcroute.conf 

