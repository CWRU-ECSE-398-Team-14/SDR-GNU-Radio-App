#!/bin/bash

# set environment variables
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
export QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS=/dev/input/event0:rotate=90

# run the executable
/usr/local/bin/sdr_gnu_radio_app