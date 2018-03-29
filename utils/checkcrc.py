#!/bin/python

import sys

if __name__ == "__main__":
    bytes_string = sys.argv[1]
    bytes = [int(b,16) for b in bytes_string.split(" ")]
    checksum = hex(sum(bytes[0:-2]) & 0xff)
    print("Checksum: {}".format(checksum))
