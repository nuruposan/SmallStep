#include "MtkLogger.h"

MtkLogger::MtkLogger() {
  memset(&address, 0, sizeof(address));
  gpsSerial = new BluetoothSerial();
  buffer = new NmeaBuffer();
  sppStarted = false;

  // App may be crashed if gpsSerial.begin() is called in the constructor
  // (probably because of M5stack is not ready yet)
};

MtkLogger::~MtkLogger() {
  // disconnect the current conection if it is still connected
  if (connected()) {
    disconnect();
    gpsSerial->end();
  }

  delete gpsSerial;
}

bool MtkLogger::connect(String name) {
  if (!sppStarted) {
    sppStarted = gpsSerial->begin("SmallStep", true);
  } else if (connected()) {
    disconnect();
  }

  Serial.printf("Logger.connect: connect to %s\n", name);

  gpsSerial->setTimeout(1000);
  gpsSerial->setPin("0000");

  if (eventCallback != NULL) {
    eventCallback(ESP_SPP_INIT_EVT, NULL);
  }

  bool conn = gpsSerial->connect(name);
  if ((!conn) && (eventCallback != NULL)) {
    eventCallback(ESP_SPP_UNINIT_EVT, NULL);
  }

  return conn;
}

bool MtkLogger::connect(uint8_t *addr) {
  if (!sppStarted) {
    sppStarted = gpsSerial->begin("SmallStep", true);
  } else if (connected()) {
    disconnect();
  }

  Serial.printf(
      "Logger.connect: "
      "connect to logger %02X%02X-%02X%02X-%02X%02X%\n",
      addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

  // register event callback function
  if (eventCallback != NULL) {
    eventCallback(ESP_SPP_INIT_EVT, NULL);
  }

  // start the SPP as master and set PIN code
  gpsSerial->setTimeout(1000);
  gpsSerial->setPin("0000");

  bool conn = gpsSerial->connect(addr);
  if ((!conn) && (eventCallback != NULL)) {
    eventCallback(ESP_SPP_UNINIT_EVT, NULL);
  }

  // connect to the GPS logger and return the result
  return conn;
}

bool MtkLogger::connected() {
  bool conn = (sppStarted && gpsSerial->connected());
  return conn;
}

void MtkLogger::disconnect() {
  if (!connected()) return;

  gpsSerial->disconnect();
  buffer->clear();

  if (eventCallback != NULL) {
    eventCallback(ESP_SPP_UNINIT_EVT, NULL);
  }
}

int32_t MtkLogger::modelIdToFlashSize(uint16_t modelId) {
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

uint8_t MtkLogger::calcNmeaChecksum(const char *cmd) {
  uint8_t chk = 0;
  for (uint8_t i = 0; cmd[i] > 0; i++) {
    chk ^= (uint8_t)(cmd[i]);
  }

  return chk;
}

bool MtkLogger::sendNmeaCommand(const char *cmd) {
  // return false if the GPS logger is not connected
  if (!connected()) return false;

  char sendbuf[256];
  sprintf(sendbuf, "$%s*%02X\r\n", cmd, calcNmeaChecksum(cmd));

  // send the NMEA command to the GPS logger
  for (uint16_t i = 0; sendbuf[i] != 0; i++) {
    gpsSerial->write(sendbuf[i]);
  }
  // Note: DO NOT call gpsSerial->flush() here!!

  return true;
}

bool MtkLogger::sendDownloadCommand(int startPos, int reqSize) {
  char cmdstr[32];
  sprintf(cmdstr, "PMTK182,7,%06X,%04X", startPos, reqSize);

  return sendNmeaCommand(cmdstr);
}

bool MtkLogger::getLastRecordAddress(int32_t *address) {
  // return false if the GPS logger is not connected
  if (!connected()) return false;

  // set the timeout period to 2 seconds
  uint32_t timeLimit = millis() + 1000;

  // get the last address of the log data to be downloaded
  int32_t endAddr = 0;
  sendNmeaCommand("PMTK182,2,6");  // query log recording mode
  while (gpsSerial->connected()) {
    // abort queries if the timeout reached
    if (millis() > timeLimit) break;

    // read charactor until get a NMEA sentence
    if (!gpsSerial->available()) continue;
    if (!buffer->put(gpsSerial->read())) continue;

    if (buffer->match("$PMTK182,3,6,")) {  // log mode
      int32_t logMode = 0;
      buffer->readColumnAsInt(3, &logMode);

      if (logMode == 2) {                // fullstop mode
        sendNmeaCommand("PMTK182,2,8");  // send query last record address
      } else {                           // overwrite mode
        sendNmeaCommand("PMTK605");      // send query firmware release
      }

      continue;

    } else if (buffer->match("$PMTK182,3,8,")) {  // last record address
      buffer->readColumnAsInt(3, &endAddr);
      break;

    } else if (buffer->match("$PMTK705,")) {  // firmware release
      int32_t modelId = 0;
      buffer->readColumnAsInt(2, &modelId);  // read modelID
      endAddr = modelIdToFlashSize(modelId);
      break;
    }
  }
  // set return value
  *address = endAddr;

  // clear the receive buffer
  buffer->clear();

  // return true if the last address is obtained
  return (endAddr > 0);
}

bool MtkLogger::downloadLog(File32 *output, void (*callback)(int)) {
  const uint8_t MAX_RETRY = 3;

  bool nextReq = true;
  int32_t reqSize = 0x4000;
  int32_t recvSize = 0;
  int32_t endAddr = 0;
  int32_t nextAddr = 0x000000;
  int8_t retries = 0;

  if (callback != NULL) callback(0);

  if (!getLastRecordAddress(&endAddr)) {
    Serial.println("Failed to get the last address of the log data.");
    return false;
  }

  uint32_t timeLimit = millis() + 1000;  // set initial timeout period

  while (gpsSerial->connected()) {
    // exit loop when download process is finished or timeout occurred
    if (nextAddr >= endAddr) {
      Serial.printf("Logger.download: finished [size=0x%05X]\n", endAddr);
      break;
    }

    if (millis() > timeLimit) {
      nextReq = true;
      retries += 1;

      if (retries > MAX_RETRY) {
        Serial.println("Logger.download: timeout occurred");
        break;
      }
    }

    // check if the flag to send the next request is set
    if (nextReq) {  // need to send the next download requestsf
      // send the download command to the GPS logger
      sendDownloadCommand(nextAddr, reqSize);

      nextReq = false;  // clear the flag to send the next request
      recvSize = 0;     // clear the received size to 0
      timeLimit = millis() + ((reqSize / SIZE_REPLY) * 800);  // update timeout

      // print the debug message to the serial console
      Serial.printf(
          "Logger.download: send request [start=0x%05X, size=0x%04X]\n",
          nextAddr, reqSize);
    }

    // receive data (NMEA sentenses) and store them into buffer
    if (!gpsSerial->available()) continue;
    if (!buffer->put(gpsSerial->read())) continue;

    // check if the received line is the expected responses
    if (buffer->match("$PMTK182,8,")) {  // recv data block
      // read the starting address from the 3rd column of the line
      int32_t startAddr = 0;
      buffer->readColumnAsInt(2, &startAddr);

      // ignore this line if the starting address is not the expected value
      if (startAddr != nextAddr) continue;

      // call the callback function to notify the progress
      if (callback != NULL) {  // call the progress function if available
        callback(((float)nextAddr / endAddr) * 100);
      }

      // print the debug message to the serial console
      Serial.printf("Logger.download: recv data [start=0x%05X]\n", startAddr);

      uint8_t b = 0;         // variable to store the next byte
      uint16_t ffCount = 0;  // counter for how many 0xFFs are continuous

      // read the data from the buffer and write it to the output file
      buffer->seekToColumn(3);  // move to the 4th column (data column)
      while (buffer->readHexByteFull(&b)) {
        output->write(b);
        ffCount = (b != 0xFF) ? 0 : (ffCount + 1);  // count continuous 0xFF
      }

      // update the next address and the received size
      nextAddr += 0x0800;
      recvSize += 0x0800;
      nextReq = (recvSize >= reqSize);
      if (ffCount >= 0x200) endAddr = nextAddr;

      continue;
    }  // if (buffer->match("$PMTK182,8,"))
  }    // while (gpsSerial.connected())

  if (callback != NULL) callback(100);

  // close the output file, then clear the buffer
  output->flush();
  buffer->clear();

  // return true if the download process is finished successfully
  return (nextAddr >= endAddr);
}

bool MtkLogger::fixRTCdatetime() {
  if (!connected()) return false;

  bool sendCommand = true;
  uint32_t timeLimit = 0;
  int8_t retries = 0;

  while ((gpsSerial->connected())) {
    if (sendCommand) {
      sendNmeaCommand("PMTK335,2020,1,1,0,0,0");
      sendCommand = false;
      timeLimit = millis() + 1000;
    }

    if (millis() > timeLimit) {
      sendCommand = true;
      retries += 1;
      if (retries >= 3) break;
    }

    if (!gpsSerial->available()) continue;
    if (!buffer->put(gpsSerial->read())) continue;

    // check if the received line is the expected responses
    if (buffer->match("$PMTK001,335,3")) {  // success
      // restart GPS logger to apply new RTC datetime
      sendNmeaCommand("PMTK101");
      break;
    }
  }

  // clear receive buffer
  buffer->clear();

  return (retries < 3);
}

bool MtkLogger::getFlashSize(int32_t *size) {
  // return false if the GPS logger is not connected
  if (!connected()) return false;

  uint32_t timeLimit = millis() + 1000;  // timeout period

  int32_t value = 0;
  sendNmeaCommand("PMTK605");  // send query firmware release
  while (gpsSerial->connected()) {
    if (millis() > timeLimit) {
      Serial.println("Logger.getModelId: timeout occurred");
      break;
    }
    if (!gpsSerial->available()) continue;
    if (!buffer->put(gpsSerial->read())) continue;

    if (buffer->match("$PMTK705,")) {
      int32_t modelId = 0;
      buffer->readColumnAsInt(2, &modelId);  // read modelID
      value = modelIdToFlashSize(modelId);

      Serial.printf("Logger.getFlashSize: flash size = 0x%08X\n", value);
    }
  }
  buffer->clear();

  *size = value;

  return (value > 0);
}

bool MtkLogger::clearFlash(void (*rateCallback)(int32_t)) {
  // return false if the GPS logger is not connected
  if (!connected()) return false;

  if (rateCallback != NULL) rateCallback(0);

  int32_t flashSize = 0;
  if (!getFlashSize(&flashSize)) return false;

  bool success = false;
  int32_t progRate = 0;
  uint32_t timeStartAt = millis();
  uint32_t timeEstimated = (flashSize / SIZE_8MBIT) * 7500;
  uint32_t timeLimit = timeStartAt + 45000;  // 45 sec

  sendNmeaCommand("PMTK182,6,1");
  while (gpsSerial->connected()) {
    delay(5);

    uint32_t timeNow = millis();
    if (timeNow > timeLimit) {
      Serial.println("Logger.clearFlash: timeout occurred");
      break;
    }

    if (rateCallback != NULL) {
      uint32_t timeElapsed = timeNow - timeStartAt;
      int32_t _pr = 100 * ((float)timeElapsed / timeEstimated);
      if (_pr > progRate) {
        rateCallback(_pr);
        progRate = _pr;
      }
    }

    if (!gpsSerial->available()) continue;
    if (!buffer->put(gpsSerial->read())) continue;

    if (buffer->match("$PMTK001,182,6,3")) {  // success
      // restart GPS logger
      sendNmeaCommand("PMTK101");
      success = true;
      break;
    } else if (buffer->match("$PMTK001,182,6,")) {  // failed
      break;
    }
  }
  buffer->clear();

  if (rateCallback != NULL) rateCallback(100);

  return success;
}

void MtkLogger::setEventCallback(esp_spp_cb_t evtCallback) {
  if (evtCallback != NULL) {
    gpsSerial->register_callback(evtCallback);
  }

  eventCallback = evtCallback;
}
