# snopf bootloader

This is a simple USB bootloader for an ATtiny85 for the snopf password token project.

For a schematic etc. see the snopf main project.

To flash the AVR run
```
$ make program
```
in the `avr` subfolder.

To write a new program to the device you should first set up the python venv by running
```
$ ./setup_py_env.sh
```
To program the device without root privileges you need to copy `53-snopf-boot.rules` to `/etc/udev/rules.d`, you can also do that (with root privileges) by running
```
$ ./install_usb_rule.sh
```

To initalize the bootloader and prevent a jump to the main application you have to press the button on the snopf device **when plugging in** and keep pressing the button for at least 10 seconds. In any other case the device will jump to the main application instead of the bootloader.

After that you can send a new application to the device by running
```
$ python3 update_firmwary.py hexfile
```
in the `host` folder. `hexfile` is an intel hexfile as it's created by running `make` in the snopf avr directory.

## License
All code and schematics are licensed under GNU General Public License (GPL) Version 2, see file License.txt.
