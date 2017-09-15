#!/bin/bash

TEMPDIR=""
INFILE=""

on_err() {
    echo "Error $INFILE"
}

on_exit() {
    if [ -n $TEMPDIR ]; then
        rm -Rf $TEMPDIR
    fi
}

trap "on_err" ERR
trap "on_exit" EXIT

INFILE="$1"
#echo "$INFILE"
TEMPDIR=$(mktemp -d /tmp/test-lzsXXXXXXXX)
#echo "    $TEMPDIR"

lzs-compress "$1" "$TEMPDIR/file.lzs"
lzs-decompress "$TEMPDIR/file.lzs" "$TEMPDIR/file.lzs.out"

cmp -s "$1" "$TEMPDIR/file.lzs.out"
