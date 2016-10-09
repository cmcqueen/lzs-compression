#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
General-purpose sliding-window lossless compression
Based on LZ77/78 work
Aiming to be suitable for embedded systems
"""

from __future__ import print_function

from lzs import *

def print_binary(in_string):
    for char in in_string:
        value = ord(char)
        for i in range(7,-1,-1):
            print(1 if (value & (2**i)) else 0, sep=u'', end=u'')
        print()

def print_hex_list(in_string):
    count = 0
    for char in in_string:
        if (count % 8) == 0:
            print()
        print("0x{0:02X}, ".format(ord(char)), end='')
        count += 1
    print()

def file_chars_iter(file_object, chunksize=1024):
    while True:
        chunk = file_object.read(chunksize)
        if not chunk:
            break
        for b in chunk:
            yield b

def test_compression(in_data):
    # Make the compression coder, with a chosen length coder
    LZCM = LZCMCoder(OffsetCoder1(7,11), LengthCoder1)
#    LZCM = LZCMCoder(OffsetCoder2(12), LengthCoder8)

#    print(original_string)
    original_data = in_data
    compressed_data = LZCM.compress(original_data)
#    print(compressed_data)
#    print(u"(length = {0})".format(len(compressed_data)))
    encoded = LZCM.encode(compressed_data)
#    print_binary(encoded)
    print_hex_list(encoded)
    print(u"length of encoded data = {0}".format(len(encoded)))

    decoded = LZCM.decode(encoded)
    if 0:
        print(decoded)
        for token in LZCM.gen_decode(encoded):
            print(token)
    if 0:
        decompressed_data = LZCM.decompress(compressed_data)
    if 0:
        decompressed_data = LZCM.decompress(decoded)
    if 1:
        decompressed_data = bytes()
        for i in LZCM.gen_decompress(LZCM.gen_decode(encoded)):
            sys.stdout.write(i)
            decompressed_data += i
        print()
#    print(decompressed_data.decode('utf-8'))
    print(u"(length = {0})".format(len(decompressed_data)))

    if decompressed_data == original_data:
        print(u"Compressed data matches")
    else:
        print(u"Compressed data MISMATCH!")
    print(u"Compressed size is {0:0.0f}%".format(float(len(encoded))/len(decompressed_data)*100))


def main():
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
    
    if 0:
        cbb = CircularBytesBuffer(10)
        cbb.append("abcdefg")
        cbb.append("hijklmnop")
        print(cbb[0])
        print(cbb[1])
        print(cbb[9])
        print(cbb[-1])
        print(cbb[-2])
        print(cbb[-3:])
        pop_stuff = cbb.pop(4)
        print(pop_stuff, type(pop_stuff))
        pop_stuff = cbb.pop(4)
        print(pop_stuff, type(pop_stuff))
        pop_stuff = cbb.pop(4)
        print(pop_stuff, type(pop_stuff))

    if 1:
        strings = [
u"""Return a string containing a printable representation of an object. For many types, this function makes an attempt to return a string that would yield an object with the same value when passed to eval(), otherwise the representation is a string enclosed in angle brackets that contains the name of the type of the object together with additional information often including the name and address of the object. A class can control what this function returns for its instances by defining a __repr__() method.""",

u"""If encoding and/or errors are given, unicode() will decode the object which can either be an 8-bit string or a character buffer using the codec for encoding. The encoding parameter is a string giving the name of an encoding; if the encoding is not known, LookupError is raised. Error handling is done according to errors; this specifies the treatment of characters which are invalid in the input encoding. If errors is 'strict' (the default), a ValueError is raised on errors, while a value of 'ignore' causes errors to be silently ignored, and a value of 'replace' causes the official Unicode replacement character, U+FFFD, to be used to replace input characters which cannot be decoded. See also the codecs module. If no optional parameters are given, unicode() will mimic the behaviour of str() except that it returns Unicode strings instead of 8-bit strings. More precisely, if object is a Unicode string or subclass it will return that Unicode string without any additional decoding applied.
For objects which provide a __unicode__() method, it will call this method without arguments to create a Unicode string. For all other objects, the 8-bit string version or representation is requested and then converted to a Unicode string using the codec for the default encoding in 'strict' mode.
For more information on Unicode strings see Sequence Types — str, unicode, list, tuple, buffer, xrange which describes sequence functionality (Unicode strings are sequences), and also the string-specific methods described in the String Methods section. To output formatted strings use template strings or the % operator described in the String Formatting Operations section. In addition see the String Services section. See also str().
New in version 2.0.
Changed in version 2.2: Support for __unicode__() added.vars([object])
Without an argument, act like locals().
With a module, class or class instance object as argument (or anything else that has a __dict__ attribute), return that attribute.
Note
The returned dictionary should not be modified: the effects on the corresponding symbol table are undefined. [3]
xrange([start], stop[, step])¶
This function is very similar to range(), but returns an “xrange object” instead of a list. This is an opaque sequence type which yields the same values as the corresponding list, without actually storing them all simultaneously. The advantage of xrange() over range() is minimal (since xrange() still has to create the values when asked for them) except when a very large range is used on a memory-starved machine or when all of the range’s elements are never used (such as when the loop is usually terminated with break).""",

u"That Sam-I-am, that Sam-I-am, I do not like that Sam-I-am.000000000000000000",
u"a",
]

        # Choose a string from the above
        original_string = strings[1]
        original_data = original_string.encode('utf-8')
        test_compression(original_data)

    if 0:
        with open(sys.argv[1], "rb") as f:
            in_data = f.read()
            test_compression(in_data)

if __name__ == '__main__':
    main()
