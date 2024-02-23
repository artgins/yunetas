#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
set -e

##########################################
#       gobj
##########################################
cd /yuneta/development/yunetas/kernel/c/gobj-c
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug ..
make install

##########################################
#       core-linux
##########################################
cd /yuneta/development/yunetas/kernel/c/root-linux
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug ..
make install

##########################################
#       core-linux
##########################################
cd /yuneta/development/yunetas/kernel/c/root-esp32
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug ..
make install

##########################################
#       c_prot
##########################################
cd /yuneta/development/yunetas/libs/c/c_prot
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug ..
make install

##########################################
#       all
##########################################
cd /yuneta/development/yunetas
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug ..
make install
