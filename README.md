# Quarklink Getting Started

This project shows an exmaple of get started with Quarklink to make a secure IoT device using an ESP32.

The result is an ESP32 that is using secure boot, flash encryption, has a Root-of-Trust, and which can only be updated Over-The-Air with firmware signed by a key from the Quarklink Hardware Security Module (HSM).

See the [Quarklink Getting Started Guide](https://cryptoquantique.github.io/QuarklinkGettingStartedGuide.pdf) for more detailed information on how to use this example project.

## Pre-built binaries

To make getting started easy there are pre-built binaries of this project available [here](https://github.com/cryptoquantique/quarklink-binaries) 

## Building this project

The project uses the PlatformIO build environment. If you don't already have this follow the installation instructions [here](https://platformio.org/install). You can verify PlatformIO is installed with ```pio --version``` command:
```
>pio --version
PlatformIO Core, version 6.1.7
``` 

To build the project use the ```pio run``` command:
```
>pio run
Processing esp32c3 (board: esp32-c3-devkitm-1; platform: espressif32 @^6.0.0; framework: espidf)
------------------------------------------------------------------------------------------------------------------------
Verbose mode can be enabled via `-v, --verbose` option
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32-c3-devkitm-1.html
PLATFORM: Espressif 32 (6.0.1+sha.5e0a6c3) > Espressif ESP32-C3-DevKitM-1
HARDWARE: ESP32C3 160MHz, 320KB RAM, 4MB Flash

. . .

Creating esp32c3 image...
Merged 2 ELF sections
Successfully created esp32c3 image.
============================================= [SUCCESS] Took 46.14 seconds =============================================

Environment    Status    Duration
-------------  --------  ------------
esp32c3        SUCCESS   00:00:46.141
============================================= 1 succeeded in 00:00:46.141 =============================================
```

The build will create a firmware binary file within the .pio directory:
```
>dir /b .pio\build\esp32c3\firmware.bin
firmware.bin
```

That ```firmware.bin``` file is what you upload to Quarklink.
