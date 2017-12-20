#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
General-purpose sliding-window lossless compression
Based on LZ77/78 work
Aiming to be suitable for embedded systems

This code is licensed according to the MIT license as follows:
----------------------------------------------------------------------------
Copyright (c) 2017 Craig McQueen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
----------------------------------------------------------------------------
"""

from __future__ import print_function

import sys
from collections import defaultdict


if sys.version_info[0] == 2:
    byteschr = chr
else:
    def byteschr(x):
        return bytes([x])

class BitFieldQueue(object):
    __slots__ = [ 'value', 'width' ]
    MAX_WIDTH = 32

    def __init__(self, value = 0, width = 0):
        safe_value = (value & ((1 << width) - 1))
        if safe_value != value:
#            raise Exception(u"value {0} is wider than given width".format(value))
            raise Exception(u"value is wider than given width")
        if width > BitFieldQueue.MAX_WIDTH:
            raise Exception(u"width is too big")
        self.value = value
        self.width = width
    def append(self, new_item):
        self.value = (self.value << new_item.width) | (new_item.value & ((1 << new_item.width) - 1))
        self.width += new_item.width
        if self.width > BitFieldQueue.MAX_WIDTH:
            raise Exception(u"new width is too big")
    def get(self, width):
        if width > self.width:
            raise Exception(u"requested get width is not available")
        return (self.value >> (self.width - width)) & ((1 << width) - 1)
    def pop(self, width):
        if width > self.width:
            raise Exception(u"requested pop width is not available")
        self.width -= width
        return_value = (self.value >> self.width) & ((1 << width) - 1)
        self.value &= ((1 << self.width) - 1)
        return return_value
    def __str__(self):
        return u"{0}, width {1}".format(self.value, self.width)

class EndMarker(object):
    pass


class CircularBytesBuffer(object):
    """Circular bytearray buffer"""
    def __init__(self, size):
        self.buffer_size = size

        self.buffer = bytearray(self.buffer_size)
        self.num_items = 0
        self.newest = 0
        self.oldest = 0

    def _add_index_wrapped(self, index, count):
        return (index + count) % self.buffer_size

    def append(self, item):
        item_len = len(item)
        if item_len > self.buffer_size:
            raise Exception(u"Cannot handle item bigger than circular buffer")
        # Add to buffer, with wrapping
        # First bit
        space_at_end = self.buffer_size - self.newest
        first_len = min(item_len, space_at_end)
        if first_len:
            self.buffer[self.newest:self.newest+first_len] = item[:first_len]
        # Second bit (wrapped)
        second_len = item_len - first_len
        if second_len:
            self.buffer[:second_len] = item[first_len:]
        # Update counts and indices
        self.newest = self._add_index_wrapped(self.newest, item_len)
        self.num_items += item_len
        if self.num_items > self.buffer_size:
            self.oldest = self.newest
            self.num_items = self.buffer_size

    def pop(self, count = None):
        if not count:
            count = self.num_items
        if count > self.num_items:
            count = self.num_items
        # First bit
        space_at_end = self.buffer_size - self.oldest
        first_len = min(count, space_at_end)
        first_bit = self.buffer[self.oldest:self.oldest+first_len]
        # Second bit (wrapped)
        second_len = count - first_len
        second_bit = self.buffer[:second_len]
        # Update index, count
        self.oldest = self._add_index_wrapped(self.oldest, count)
        self.num_items -= count

        return first_bit + second_bit

    def _normalise_index(self, index):
        if index >= 0:
            if index >= self.num_items:
                raise IndexError(u"CircularBytesBuffer index out of range")
            return self._add_index_wrapped(self.oldest, index)
        else:
            if -index > self.num_items:
                raise IndexError(u"CircularBytesBuffer index out of range")
            return self._add_index_wrapped(self.newest, index)
    def _normalise_slice(self, key):
        (start, end, step) = key.indices(self.num_items)
        if step != 1:
            raise IndexError(u"Cannot slice CircularBytesBuffer with a step value")
        if start > end:
            if end == 0:
                end = self.num_items
            #raise IndexError(u"Start value {} must not be greater than end value {} for len {}".format(start, end, self.num_items))
        count = end - start
        start = self._add_index_wrapped(self.oldest, start)
        end = self._add_index_wrapped(self.oldest, end)
        return (start, end, count)
        
    def __getitem__(self, key):
        if isinstance(key, int):
            return bytes( [ self.buffer[self._normalise_index(key)] ] )
        elif isinstance(key, slice):
            (start, end, count) = self._normalise_slice(key)
            # First bit
            space_at_end = self.buffer_size - start
            first_len = min(count, space_at_end)
            first_bit = self.buffer[start:start+first_len]
            # Second bit (wrapped)
            second_len = count - first_len
            second_bit = self.buffer[:second_len]
            return first_bit + second_bit
            
        else:
            raise TypeError(u"CircularBytesBuffer index must be integer, not %s" % type(key))

class OffsetCoder1(object):
    def __init__(self, short_bits=7, long_bits=11):
        self.short_bits = short_bits
        self.long_bits = long_bits
        self.max_short_offset = (2**self.short_bits) - 1
        self.max_long_offset = (2**self.long_bits) - 1
        self.max_offset = self.max_long_offset
        self.MAX_OFFSET = self.max_long_offset

    def encode(self, offset):
        """
        Encode an offset into a variable-bit-width value.
        Returns a BitFieldQueue that encodes the offset.
        """
        if offset == EndMarker:
            offset = 0
        if offset <= self.max_short_offset:
            # Short offset
            bit_field = BitFieldQueue(1, 1)
            bit_field.append(BitFieldQueue(offset, self.short_bits))
            return bit_field
        elif offset <= self.max_long_offset:
            # Long offset
            bit_field = BitFieldQueue(0, 1)
            bit_field.append(BitFieldQueue(offset, self.long_bits))
            return bit_field
        else:
            raise Exception(u"Offset {0} is too long to encode".format(offset))

    def decode(self, bit_field_queue):
        """
        Reads a variable-bit-width value from a BitFieldQueue.
        Returns a decoded offset. The value is removed from the BitFieldQueue.
        """
        if bit_field_queue.pop(1):
            # Short offset
            offset = bit_field_queue.pop(self.short_bits)
            if offset == 0:
                offset = EndMarker
            return offset
        else:
            # Long offset
            return bit_field_queue.pop(self.long_bits)

class OffsetCoder1b(object):
    def __init__(self, short_bits=7, long_bits=11):
        self.short_bits = short_bits
        self.long_bits = long_bits
        self.max_short_offset = (2**self.short_bits) - 1
        self.max_long_offset = (2**self.long_bits) - 1 + self.max_short_offset
        self.max_offset = self.max_long_offset
        self.MAX_OFFSET = self.max_long_offset

    def encode(self, offset):
        """
        Encode an offset into a variable-bit-width value.
        Returns a BitFieldQueue that encodes the offset.
        """
        if offset == EndMarker:
            offset = 0
        if offset <= self.max_short_offset:
            # Short offset
            bit_field = BitFieldQueue(1, 1)
            bit_field.append(BitFieldQueue(offset, self.short_bits))
            return bit_field
        elif offset <= self.max_long_offset:
            # Long offset
            bit_field = BitFieldQueue(0, 1)
            bit_field.append(BitFieldQueue(offset - self.max_short_offset, self.long_bits))
            return bit_field
        else:
            raise Exception(u"Offset {0} is too long to encode".format(offset))

    def decode(self, bit_field_queue):
        """
        Reads a variable-bit-width value from a BitFieldQueue.
        Returns a decoded offset. The value is removed from the BitFieldQueue.
        """
        if bit_field_queue.pop(1):
            # Short offset
            offset = bit_field_queue.pop(self.short_bits)
            if offset == 0:
                offset = EndMarker
            return offset
        else:
            # Long offset
            return bit_field_queue.pop(self.long_bits) + self.max_short_offset

class OffsetCoder2(object):
    def __init__(self, num_bits=10):
        self.num_bits = num_bits
        self.max_offset = (2**self.num_bits) - 1
        self.MAX_OFFSET = self.max_offset

    def encode(self, offset):
        """
        Encode an offset into a variable-bit-width value.
        Returns a BitFieldQueue that encodes the offset.
        """
        if offset == EndMarker:
            offset = 0
        if offset <= self.max_offset:
            # Long offset
            return BitFieldQueue(offset, self.num_bits)
        else:
            raise Exception(u"Offset {0} is too long to encode".format(offset))

    def decode(self, bit_field_queue):
        """
        Reads a variable-bit-width value from a BitFieldQueue.
        Returns a decoded offset. The value is removed from the BitFieldQueue.
        """
        offset = bit_field_queue.pop(self.num_bits)
        if offset == 0:
            offset = EndMarker
        return offset


class LengthCoder1(object):
    """
    This is the standard LZS coding of length.
    """
    MIN_INITIAL_LEN = 2
    MAX_INITIAL_LEN = 8
    MAX_CONTINUED_LEN = 15

    def __init__(self):
        pass

    @staticmethod
    def encode(length):
        """
        Encode a length into a variable-bit-width value.
        Returns a BitFieldQueue that encodes the length.
        """
        return {
            2: BitFieldQueue(0b00, 2),
            3: BitFieldQueue(0b01, 2),
            4: BitFieldQueue(0b10, 2),
            5: BitFieldQueue(0b1100, 4),
            6: BitFieldQueue(0b1101, 4),
            7: BitFieldQueue(0b1110, 4),
            8: BitFieldQueue(0b1111, 4),
        } [length]
    @staticmethod
    def decode(bit_field_queue):
        """
        Reads a variable-bit-width value from a BitFieldQueue.
        Returns a decoded length. The value is removed from the BitFieldQueue.
        """
        length_temp = bit_field_queue.get(4)
        length, width = {
            0b0000: (2,2),
            0b0001: (2,2),
            0b0010: (2,2),
            0b0011: (2,2),
            0b0100: (3,2),
            0b0101: (3,2),
            0b0110: (3,2),
            0b0111: (3,2),
            0b1000: (4,2),
            0b1001: (4,2),
            0b1010: (4,2),
            0b1011: (4,2),
            0b1100: (5,4),
            0b1101: (6,4),
            0b1110: (7,4),
            0b1111: (8,4),
        }[length_temp]
        bit_field_queue.pop(width)
        return length

class LengthCoder2(object):
    MIN_INITIAL_LEN = 2
    MAX_INITIAL_LEN = 7
    MAX_CONTINUED_LEN = 15

    def __init__(self):
        pass

    @staticmethod
    def encode(length):
        """
        Encode a length into a variable-bit-width value.
        Returns a BitFieldQueue that encodes the length.
        """
        return {
            2: BitFieldQueue(0b0, 1),
            3: BitFieldQueue(0b10, 2),
            4: BitFieldQueue(0b1100, 4),
            5: BitFieldQueue(0b1101, 4),
            6: BitFieldQueue(0b1110, 4),
            7: BitFieldQueue(0b1111, 4),
        } [length]
    @staticmethod
    def decode(bit_field_queue):
        """
        Reads a variable-bit-width value from a BitFieldQueue.
        Returns a decoded length. The value is removed from the BitFieldQueue.
        """
        length_temp = bit_field_queue.get(4)
        length, width = {
            0b0000: (2,1),
            0b0001: (2,1),
            0b0010: (2,1),
            0b0011: (2,1),
            0b0100: (2,1),
            0b0101: (2,1),
            0b0110: (2,1),
            0b0111: (2,1),
            0b1000: (3,2),
            0b1001: (3,2),
            0b1010: (3,2),
            0b1011: (3,2),
            0b1100: (4,4),
            0b1101: (5,4),
            0b1110: (6,4),
            0b1111: (7,4),
        }[length_temp]
        bit_field_queue.pop(width)
        return length

class LengthCoder3(object):
    MIN_INITIAL_LEN = 2
    MAX_INITIAL_LEN = 6
    MAX_CONTINUED_LEN = 15

    def __init__(self):
        pass

    @staticmethod
    def encode(length):
        """
        Encode a length into a variable-bit-width value.
        Returns a BitFieldQueue that encodes the length.
        """
        return {
            2: BitFieldQueue(0b0, 1),
            3: BitFieldQueue(0b10, 2),
            4: BitFieldQueue(0b110, 3),
            5: BitFieldQueue(0b1110, 4),
            6: BitFieldQueue(0b1111, 4),
        } [length]
    @staticmethod
    def decode(bit_field_queue):
        """
        Reads a variable-bit-width value from a BitFieldQueue.
        Returns a decoded length. The value is removed from the BitFieldQueue.
        """
        length_temp = bit_field_queue.get(4)
        length, width = {
            0b0000: (2,1),
            0b0001: (2,1),
            0b0010: (2,1),
            0b0011: (2,1),
            0b0100: (2,1),
            0b0101: (2,1),
            0b0110: (2,1),
            0b0111: (2,1),
            0b1000: (3,2),
            0b1001: (3,2),
            0b1010: (3,2),
            0b1011: (3,2),
            0b1100: (4,3),
            0b1101: (4,3),
            0b1110: (5,4),
            0b1111: (6,4),
        }[length_temp]
        bit_field_queue.pop(width)
        return length

class LengthCoder4(object):
    MIN_INITIAL_LEN = 2
    MAX_INITIAL_LEN = 9
    MAX_CONTINUED_LEN = 15

    def __init__(self):
        pass

    @staticmethod
    def encode(length):
        """
        Encode a length into a variable-bit-width value.
        Returns a BitFieldQueue that encodes the length.
        """
        return {
            2: BitFieldQueue(0b00, 2),
            3: BitFieldQueue(0b01, 2),
            4: BitFieldQueue(0b100, 3),
            5: BitFieldQueue(0b101, 3),
            6: BitFieldQueue(0b1100, 4),
            7: BitFieldQueue(0b1101, 4),
            8: BitFieldQueue(0b1110, 4),
            9: BitFieldQueue(0b1111, 4),
        } [length]
    @staticmethod
    def decode(bit_field_queue):
        """
        Reads a variable-bit-width value from a BitFieldQueue.
        Returns a decoded length. The value is removed from the BitFieldQueue.
        """
        length_temp = bit_field_queue.get(4)
        length, width = {
            0b0000: (2,2),
            0b0001: (2,2),
            0b0010: (2,2),
            0b0011: (2,2),
            0b0100: (3,2),
            0b0101: (3,2),
            0b0110: (3,2),
            0b0111: (3,2),
            0b1000: (4,3),
            0b1001: (4,3),
            0b1010: (5,3),
            0b1011: (5,3),
            0b1100: (6,4),
            0b1101: (7,4),
            0b1110: (8,4),
            0b1111: (9,4),
        }[length_temp]
        bit_field_queue.pop(width)
        return length

class LengthCoder5(object):
    MIN_INITIAL_LEN = 2
    MAX_INITIAL_LEN = 7
    MAX_CONTINUED_LEN = 15

    def __init__(self):
        pass

    @staticmethod
    def encode(length):
        """
        Encode a length into a variable-bit-width value.
        Returns a BitFieldQueue that encodes the length.
        """
        return {
            2: BitFieldQueue(0b00, 2),
            3: BitFieldQueue(0b01, 2),
            4: BitFieldQueue(0b10, 2),
            5: BitFieldQueue(0b110, 3),
            6: BitFieldQueue(0b1110, 4),
            7: BitFieldQueue(0b1111, 4),
        } [length]
    @staticmethod
    def decode(bit_field_queue):
        """
        Reads a variable-bit-width value from a BitFieldQueue.
        Returns a decoded length. The value is removed from the BitFieldQueue.
        """
        length_temp = bit_field_queue.get(4)
        length, width = {
            0b0000: (2,2),
            0b0001: (2,2),
            0b0010: (2,2),
            0b0011: (2,2),
            0b0100: (3,2),
            0b0101: (3,2),
            0b0110: (3,2),
            0b0111: (3,2),
            0b1000: (4,2),
            0b1001: (4,2),
            0b1010: (4,2),
            0b1011: (4,2),
            0b1100: (5,3),
            0b1101: (5,3),
            0b1110: (6,4),
            0b1111: (7,4),
        }[length_temp]
        bit_field_queue.pop(width)
        return length

class LengthCoder6(object):
    MIN_INITIAL_LEN = 2
    MAX_INITIAL_LEN = 10
    MAX_CONTINUED_LEN = 15

    def __init__(self):
        pass

    @staticmethod
    def encode(length):
        """
        Encode a length into a variable-bit-width value.
        Returns a BitFieldQueue that encodes the length.
        """
        return {
            2:  BitFieldQueue(0b000, 3),
            3:  BitFieldQueue(0b001, 3),
            4:  BitFieldQueue(0b010, 3),
            5:  BitFieldQueue(0b011, 3),
            6:  BitFieldQueue(0b100, 3),
            7:  BitFieldQueue(0b101, 3),
            8:  BitFieldQueue(0b110, 3),
            9:  BitFieldQueue(0b1110, 4),
            10: BitFieldQueue(0b1111, 4),
        } [length]
    @staticmethod
    def decode(bit_field_queue):
        """
        Reads a variable-bit-width value from a BitFieldQueue.
        Returns a decoded length. The value is removed from the BitFieldQueue.
        """
        length_temp = bit_field_queue.get(4)
        length, width = {
            0b0000: (2,3),
            0b0001: (2,3),
            0b0010: (3,3),
            0b0011: (3,3),
            0b0100: (4,3),
            0b0101: (4,3),
            0b0110: (5,3),
            0b0111: (5,3),
            0b1000: (6,3),
            0b1001: (6,3),
            0b1010: (7,3),
            0b1011: (7,3),
            0b1100: (8,3),
            0b1101: (8,3),
            0b1110: (9,4),
            0b1111: (10,4),
        }[length_temp]
        bit_field_queue.pop(width)
        return length

class LengthCoder7(object):
    MIN_INITIAL_LEN = 2
    MAX_INITIAL_LEN = 16
    MAX_CONTINUED_LEN = 15

    def __init__(self):
        pass

    @staticmethod
    def encode(length):
        """
        Encode a length into a variable-bit-width value.
        Returns a BitFieldQueue that encodes the length.
        """
        return BitFieldQueue(length - 2, 4)
    @staticmethod
    def decode(bit_field_queue):
        """
        Reads a variable-bit-width value from a BitFieldQueue.
        Returns a decoded length. The value is removed from the BitFieldQueue.
        """
        return bit_field_queue.pop(4) + 2

class LengthCoder8(object):
    MIN_INITIAL_LEN = 3
    MAX_INITIAL_LEN = 16
    MAX_CONTINUED_LEN = None

    def __init__(self):
        pass

    @staticmethod
    def encode(length):
        """
        Encode a length into a variable-bit-width value.
        Returns a BitFieldQueue that encodes the length.
        """
        return BitFieldQueue(length - LengthCoder8.MIN_INITIAL_LEN, 4)
    @staticmethod
    def decode(bit_field_queue):
        """
        Reads a variable-bit-width value from a BitFieldQueue.
        Returns a decoded length. The value is removed from the BitFieldQueue.
        """
        return bit_field_queue.pop(4) + LengthCoder8.MIN_INITIAL_LEN
    
class LZCMCoder(object):
    MAX_DICT_SEARCH_LEN = 15

    def __init__(self, offset_coder, length_coder):
        self.offset_coder = offset_coder
        self.length_coder = length_coder
        self.max_dict_search_len = min(LZCMCoder.MAX_DICT_SEARCH_LEN, self.length_coder.MAX_INITIAL_LEN)
        self.dict_size = self.offset_coder.MAX_OFFSET + 1
#        print(u"self.max_dict_search_len = {0}".format(self.max_dict_search_len))

    def compress(self, in_data):

        def add_to_match_dict(offset):
            # Add fragments of each length to dictionary
            for match_len in range(self.length_coder.MIN_INITIAL_LEN, self.max_dict_search_len + 1):
                fragment = in_data[offset : (offset + match_len)]
                if len(fragment) == match_len:
                    match_dict[fragment].append(offset)
            # Delete old fragments from dictionary
            if offset >= self.offset_coder.MAX_OFFSET:
                old_offset = offset - self.offset_coder.MAX_OFFSET
                for match_len in range(self.length_coder.MIN_INITIAL_LEN, self.max_dict_search_len + 1):
                    fragment = in_data[old_offset : (old_offset + match_len)]
                    if len(fragment) == match_len:
                        match_dict[fragment].pop(0)
                        if not match_dict[fragment]:
                            del(match_dict[fragment])

        match_dict = defaultdict(list)
        out_data = []

        in_data_offset = 0
        while in_data_offset < len(in_data):
            # Try to find previous match
            found = False
            for match_len in range(self.max_dict_search_len, self.length_coder.MIN_INITIAL_LEN - 1, -1):
                fragment = in_data[in_data_offset : (in_data_offset + match_len)]
                if len(fragment) == match_len and fragment in match_dict:
                    # We found a match in the dictionary, of length match_len
                    found = True
                    match_offset = match_dict[fragment][-1]
                    # Extend match up to self.length_coder.MAX_INITIAL_LEN if possible
                    for match_len in range(self.length_coder.MAX_INITIAL_LEN, match_len - 1, -1):
                        fragment = in_data[in_data_offset : (in_data_offset + match_len)]
                        if len(fragment) == match_len and fragment == in_data[match_offset:match_offset + match_len]:
                            break
                    out_data.append((match_offset - in_data_offset, match_len))
                    for i in range(0, match_len):
                        add_to_match_dict(in_data_offset + i)
                    in_data_offset += match_len
                    match_offset += match_len

                    # Continue match if possible
                    continue_match_found =  (
                                                (match_len == self.length_coder.MAX_INITIAL_LEN) and
                                                (self.length_coder.MAX_CONTINUED_LEN != None)
                                            )
                    while continue_match_found:
                        for match_len in range(self.length_coder.MAX_CONTINUED_LEN, -1, -1):
                            fragment = in_data[in_data_offset : (in_data_offset + match_len)]
                            if len(fragment) == match_len and fragment == in_data[match_offset:match_offset + match_len]:
                                break
                        out_data.append((None, match_len))
                        for i in range(0, match_len):
                            add_to_match_dict(in_data_offset + i)
                        in_data_offset += match_len
                        match_offset += match_len
                        continue_match_found = (match_len == self.length_coder.MAX_CONTINUED_LEN)
                    break
            if not found:
                out_data.append(in_data[in_data_offset:in_data_offset+1])
                add_to_match_dict(in_data_offset) 
                in_data_offset += 1
        return out_data

    def encode(self, in_data):
        """Encode the compressed tokens to a binary stream"""
        def _do_output():
            while bit_field_queue.width >= 8:
                out_data.append(bit_field_queue.pop(8))
        bit_field_queue = BitFieldQueue()
        out_data = bytearray()
        for token in in_data:
            if isinstance(token, bytes):
                bit_field_queue.append(BitFieldQueue(0,1))
                bit_field_queue.append(BitFieldQueue(ord(token), 8))
            else:
                offset, length = token
                if offset == None:
                    bit_field_queue.append(BitFieldQueue(length, 4))
                else:
                    bit_field_queue.append(BitFieldQueue(1,1))
                    offset = -offset
                    bit_field_queue.append(self.offset_coder.encode(offset))
                    bit_field_queue.append(self.length_coder.encode(length))
            _do_output()

        # Output an end-marker
        bit_field_queue.append(BitFieldQueue(0b1,1))
        bit_field_queue.append(self.offset_coder.encode(EndMarker))
        # Calculate number of bits of padding needed to make a complete byte
        needed_pad = 7 - ((bit_field_queue.width + 7) % 8)
        bit_field_queue.append(BitFieldQueue(0,needed_pad))
        _do_output()

        return bytes(out_data)

    def decode(self, in_data):
        """Decode the compressed tokens to a binary stream"""
        def _load_input_generator():
            for char in in_data:
                if isinstance(char, int):
                    char = bytes( [ char ] )
                while bit_field_queue.width > 23:
                    yield
                bit_field_queue.append(BitFieldQueue(ord(char), 8))
            for _i in range(10):
                yield

        bit_field_queue = BitFieldQueue()
        out_data = []
        _load_input_instance = _load_input_generator()
        _load_input = lambda: next(_load_input_instance)

        _load_input()
        while bit_field_queue.width != 0:
            _load_input()
            token_type = bit_field_queue.pop(1)
            if token_type == 0:
                # Literal
                char = byteschr(bit_field_queue.pop(8))
                out_data.append(char)
            else:
                # Offset, Length pair
                offset = self.offset_coder.decode(bit_field_queue)
                if offset != EndMarker: 
                    offset = -offset
                    length = self.length_coder.decode(bit_field_queue)
                    out_data.append((offset, length))
                    if ((length == self.length_coder.MAX_INITIAL_LEN) and
                        (self.length_coder.MAX_CONTINUED_LEN != None)):
                        while True:
                            _load_input()
                            length = bit_field_queue.pop(4)
                            out_data.append((None, length))
                            if length != self.length_coder.MAX_CONTINUED_LEN:
                                break
                else:
                    # End token -- pop any fraction of a byte
                    bit_field_queue.pop(bit_field_queue.width % 8)

        return out_data

    def gen_decode(self, in_data):
        """Decode the compressed tokens to a binary stream"""
        def _load_input_generator():
            for char in in_data:
                if isinstance(char, int):
                    char = bytes( [ char ] )
                while bit_field_queue.width > 23:
                    yield
                bit_field_queue.append(BitFieldQueue(ord(char), 8))
            for _i in range(10):
                yield

        bit_field_queue = BitFieldQueue()
        _load_input_instance = _load_input_generator()
        _load_input = lambda: next(_load_input_instance)

        _load_input()
        while bit_field_queue.width != 0:
            _load_input()
            token_type = bit_field_queue.pop(1)
            if token_type == 0:
                # Literal
                char = byteschr(bit_field_queue.pop(8))
                yield char
            else:
                # Offset, Length pair
                offset = self.offset_coder.decode(bit_field_queue)
                if offset != EndMarker: 
                    offset = -offset
                    length = self.length_coder.decode(bit_field_queue)
                    yield (offset, length)
                    if ((length == self.length_coder.MAX_INITIAL_LEN) and
                        (self.length_coder.MAX_CONTINUED_LEN != None)):
                        while True:
                            _load_input()
                            length = bit_field_queue.pop(4)
                            yield (None, length)
                            if length != self.length_coder.MAX_CONTINUED_LEN:
                                break
                else:
                    # End token -- pop any fraction of a byte
                    bit_field_queue.pop(bit_field_queue.width % 8)

    def decompress(self, in_data):
        out_data = bytearray()
        for token in in_data:
            if isinstance(token, bytes):
                out_data.append(ord(token))
            else:
                offset, length = token
                if offset != None:
                    match_offset = offset
                for i in range(length):
                    next_char = out_data[match_offset]
                    out_data.append(next_char)
        return bytes(out_data)

    def gen_decompress(self, in_data):
        cbb = CircularBytesBuffer(self.dict_size)
        for token in in_data:
            if isinstance(token, bytes):
                yield token
                cbb.append(token)
            else:
                offset, length = token
                if offset != None:
                    match_offset = offset
                for i in range(length):
#                    print(match_offset)
                    next_char = cbb[match_offset:match_offset+1]
                    yield next_char
                    cbb.append(next_char)
