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

# generate the systemd service file
echo "[Unit]
Description=the SDR GNU Radio GUI application
After=multi-user.target


[Service]
Type=simple
Restart=always
User=$USER
Group=$USER
ExecStart=/usr/local/bin/start_sdr_gnu_radio.sh


[Install]
WantedBy=multi-user.target" > $SOURCE_DIR/tools/sdrgnuradio.service

# copy start script to correct locations
sudo cp $SOURCE_DIR/tools/start_sdr_gnu_radio.sh /usr/local/bin
sudo chown $USER /usr/local/bin/start_sdr_gnu_radio.sh
sudo chmod 755 /usr/local/bin/start_sdr_gnu_radio.sh

# move systemd service file
sudo mv $SOURCE_DIR/tools/sdrgnuradio.service /lib/systemd/system


# ==== LOGGER ====
# stop the service if it's running
sudo systemctl stop sdrlogger.service

# generate the systemd service file
echo "[Unit]
Description=Logger for the SDR GNU radio application
After=multi-user.target


[Service]
Type=simple
Restart=always
User=$USER
Group=$USER
ExecStart=/usr/local/bin/logger.py


[Install]
WantedBy=multi-user.target" > $SOURCE_DIR/tools/sdrlogger.service

# copy logger script to sensible locations
sudo cp $SOURCE_DIR/tools/logger.py /usr/local/bin
sudo chown $USER /usr/local/bin/logger.py
sudo chmod 755 /usr/local/bin/logger.py

# move systemd service file
sudo mv $SOURCE_DIR/tools/sdrlogger.service /lib/systemd/system

# create log directory
if [[ ! -d "/var/log/sdr" ]]; then
  sudo mkdir /var/log/sdr
fi

# set correct group of log directory
if [[ $(stat -c '%G' /var/log/sdr) != "$USER" ]]; then
  sudo chgrp $USER /var/log/sdr
fi

# set necessary permissions
sudo chmod 775 /var/log/sdr

# check if log file exists and group is correct
if [[ -f "/var/log/sdr/sdr.log" ]]; then
  if [[ $(stat -c '%G' /var/log/sdr/sdr.log) != "$USER" ]]; then
    sudo chgrp $USER /var/log/sdr/sdr.log
  fi
fi


# activate the new service(s)
sudo systemctl daemon-reload
sudo systemctl enable sdrgnuradio.service
sudo systemctl enable sdrlogger.service

# ==== LSWIFI ====
# edit sudoers file to allow no password when running iwlist wlan0 scan
SUDOERS_FILE=$(sudo cat /etc/sudoers)
if ! echo "$SUDOERS_FILE" | grep -qw "iwlist"; then
  echo "$USER ALL=(root) NOPASSWD: /sbin/iwlist wlan0 scan" | sudo EDITOR='tee -a' visudo
fi


## create app data directory
SHARED_DATA_DIR=/var/lib/sdrapp
if [[ ! -d $SHARED_DATA_DIR ]]; then
  sudo mkdir $SHARED_DATA_DIR
fi
sudo chown $USER $SHARED_DATA_DIR
sudo chgrp $USER $SHARED_DATA_DIR
sudo chmod 775 $SHARED_DATA_DIR

# copy over data files
cp $SOURCE_DIR/tools/us_counties.csv $SHARED_DATA_DIR

# copy lswifi script to expected location
sudo cp $SOURCE_DIR/tools/lswifi.py $SHARED_DATA_DIR
sudo chown $USER $SHARED_DATA_DIR/lswifi.py
sudo chmod 755 $SHARED_DATA_DIR/lswifi.py

# ==== WebScraping ====
# make sure web scraping stuff is there
if [[ ! -d "$SHARED_DATA_DIR/WebScraping" ]]; then
  mkdir $SHARED_DATA_DIR/WebScraping
  git clone https://github.com/CWRU-ECSE-398-Team-14/WebScraping.git $SHARED_DATA_DIR/WebScraping
  python3 -m pip install $SHARED_DATA_DIR/WebScraping/requirements.txt
fi

echo "Install done. Reboot required."
