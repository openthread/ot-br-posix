#!/usr/bin/python

import sys
import subprocess
import os

format_str = 'mroute from {if_from} source {ula_prefix} group {group} to {if_to}'

ula_prefix = subprocess.check_output(['uci', 'get', 'network.globals.ula_prefix']).decode('utf-8').strip('\n')

group='{}/{}'.format(sys.argv[1], sys.argv[2])

to_thread = format_str.format(
	if_from='br-lan',
	ula_prefix=ula_prefix,
	group=group,
	if_to='wpan0'
	)

from_thread = format_str.format(
	if_from='wpan0 ',
	ula_prefix=ula_prefix,
	group=group,
	if_to='br-lan'
	)

with open('/etc/smcroute.conf', 'a') as file:
	file.write('\n' + to_thread + '\n')
	file.write(from_thread + '\n')
