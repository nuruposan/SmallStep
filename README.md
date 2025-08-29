# SmallStep

## Overview

### What is this?

SmallStep is a portable data downloading and configuration utility for classic MTK GPS Loggers runs on M5Stack Basic.

The software communicates to the your logger using Bluetooth Serial Port Profile (SPP).
You can download the log data from the logger and configure its logging settings on the logger with the software.

## Installation

### Requirement

You need to install [VScode](https://code.visualstudio.com/download), [PlatformIO IDE](https://platformio.org/install/) and [Git](https://git-scm.com/downloads) to your PC.
To install these preresuiqite software, please refer to the instructions on their official websites.

You also need to have a M5Stack Basic that have a SD card run SmallStep. No expansion module required.

### Summary Step

1. Install required software to your PC
2. Clone [SmallStep repositoly](https://github.com/nuruposan/SmallStep) to a local directory
3. Open the directory using VScode
4. Edit `SimpleBeep.h` and fix `include <arduino.h>` to `include <Arduino.h>`
5. Edit `BluetoothSerial.cpp` and increase `RX_QUEUE_SIZE` from 512 to 4094 (bytes)
6. Connect a M5Stack Basic to the PC
7. Run "PlatformIO: Upload" task and install SmallStep to the M5Stack

**Important Notice:**<br>

Please __don't forget to perform step 4 and step 5__.

`SimpleBeep.h` is in `SmallStep/.pio/libdeps/m5stack-core-esp32/M5Stack_SimpleBeep/src/SimpleBeep.h`.
`BluetoothSerial.cpp` is in `$HOME/.platformio/packages/framework-arduinoespressif32/libraries/BluetoothSerial/src/`, **NOT** in the project directory.

If you don't performt them, an error will occured on compiling or get communication error on downloading data.

### Suppored GPS Loggers

SmallStep supports following GPS loggers in the current release.

- Transystem 747PRO
- Holux m-241 (all fw ver.)
  - Changing the log mode (by distance or by time) and the log format (enable additional data to record such as RCR, DOP, DGPS) is not supported on m-241

**Request for users:**<br>
If you have one of MTK GPS loggers below, please let the author its Bluetooth device name.
SmallStep should also work correctly for these models.
But the author cannot provide support them because the device name for discovery/pairing is not known.

- Holux M-1200
- Holux M-1000
- Transystem i-Blue 737
- Transystem i-Blue 747
- Polaris iBT-GPS
- Qstarz BT-1200

## Features

SmallStep provides following features works with your GPS logger.

- Download log data and save it as GPX file
- Fix GPS week number rollover problem
- Clear flash memory of logger
- Change logging mode setting
- Change logging format

## Common issue

- Bluetooth connection between SmallStep(M5Stack) and GPS logger may be unstable or become impossible temporarily.
- Even if the connection is successful, sending and receiving data may not work properly.

**Workaround:**

In these case, please restart the logger and M5Stack.

## Author

nuruposan [Twittter/X](https://x.com/yaeh77)

## License

SmallSteps is licensed under the [Apache 2.0 License](https://www.apache.org/licenses/LICENSE-2.0).
