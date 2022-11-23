#!/usr/bin/python

import sys

with open('/etc/smcroute.conf', 'r') as file:
	for line in file:
		if 'mroute' in line:
			print(line.strip('\n'))
