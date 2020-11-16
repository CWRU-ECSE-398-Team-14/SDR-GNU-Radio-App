#!/bin/bash

# set environment variables
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
export QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS=/dev/input/event0:rotate=90
export US_COUNTIES_FILE_PATH=/var/lib/sdrapp/us_counties.csv
export LSWIFI_PATH=/var/lib/sdrapp/lswifi.py
export WEB_SCRAPE_PROGRAM_PATH=/var/lib/sdrapp/WebScraping/getSystemTalkGroups.py
export SCRAPE_SYSTEMS_PATH=/var/lib/sdrapp/WebScraping/getCountySystems.py

# run the executable
/usr/local/bin/sdr_gnu_radio_app