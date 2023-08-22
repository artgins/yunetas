#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
set -e

##########################################
#       gobj
##########################################
cd /yuneta/development/yuneta/yunetas/gobj
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make install

##########################################
#       core-linux
##########################################
cd /yuneta/development/yuneta/yunetas/core-linux
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make install

##########################################
#       core-linux
##########################################
cd /yuneta/development/yuneta/yunetas/core-esp32
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make install

##########################################
#       c_prot
##########################################
cd /yuneta/development/yuneta/yunetas/c_prot
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make install

##########################################
#       all
##########################################
cd /yuneta/development/yuneta/yunetas
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make install
