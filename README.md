# QuarkLink Getting Started

This project provides instructions on how to get started with QuarkLink to make a secure IoT device using an ESP32 ([esp32-c3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/hw-reference/esp32c3/user-guide-devkitm-1.html)), ([esp32-s3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html)) or an ([esp32-s2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-devkitm-1-v1.html)).

The result is an ESP32 that is using secure boot, flash encryption, has a Root-of-Trust, and which can only be updated Over-The-Air with firmware signed by a key from the QuarkLink Hardware Security Module (HSM).

See the [QuarkLink Getting Started Guide](https://docs.quarklink.io/docs/getting-started-with-quarklink-ignite) for more detailed information on how to use this example project.

## Requirements

There are a few requirements needed in order to get started with this project:

- **PlatformIO**
    The project uses the PlatformIO build environment. If you don't already have this follow the installation instructions [here](https://platformio.org/install).  
    You can verify PlatformIO is installed with ```pio --version``` command:
    ```sh
    >pio --version
    PlatformIO Core, version 6.9.0
    ``` 
- **quarklink-client libraries**
    The quarklink-client library comes in the form of compiled binaries and can be found in the [quarklink-binaries repository](https://github.com/cryptoquantique/quarklink-binaries/tree/main/quarklink-client).  
    Copy the required files into the `lib` folder of this project.  
    For example, if building for `esp32-c3-ds`: copy the file `libquarklink-client-esp32-c3-ds-v1.4.0.a` to the local clone of this repository, inside `lib`.

## Pre-built binaries

To make getting started easy there are pre-built binaries of this project available in the [quarklink-binaries repository](https://github.com/cryptoquantique/quarklink-binaries/tree/main/quarklink-getting-started).  
These binaries can be programmed into the ESP32 device using the QuarkLink provisioning facility. No need for third party programming tools.

## Building this project

To build the project use the ```pio run``` command:
```sh
> pio run
Processing esp32-c3-ds-vefuse (board: esp32-c3-devkitm-1; platform: espressif32 @6.4.0; framework: espidf)
-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------Verbose mode can be enabled via `-v, --verbose` option
Detected OS: Windows
Found patch command at C:\Program Files\Git\usr\bin\patch.exe
Project dir: C:\quarklink-getting-started-esp32-platformio  
. . .     
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32-c3-devkitm-1.html
PLATFORM: Espressif 32 (6.9.0) > Espressif ESP32-C3-DevKitM-1
HARDWARE: ESP32C3 160MHz, 320KB RAM, 4MB Flash

. . .

Creating esp32c3 image...
Merged 2 ELF sections
Successfully created esp32c3 image.
==========================================================================================
[SUCCESS] Took 155.22 seconds 
==========================================================================================
Environment         Status    Duration
------------------  --------  ------------
esp32-c3-ds-vefuse  SUCCESS   00:02:35.219
===========================================================================================
1 succeeded in 00:02:35.219
===========================================================================================

```

The build will create a firmware binary file within the .pio directory:
```sh
>dir /b .pio\build\esp32-c3-ds-vefuse\firmware.bin
firmware.bin
```

The ```firmware.bin``` file is what you upload to QuarkLink. Click on the "Firmwares" option of the QuarkLink main menu to access the uploading function.  
Once uploaded to QuarkLink configure your Batch with the new firmware image and it will be automatically downloaded to the ESP32.  
See the [QuarkLink Getting Started Guide](https://docs.quarklink.io/docs/getting-started-with-quarklink-ignite) for more details.

>**Note:** the `pio run` command will build all the configurations (envs) that are listed as part of `default_envs` at the top of the [platformio.ini](./platformio.ini) file. To build a specific env, either add it to this list as the sole item or build with the command `pio run -e <env_name>`
where `<env_name>` is the desired configuration (e.g. `esp32-c3-ds-release`).

## Configurations
There are several configurations available for this firmware, defined in the platformio.ini file and summarised in the table below:

|Device|Key Management|Type|
---|---|---|
|esp32-c3|Digital Signature peripheral|dev / virtual efuses|
|esp32-c3|Digital Signature peripheral|release|
|esp32-s3|Digital Signature peripheral|dev / virtual efuses|
|esp32-s3|Digital Signature peripheral|release|
|esp32-s2|Digital Signature peripheral|dev / virtual efuses|
|esp32-s2|Digital Signature peripheral|release|


When building, make sure to choose the same configuration that the device was provisioned with via QuarkLink.

The default environment is `esp32-c3-ds-vefuse`, as can be seen from the [example above](#building-this-project). To build the firmware with another configuration (e.g. `esp32-c3-ds-release`), run the command 
```sh
>pio run -e esp32-c3-ds-release
```

>**Note**  
It is assumed that the user has already familiarised with the benefits of using the *Digital Signature peripheral* and the difference between *virtual-efuse* and *release* during the QuarkLink provisioning process.  
More information can be found as part of the espressif programming guide:  
\- [Digital Signature](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/ds.html)  
\- [Virtual eFuses](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/efuse.html#virtual-efuses)

>**⚠️ Important**  
Updating a device from **vefuse** to **release** or vice-versa, will break the device. When performing an OTA update, make sure to select a firmware that is of the same type as the one provisioned.

### Debug
All the environments can be updated to use the debug version of the QuarkLink client library if needed. In order to do this, open the file [platformio.ini](platformio.ini) and update the desired configuration `build_flags` option.  
Example:
```ini
[env:esp32-c3-ds-release]
board = esp32-c3-devkitm-1
build_flags = -Llib -lquarklink-client-esp32-c3-ds-v1.4.0 -Iinclude
```
Becomes:
```ini
[env:esp32-c3-ds-release]
board = esp32-c3-devkitm-1
build_flags = -Llib -lquarklink-client-esp32-c3-ds-v1.4.0-debug -Iinclude
```

## Building project version with RGB LED
This is currently supported for the esp32-c3 and esp32-s3 boards.

To enable the use of the RGB LED, you need to configure its colour before building the project with ```pio run```. This is done by setting the following environment variable:
```PLATFORMIO_BUILD_FLAGS="-DLED_COLOUR=<COLOUR>"```

On Windows systems this is achieved as follows (e.g. green LED):
```sh
>$Env:PLATFORMIO_BUILD_FLAGS="-DLED_COLOUR=GREEN"
```
Whereas on Unix systems the analogous command is:
```sh
export PLATFORMIO_BUILD_FLAGS="-DLED_COLOUR=GREEN"
``` 

>**Note**  
Currently supported colours are: GREEN and BLUE

To reset the LED configuration and disable the LED simply reset the variable:
```sh
>$Env:PLATFORMIO_BUILD_FLAGS=""
```

The device will need to be un-plugged from power for the change to take effect.

## ML-KEM-768 support
This project now supports hybrid Post-Quantum Cryptography using ML-KEM-768. Because hybrid key exchange is only supported in TLS1.3 and above, TLS1.3 has been enabled as part of the sdkconfig. Furthermore, ESP-IDF 5.3.1 or above must be used.
In the directory [patches](./patches/) there are the patch files that apply the necessary changes to the esp-idf and mbedtls stacks, in order to enable ML-KEM-768.
In particular:
- `mlkem_mbedtls.patch`: applies to the mbedtls component, adds all the ML-KEM-768 related files and enables its use in handshake process. 
- `ds_mbedtls.patch`: applies to the mbedtls component, updates Digital Signature specific functions.
- `ds_idf.patch`: applies to esp-idf, updates Digital Signature peripheral specific files to add PKCS#1v2.1 (needed by TLS1.3) support vs 1.5.

The patches are applied automatically to the platformio esp-idf package via the `apply_patch.py` script (i.e. `extra_scripts = pre:patches/apply_patch.py` in `platformio.ini`).

Once the patches have been applied and the application has been built successfully, the binaries can be used for hybrid PQC enabled communication.  

## Further Notes
**Custom Partition Table:** users might be interested in using their own partition table with QuarkLink. Currently, support for this feature is only for paid tiers, however users are welcome to request a custom partition table via the GitHub issues on this project.  
**Firmware size reduction:** Users may wanted to reduce the firmware footprint for this getting started program. This can be achieved by enabling `CONFIG_COMPILER_OPTIMIZATION_SIZE=y` in the sdkconfig file. Moreover further memory optimization techniques can be found in [this link](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-guides/performance/size.html )