# tools/
Directory containing tools intended for setting up the application on a Raspberry Pi.

## `install.sh`
Sets certain permissions and copies/moves various scripts and files to appropriate locations in the filesystem. Will configure systemd service scripts to run as the user that runs `install.sh`.
Although this script does many things, see the Environment Variables section to see what you still need to configure.

## `configure`
Configures the cmake build environment. Assumes that Qt is installed in `/usr/local/Qt-5.15.0`. Most other assumptions are based on the Raspberry Pi OS. 

## `build.sh`
Builds the gui application. Run `install.sh` after building.

## `clean.sh`
Cleans the build directory and creates empty files necessary for the build step to not fail.

# Running the application
If the install script ran successfully, the application will start automatically on boot-up. For running the program manually for debugging or development, the following details will be useful.
## Environment Variables
Several environment variables need to be set for the program to run correctly. They are as follows:
- `QT_QPA_PLATFORM`
- `QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS`
- `US_COUNTIES_FILE_PATH`
- `LSWIFI_PATH`
- `WEB_SCRAPE_PROGRAM_PATH`
- `SCRAPE_SYSTEMS_PATH`

You should change these values as needed in the `start_sdr_gnu_radio.sh` script then run `install.sh`, which copies that script to the necessary location for systemd to find it.

### `QT_QPA_PLATFORM`
This specifies the plugin for Qt to use. We chose LinuxFB, which allows us to write directly to the frame buffer of our touchscreen. Our framebuffer device was `/dev/fb0`, so we set this environment variable to `linuxfb:fb=/dev/fb0`.
### `QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS`
This specifies the name of the input device, which is our touchscreen. On our system, our touchscreen input was `/dev/input/event0` - yours may be different. Look in your `/dev/input` directory and see what options you have. We found we also had to rotate our coordinates by 90 degrees. So our final value for this environment variable was `/dev/input/event0:rotate=90`. You may also have to invert your coordinate system (we did not). To do so, you would add `:invertx` or `:inverty` to the value.
### `US_COUNTIES_FILE_PATH`
Install script takes care of this. 
### `LSWIFI_PATH`
Install script takes care of this. 
### `WEB_SCRAPE_PROGRAM_PATH`
Install script takes care of this. 
### `SCRAPE_SYSTEMS_PATH`
Install script takes care of this. 