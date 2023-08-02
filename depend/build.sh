#!/bin/bash
set -e

SHELL_FOLDER=$(cd "$(dirname "$0")";pwd)
tar -xzvf zlib-1.2.11.tar.gz
cd $SHELL_FOLDER/zlib-1.2.11
./configure --prefix=/usr/local/qfsviewer --static
make -j4
make install
rm -rf $SHELL_FOLDER/zlib-1.2.11
