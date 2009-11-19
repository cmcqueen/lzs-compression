# -*- coding: utf-8 -*-
"""
General-purpose sliding-window lossless compression
Based on LZ77/78 work
Aiming to be suitable for embedded systems
"""

from __future__ import print_function

from collections import defaultdict


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

class LengthCoder1(object):
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

class LZCM(object):
    MAX_DICT_LEN = 5
    MAX_INITIAL_LEN = 8
    MAX_CONTINUED_LEN = 15
    MAX_OFFSET = (2**11 - 1)
    MAX_SHORT_OFFSET = (2**7 - 1)

    def __init__(self):
        pass

    @staticmethod
    def compress(in_data):

        def add_to_match_dict(offset):
            for match_len in range(2, LZCM.MAX_DICT_LEN + 1):
                fragment = in_data[offset : (offset + match_len)]
                if len(fragment) == match_len:
                    match_dict[fragment].append(offset)

        match_dict = defaultdict(list)
        out_data = []

        in_data_offset = 0
        while in_data_offset < len(in_data):
            # Try to find previous match
            found = False
            for match_len in range(LZCM.MAX_DICT_LEN, 1, -1):
                fragment = in_data[in_data_offset : (in_data_offset + match_len)]
                if len(fragment) == match_len and fragment in match_dict:
                    # We found a match in the dictionary, of length match_len
                    match_offset = match_dict[fragment][-1]
                    # Extend match up to LZCM.MAX_INITIAL_LEN if possible
                    for match_len in range(LZCM.MAX_INITIAL_LEN, match_len - 1, -1):
                        fragment = in_data[in_data_offset : (in_data_offset + match_len)]
                        if len(fragment) == match_len and fragment == in_data[match_offset:match_offset + match_len]:
                            break
                    out_data.append((match_offset - in_data_offset, match_len))
                    found = True
                    for i in range(0, match_len):
                        add_to_match_dict(in_data_offset + i)
                    in_data_offset += match_len
                    match_offset += match_len

                    # Continue match if possible
                    continue_match_found = (match_len == LZCM.MAX_INITIAL_LEN)
                    while continue_match_found:
                        for match_len in range(LZCM.MAX_CONTINUED_LEN, -1, -1):
                            fragment = in_data[in_data_offset : (in_data_offset + match_len)]
                            if len(fragment) == match_len and fragment == in_data[match_offset:match_offset + match_len]:
                                break
                        out_data.append((None, match_len))
                        for i in range(0, match_len):
                            add_to_match_dict(in_data_offset + i)
                        in_data_offset += match_len
                        match_offset += match_len
                        continue_match_found = (match_len == LZCM.MAX_CONTINUED_LEN)
                    break
            if not found:
                out_data.append(in_data[in_data_offset])
                add_to_match_dict(in_data_offset) 
                in_data_offset += 1
        return out_data

    @staticmethod
    def encode(in_data):
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
                    if offset < LZCM.MAX_SHORT_OFFSET:
                        bit_field_queue.append(BitFieldQueue(1,1))
                        bit_field_queue.append(BitFieldQueue(offset, 7))
                    elif offset < LZCM.MAX_OFFSET:
                        bit_field_queue.append(BitFieldQueue(0,1))
                        bit_field_queue.append(BitFieldQueue(offset, 11))
                    else:
                        raise Exception(u"Offset {0} is too big".format(offset))
                    bit_field_queue.append(
                        {
                            2: BitFieldQueue(0b00, 2),
                            3: BitFieldQueue(0b01, 2),
                            4: BitFieldQueue(0b10, 2),
                            5: BitFieldQueue(0b1100, 4),
                            6: BitFieldQueue(0b1101, 4),
                            7: BitFieldQueue(0b1110, 4),
                            8: BitFieldQueue(0b1111, 4),
                        } [length]
                    )
            _do_output()

        # Output an end-marker
        # Calculate number of bits of padding needed
        needed_pad = 7 - ((bit_field_queue.width + 7) % 8)
        bit_field_queue.append(BitFieldQueue(0b11,2))
        bit_field_queue.append(BitFieldQueue(0,7))
        bit_field_queue.append(BitFieldQueue(0,needed_pad))
        _do_output()

        return str(out_data)

    @staticmethod
    def decode(in_data):
        """Decode the compressed tokens to a binary stream"""
        def _load_input_generator():
            for char in in_data:
                while bit_field_queue.width > 16:
                    yield
                bit_field_queue.append(BitFieldQueue(ord(char), 8))
            while True:
                yield

        bit_field_queue = BitFieldQueue()
        out_data = []
        _load_input_instance = _load_input_generator()
        _load_input = _load_input_instance.next

        _load_input()
        while bit_field_queue.width != 0:
            _load_input()
            token_type = bit_field_queue.pop(1)
            if token_type == 0:
                # Literal
                char = chr(bit_field_queue.pop(8))
                out_data.append(char)
            else:
                # Offset, Length pair 
                offset_type = bit_field_queue.pop(1)
                end_token = False
                if offset_type == 0:
                    offset = bit_field_queue.pop(11)
                else:
                    offset = bit_field_queue.pop(7)
                    if offset == 0:
                        # End token
                        end_token = True
                if not end_token:
                    offset = -offset
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
                    out_data.append((offset, length))
                    if length == LZCM.MAX_INITIAL_LEN:
                        while True:
                            _load_input()
                            length = bit_field_queue.pop(4)
                            out_data.append((None, length))
                            if length != LZCM.MAX_CONTINUED_LEN:
                                break
                else:
                    # End token -- pop any fraction of a byte
                    bit_field_queue.pop(bit_field_queue.width % 8)


        return out_data

    @staticmethod
    def decompress(in_data):
        out_data = bytearray()
        for token in in_data:
            if isinstance(token, bytes):
                out_data.append(token)
            else:
                offset, length = token
                if offset != None:
                    match_offset = offset
                for i in range(length):
                    next_char = out_data[match_offset]
                    out_data.append(next_char)
#                    match_offset += 1
        #return "".join(out_data)
        return out_data

if 0:
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

def print_binary(in_string):
    for char in in_string:
        value = ord(char)
        for i in range(7,-1,-1):
            print(1 if (value & (2**i)) else 0, sep=u'', end=u'')
        print()

if 1:
    original_string = u"""Return a string containing a printable representation of an object. For many types, this function makes an attempt to return a string that would yield an object with the same value when passed to eval(), otherwise the representation is a string enclosed in angle brackets that contains the name of the type of the object together with additional information often including the name and address of the object. A class can control what this function returns for its instances by defining a __repr__() method."""
#    original_string = u"That Sam-I-am, that Sam-I-am, I do not like that Sam-I-am.000000000000000000"
    print(original_string)
    original_data = original_string.encode('utf-8')
    compressed_data = LZCM.compress(original_data)
    print(compressed_data)
#    print(u"(length = {0})".format(len(compressed_data)))
    encoded = LZCM.encode(compressed_data)
#    print_binary(encoded)
    print(u"length of encoded data = {0}".format(len(encoded)))

    decoded = LZCM.decode(encoded)
    print(decoded)
    decompressed_data = LZCM.decompress(compressed_data)
    print(decompressed_data.decode('utf-8'))
    print(u"(length = {0})".format(len(decompressed_data)))

    print(u"Compressed size is {0:0.0f}%".format(float(len(encoded))/len(decompressed_data)*100))
