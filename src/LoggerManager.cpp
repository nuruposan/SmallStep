#include "LoggerManager.hpp"

LoggerManager::LoggerManager() {
  Serial.printf("DEBUG: LoggerManager::LoggerManager()\n");

  memset(&address, 0, sizeof(address));
  gpsSerial = new BluetoothSerial();
  buffer = new ReceiveBuffer();
  sppStarted = false;

  // App may be crashed if gpsSerial.begin() is called in the constructor
  // (probably because of M5stack is not ready yet)
};

LoggerManager::~LoggerManager() {
  // disconnect the current conection if it is still connected
  if (connected()) {
    disconnect();
    gpsSerial->end();
  }

  delete gpsSerial;
}

bool LoggerManager::connect(uint8_t *address) {
  if (!sppStarted) {
    sppStarted = gpsSerial->begin("SmallStep", true);
  }

  // disconnect the GPS logger if it is already connected
  if (gpsSerial->connected()) {
    disconnect();
  }

  // print the debug message to the serial console
  Serial.printf("DEBUG: connecting to GPS logger...\n");

  // start the SPP as master and set PIN code
  gpsSerial->setTimeout(1000);
  gpsSerial->setPin("0000");

  // connect to the GPS logger and return the result
  return gpsSerial->connect(address);
}

bool LoggerManager::connected() {
  return (sppStarted && gpsSerial->connected());
}

void LoggerManager::disconnect() {
  if (connected()) {
    gpsSerial->disconnect();
    buffer->clear();
  }
}

uint8_t LoggerManager::calcNmeaChecksum(const char *cmd, uint8_t len) {
  byte chk = 0;
  for (byte i = 0; i < len; i++) {
    chk ^= (byte)(cmd[i]);
  }

  return chk;
}

int32_t LoggerManager::modelIdToFlashSize(uint16_t modelId) {
  int32_t flashSize = 0;
  switch (modelId) {
    // 8Mbit flash models
    case 0x1388:  // 757/ZI v1
    case 0x5202:  // 757/ZI v2
      flashSize = SIZE_8MBIT;
      break;

    // 16Mbit flash models
    case 0x0002:  // Qstarz 815
    case 0x001b:  // Transystem i-Blue 747
    case 0x001d:  // BT-Q1000, BGL-32
    case 0x0021:  // Holux M-241
    case 0x0023:  // Holux M-241
    case 0x0043:  // Holux M-241 (FW 1.13)
    case 0x0051:  // Transystem i-Blue 737, Qstarz 810,
                  // Polaris iBT-GPS, Holux M1000
    case 0x0131:  // EB-85A
      flashSize = SIZE_16MBIT;
      break;

    // 32Mbit flash models
    case 0x0000:  // Holux M-1200E
    case 0x0004:  // Transystem747 A+ GPS Trip Recorder
    case 0x0005:  // Qstarz BT-Q1000P
    case 0x0006:  // Transystem 747 A+ GPS Trip Recorder
    case 0x0008:  // Pentagram PathFinder P 3106
    case 0x000F:  // Transystem 747 A+ GPS Trip Recorder
    case 0x8300:  // Qstarz BT-1200
    default:      // assume 4Mbit for unknown models
      flashSize = SIZE_32MBIT;
      break;
  }

  return flashSize;
}

bool LoggerManager::sendNmeaCommand(const char *cmd, uint16_t len) {
  // return false if the GPS logger is not connected
  if (!gpsSerial->connected()) return false;

  uint16_t len2 = len + 8;
  char buf[len2];

  // build the NMEA command line with checksum
  uint8_t chk = calcNmeaChecksum(cmd, len);
  memset(buf, 0, len2);
  sprintf(buf, "$%s*%X\r\n", cmd, chk);

  // send the NMEA command to the GPS logger
  // note: DO NOT call gpsSerial-> flush() after write()s because it takes a bit
  // long time and cause a reception failure
  for (uint16_t i = 0; (i < len2) && (buf[i] != 0); i++) {
    gpsSerial->write(buf[i]);
  }
  // gpsSerial->flush(); // DO NOT call this!!

  return true;
}

bool LoggerManager::sendDownloadCommand(int startPos, int reqSize) {
  char cmdstr[40];
  sprintf(cmdstr, "PMTK182,7,%06X,%06X", startPos, reqSize);

  return sendNmeaCommand(cmdstr, sizeof(cmdstr));
}

bool LoggerManager::getLogEndAddress(int32_t *address) {
  // return false if the GPS logger is not connected
  if (!gpsSerial->connected()) return false;

  // set the timeout period to 2 seconds
  uint32_t timeLimit = millis() + 2000;

  // get the last address of the log data to be downloaded
  int32_t endAddr = 0;
  sendNmeaCommand("PMTK182,2,6", 13);  // query log recording mode
  while (gpsSerial->connected() && (endAddr == 0)) {
    while (gpsSerial->available()) {
      // read the data from the GPS logger and put it into the buffer
      // until NMEA sentence (line) is completed
      if (!buffer->put(gpsSerial->read())) continue;

      // abort queries if the timeout reached
      if (millis() > timeLimit) {  // timeout reached
        endAddr = -1;  // set the last address to -1 (timeout occurred)
        break;
      }

      // check if the received line is the expected responses
      if (buffer->match(
              "$PMTK182,3,6,")) {  // response to the query log recording mode
        int32_t logMode = 0;
        buffer->readColumnAsInt(3, &logMode);

        Serial.printf("DEBUG: got the log mode. [logMode=%d]\n", logMode);

        // determine the log recording mode (overwrap or fullstop)
        if (logMode == 2) {                    // fullstop mode
          sendNmeaCommand("PMTK182,2,8", 13);  // query the record address
        } else {                               // overwrite mode
          sendNmeaCommand("PMTK605", 8);       // query the firmware release
        }
        continue;  // continue to read the next line

      } else if (buffer->match("$PMTK182,3,8,")) {  // response to the query
                                                    // record address
        buffer->readColumnAsInt(
            3, &endAddr);  // read the address from the 4th column

        // print the debug message to the serial console
        Serial.printf(
            "DEBUG: got the address of the last record. [endAddr=0x%06X]\n",
            endAddr);

        continue;  // continue to read the next line
      } else if (buffer->match(
                     "$PMTK705,")) {  // response to the query firmware release
        int32_t modelId = 0;
        buffer->readColumnAsInt(
            2, &modelId);  // read the firmware ID from the 3rd column

        // print the debug message to the serial console
        Serial.printf(
            "DEBUG: got the model ID of GPS logger. [modelId: 0x%04X]\n",
            modelId);

        // set the download size based on the model ID
        endAddr = modelIdToFlashSize(modelId);

        // print the debug message to the serial console
        Serial.printf(
            "DEBUG: got the address of the last record. [endAddr=0x%06X]\n",
            endAddr);
      }  // if (buffer->match("$PMTK182,3,6,")) else if ...ss
    }    // while (gpsSerial.available())
  }      // while (gpsSerial.connected() && (endAddr == 0))

  // clear the receive buffer
  buffer->clear();

  // set return value
  *address = endAddr;

  // return true if the last address is obtained
  return (endAddr > 0);
}

bool LoggerManager::downloadLogData(File32 *output, void (*callback)(int)) {
  bool nextRequest = true;
  int32_t requestSize = 0x4000;
  int32_t receivedSize = 0;
  int32_t endAddr = 0;
  int32_t nextAddr = 0x000000;
  int8_t retryCount = 0;

  if (!getLogEndAddress(&endAddr)) {
    Serial.println("Failed to get the last address of the log data.");
    return false;
  }

  uint32_t timeLimit = millis() + 1000;  // set the initial timeout period

  // download the log data from the GPS logger
  while (gpsSerial->connected()) {  // loop until the SPP is connected
    // check if any termination condition is met
    if (nextAddr >= endAddr) {  // reached to the end of the data
      // print the debug message to the serial console
      Serial.printf(
          "DEBUG: the download process is finished. [nextAddr=0x%06X, "
          "endAddr=0x%06X]\n",
          nextAddr, endAddr);

      // break the loop to finish the download process
      break;
    } else if (retryCount > 5) {  // cause of too many retries
      // print the debug message to the serial console
      Serial.printf(
          "DEBUG: too many retries has occurred, abort. [retries=%d]\n",
          (retryCount - 1));

      // break the loop to abort the download process
      break;
    }

    // check if the flag to send the next request is set
    if (nextRequest) {  // need to send the next download requestsf
      // send the download command to the GPS logger
      sendDownloadCommand(nextAddr, requestSize);

      nextRequest = false;          // clear the flag to send the next request
      receivedSize = 0;             // clear the received size to 0
      timeLimit = millis() + 1000;  // update the timeout period

      // print the debug message to the serial console
      Serial.printf(
          "LoggerManager.download: "
          "sent request [startAddr=0x%06X, reqSize=0x%04X, retries=%d]\n",
          nextAddr, requestSize, retryCount);
    }

    // set the flag to retry if the timeout reached
    if (millis() > timeLimit) {  // timeout reached
      // set the flag to send the next request and increase the retry count
      nextRequest = true;
      retryCount += 1;

      // print the debug message to the serial console
      Serial.printf(
          "LoggerManager.download: "
          "timeout reached, abort. [retries=%d]\n",
          retryCount);

      // continue to send the next request
      continue;
    }

    // receive data (NMEA sentenses) from the GPS logger and parse it
    while (gpsSerial->available()) {  // repeat until the SPP is available
      // read the data from the GPS logger and put it into the buffer
      // until NMEA sentence (line) is completed
      if (!buffer->put(gpsSerial->read())) continue;

      // check if the received line is the expected responses
      if (buffer->match("$PMTK182,8,")) {  // received the next sector
        // read the starting address from the 3rd column of the line
        int32_t startAddr = 0;
        buffer->readColumnAsInt(2, &startAddr);

        // ignore this line if the starting address is not the expected value
        if (startAddr != nextAddr) continue;

        // print the debug message to the serial console
        Serial.printf(
            "LoggerManager.download: "
            "received data [startAddr=0x%06X]\n",
            startAddr);

        uint8_t b = 0;            // variable to store the next byte
        uint16_t ffCount = 0;     // counter for how many 0xFFs are continuous
        buffer->seekToColumn(3);  // move to the 4th column (data column)
        while (buffer->readHexByteFull(&b)) {  // read the next byte
          output->write(b);                    // write the byte to the file
          ffCount =
              (b != 0xFF) ? 0 : ffCount + 1;  // count the continuous 0xFFs
        }

        // update the next address and the received size
        nextAddr += 0x0800;
        receivedSize += 0x0800;

        // call the callback function to notify the progress
        if (callback != NULL) {  // call the progress function if available
          callback(((float)nextAddr / endAddr) * 100);
        }

        // set the download size to the current address if the continuous 0xFFs
        // are more than 0x200
        if ((ffCount >= 0x0200) && (nextAddr < endAddr)) {
          endAddr = nextAddr;  // set the download size to the current address

          // print the debug message to the serial console
          Serial.printf(
              "LoggerManager.download:"
              "reached to the end of the data. [endAddr=0x%06X]\n",
              endAddr);
        }

        // update the timeout period
        timeLimit = millis() + 1000;

        // continue to read the next line
        continue;
      } else if (buffer->match(
                     "$PMTK001,182,7,")) {  // received operation result line
        nextRequest = true;  // set the flag to send the next request
        if (receivedSize < requestSize)
          retryCount += 1;  // received size is less than requested size,
                            // increase retry count

        // break the loop to send the next request
        break;
      }  // if (buffer->match("$PMTK182,8,")) else if ...
    }    // while (gpsSerial.available())
  }      // while (gpsSerial.connected())

  if (callback != NULL) callback(100);

  // close the output file, then clear the buffer
  output->flush();
  buffer->clear();

  // return true if the download process is finished successfully
  return (nextAddr >= endAddr);
}

bool LoggerManager::fixRTCdatetime() {
  if (!gpsSerial->connected()) return false;

  bool sendCommand = true;
  uint32_t timeLimit = 0;  // set the initial timeout period
  int8_t retryCount = 0;

  while ((gpsSerial->connected())) {
    if ((retryCount > 3) || (retryCount == -1)) {
      break;
    }

    if (sendCommand) {
      sendNmeaCommand("PMTK335,2020,1,1,0,0,0", 23);
      sendCommand = false;
      timeLimit = millis() + 1000;
    }

    if (millis() > timeLimit) {
      sendCommand = true;
      retryCount += 1;
      continue;  // continue to retry to send set rtc command
    }

    while (gpsSerial->available()) {
      // read the data from the GPS logger and put it into the buffer
      if (!buffer->put(gpsSerial->read())) continue;

      // check if the received line is the expected responses
      if (buffer->match("$PMTK001,335,")) {  // received operation result line
        // read the result code from the 3rd column
        int32_t resultCode = 0;
        buffer->readColumnAsInt(2, &resultCode);

        if (resultCode == 3) {  // success
          // set retryCount to -1 to indicate success
          retryCount = -1;
        } else {  // failed
          //  set sendCommand to true and increase retryCount
          sendCommand = true;
          retryCount += 1;
          delay(100);  // wait for a while before retry
        }

        break;  // break to exit or retry
      }
    }
  }

  buffer->clear();

  return (retryCount == -1);
}
