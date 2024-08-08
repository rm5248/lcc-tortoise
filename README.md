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
```

## Use Stm32CubeIDE

https://github.com/zephyrproject-rtos/zephyr/discussions/69812

## License

GPL v2 ONLY
