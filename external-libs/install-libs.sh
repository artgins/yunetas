#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
set -e

#------------------------------------------
#   Jansson
#------------------------------------------
cd build/jansson-2.14-gines
cd build
make install
cd ..
cd ../..

#------------------------------------------
#   unwind
#------------------------------------------
cd build/libunwind-1.7.0
make install
cd ../..

#------------------------------------------
#   Libuv
#------------------------------------------
cd build/libuv-1.45.0-gines
make install
cd ../..

#------------------------------------------
#   libncurses WARNING falla en kubuntu,
#   pero da igual, kubuntu ya tiene la 6.0
#   (Debian y RedHat no)
#------------------------------------------
cd build/ncurses-6.4
make install
cd ../..

#------------------------------------------
#   PCRE
#------------------------------------------
cd build/pcre2-10.42
make install
cd ../..

#------------------------------------------
#   Nginx
#------------------------------------------
cd build/nginx-1.24.0
make install
cd ../..

#------------------------------------------
#   mbedtls
#------------------------------------------
cd build/mbedtls-3.4.0
cd build
make install
cd ..
cd ../..


#------------------------------------------
#   liburing WARNING  master version!
#------------------------------------------
cd build/liburing-master-2023-06-08
make install
cd ../..
