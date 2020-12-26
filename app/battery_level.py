import os
import time

filename = "/dev/razer0"


def battery_level(filename):
    request = [0] * 90
    request[1] = 0x1F
    request[5] = 0x02
    request[6] = 0x07
    request[7] = 0x80
    request[88] = 0x85
    request = bytes(request)
    fd = os.open(filename, os.O_RDWR)
    try:
        os.write(fd, request)
        time.sleep(0.015)
        response = os.read(fd, 90)
    finally:
        os.close(fd)
    return "%d" % (100.0 * response[9] / 255.0)


if __name__ == "__main__":
    print(battery_level(filename))
