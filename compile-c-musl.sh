#!/bin/bash

#  Exit immediately if a command exits with a non-zero status.
set -e

#-----------------------------------------------------#
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#-----------------------------------------------------#
if [ -n "$YUNETAS_BASE" ]; then
    YUNETAS_BASE_DIR="$YUNETAS_BASE"
else
    YUNETAS_BASE_DIR="/yuneta/development/yunetas"
fi

##########################################
#       gobj
##########################################
cd "${YUNETAS_BASE_DIR}/kernel/c/gobj-c"
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/musl-gcc ..
make install

##########################################
#       ytls
##########################################
cd "${YUNETAS_BASE_DIR}/kernel/c/ytls"
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/musl-gcc ..
make install

##########################################
#       yev_loop
#       WARNING compile with gcc!!
##########################################
cd "${YUNETAS_BASE_DIR}/kernel/c/yev_loop"
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/cc ..
make install

##########################################
#       timeranger2
##########################################
cd "${YUNETAS_BASE_DIR}/kernel/c/timeranger2"
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/musl-gcc ..
make install

##########################################
#       root-linux
##########################################
cd "${YUNETAS_BASE_DIR}/kernel/c/root-linux"
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/musl-gcc ..
make install

##########################################
#       root-esp32
##########################################
cd "${YUNETAS_BASE_DIR}/kernel/c/root-esp32"
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/musl-gcc ..
make install

##########################################
#       c_prot
##########################################
cd "${YUNETAS_BASE_DIR}/modules/c/c_prot"
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/musl-gcc ..
make install

##########################################
#       utils
##########################################
cd "${YUNETAS_BASE_DIR}/utils/c"
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/musl-gcc ..
make install

##########################################
#       all
##########################################
cd "${YUNETAS_BASE_DIR}"
rm -rf build; mkdir build
cd build; cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/musl-gcc ..
make install
