# Steps code project :bulb:
This code is the firmware of each step dealing with gauge sensors, led strip and running steps application.

## How to compile the code :computer:

 1. Download and install [Platformio](https://platformio.org/platformio-ide)
 2. Open this folder into Platformio
 3. Build (Platformio will do the rest)
 4. Upload using Platformio

Alternatively you cns install a Luos_bootloader on your board and compile and upload everything using `./update.sh` command.

This project have been based on a [CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html) code base. Feel free to reuse and modify the `stair_step.ioc` as you want using it.