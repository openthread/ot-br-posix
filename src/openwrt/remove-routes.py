#!/usr/bin/python

import sys

filtered_lines = []

with open('/etc/smcroute.conf', 'r') as file:
	for line in file:
		if 'mroute' in line and sys.argv[1] in line:
			print("Removing ' " + line.strip('\n') + "'...")
		else:
			filtered_lines.append(line)


with open('/etc/smcroute.conf', 'w') as file:
	for line in filtered_lines:
		file.write(line)

