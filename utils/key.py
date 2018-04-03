#!/bin/python

import sys

if __name__ == "__main__":
    keys_args = sys.argv[1:]
    keys = [int(k) for k in keys_args]
    for k in keys:
        checksum = (((k >> 4) & 0x03) + ((k >> 2) & 0x03) + (k & 0x03)) & 0x03
        write_val = ((k << 2) | checksum) & 0xff
        print("Key: {:x} [{:x}]".format(write_val,checksum))
