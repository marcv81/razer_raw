# Intro

This is a driver for the Razer DeathAdder V2 Pro. It supports the HID protocol, and also provides a character device to expose the proprietary configuration protocol to userspace. It works over wireless and wired USB, but not over Bluetooth.

We provide a minimal sample app to read the battery level.

# Usage

Build the driver.

    make -C /lib/modules/$(uname -r)/build M=$(pwd)/driver modules

Install the driver.

    sudo cp driver/razer_raw.ko /lib/modules/$(uname -r)/kernel/drivers/hid/
    sudo depmod

Configure udev.

    sudo cp conf/50-razer-raw.rules /etc/udev/rules.d/
    sudo udevadm control --reload-rules
    sudo udevadm trigger

Run the app.

    python3 app/battery_level.py

# Hacking

Lint the code.

    clang-format -i --style=Microsoft driver/razer_raw.c
    black app/
