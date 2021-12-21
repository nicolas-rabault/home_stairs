# home_stairs

This project contain everything allowing you to create your own led animated stairs at home.
This project use :

 - [Kicad](https://www.kicad.org/) for the hardware design
 - [Platformio](https://platformio.org/) as IDE
 - [Luos](https://www.luos.io/) as embedded software architecture
 - [CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html) to generate MCU low level interfaces

## How to open the electronic design :electric_plug:
Electronics have been based on [Luos example electronic design](https://github.com/Luos-io/Examples/tree/master/Hardware) except for [the fuel gauge shield](/hardware/l0-shield/l0_load). You can open and modify all of them using Kicad. This design use Luos_components library for more information to install and use it read [Luos doc](https://docs.luos.io).