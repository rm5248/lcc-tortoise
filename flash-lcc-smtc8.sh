#!/bin/bash

STM32_CLI=~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI

# Set nBOOT0 to 0 to allow us to start the bootloader
"$STM32_CLI" -c port=swd -ob nBOOT_SEL=0
# Flash the main code
"$STM32_CLI" -c port=swd -w build/lcc-smtc8/zephyr/lcc-smtc8.signed.bin 0x08008000
# Flash the bootloader
"$STM32_CLI" -c port=swd -w build/mcuboot/zephyr/lcc-smtc8-bootloader.bin 0x08000000
# Start it up
#"$STM32_CLI" -c port=swd --start 0x08000000
