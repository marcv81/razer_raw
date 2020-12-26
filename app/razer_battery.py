#!/bin/env python3

import glob
import os
import time


def checksum(request):
    value = 0
    for i in range(2, 88):
        value ^= request[i]
    request[88] = value


def query(filename, request):
    fd = os.open(filename, os.O_RDWR)
    try:
        os.write(fd, request)
        time.sleep(0.010)
        return os.read(fd, 90)
    finally:
        os.close(fd)


def battery_level(filename):
    request = [0] * 90
    request[1] = 0x1F
    request[5] = 0x02
    request[6] = 0x07
    request[7] = 0x80
    checksum(request)
    response = query(filename, bytes(request))
    return int(100.0 * response[9] / 255.0)


def serial_number(filename):
    request = [0] * 90
    request[1] = 0x08
    request[5] = 0x16
    request[7] = 0x82
    checksum(request)
    response = query(filename, bytes(request))
    result = []
    i = 8
    while i < 88 and response[i] != 0:
        result.append(chr(response[i]))
        i += 1
    return "".join(result)


if __name__ == "__main__":
    for filename in glob.glob("/dev/razer*"):
        b = battery_level(filename)
        sn = serial_number(filename)
        if b > 0 and len(sn) > 0:
            print("razer,sn=%s battery=%d" % (sn, b))
