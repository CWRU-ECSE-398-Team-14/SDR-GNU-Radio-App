# tools
Intended for setting up the application on a Raspberry Pi.

## install.sh
Sets certain permissions and copies/moves various scripts and files to appropriate locations in the filesystem. Will configure systemd service scripts to run as the user that runs `install.sh`.

## configure
Configures the cmake build environment. Assumes that Qt is installed in `/usr/local/Qt-5.15.0`. Most other assumptions are based on the Raspberry Pi OS. 

## build.sh
Builds the gui application. Run `install.sh` after building.