#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
set -e

##########################################
#       gobj
##########################################
cd /yuneta/development/yunetas/kernel/gobj-c
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make install

##########################################
#       core-linux
##########################################
cd /yuneta/development/yunetas/kernel/root-c-linux
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make install

##########################################
#       core-linux
##########################################
cd /yuneta/development/yunetas/kernel/root-c-esp32
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make install

##########################################
#       c_prot
##########################################
cd /yuneta/development/yunetas/libs/c_prot
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make install

##########################################
#       all
##########################################
cd /yuneta/development/yunetas
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make install
