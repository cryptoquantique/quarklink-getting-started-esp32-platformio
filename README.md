# Quarklink Getting Started

This project provides instructions on how to get started with QuarkLink to make a secure IoT device using an ESP32 (esp32-c3).

The result is an ESP32 that is using secure boot, flash encryption, has a Root-of-Trust, and which can only be updated Over-The-Air with firmware signed by a key from the QuarkLink Hardware Security Module (HSM).

See the [QuarkLink Getting Started Guide](https://cryptoquantique.github.io/QuarklinkGettingStartedGuide.pdf) for more detailed information on how to use this example project.

## Pre-built binaries

To make getting started easy there are pre-built binaries of this project available [here](https://github.com/cryptoquantique/quarklink-binaries/tree/main/quarklink-getting-started). These binaries can be programmed
into the ESP32 device using the QuarkLink provisioning facility. No need for third party programming tools.

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

The ```firmware.bin``` file is what you upload to Quarklink. Click on the "Firmwares" option of the QuarkLink main menu to access the uploading function. Once uploaded to QuarkLink configure your
Batch with the new firmware image and it will be automatically downloaded to the ESP32. See the [Quarklink Getting Started Guide](https://cryptoquantique.github.io/QuarklinkGettingStartedGuide.pdf)
for more details.

The default environment is esp32-c3, to enable the Quarklink Client Debug environment run the command ```pio run -e esp32-c3-debug```.

## Building project version RGB LED ON (only for esp32-c3 board)

To set the RGB to Green colour use the ``` $Env:PLATFORMIO_BUILD_FLAGS="-DLED_COLOUR=GREEN"``` command before ``` pio run``` :

```
>$Env:PLATFORMIO_BUILD_FLAGS="-DLED_COLOUR=GREEN"
>pio run
```

To set the RGB to Blue use:
```
>$Env:PLATFORMIO_BUILD_FLAGS="-DLED_COLOUR=BLUE"
>pio run
```

To unset the RGB use:
```
>$Env:PLATFORMIO_BUILD_FLAGS=""
>pio run
```
In these case unplug an replug the board
