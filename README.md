# SmallStep

A portable data downloading and configuration utility for classic MTK GPS Loggers runs on M5Stack Basic.

## Overview

## Instruction

### Requirement

You will need to install [VScode](https://code.visualstudio.com/download), [PlatformIO IDE](https://platformio.org/install/) and [Git](https://git-scm.com/downloads) to your PC.
To install these software, please refer to the instructions on their official websites.

You also need to have a M5Stack Basic that have a SD card run SmallStep. No expansion module required.

### Installation

1. Install required software to your PC
2. Clot [SmallStep repositoly](https://github.com/nuruposan/SmallStep) to a local directory
3. Open the directory using VScode
4. Edit `BluetoothSerial.h` and increase `RX_QUEUE_SIZE` from 512 to 4094 (bytes)
5. Connect a M5Stack Basic to the PC
6. Run "PlatformIO: Upload" task and install SmallStep to the M5Stack

**Important Notice:**<br>
Please don't forget to perform Step 4 or you will get a error while downloading log data from your GPS logger.
You can find `BluetoothSerial.h` in `$HOME/.platformio/packages/framework-arduinoespressif32/libraries/BluetoothSerial/src/`, not in the project directory.

### Suppored GPS Loggers

SmallStep supports following GPS loggers in the current release.

- Transystem 747PRO GPS
- Holux M-241 (all fw ver.)
  - Changing the log mode (by distance or time) and the log format (enable additional data to record) is not supported Holux M-241

**Request to the users:**<br>
If you have one of MTK GPS loggers below, please let the author its the Bluetooth device name.
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

**Workaround**
In that case, please restart the logger and M5Stack.

## Author

nuruposan [Twittter/X](https://x.com/yaeh77)

## License

SmallSteps is licensed under the [Apache 2.0 License](https://www.apache.org/licenses/LICENSE-2.0).

