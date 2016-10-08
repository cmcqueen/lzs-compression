#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
General-purpose sliding-window lossless compression
Based on LZ77/78 work
Aiming to be suitable for embedded systems
"""

from __future__ import print_function

from lzs import *

def decompress(in_data):
    # Make the compression coder, with a chosen length coder
    LZCM = LZCMCoder(OffsetCoder1(7,11), LengthCoder1)
#    LZCM = LZCMCoder(OffsetCoder2(12), LengthCoder8)
    decoded = LZCM.decode(in_data)
    out_data = LZCM.decompress(decoded)
    return out_data

def main():
    if 1:
        with open(sys.argv[1], "rb") as in_stream, open(sys.argv[2], "wb") as out_stream:
            in_data = in_stream.read()
            decompressed_data = decompress(in_data)
            out_stream.write(decompressed_data)

if __name__ == '__main__':
    main()
