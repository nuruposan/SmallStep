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

bool LoggerManager::connect(String name) {
  if (!sppStarted) {
    sppStarted = gpsSerial->begin("SmallStep", true);
  } else if (connected()) {
    disconnect();
  }

  // register the event callback function
  if (eventCallback != NULL) {
    gpsSerial->register_callback(eventCallback);
  }
  gpsSerial->setTimeout(1000);
  gpsSerial->setPin("0000");

  return gpsSerial->connect(name);
}

bool LoggerManager::connect(uint8_t *address) {
  if (!sppStarted) {
    sppStarted = gpsSerial->begin("SmallStep", true);
  } else if (connected()) {
    disconnect();
  }

  // register the event callback function
  if (eventCallback != NULL) {
    gpsSerial->register_callback(eventCallback);
  }
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

  if (sppStarted) {
    gpsSerial->end();
    sppStarted = false;
  }
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

uint8_t LoggerManager::calcNmeaChecksum(const char *cmd) {
  uint8_t chk = 0;
  for (uint8_t i = 0; cmd[i] > 0; i++) {
    chk ^= (uint8_t)(cmd[i]);
  }

  return chk;
}

bool LoggerManager::sendNmeaCommand(const char *cmd) {
  // return false if the GPS logger is not connected
  if (!connected()) return false;

  char sendbuf[256];
  sprintf(sendbuf, "$%s*%X\r\n", cmd, calcNmeaChecksum(cmd));

  // send the NMEA command to the GPS logger
  for (uint16_t i = 0; sendbuf[i] != 0; i++) {
    gpsSerial->write(sendbuf[i]);
  }

  // Note: DO NOT call gpsSerial->flush() after write().
  // It is nessesary and will cause a reception failure.

  return true;
}

bool LoggerManager::sendDownloadCommand(int startPos, int reqSize) {
  char cmdstr[32];
  sprintf(cmdstr, "PMTK182,7,%06X,%04X", startPos, reqSize);

  return sendNmeaCommand(cmdstr);
}

bool LoggerManager::getLastRecordAddress(int32_t *address) {
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

bool LoggerManager::downloadLogData(File32 *output, void (*callback)(int)) {
  bool nextReq = true;
  int32_t reqSize = 0x2000;
  int32_t recvSize = 0;
  int32_t endAddr = 0;
  int32_t nextAddr = 0x000000;
  int8_t retries = 0;

  if (!getLastRecordAddress(&endAddr)) {
    Serial.println("Failed to get the last address of the log data.");
    return false;
  }

  uint32_t timeLimit = millis() + 1000;  // set initial timeout period

  while (gpsSerial->connected()) {
    // exit loop when download process is finished or timeout occurred
    if (nextAddr >= endAddr) {
      Serial.printf("LogMan.download: finished [size=0x%05X]\n", endAddr);
      break;
    }

    if (millis() > timeLimit) {
      nextReq = true;
      retries += 1;

      if (retries > 3) {
        Serial.println("LogMan.download: timeout occurred");
        break;
      }
    }

    // check if the flag to send the next request is set
    if (nextReq) {  // need to send the next download requestsf
      // send the download command to the GPS logger
      sendDownloadCommand(nextAddr, reqSize);

      nextReq = false;  // clear the flag to send the next request
      recvSize = 0;     // clear the received size to 0
      timeLimit = millis() + ((reqSize / 0x800) * 800);  // update timeout

      // print the debug message to the serial console
      Serial.printf(
          "LogMan.download: send request [start=0x%05X, size=0x%04X]\n",
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
      Serial.printf("LogMan.download: recv data [start=0x%05X]\n", startAddr);

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

bool LoggerManager::fixRTCdatetime() {
  if (!gpsSerial->connected()) return false;

  bool sendCommand = true;
  uint32_t timeLimit = 0;  // set the initial timeout period
  int8_t retries = 0;

  while ((gpsSerial->connected())) {
    if (sendCommand) {
      sendNmeaCommand("PMTK335,2020,1,1,0,0,0");
      sendCommand = false;
      timeLimit = millis() + 500;
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
      // exit loop and will return true
      break;
    }
  }

  // clear receive buffer
  buffer->clear();

  return (retries < 3);
}

bool LoggerManager::discover(const char *name, esp_spp_cb_t callback) {
  if (!sppStarted) {
    sppStarted = gpsSerial->begin("SmallStep", true);
  }

  gpsSerial->register_callback(callback);
  if (gpsSerial->connect(name)) {
    gpsSerial->disconnect();
    return true;
  }

  return false;
}

void LoggerManager::setEventCallback(esp_spp_cb_t callback) {
  eventCallback = callback;
}
