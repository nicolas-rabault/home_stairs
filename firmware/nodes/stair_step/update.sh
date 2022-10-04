#!/bin/bash

# build
pio run -e l0_bootloader -e l0_master_bootloader

pyluos-bootloader reset
# flash slave steps
pyluos-bootloader flash -t 3 4 5 6 7 8 9 10 11 12 -b .pio/build/l0_bootloader/firmware.bin

# flash master steps
pyluos-bootloader flash -t 2 -b .pio/build/l0_master_bootloader/firmware.bin
