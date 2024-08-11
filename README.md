# LCC Tortoise

Code for LCC Tortoise.  Uses Zephyr RTOS.

## Build and Compile

1. follow zephyr [getting started guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
2. Run the following commands:
```
west init -m github.com:rm5248/lcc-tortoise.git -mr master my-workspace
cd my-workspace
west update
```
3. Build and run:
```
cd lcc-tortoise
west build -b lcc_tortoise lcc-tortoise
west flash
```

## Bootloader

This is an STM32CubeIDE project with a pre-compiled hex file.  This is a very
simple bootloader that is loosely compatible with mcuboot.  It is designed to
be much smaller than mcuboot, which it mostly achieves.

Gold LED blinking fast: Fatal error, unable to move secondary image to primary
Blue LED blinking fast: Fatal error, no image in primary or secondary partition

## Use Stm32CubeIDE

https://github.com/zephyrproject-rtos/zephyr/discussions/69812

## License

GPL v2 ONLY
