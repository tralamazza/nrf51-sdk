# nrf51-sdk

Build infrastructure for using the nrf51 SDK


## Getting started

Download the Nrf51 SDK from http://developer.nordicsemi.com/nRF51_SDK/ and unzip it here.


## FAQ

1. My build fails with `ld: cannot find -lc_s`

The Nrf SDK uses `libc_s` which was replaced by `libc_nano` in GCC 4.9.
Go to your lib folder (e.g. `gcc-arm-none-eabi-4_9-2014q4/arm-none-eabi/lib`) and create a link to `libc_s.a`:

    ln -s libc_nano.a libc_s.a
