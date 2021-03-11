# Intro

This is a driver for the Razer DeathAdder V2 Pro. It supports the HID protocol, and also provides a character device to expose the proprietary configuration protocol to userspace. It works over wireless and wired USB, but not over Bluetooth.

The app reads and outputs the battery charge level. It uses the InfluxDB line protocol format.

# Usage

## Driver

### Manual installation

Build the driver.

    make -C /lib/modules/$(uname -r)/build M=$(pwd)/driver modules

Install the driver.

    sudo cp driver/razer_raw.ko /lib/modules/$(uname -r)/kernel/drivers/hid/

### DKMS installation

    sudo cp -R driver/ /usr/src/razer_raw-1.0
    sudo dkms add -m razer_raw -v 1.0
    sudo dkms build -m razer_raw -v 1.0
    sudo dkms install -m razer_raw -v 1.0

### Configuration

Configure udev.

    sudo cp conf/50-razer-raw.rules /etc/udev/rules.d/
    sudo udevadm control --reload-rules
    sudo udevadm trigger

## App

Build the app.

    cd app/
    cargo build --release

Install the app.

    sudo cp target/release/razer-battery /usr/local/bin/

# Telegraf integration

Add the following to `/etc/sudoers.d/telegraf`.

    telegraf ALL=(root) NOPASSWD: /usr/local/bin/razer-battery

Add the following to `/etc/telegraf/telegraf.d/razer.conf`.

    [[inputs.exec]]
      commands = ["sudo razer-battery"]
      timeout = "5s"
      data_format = "influx"

Restart Telegraf.

    sudo systemctl restart telegraf

# Hacking

Lint the code.

    clang-format -i --style=Microsoft driver/razer_raw.c
