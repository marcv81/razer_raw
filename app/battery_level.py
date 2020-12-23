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
    with open(filename, "wb") as f:
        f.write(request)
    time.sleep(0.015)
    with open(filename, "rb") as f:
        response = f.read(90)
    return "%d" % (100.0 * response[9] / 255.0)


if __name__ == "__main__":
    print(battery_level(filename))
