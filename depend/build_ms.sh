#!/bin/bash
set -e

SHELL_FOLDER=$(cd "$(dirname "$0")";pwd)
tar -xzvf zlib-1.2.11.tar.gz
cd $SHELL_FOLDER/zlib-1.2.11
make -f win32/Makefile.gcc install \
 INCLUDE_PATH=/d/qfsviewer/include \
 LIBRARY_PATH=/d/qfsviewer/lib \
 BINARY_PATH=/d/qfsviewer/bin 
rm -rf $SHELL_FOLDER/zlib-1.2.11
