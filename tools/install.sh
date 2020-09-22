#!/bin/bash

## install the SDR GNU radio application

# add user to necessary groups
if ! groups $USER | grep -qw "video"; then
  echo "Adding $USER to video group"
  sudo usermod -aG video $USER
fi

if ! groups $USER | grep -qw "input"; then
  echo "Adding $USER to input group"
  sudo usermod -aG input $USER
fi

# move fonts over
# echo "Switching fonts directory to $FONTS_DIR."
FONTS_DIR=/usr/share/fonts/truetype

SOURCE_DIR=$(readlink -f "..")

# move executable to a sensible location
sudo cp $SOURCE_DIR/build/sdr_gnu_radio_app /usr/local/bin

# move systemd service file and start script to correct locations
sudo cp $SOURCE_DIR/tools/start_sdr_gnu_radio.sh /usr/local/bin
sudo chown sdr /usr/local/bin/start_sdr_gnu_radio.sh
sudo chmod 755 /usr/local/bin/start_sdr_gnu_radio.sh

sudo cp $SOURCE_DIR/tools/sdrgnuradio.service /lib/systemd/system

# activate the new service
sudo systemctl daemon-reload
sudo systemctl enable sdrgnuradio.service

echo "Install done. Reboot required."
