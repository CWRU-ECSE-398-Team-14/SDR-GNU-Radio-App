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

# ==== GUI ====
# stop the service if it's running
sudo systemctl stop sdrgnuradio.service

# move executable to a sensible location
sudo cp $SOURCE_DIR/build/sdr_gnu_radio_app /usr/local/bin

# move systemd service file and start script to correct locations
sudo cp $SOURCE_DIR/tools/start_sdr_gnu_radio.sh /usr/local/bin
sudo chown sdr /usr/local/bin/start_sdr_gnu_radio.sh
sudo chmod 755 /usr/local/bin/start_sdr_gnu_radio.sh
sudo cp $SOURCE_DIR/tools/sdrgnuradio.service /lib/systemd/system


# ==== LOGGER ====

# move systemd service file and script to sensible locations
sudo cp $SOURCE_DIR/tools/logger.py /usr/local/bin
sudo chown sdr /usr/local/bin/logger.py
sudo chmod 755 /usr/local/bin/logger.py
sudo cp $SOURCE_DIR/tools/sdrlogger.service /lib/systemd/system

# create log directory
if [[ ! -d "/var/log/sdr" ]]; then
  sudo mkdir /var/log/sdr
fi

# set necessary permissions
sudo chmod 775 /var/log/sdr

# set correct group of log directory
if [[ $(stat -c '%G' /var/log/sdr) != "$USER" ]]; then
  sudo chgrp $USER /var/log/sdr
fi

# check if log file exists and group is correct
if [[ -f "/var/log/sdr/sdr.log" ]]; then
  if [[ $(stat -c '%G' /var/log/sdr/sdr.log) != "$USER" ]]; then
    sudo chgrp $USER /var/log/sdr/sdr.log
  fi
fi


# activate the new service(s)
sudo systemctl daemon-reload
sudo systemctl enable sdrlogger.service
sudo systemctl enable sdrgnuradio.service

echo "Install done. Reboot required."
