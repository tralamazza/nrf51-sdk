# nrf51-sdk

Build infrastructure for using the nrf51 SDK


## Getting started

Download the Nrf51 SDK 8.0 from http://developer.nordicsemi.com/nRF51_SDK/nRF51_SDK_v8.x.x/ and unzip it into `nordic`.

Download the Segger RTT library https://www.segger.com/jlink-real-time-terminal.html and unzip it into `segger`.

## FAQ

1. My build fails with `ld: cannot find -lc_s`

The Nrf SDK uses `libc_s` which was replaced by `libc_nano` in GCC 4.9.
Go to your lib folder (e.g. `gcc-arm-none-eabi-4_9-2014q4/arm-none-eabi/lib`) and create a link to `libc_s.a`:

    ln -s libc_nano.a libc_s.a

Do this for the subfolders as well (armv6 for nrf51, armv7 for freescale  ...).
