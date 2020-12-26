# Intro

This is a driver for the Razer DeathAdder V2 Pro. It supports the HID protocol, and also provides a character device to expose the proprietary configuration protocol to userspace. It works over wireless and wired USB, but not over Bluetooth.

The app reads and outputs the battery charge level. It uses the InfluxDB line protocol format.

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

Install the app.

    sudo cp app/razer_battery.py /usr/local/bin/

Run the app.

    razer_battery.py

# Telegraf integration

Add the following to `/etc/sudoers.d/telegraf`.

    telegraf ALL=(root) NOPASSWD: /usr/local/bin/razer_battery.py

Add the following to `/etc/telegraf/telegraf.d/razer.conf`.

    [[inputs.exec]]
      commands = ["sudo razer_battery.py"]
      timeout = "5s"
      data_format = "influx"

# Hacking

Lint the code.

    clang-format -i --style=Microsoft driver/razer_raw.c
    black app/
