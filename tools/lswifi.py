#!/usr/bin/env python3

import os

stream = os.popen("sudo iwlist wlan0 scan") # will work without password if install succeeded in modifying the sudoers file

scan = stream.read()

ssid_list = []

for line in scan:
    if 'ESSID' in line:
        ssid_list.append(line.strip().split(':')[1])

print(','.join(ssid_list))