#!/bin/sh

basedir=`pwd`

mkdir -p lua_build && cd lua_build
curl -R -O http://www.lua.org/ftp/lua-5.4.6.tar.gz
tar zxf lua-5.4.6.tar.gz
cd lua-5.4.6
make linux test
sudo make install
cd $basedir

mkdir -p build && cd build
cmake ..
make
sudo make install
cd $basedir
