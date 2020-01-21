cp 53-snopf_boot.rules /etc/udev/rules.d
udevadm control --reload-rules
udevadm trigger
