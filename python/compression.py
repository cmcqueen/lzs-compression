# -*- coding: utf-8 -*-
"""
General-purpose sliding-window lossless compression
Based on LZ77/78 work
Aiming to be suitable for embedded systems
"""


class BitFieldQueue(object):
    __slots__ = [ 'value', 'width' ]
    MAX_WIDTH = 32

    def __init__(self, value, width):
        safe_value = (value & ((1 << width) - 1))
        if safe_value != value:
            raise Exception("value is wider than given width")
        if width > BitFieldQueue.MAX_WIDTH:
            raise Exception("width is too big")
        self.value = value
        self.width = width
    def append(self, new_item):
        self.value = (self.value << new_item.width) | (new_item.value & ((1 << new_item.width) - 1))
        self.width += new_item.width
        if self.width > BitFieldQueue.MAX_WIDTH:
            raise Exception("new width is too big")
    def get(self, width):
        if width > self.width:
            raise Exception("requested get width is not available")
        return (self.value >> (self.width - width)) & ((1 << width) - 1)
    def pop(self, width):
        if width > self.width:
            raise Exception("requested pop width is not available")
        self.width -= width
        return_value = (self.value >> self.width) & ((1 << width) - 1)
        self.value &= ((1 << self.width) - 1)
        return return_value
    def __str__(self):
        return "{0}, width {1}".format(self.value, self.width)

class LZCM(object):
    def __init__(self):
        pass

    def compress(self, in_data):
        pass



test = BitFieldQueue(2, 2)
test.append(BitFieldQueue(2,2))
test.append(BitFieldQueue(2,2))
test.append(BitFieldQueue(2,2))
test.append(BitFieldQueue(2,2))
test.append(BitFieldQueue(2,2))
test.append(BitFieldQueue(2,2))
print(test)
print(test.pop(4))
print(test.pop(4))
print(test.pop(4))
print(test)
