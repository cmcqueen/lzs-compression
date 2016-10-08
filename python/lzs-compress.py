#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
General-purpose sliding-window lossless compression
Based on LZ77/78 work
Aiming to be suitable for embedded systems
"""

from __future__ import print_function

from lzs import *

def compress(in_data):
    # Make the compression coder, with a chosen length coder
    LZCM = LZCMCoder(OffsetCoder1(7,11), LengthCoder1)
#    LZCM = LZCMCoder(OffsetCoder2(12), LengthCoder8)
    compressed_data = LZCM.compress(in_data)
    encoded = LZCM.encode(compressed_data)
    return encoded

def main():
    if 1:
        with open(sys.argv[1], "rb") as in_stream, open(sys.argv[2], "wb") as out_stream:
            in_data = in_stream.read()
            compressed_data = compress(in_data)
            out_stream.write(compressed_data)

if __name__ == '__main__':
    main()
