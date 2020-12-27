#!/bin/env python3

import glob
import os
import time


def checksum(message):
    result = 0
    for i in range(2, 88):
        result ^= message[i]
    return result


def query(filename, request):
    fd = os.open(filename, os.O_RDWR)
    try:
        os.write(fd, bytes(request))
        time.sleep(0.005)
        return os.read(fd, 90)
    finally:
        os.close(fd)


def safe_query(filename, request):
    response = query(filename, request)
    assert response[0] == 0x02
    assert response[88] == checksum(response)
    for i in range(1, 8):
        assert request[i] == response[i]
    return response


def battery_level(filename):
    request = [0] * 90
    request[1] = 0x1F
    request[5] = 0x02
    request[6] = 0x07
    request[7] = 0x80
    request[88] = checksum(request)
    response = safe_query(filename, request)
    return int(100.0 * response[9] / 255.0)


def serial_number(filename):
    request = [0] * 90
    request[1] = 0x08
    request[5] = 0x16
    request[7] = 0x82
    request[88] = checksum(request)
    response = safe_query(filename, request)
    result = []
    i = 8
    while i < 88 and response[i] != 0:
        result.append(chr(response[i]))
        i += 1
    return "".join(result)


if __name__ == "__main__":
    for filename in glob.glob("/dev/razer*"):
        try:
            b = battery_level(filename)
            sn = serial_number(filename)
            print("razer,sn=%s battery=%d" % (sn, b))
        except AssertionError:
            pass
