#!/bin/sh

export APPROOT=/CIOS/Development/ZOS/Applications/Linguistics
export SRCROOT=${APPROOT}/CiosAudio
export DATEFMT=`date "+%Y-%m%d"`
export STATUS=beta
export VERNUM=1.5.11

mkdir -p CiosAudio
mkdir -p CaPlay
mkdir -p CiosAudio-Mac-x64

cd CiosAudio

rm -rf *

cmake ${SRCROOT}/Compile/MacOSX

make
sudo make install

cd ..

cd CaPlay

rm -rf *

cmake ${SRCROOT}/Examples/CaPlay/MacOSX/x64
make
sudo make install

cd ..

cd CiosAudio-Mac-x64

cp -f /usr/local/bin/CaPlay bin
cp -f /usr/local/include/CiosAudio.hpp include
cp -f /usr/local/lib/libCaCore.a lib

cd ..

tar cfz CiosAudio-Mac-x64-${VERNUM}-${STATUS}-${DATEFMT}.tar.gz CiosAudio-Mac-x64
