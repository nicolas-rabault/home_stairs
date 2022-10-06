#!/bin/bash

# This script allow to update the complete product.
#    - Step 1 compile all binaries for every boards
#    - Step 2 deploy all binaries on every boards
# For additional information, please refer to the README.md file.

# Compile all binaries. (In this case the code is the same but with 2 different configurations depending on the detection initiation)
pio run -e l0_bootloader -e l0_detector_bootloader

# Flash the normal steps
pyluos-bootloader flash -t 3 4 5 6 7 8 9 10 11 12 -b .pio/build/l0_bootloader/firmware.bin

# Flash the detector steps (the one on the botom)
pyluos-bootloader flash -t 2 -b .pio/build/l0_detector_bootloader/firmware.bin
